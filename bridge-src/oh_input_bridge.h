/*
 * oh_input_bridge.h
 *
 * Native input event bridge between OH MMI and Android InputChannel.
 *
 * Manages per-session InputPublisher instances that write Android-format
 * MotionEvents to the server side of an InputChannel pair. ViewRootImpl
 * reads from the client side, completing the touch event pipeline.
 *
 * Also monitors OH input event fds (obtained during session creation)
 * and forwards events through the InputPublisher.
 */
#ifndef OH_INPUT_BRIDGE_H
#define OH_INPUT_BRIDGE_H

#include <jni.h>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <thread>
#include <atomic>

// Forward decls (full OH MMI headers pulled in by .cpp only — keep .h lean)
namespace OHOS {
namespace MMI {
struct IInputEventConsumer;
}
namespace AppExecFwk {
class EventHandler;
}
}

namespace oh_adapter {

/**
 * Manages input event bridging for all window sessions.
 *
 * For each window session, holds:
 * - The server-side InputChannel fd (from Java InputChannel pair)
 * - The OH input event fd (from OH session creation)
 * - An event forwarding mechanism between the two
 */
class OHInputBridge {
public:
    static OHInputBridge& getInstance();

    /**
     * Register a server-side Android InputChannel fd for a session.
     * Called from Java when InputEventBridge creates a channel pair.
     */
    void registerInputChannel(int32_t sessionId, int serverFd);

    /**
     * Unregister and clean up for a session.
     */
    void unregisterInputChannel(int32_t sessionId);

    /**
     * Register the OH-side input event fd for a session.
     * Called when the OH session is created and returns its input channel fd.
     * Starts monitoring for OH input events on this fd.
     */
    void registerOHInputFd(int32_t sessionId, int ohInputFd);

    /**
     * Inject a touch event into the Android InputChannel for a session.
     * Converts parameters to Android InputMessage format and writes
     * to the server-side fd.
     *
     * @param sessionId  Window session ID
     * @param action     MotionEvent action (0=DOWN, 1=UP, 2=MOVE)
     * @param x          Touch X coordinate in window space
     * @param y          Touch Y coordinate in window space
     * @param downTime   Timestamp of ACTION_DOWN in nanoseconds
     * @param eventTime  Timestamp of this event in nanoseconds
     * @return 0 on success, negative on error
     */
    int32_t injectTouchEvent(int32_t sessionId, int32_t action,
                              float x, float y,
                              int64_t downTime, int64_t eventTime);

    /**
     * Inject an Android KeyEvent into the session's InputChannel (D-pad/keys).
     * action: AKEY_EVENT_ACTION_DOWN(0)/UP(1); androidKeyCode: AOSP keycode.
     */
    int32_t injectKeyEvent(int32_t sessionId, int32_t action,
                            int32_t androidKeyCode,
                            int64_t downTime, int64_t eventTime);

    /**
     * Dispatch a key DIRECTLY into the focused window's ViewRootImpl, bypassing
     * the InputChannel consumer (whose KEY case is a stub on the deployed
     * runtime 16e08711). Builds an android.view.KeyEvent via JNI, locates the
     * focused ViewRootImpl's mInputEventReceiver through WindowManagerGlobal
     * reflection, and posts it on the main looper via
     * adapter.window.InputEventBridge.dispatchOnMainThread (the same helper the
     * MOTION path uses). Returns 0 on success, negative on failure (caller may
     * fall back to injectKeyEvent's InputChannel write).
     */
    int32_t dispatchKeyViaViewRoot(int32_t action, int32_t androidKeyCode,
                                   int64_t downTime, int64_t eventTime);

    /**
     * Dispatch a touch MotionEvent DIRECTLY into the focused window's
     * ViewRootImpl (same bypass as dispatchKeyViaViewRoot). Builds an
     * android.view.MotionEvent (source=TOUCHSCREEN) and posts it on the main
     * looper via InputEventBridge.dispatchOnMainThread → ViewRootImpl touch
     * stages → View.dispatchTouchEvent (onClick / nav-tab selection). Returns
     * 0 on success, negative on failure (caller may fall back to
     * injectTouchEvent's InputChannel write).
     */
    int32_t dispatchTouchViaViewRoot(int32_t action, float x, float y,
                                     int64_t downTime, int64_t eventTime);

