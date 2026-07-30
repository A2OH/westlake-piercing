// ============================================================================
// android_view_InputEventReceiver.cpp
//
// JNI binding for android.view.InputEventReceiver — registered into
// liboh_android_runtime's kRegJNI table.
//
// 2026-05-18 Phase 2 (Input_Adapter_design §3.3.5):
// Real InputChannel client-side consumer. Each receiver spawns a worker
// thread that polls its InputChannel client-side socketpair fd, reads
// AOSP-format InputMessage frames (server side: OHInputBridge::writeMotionEvent
// in liboh_adapter_bridge.so), parses motion bodies, constructs Java
// MotionEvent objects via JNI, and dispatches them to
// InputEventReceiver.dispatchInputEvent on the main looper thread.
//
// Java side (frameworks/base/core/java/android/view/InputEventReceiver.java):
//   private static native long    nativeInit(WeakReference<InputEventReceiver>,
//                                            InputChannel, MessageQueue);
//   private static native void    nativeDispose(long receiverPtr);
//   private static native void    nativeFinishInputEvent(long receiverPtr,
//                                                         int seq, boolean handled);
//   private static native void    nativeReportTimeline(long receiverPtr, int eventId,
//                                                       long gpuTime, long presentTime);
//   private static native boolean nativeConsumeBatchedInputEvents(long receiverPtr,
//                                                                  long frameTimeNanos);
//   private static native String  nativeDump(long receiverPtr, String prefix);
//
// And the receiver-side callback we invoke:
//   private void dispatchInputEvent(int seq, InputEvent event, long displayId);
// ============================================================================

#include "AndroidRuntime.h"

#include <jni.h>
#include <android/log.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <new>
#include <poll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

extern "C" int HiLogPrint(int type, int level, unsigned int domain,
                          const char* tag, const char* fmt, ...)
    __attribute__((__format__(printf, 5, 6)));
#define HLOG_INFO(fmt, ...) HiLogPrint(3, 4, 0xD000F00u, "OH_IER", fmt, ##__VA_ARGS__)
#define HLOG_ERR(fmt, ...)  HiLogPrint(3, 6, 0xD000F00u, "OH_IER", fmt, ##__VA_ARGS__)
#define HLOG_DBG(fmt, ...)  HiLogPrint(3, 3, 0xD000F00u, "OH_IER", fmt, ##__VA_ARGS__)

// Internal accessor exported by android_view_InputChannel.cpp in the same .so.
extern "C" int oh_input_channel_get_fd(jlong native_ptr);

