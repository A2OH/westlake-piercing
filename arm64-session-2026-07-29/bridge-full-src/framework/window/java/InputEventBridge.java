/*
 * InputEventBridge.java
 *
 * Bridges OH input events to Android InputChannel.
 *
 * When OH's MMI framework delivers touch/key events to an adapter window,
 * this class receives them (via JNI from the native layer) and publishes
 * them to the Android InputChannel's server end. ViewRootImpl's
 * WindowInputEventReceiver reads from the client end, completing the pipeline:
 *
 *   OH MMI -> OH Input Channel fd -> Native InputEventBridge
 *   -> InputPublisher -> Android InputChannel (server)
 *   -> Android InputChannel (client) -> ViewRootImpl -> View hierarchy
 */
package adapter.window;

import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.util.Log;
import android.view.InputChannel;
import android.view.InputEvent;
import android.view.InputEventReceiver;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

public class InputEventBridge {

    private static final String TAG = "OH_InputEventBridge";
    private static volatile InputEventBridge sInstance;

    // Maps window token -> session ID for input routing
    private final Map<IBinder, Integer> mWindowSessionMap = new ConcurrentHashMap<>();

    // Maps session ID -> server-side InputChannel
    private final Map<Integer, InputChannel> mServerChannels = new ConcurrentHashMap<>();

    // Lazily-initialized handler bound to the main looper, used to post
    // InputEventReceiver.dispatchInputEvent calls onto the UI thread. See
    // dispatchOnMainThread below for full rationale.
    private static volatile Handler sMainHandler;

    private static Handler getMainHandler() {
        if (sMainHandler == null) {
            synchronized (InputEventBridge.class) {
                if (sMainHandler == null) {
                    sMainHandler = new Handler(Looper.getMainLooper());
                }
            }
        }
        return sMainHandler;
    }

    // Cached reflection handle for InputEventReceiver.dispatchInputEvent(int, InputEvent),
    // which is declared private — direct Java invocation would fail with
    // IllegalAccessException, so we use setAccessible(true) once at first use.
    // (Native JNI bypasses Java access control and could call it directly, but
    // we want to keep the post-to-main-looper logic on the Java side where
    // Handler/Looper/Runnable are first-class — JNI roundtripping a Runnable
    // back into native is heavier than this one-time reflection setup.)
    private static volatile java.lang.reflect.Method sDispatchMethod;

    private static java.lang.reflect.Method resolveDispatchMethod() {
        java.lang.reflect.Method m = sDispatchMethod;
        if (m == null) {
            synchronized (InputEventBridge.class) {
                m = sDispatchMethod;
                if (m == null) {
                    try {
                        m = InputEventReceiver.class.getDeclaredMethod(
                                "dispatchInputEvent", int.class, InputEvent.class);
                        m.setAccessible(true);
                        sDispatchMethod = m;
                    } catch (Throwable t) {
                        Log.e(TAG, "resolveDispatchMethod failed: " + t);
                    }
                }
            }
        }
        return m;
    }

    /**
     * Post an InputEventReceiver.dispatchInputEvent invocation onto the main
     * Looper thread.
     *
     * Called from native (android_view_InputEventReceiver.cpp's
     * dispatchMotionFromWorker) after a MotionEvent has been constructed in
     * the InputChannel-polling worker thread.  Worker-thread invocation is NOT
     * safe: ViewRootImpl.enqueueInputEvent runs with processImmediately=true,
     * synchronously dispatching the event through View.dispatchPointerEvent on
     * the calling thread.  Material Theme Buttons use RippleDrawable as
     * background; pressed-state transition calls
     * RippleDrawable.onStateChange -> setBackgroundActive ->
     * exitPatternedBackgroundAnimation -> ValueAnimator.cancel, which enforces
     * Looper-thread-only via "Animators may only be run on Looper threads"
     * AndroidRuntimeException.  Posting via this helper ensures the entire
     * dispatch chain runs on the main looper, matching AOSP's native model
     * where InputEventReceiver registers its fd via Looper.addFd and so
     * dispatchInputEvent is naturally invoked on the main thread.
     *
     * Pattern matches existing adapter Java helpers in WindowCallbackBridge,
     * LifecycleAdapter, AppSchedulerBridge etc. that post OH-callback-thread
     * work back onto the main looper.
     */
    public static void dispatchOnMainThread(final InputEventReceiver receiver,
                                            final int seq,
                                            final InputEvent event) {
        if (receiver == null || event == null) {
            return;
        }
        final java.lang.reflect.Method m = resolveDispatchMethod();
        if (m == null) {
            Log.w(TAG, "dispatchOnMainThread: dispatchInputEvent method not resolved, dropping event");
            return;
        }
        Handler h = getMainHandler();
        // Fast path: already on main thread (defensive — current native path
        // always calls from worker, but stays correct if invoked from main).
        if (Looper.myLooper() == h.getLooper()) {
            try {
                m.invoke(receiver, Integer.valueOf(seq), event);
            } catch (Throwable t) {
                Log.w(TAG, "dispatchOnMainThread inline: " + t);
            }
            return;
        }
        h.post(new Runnable() {
            @Override
            public void run() {
                try {
                    m.invoke(receiver, Integer.valueOf(seq), event);
                } catch (Throwable t) {
                    Log.w(TAG, "dispatchOnMainThread post: " + t);
                }
            }
        });
    }