    /**
     * Set JNI context for callbacks to Java layer.
     */
    void setJavaVM(JavaVM* jvm) { jvm_ = jvm; }

    /**
     * Subscribe to OH MMI input events for the given window session.
     *
     * Called once per process from createSession after the window is added.
     * Internally instantiates an MMI::IInputEventConsumer subclass and calls
     * MMI::InputManager::GetInstance()->SetWindowInputEventConsumer to register
     * it with the MMI service. The MMI service then dispatches PointerEvent /
     * KeyEvent / AxisEvent to the consumer in this process; the consumer
     * forwards them to injectTouchEvent / injectKeyEvent (Phase 2) using the
     * activeSessionId_ as the target window session.
     *
     * Idempotent — subsequent calls just update activeSessionId_.
     *
     * Implements the missing piece identified in
     * doc/Input_Adapter_design.html §3.3.4 / §3.3.5.
     *
     * @param sessionId  Window session ID that this process's input events
     *                   should be routed to.
     */
    void subscribeMmi(int32_t sessionId);

    /**
     * Start a background poller that reads tap commands from a control file
     * (/data/local/tmp/noice_tap) and fires an IN-PROCESS tap via
     * dispatchTouchViaViewRoot. This is a reliable input side-channel that
     * bypasses the foundation-flaky OHOS MMI->process pointer delivery
     * (displayId:-1): write "X Y" (raw coords) or a single digit "N" (1..5 =
     * bottom-nav tab N) to the file and the bridge dispatches it on the UI
     * thread. Started once per process from subscribeMmi.
     */
    void startTapControlChannel();

    /**
     * Get the current active session id (for the MMI consumer to route to).
     * Returns -1 if subscribeMmi has never been called.
     */
    int32_t getActiveSessionId() const { return activeSessionId_.load(); }

private:
    OHInputBridge() = default;
    ~OHInputBridge() = default;

    struct SessionInput {
        int serverFd = -1;      // Android server-side InputChannel fd
        int ohInputFd = -1;     // OH input event fd
        uint32_t seq = 0;       // Sequence number for InputPublisher protocol
    };

    JavaVM* jvm_ = nullptr;
    std::mutex mutex_;
    std::unordered_map<int32_t, SessionInput> sessions_;

    // OH input event monitoring
    std::atomic<bool> monitoring_{false};
    std::thread monitorThread_;

    // MMI consumer subscription state (single per process).
    // Held as shared_ptr so MMI service can keep using it after subscribeMmi
    // returns; activeSessionId_ tells the consumer which session to route to
    // (HelloWorld has 1 main window, so this is effectively constant after
    // the first subscribeMmi call).
    std::shared_ptr<OHOS::MMI::IInputEventConsumer> mmiConsumer_;
    std::shared_ptr<OHOS::AppExecFwk::EventHandler> mmiEventHandler_;
    std::atomic<int32_t> activeSessionId_{-1};
    std::atomic<bool> mmiSubscribed_{false};

    /**
     * Write an InputMessage to the server fd.
     * Constructs a minimal InputMessage struct for a single-pointer MotionEvent.
     */
    int writeMotionEvent(int fd, uint32_t seq, int32_t action,
                          float x, float y,
                          int64_t downTime, int64_t eventTime);

    /**
     * Write a single-key Android KeyEvent InputMessage to the server fd.
     */
    int writeKeyEvent(int fd, uint32_t seq, int32_t action,
                       int32_t keyCode, int64_t downTime, int64_t eventTime);

    /**
     * Monitor thread: polls OH input fds and forwards events.
     */
    void monitorOHInputEvents();
};

}  // namespace oh_adapter

#endif  // OH_INPUT_BRIDGE_H