namespace android {
namespace {

// ============================================================
// InputMessage wire format — must mirror writeMotionEvent in
// framework/window/jni/oh_input_bridge.cpp (audited 2026-05-18 against
// AOSP 14 frameworks/native/include/input/InputTransport.h).
// ============================================================

constexpr uint32_t INPUT_MSG_TYPE_KEY      = 0;
constexpr uint32_t INPUT_MSG_TYPE_MOTION   = 1;
constexpr uint32_t INPUT_MSG_TYPE_FINISHED = 2;
constexpr uint32_t INPUT_MSG_TYPE_FOCUS    = 3;
// (CAPTURE / DRAG / TIMELINE / TOUCHMODE consumed but not dispatched yet.)

constexpr size_t MAX_POINTERS = 16;
constexpr size_t MAX_POINTER_COORDS_AXES = 30;
constexpr uint64_t AXIS_X_BIT = (1ULL << 0);
constexpr uint64_t AXIS_Y_BIT = (1ULL << 1);

struct InputMessageHeader {
    uint32_t type;
    uint32_t seq;
};
static_assert(sizeof(InputMessageHeader) == 8, "Header must be 8 bytes");

struct WirePointerProperties {
    int32_t id;
    int32_t toolType;
};
static_assert(sizeof(WirePointerProperties) == 8, "PP must be 8B");

struct WirePointerCoords {
    uint64_t bits __attribute__((aligned(8)));
    float values[MAX_POINTER_COORDS_AXES];
    bool isResampled;
    uint8_t empty[7];
};
static_assert(sizeof(WirePointerCoords) == 136, "PC must be 136B");

struct WirePointer {
    WirePointerProperties properties;
    WirePointerCoords coords;
};
static_assert(sizeof(WirePointer) == 144, "Pointer must be 144B");

struct WireMotionBody {
    int32_t eventId;
    uint32_t pointerCount;
    int64_t eventTime __attribute__((aligned(8)));
    int32_t deviceId;
    int32_t source;
    int32_t displayId;
    uint8_t hmac[32];
    int32_t action;
    int32_t actionButton;
    int32_t flags;
    int32_t metaState;
    int32_t buttonState;
    uint8_t classification;
    uint8_t empty2[3];
    int32_t edgeFlags;
    int64_t downTime __attribute__((aligned(8)));
    float dsdx, dtdx, dtdy, dsdy, tx, ty;
    float xPrecision, yPrecision, xCursorPosition, yCursorPosition;
    float dsdxRaw, dtdxRaw, dtdyRaw, dsdyRaw, txRaw, tyRaw;
    WirePointer pointers[MAX_POINTERS] __attribute__((aligned(8)));
};

struct WireFinishedBody {
    bool handled;
    uint8_t empty[7];
    int64_t consumeTime;
};
static_assert(sizeof(WireFinishedBody) == 16, "Finished must be 16B");

// Key body — bit-exact mirror of KeyEventBody in oh_input_bridge.cpp
// (AOSP 14 frameworks/native/include/input/InputTransport.h struct Key).
struct WireKeyBody {
    int32_t eventId;
    uint32_t empty1;
    int64_t eventTime __attribute__((aligned(8)));
    int32_t deviceId;
    int32_t source;
    int32_t displayId;
    uint8_t hmac[32];
    int32_t action;
    int32_t flags;
    int32_t keyCode;
    int32_t scanCode;
    int32_t metaState;
    int32_t repeatCount;
    uint32_t empty2;
    int64_t downTime __attribute__((aligned(8)));
};
static_assert(sizeof(WireKeyBody) == 96, "Key body must be 96B");

// Max possible InputMessage = header + motion body with all pointers.
constexpr size_t MAX_INPUT_MESSAGE_SIZE =
    sizeof(InputMessageHeader) + sizeof(WireMotionBody);

// ============================================================
// Java method cache (populated lazily on first use)
// ============================================================
struct JavaRefs {
    std::once_flag initFlag;
    jclass motionEventCls = nullptr;     // android.view.MotionEvent
    jmethodID motionEventObtain = nullptr;
    jmethodID motionEventSetSource = nullptr;  // void setSource(int)
    jclass keyEventCls = nullptr;        // android.view.KeyEvent
    jmethodID keyEventCtor = nullptr;    // 10-arg KeyEvent(...) constructor
    jclass receiverCls = nullptr;        // android.view.InputEventReceiver
    jmethodID dispatchInputEvent = nullptr;
    jclass weakRefCls = nullptr;         // java.lang.ref.WeakReference
    jmethodID weakRefGet = nullptr;
    jclass handlerCls = nullptr;         // android.os.Handler
    jmethodID handlerCtor = nullptr;
    jmethodID handlerPost = nullptr;
    jclass looperCls = nullptr;          // android.os.Looper
    jmethodID looperGetMainLooper = nullptr;
    jclass runnableCls = nullptr;        // adapter helper runnable class (unused; see below)
    // 2026-05-19: helper bridge that posts dispatchInputEvent onto the main
    // looper via Handler.  See dispatchMotionFromWorker comment for rationale.
    jclass inputEventBridgeCls = nullptr;        // adapter.window.InputEventBridge
    jmethodID dispatchOnMainThread = nullptr;    // static dispatchOnMainThread(IER,int,InputEvent)
    bool valid = false;
};
static JavaRefs g_refs;

void ensureJavaRefs(JNIEnv* env) {
    std::call_once(g_refs.initFlag, [env]() {
        auto findGlobal = [env](const char* name) -> jclass {
            jclass local = env->FindClass(name);
            if (!local || env->ExceptionCheck()) {
                env->ExceptionClear();
                HLOG_ERR("ensureJavaRefs: FindClass(%s) failed", name);
                return nullptr;
            }
            jclass global = static_cast<jclass>(env->NewGlobalRef(local));
            env->DeleteLocalRef(local);
            return global;
        };

        g_refs.motionEventCls       = findGlobal("android/view/MotionEvent");
        g_refs.keyEventCls          = findGlobal("android/view/KeyEvent");
        g_refs.receiverCls          = findGlobal("android/view/InputEventReceiver");
        g_refs.weakRefCls           = findGlobal("java/lang/ref/WeakReference");
        g_refs.handlerCls           = findGlobal("android/os/Handler");
        g_refs.looperCls            = findGlobal("android/os/Looper");
        g_refs.inputEventBridgeCls  = findGlobal("adapter/window/InputEventBridge");
        if (!g_refs.motionEventCls || !g_refs.keyEventCls || !g_refs.receiverCls
                || !g_refs.weakRefCls || !g_refs.handlerCls
                || !g_refs.looperCls
                || !g_refs.inputEventBridgeCls) {
            HLOG_ERR("ensureJavaRefs: class lookups failed "
                     "(InputEventBridge=%p)", g_refs.inputEventBridgeCls);
            return;
        }

        // MotionEvent.obtain(long downTime, long eventTime, int action,
        //                    float x, float y, int metaState)
        g_refs.motionEventObtain = env->GetStaticMethodID(
            g_refs.motionEventCls, "obtain",
            "(JJIFFI)Landroid/view/MotionEvent;");
        // MotionEvent.setSource(int): needed because 6-arg obtain forwards to
        // 12-arg form with source=SOURCE_UNKNOWN(0). ViewGroup.dispatchTouchEvent
        // first checks isTouchEvent() which is (source & SOURCE_CLASS_POINTER != 0);
        // SOURCE_UNKNOWN fails this — every dispatch gets rejected. We override
        // source to AINPUT_SOURCE_TOUCHSCREEN (0x1002) post-obtain.
        g_refs.motionEventSetSource = env->GetMethodID(
            g_refs.motionEventCls, "setSource", "(I)V");
        // KeyEvent(long downTime, long eventTime, int action, int code,
        //          int repeat, int metaState, int deviceId, int scancode,
        //          int flags, int source) — the 10-arg ctor is stable across
        //          AOSP versions (avoids obtain()'s displayId-variant skew).
        g_refs.keyEventCtor = env->GetMethodID(
            g_refs.keyEventCls, "<init>", "(JJIIIIIIII)V");
        // AOSP 14 InputEventReceiver.dispatchInputEvent(int seq, InputEvent event)
        // — 2-param form (the 3rd `long displayId` param is AOSP 15+ only).
        g_refs.dispatchInputEvent = env->GetMethodID(
            g_refs.receiverCls, "dispatchInputEvent",
            "(ILandroid/view/InputEvent;)V");
        // WeakReference.get()
        g_refs.weakRefGet = env->GetMethodID(
            g_refs.weakRefCls, "get", "()Ljava/lang/Object;");
        // Looper.getMainLooper()
        g_refs.looperGetMainLooper = env->GetStaticMethodID(
            g_refs.looperCls, "getMainLooper", "()Landroid/os/Looper;");
        // Handler(Looper)
        g_refs.handlerCtor = env->GetMethodID(
            g_refs.handlerCls, "<init>", "(Landroid/os/Looper;)V");
        // Handler.post(Runnable)
        g_refs.handlerPost = env->GetMethodID(
            g_refs.handlerCls, "post", "(Ljava/lang/Runnable;)Z");
        // adapter.window.InputEventBridge.dispatchOnMainThread(IER, int, InputEvent)
        // — posts the dispatchInputEvent call onto the main looper via Handler.
        g_refs.dispatchOnMainThread = env->GetStaticMethodID(
            g_refs.inputEventBridgeCls, "dispatchOnMainThread",
            "(Landroid/view/InputEventReceiver;ILandroid/view/InputEvent;)V");

        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            HLOG_ERR("ensureJavaRefs: method lookups failed");
            return;
        }
        if (!g_refs.motionEventObtain || !g_refs.motionEventSetSource
                || !g_refs.keyEventCtor
                || !g_refs.dispatchInputEvent
                || !g_refs.weakRefGet || !g_refs.looperGetMainLooper
                || !g_refs.handlerCtor || !g_refs.handlerPost
                || !g_refs.dispatchOnMainThread) {
            HLOG_ERR("ensureJavaRefs: required methods not found "
                     "(motionObtain=%p setSource=%p dispatch=%p weakGet=%p "
                     "mainLoop=%p handlerCtor=%p handlerPost=%p mainDispatch=%p)",
                     g_refs.motionEventObtain, g_refs.motionEventSetSource,
                     g_refs.dispatchInputEvent,
                     g_refs.weakRefGet, g_refs.looperGetMainLooper,
                     g_refs.handlerCtor, g_refs.handlerPost,
                     g_refs.dispatchOnMainThread);
            return;
        }

        g_refs.valid = true;
        HLOG_INFO("ensureJavaRefs: cached MotionEvent/dispatchInputEvent/"
                  "Handler/Looper refs");
    });
}

// ============================================================
// OhInputEventReceiver state
// ============================================================
constexpr int32_t kMagic = 0x4F484552;  // 'OHER'
static std::atomic<int64_t> g_receiverId{1};

struct OhInputEventReceiver {
    int32_t magic;
    int64_t id;
    JavaVM* jvm;
    jobject receiverWeakRef;      // global ref to WeakReference<InputEventReceiver>
    jobject mainHandler;          // global ref to Handler(Looper.getMainLooper())
    int clientFd;                 // socketpair endpoint we read from
    std::atomic<bool> stop;
    std::thread workerThread;
    std::mutex mu;
};

OhInputEventReceiver* validate(jlong ptr) {
    auto* r = reinterpret_cast<OhInputEventReceiver*>(static_cast<uintptr_t>(ptr));
    if (!r || r->magic != kMagic) return nullptr;
    return r;
}

// ============================================================
// Dispatch path: build Java MotionEvent + post to main looper
// ============================================================
//
// MotionEvent is constructed on the InputChannel worker thread
// (MotionEvent.obtain is documented thread-safe — uses internal pool with
// synchronization).  The dispatch itself MUST happen on the main looper
// thread: AOSP ViewRootImpl.enqueueInputEvent runs with processImmediately=
// true, synchronously walking through View.dispatchPointerEvent etc. on the
// calling thread.  Material Theme Buttons use RippleDrawable as background;
// its onStateChange path calls ValueAnimator.cancel which throws
// AndroidRuntimeException("Animators may only be run on Looper threads")
// when called off the main thread — crashing the app on every Button tap.
//
// AOSP native InputEventReceiver registers its fd via Looper.addFd, so its
// dispatchInputEvent naturally runs on the main thread.  Adapter polls the
// fd from a std::thread instead (necessary because OH MMI client lifecycle
// differs from AOSP InputDispatcher), so we have to bridge worker→main
// ourselves.
//
// We hand off via adapter.window.InputEventBridge.dispatchOnMainThread:
// the Java helper posts a Runnable onto Handler(Looper.getMainLooper()),
// the Runnable calls InputEventReceiver.dispatchInputEvent via reflection
// (the method is private on the AOSP class — JNI can call it directly but
// pure Java can't, hence setAccessible reflection in the helper).
//
// Earlier comment claiming "worker-thread invocation is safe" was empirically
// disproven 2026-05-19 (helloworld crash on first Button tap with the above
// Animator-Looper-thread exception).
// ============================================================
void dispatchMotionFromWorker(OhInputEventReceiver* r, const InputMessageHeader& hdr,
                               const WireMotionBody& body) {
    if (!r || !r->jvm) return;
    JNIEnv* env = nullptr;
    bool needDetach = false;
    int gotEnv = r->jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (gotEnv != JNI_OK) {
        if (r->jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            HLOG_ERR("dispatchMotionFromWorker: AttachCurrentThread failed");
            return;
        }
        needDetach = true;
    }

    ensureJavaRefs(env);
    if (!g_refs.valid) {
        HLOG_ERR("dispatchMotionFromWorker: Java refs not valid");
        if (needDetach) r->jvm->DetachCurrentThread();
        return;
    }

    // Resolve receiver from WeakReference. If null (GC'd) drop the event.
    jobject receiver = env->CallObjectMethod(r->receiverWeakRef, g_refs.weakRefGet);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        if (needDetach) r->jvm->DetachCurrentThread();
        return;
    }
    if (!receiver) {
        // Receiver GC'd already; drop.
        if (needDetach) r->jvm->DetachCurrentThread();
        return;
    }