    public static InputEventBridge getInstance() {
        if (sInstance == null) {
            synchronized (InputEventBridge.class) {
                if (sInstance == null) {
                    sInstance = new InputEventBridge();
                }
            }
        }
        return sInstance;
    }

    private InputEventBridge() {
    }

    /**
     * Create an InputChannel pair for a window.
     * Returns the client-side InputChannel (for ViewRootImpl).
     * The server-side channel is retained and registered with the native layer.
     *
     * @param windowToken Android IWindow binder token
     * @param sessionId   OH window session ID
     * @param windowName  Window name for debugging
     * @return Client-side InputChannel to be given to ViewRootImpl
     */
    public InputChannel createInputChannelPair(IBinder windowToken, int sessionId,
                                                String windowName) {
        String channelName = windowName + " (OH session " + sessionId + ")";
        InputChannel[] channels = InputChannel.openInputChannelPair(channelName);

        // channels[0] = server (we write to this)
        // channels[1] = client (ViewRootImpl reads from this)
        InputChannel serverChannel = channels[0];
        InputChannel clientChannel = channels[1];

        mServerChannels.put(sessionId, serverChannel);
        mWindowSessionMap.put(windowToken, sessionId);

        // Register the server channel's fd with the native InputPublisher
        nativeRegisterInputChannel(sessionId, serverChannel);

        Log.i(TAG, "InputChannel pair created: session=" + sessionId
                + ", name=" + channelName);

        return clientChannel;
    }

    /**
     * Remove an InputChannel pair when a window is destroyed.
     */
    public void destroyInputChannel(IBinder windowToken) {
        Integer sessionId = mWindowSessionMap.remove(windowToken);
        if (sessionId != null) {
            InputChannel serverChannel = mServerChannels.remove(sessionId);
            nativeUnregisterInputChannel(sessionId);
            if (serverChannel != null) {
                serverChannel.dispose();
            }
            Log.i(TAG, "InputChannel destroyed: session=" + sessionId);
        }
    }

    /**
     * Get session ID for a window token.
     */
    public int getSessionId(IBinder windowToken) {
        Integer sessionId = mWindowSessionMap.get(windowToken);
        return sessionId != null ? sessionId : -1;
    }

    // ==================== Native Methods ====================

    /**
     * Register a server-side InputChannel with the native InputPublisher.
     * The native layer creates an InputPublisher wrapping the channel's fd,
     * ready to receive OH input events and publish them as Android MotionEvents.
     */
    private static native void nativeRegisterInputChannel(int sessionId,
                                                           InputChannel serverChannel);

    /**
     * Unregister and clean up the native InputPublisher for a session.
     */
    private static native void nativeUnregisterInputChannel(int sessionId);

    /**
     * Called from native code when OH delivers a touch event.
     * This is a callback entry point from the C++ OH input monitoring thread.
     *
     * @param sessionId  OH window session ID
     * @param action     MotionEvent action (ACTION_DOWN=0, ACTION_UP=1, ACTION_MOVE=2)
     * @param x          Touch X coordinate
     * @param y          Touch Y coordinate
     * @param downTime   Time of the initial ACTION_DOWN (nanoseconds)
     * @param eventTime  Time of this event (nanoseconds)
     */
    public static void onOHTouchEvent(int sessionId, int action,
                                       float x, float y,
                                       long downTime, long eventTime) {
        Log.d(TAG, "OH touch event: session=" + sessionId
                + ", action=" + action + ", x=" + x + ", y=" + y);
        // The native layer has already published this event to the InputChannel
        // via InputPublisher. This callback is for logging/debugging only.
    }
}