    // Extract x/y from pointer 0. Wire format uses values[] indexed by bit
    // position; we encoded X at index 0, Y at index 1 (writeMotionEvent).
    float x = 0.0f, y = 0.0f;
    if (body.pointerCount > 0) {
        x = body.pointers[0].coords.values[0];
        y = body.pointers[0].coords.values[1];
    }
    // AOSP MotionEvent uses ms (downTime / eventTime in nanoseconds when called
    // via obtain(long, long, int, float, float, int) — actually those ARE ms;
    // see frameworks/base/core/java/android/view/MotionEvent.java:1167.
    // OH PointerEvent timestamps were microseconds → we'd already converted to
    // ns in writeMotionEvent. Convert ns → ms here.
    jlong downTimeMs  = body.downTime  / 1000000LL;
    jlong eventTimeMs = body.eventTime / 1000000LL;

    jobject motionEvent = env->CallStaticObjectMethod(
        g_refs.motionEventCls, g_refs.motionEventObtain,
        downTimeMs, eventTimeMs, static_cast<jint>(body.action),
        x, y, static_cast<jint>(body.metaState));
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        env->DeleteLocalRef(receiver);
        if (needDetach) r->jvm->DetachCurrentThread();
        return;
    }
    if (!motionEvent) {
        HLOG_ERR("dispatchMotionFromWorker: MotionEvent.obtain returned null");
        env->DeleteLocalRef(receiver);
        if (needDetach) r->jvm->DetachCurrentThread();
        return;
    }

    // 2026-05-18: force source = AINPUT_SOURCE_TOUCHSCREEN (0x1002).
    // 6-arg MotionEvent.obtain defaults source to SOURCE_UNKNOWN (0); without
    // SOURCE_CLASS_POINTER bit set, isTouchEvent() returns false and
    // ViewGroup.dispatchTouchEvent rejects the entire chain — no onClick.
    // See AOSP frameworks/base/core/java/android/view/MotionEvent.java
    // obtain(6-arg) -> obtain(12-arg) with deviceId=0 edgeFlags=0, source
    // ultimately resolves to SOURCE_UNKNOWN.
    env->CallVoidMethod(motionEvent, g_refs.motionEventSetSource,
                        static_cast<jint>(0x1002 /* AINPUT_SOURCE_TOUCHSCREEN */));
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }

    HLOG_INFO("dispatchInputEvent seq=%u action=%d x=%.1f y=%.1f "
              "downMs=%lld evtMs=%lld src=TOUCHSCREEN (posting to main looper)",
              hdr.seq, body.action, x, y,
              (long long)downTimeMs, (long long)eventTimeMs);

    // 2026-05-19: post onto main looper via Java helper instead of calling
    // dispatchInputEvent directly from this worker thread.  See the comment
    // block above dispatchMotionFromWorker for full rationale (RippleDrawable
    // -> ValueAnimator.cancel Looper-thread check crash).
    env->CallStaticVoidMethod(g_refs.inputEventBridgeCls,
                              g_refs.dispatchOnMainThread,
                              receiver,
                              static_cast<jint>(hdr.seq),
                              motionEvent);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }

    env->DeleteLocalRef(motionEvent);
    env->DeleteLocalRef(receiver);
    if (needDetach) r->jvm->DetachCurrentThread();
}

// ============================================================
// Dispatch path: build Java KeyEvent + post to main looper
// ============================================================
//
// Mirrors dispatchMotionFromWorker: D-pad / nav keys arrive from OH MMI →
// OHInputBridge::writeKeyEvent writes a type=KEY InputMessage to the server
// end of the InputChannel socketpair; this worker reads it (WireKeyBody) and
// must turn it into a Java KeyEvent dispatched on the MAIN looper thread (the
// same Ripple/ValueAnimator Looper-thread constraint applies to key-triggered
// state changes, so we hand off via InputEventBridge.dispatchOnMainThread).
//
// Without this, KEY messages were read but dropped ("not yet dispatched"),
// so the bridge's writeKeyEvent succeeded (result=0) yet noice never saw a
// key — no focus movement, BACK was a no-op. This closes that gap.
// ============================================================
void dispatchKeyFromWorker(OhInputEventReceiver* r, const InputMessageHeader& hdr,
                            const WireKeyBody& body) {
    if (!r || !r->jvm) return;
    JNIEnv* env = nullptr;
    bool needDetach = false;
    int gotEnv = r->jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (gotEnv != JNI_OK) {
        if (r->jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            HLOG_ERR("dispatchKeyFromWorker: AttachCurrentThread failed");
            return;
        }
        needDetach = true;
    }

    ensureJavaRefs(env);
    if (!g_refs.valid) {
        HLOG_ERR("dispatchKeyFromWorker: Java refs not valid");
        if (needDetach) r->jvm->DetachCurrentThread();
        return;
    }

    jobject receiver = env->CallObjectMethod(r->receiverWeakRef, g_refs.weakRefGet);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        if (needDetach) r->jvm->DetachCurrentThread();
        return;
    }
    if (!receiver) {
        if (needDetach) r->jvm->DetachCurrentThread();
        return;
    }

    // KeyEvent ms timestamps (writeKeyEvent encodes ns); ns → ms.
    jlong downTimeMs  = body.downTime  / 1000000LL;
    jlong eventTimeMs = body.eventTime / 1000000LL;
    // Force a keyboard source so View key-navigation (focus search on DPAD)
    // accepts the event; deviceId<0 marks a virtual device.
    jint source = body.source ? body.source : 0x101 /* AINPUT_SOURCE_KEYBOARD */;

    jobject keyEvent = env->NewObject(
        g_refs.keyEventCls, g_refs.keyEventCtor,
        downTimeMs, eventTimeMs,
        static_cast<jint>(body.action),
        static_cast<jint>(body.keyCode),
        static_cast<jint>(body.repeatCount),
        static_cast<jint>(body.metaState),
        static_cast<jint>(body.deviceId),
        static_cast<jint>(body.scanCode),
        static_cast<jint>(body.flags),
        source);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        env->DeleteLocalRef(receiver);
        if (needDetach) r->jvm->DetachCurrentThread();
        return;
    }
    if (!keyEvent) {
        HLOG_ERR("dispatchKeyFromWorker: KeyEvent ctor returned null");
        env->DeleteLocalRef(receiver);
        if (needDetach) r->jvm->DetachCurrentThread();
        return;
    }

    HLOG_INFO("dispatchInputEvent KEY seq=%u action=%d keyCode=%d "
              "downMs=%lld evtMs=%lld src=0x%x (posting to main looper)",
              hdr.seq, body.action, body.keyCode,
              (long long)downTimeMs, (long long)eventTimeMs, source);

    env->CallStaticVoidMethod(g_refs.inputEventBridgeCls,
                              g_refs.dispatchOnMainThread,
                              receiver,
                              static_cast<jint>(hdr.seq),
                              keyEvent);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }

    env->DeleteLocalRef(keyEvent);
    env->DeleteLocalRef(receiver);
    if (needDetach) r->jvm->DetachCurrentThread();
}

// ============================================================
// Worker thread: poll fd + read InputMessages
// ============================================================
void workerLoop(OhInputEventReceiver* r) {
    HLOG_INFO("worker[%lld]: start fd=%d", (long long)r->id, r->clientFd);
    uint8_t buf[MAX_INPUT_MESSAGE_SIZE];

    while (!r->stop.load(std::memory_order_acquire)) {
        pollfd pfd;
        pfd.fd = r->clientFd;
        pfd.events = POLLIN | POLLHUP | POLLERR;
        pfd.revents = 0;
        int prc = ::poll(&pfd, 1, 200 /* timeout ms; re-check stop flag */);
        if (prc < 0) {
            if (errno == EINTR) continue;
            HLOG_ERR("worker[%lld]: poll errno=%d (%s)",
                     (long long)r->id, errno, strerror(errno));
            break;
        }
        if (prc == 0) continue;  // timeout — re-check stop flag

        if (pfd.revents & (POLLHUP | POLLERR)) {
            HLOG_INFO("worker[%lld]: fd closed (revents=0x%x)",
                      (long long)r->id, pfd.revents);
            break;
        }
        if (!(pfd.revents & POLLIN)) continue;

        ssize_t got = ::recv(r->clientFd, buf, sizeof(buf), 0);
        if (got < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) continue;
            HLOG_ERR("worker[%lld]: recv errno=%d (%s)",
                     (long long)r->id, errno, strerror(errno));
            break;
        }
        if (got == 0) {
            HLOG_INFO("worker[%lld]: EOF", (long long)r->id);
            break;
        }
        if (static_cast<size_t>(got) < sizeof(InputMessageHeader)) {
            HLOG_ERR("worker[%lld]: short header %zd", (long long)r->id, got);
            continue;
        }

        InputMessageHeader hdr;
        memcpy(&hdr, buf, sizeof(hdr));

        switch (hdr.type) {
            case INPUT_MSG_TYPE_MOTION: {
                if (static_cast<size_t>(got) <
                        sizeof(InputMessageHeader) + sizeof(WireMotionBody)
                        - sizeof(WirePointer) * MAX_POINTERS
                        + sizeof(WirePointer)) {
                    HLOG_ERR("worker[%lld]: motion body too small %zd",
                             (long long)r->id, got);
                    continue;
                }
                WireMotionBody body;
                memset(&body, 0, sizeof(body));
                // Wire body is variable-length (pointers truncated). Copy the
                // fixed prefix + N pointers based on pointerCount in wire data.
                size_t fixedPrefix = sizeof(WireMotionBody)
                        - sizeof(WirePointer) * MAX_POINTERS;
                memcpy(&body, buf + sizeof(InputMessageHeader), fixedPrefix);
                size_t bytesPayload = got - sizeof(InputMessageHeader);
                size_t pointersBytes = (bytesPayload > fixedPrefix)
                        ? (bytesPayload - fixedPrefix) : 0;
                size_t copyPointers = pointersBytes / sizeof(WirePointer);
                if (copyPointers > MAX_POINTERS) copyPointers = MAX_POINTERS;
                if (copyPointers > body.pointerCount) copyPointers = body.pointerCount;
                if (copyPointers > 0) {
                    memcpy(body.pointers,
                           buf + sizeof(InputMessageHeader) + fixedPrefix,
                           copyPointers * sizeof(WirePointer));
                }
                dispatchMotionFromWorker(r, hdr, body);
                break;
            }
            case INPUT_MSG_TYPE_FINISHED:
                // ACK from server — currently no state to update.
                HLOG_DBG("worker[%lld]: FINISHED seq=%u", (long long)r->id, hdr.seq);
                break;
            case INPUT_MSG_TYPE_KEY: {
                if (static_cast<size_t>(got) <
                        sizeof(InputMessageHeader) + sizeof(WireKeyBody)) {
                    HLOG_ERR("worker[%lld]: key body too small %zd",
                             (long long)r->id, got);
                    continue;
                }
                WireKeyBody kbody;
                memcpy(&kbody, buf + sizeof(InputMessageHeader), sizeof(WireKeyBody));
                dispatchKeyFromWorker(r, hdr, kbody);
                break;
            }
            case INPUT_MSG_TYPE_FOCUS:
                HLOG_DBG("worker[%lld]: type=%u seq=%u (not yet dispatched)",
                         (long long)r->id, hdr.type, hdr.seq);
                break;
            default:
                HLOG_DBG("worker[%lld]: unknown type=%u seq=%u",
                         (long long)r->id, hdr.type, hdr.seq);
                break;
        }
    }

    HLOG_INFO("worker[%lld]: exit", (long long)r->id);
}

// ============================================================
// JNI bindings
// ============================================================

jlong IER_nativeInit(JNIEnv* env, jclass /*clazz*/,
                     jobject receiverWeak, jobject inputChannel,
                     jobject /*messageQueue*/) {
    auto* r = new (std::nothrow) OhInputEventReceiver();
    if (!r) return 0;
    r->magic = kMagic;
    r->id = g_receiverId.fetch_add(1);
    r->receiverWeakRef = receiverWeak ? env->NewGlobalRef(receiverWeak) : nullptr;
    r->clientFd = -1;
    r->stop.store(false);
    if (env->GetJavaVM(&r->jvm) != JNI_OK) r->jvm = nullptr;

    // Resolve client-side socketpair fd from InputChannel.mPtr.
    if (inputChannel != nullptr) {
        jclass channelCls = env->GetObjectClass(inputChannel);
        jfieldID mPtrField = env->GetFieldID(channelCls, "mPtr", "J");
        if (mPtrField != nullptr) {
            jlong mPtr = env->GetLongField(inputChannel, mPtrField);
            r->clientFd = oh_input_channel_get_fd(mPtr);
        } else {
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
        env->DeleteLocalRef(channelCls);
    }

    if (r->clientFd < 0) {
        HLOG_ERR("nativeInit id=%lld: no client fd — falling back to stub mode",
                 (long long)r->id);
    } else {
        // Spawn worker; it polls the fd and dispatches motion events.
        r->workerThread = std::thread(workerLoop, r);
    }

    HLOG_INFO("nativeInit id=%lld clientFd=%d", (long long)r->id, r->clientFd);
    return static_cast<jlong>(reinterpret_cast<uintptr_t>(r));
}

void IER_nativeDispose(JNIEnv* env, jclass /*clazz*/, jlong ptr) {
    auto* r = validate(ptr);
    if (!r) return;
    r->stop.store(true, std::memory_order_release);
    if (r->workerThread.joinable()) r->workerThread.join();
    if (r->clientFd >= 0) ::close(r->clientFd);
    if (r->receiverWeakRef) env->DeleteGlobalRef(r->receiverWeakRef);
    if (r->mainHandler) env->DeleteGlobalRef(r->mainHandler);
    HLOG_INFO("nativeDispose id=%lld", (long long)r->id);
    delete r;
}

void IER_nativeFinishInputEvent(JNIEnv* /*env*/, jclass /*clazz*/,
                                 jlong ptr, jint seq, jboolean handled) {
    // Write FINISHED ACK back to server. AOSP InputPublisher::receiveFinishedSignal
    // would consume this; OHInputBridge currently ignores incoming bytes
    // on the server fd (no reader). Still send so the wire-protocol cycle
    // is symmetric — when OHInputBridge gains a finished-listener later
    // (Phase 3), no client-side change needed.
    auto* r = validate(ptr);
    if (!r || r->clientFd < 0) return;

    struct {
        InputMessageHeader header;
        WireFinishedBody body;
    } msg;
    memset(&msg, 0, sizeof(msg));
    msg.header.type = INPUT_MSG_TYPE_FINISHED;
    msg.header.seq = static_cast<uint32_t>(seq);
    msg.body.handled = (handled == JNI_TRUE);
    msg.body.consumeTime = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    ssize_t written = ::send(r->clientFd, &msg, sizeof(msg),
                              MSG_DONTWAIT | MSG_NOSIGNAL);
    if (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        HLOG_ERR("nativeFinishInputEvent[%lld]: send errno=%d (%s)",
                 (long long)r->id, errno, strerror(errno));
    }
}

void IER_nativeReportTimeline(JNIEnv* /*env*/, jclass /*clazz*/,
                               jlong /*ptr*/, jint /*eventId*/,
                               jlong /*gpuTime*/, jlong /*presentTime*/) {
    // No-op (Choreographer FrameInfo timeline; not needed for input transport).
}

jboolean IER_nativeConsumeBatchedInputEvents(JNIEnv* /*env*/, jclass /*clazz*/,
                                              jlong /*ptr*/, jlong /*frameTimeNanos*/) {
    // Phase 2 baseline: no frame-time batching. ViewRootImpl will still process
    // events one-per-MOVE; jank under fast scrolling is acceptable for now.
    return JNI_FALSE;
}

jstring IER_nativeDump(JNIEnv* env, jclass /*clazz*/,
                        jlong /*ptr*/, jstring /*prefix*/) {
    return env->NewStringUTF("");
}

const JNINativeMethod kInputEventReceiverMethods[] = {
    { "nativeInit",
      "(Ljava/lang/ref/WeakReference;Landroid/view/InputChannel;Landroid/os/MessageQueue;)J",
      reinterpret_cast<void*>(IER_nativeInit) },
    { "nativeDispose", "(J)V",
      reinterpret_cast<void*>(IER_nativeDispose) },
    { "nativeFinishInputEvent", "(JIZ)V",
      reinterpret_cast<void*>(IER_nativeFinishInputEvent) },
    { "nativeReportTimeline", "(JIJJ)V",
      reinterpret_cast<void*>(IER_nativeReportTimeline) },
    { "nativeConsumeBatchedInputEvents", "(JJ)Z",
      reinterpret_cast<void*>(IER_nativeConsumeBatchedInputEvents) },
    { "nativeDump", "(JLjava/lang/String;)Ljava/lang/String;",
      reinterpret_cast<void*>(IER_nativeDump) },
};

}  // namespace

int register_android_view_InputEventReceiver(JNIEnv* env) {
    if (env->ExceptionCheck()) env->ExceptionClear();

    HLOG_INFO("register_android_view_InputEventReceiver: ENTER");
    jclass clazz = env->FindClass("android/view/InputEventReceiver");
    if (clazz == nullptr) {
        HLOG_ERR("register_android_view_InputEventReceiver: FindClass returned null");
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        return -1;
    }
    jint rc = env->RegisterNatives(clazz, kInputEventReceiverMethods,
                                    sizeof(kInputEventReceiverMethods)
                                        / sizeof(kInputEventReceiverMethods[0]));
    env->DeleteLocalRef(clazz);
    if (rc != JNI_OK) {
        HLOG_ERR("register_android_view_InputEventReceiver: RegisterNatives rc=%d",
                 (int)rc);
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        return -1;
    }
    HLOG_INFO("register_android_view_InputEventReceiver: SUCCESS 6 methods");
    return 0;
}

}  // namespace android
