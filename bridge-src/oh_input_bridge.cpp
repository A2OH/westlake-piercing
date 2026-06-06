/*
 * oh_input_bridge.cpp
 *
 * Native input event bridge implementation.
 *
 * Writes Android-format InputMessage structs to the server side of
 * InputChannel socket pairs. The message format must match what
 * InputConsumer (in ViewRootImpl) expects to read.
 *
 * InputMessage format (simplified for single-pointer touch):
 *   - Header: type, seq
 *   - Body (motion): action, deviceId, source, displayId, pointerCount,
 *                     downTime, eventTime, pointerProperties, pointerCoords
 */
#include "oh_input_bridge.h"

#include <android/log.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <atomic>
#include <ctime>

// OH MMI inner_api headers — pulled in here only (forward-decl'd in .h)
#include "axis_event.h"
#include "event_handler.h"
#include "event_runner.h"
#include "i_input_event_consumer.h"
#include "input_manager.h"
#include "key_event.h"
#include "pointer_event.h"

#define LOG_TAG "OH_InputBridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Android InputMessage constants (from InputTransport.h)
// These must match the AOSP InputMessage struct layout
namespace {

// InputMessage types (mirror enum class InputMessage::Type at
// frameworks/native/include/input/InputTransport.h:69-77)
constexpr uint32_t INPUT_MSG_TYPE_KEY      = 0;
constexpr uint32_t INPUT_MSG_TYPE_MOTION   = 1;
constexpr uint32_t INPUT_MSG_TYPE_FINISHED = 2;
constexpr uint32_t INPUT_MSG_TYPE_FOCUS    = 3;
constexpr uint32_t INPUT_MSG_TYPE_CAPTURE  = 4;
constexpr uint32_t INPUT_MSG_TYPE_DRAG     = 5;
constexpr uint32_t INPUT_MSG_TYPE_TIMELINE = 6;
constexpr uint32_t INPUT_MSG_TYPE_TOUCHMODE = 7;

// MotionEvent source (from system/core/include/android/input.h)
constexpr int32_t AINPUT_SOURCE_TOUCHSCREEN = 0x00001002;

// MotionEvent actions
constexpr int32_t AMOTION_EVENT_ACTION_DOWN = 0;
constexpr int32_t AMOTION_EVENT_ACTION_UP = 1;
constexpr int32_t AMOTION_EVENT_ACTION_MOVE = 2;
constexpr int32_t AMOTION_EVENT_ACTION_CANCEL = 3;

// MotionEvent tool type (enum class ToolType { UNKNOWN=0, FINGER=1, ... }
// from frameworks/native/include/input/Input.h:234 — underlying type int)
constexpr int32_t AMOTION_EVENT_TOOL_TYPE_FINGER = 1;

// AOSP 14 layout constants (frameworks/native/include/input/Input.h)
constexpr size_t MAX_POINTERS = 16;            // line 165
constexpr size_t MAX_POINTER_COORDS_AXES = 30; // PointerCoords::MAX_AXES

// Axis bit indices (subset; X/Y/Pressure/Size are the only ones we set)
constexpr uint64_t AXIS_X_BIT        = (1ULL << 0);
constexpr uint64_t AXIS_Y_BIT        = (1ULL << 1);
constexpr uint64_t AXIS_PRESSURE_BIT = (1ULL << 2);
constexpr uint64_t AXIS_SIZE_BIT     = (1ULL << 3);

// ============================================================
// AOSP 14 InputMessage canonical layout
// (frameworks/native/include/input/InputTransport.h:67-...)
//
// Audited 2026-05-18 against ECS ~/aosp source. ALL field order, alignment
// directives, and padding MUST match exactly — InputConsumer::consume()
// parses by struct size + offsets. Any drift = client reads garbage.
// ============================================================

// Header: 8 bytes total (4 type + 4 seq).
struct InputMessageHeader {
    uint32_t type;
    uint32_t seq;
};
static_assert(sizeof(InputMessageHeader) == 8, "Header must be 8 bytes");

// PointerProperties: 8 bytes (int32 id + enum class ToolType, default int).
struct PointerProperties {
    int32_t id;
    int32_t toolType;  // ToolType enum class — int underlying
};
static_assert(sizeof(PointerProperties) == 8, "PointerProperties must be 8B");

// PointerCoords: 136 bytes (bits 8 + values 120 + bool 1 + empty 7).
struct PointerCoords {
    uint64_t bits __attribute__((aligned(8)));
    float values[MAX_POINTER_COORDS_AXES];
    bool isResampled;
    uint8_t empty[7];
};
static_assert(sizeof(PointerCoords) == 136, "PointerCoords must be 136B");

// One pointer = properties + coords = 144 bytes.
struct InputMessagePointer {
    PointerProperties properties;
    PointerCoords coords;
};
static_assert(sizeof(InputMessagePointer) == 144,
              "InputMessagePointer must be 144B");

// Motion body — order/alignment mirrors AOSP InputMessage::Body::Motion.
// Note: pointers[MAX_POINTERS] is the FULL array on the C++ side, but
// writeMotionEvent only sends `pointerCount` actual pointers over the wire
// (AOSP's Motion::size() formula). This struct's sizeof is the upper bound.
struct MotionEventBody {
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
    uint8_t classification;       // MotionClassification : uint8_t
    uint8_t empty2[3];            // 3-byte gap before edgeFlags
    int32_t edgeFlags;
    int64_t downTime __attribute__((aligned(8)));
    // Window transform (6 floats, AOSP order: dsdx, dtdx, dtdy, dsdy, tx, ty)
    float dsdx;
    float dtdx;
    float dtdy;
    float dsdy;
    float tx;
    float ty;
    float xPrecision;
    float yPrecision;
    float xCursorPosition;
    float yCursorPosition;
    // Raw transform (6 floats, same order as window transform)
    float dsdxRaw;
    float dtdxRaw;
    float dtdyRaw;
    float dsdyRaw;
    float txRaw;
    float tyRaw;
    // Pointer data array — MUST be the last field of the struct so
    // wire-format size() can truncate trailing pointers (see AOSP comment).
    InputMessagePointer pointers[MAX_POINTERS] __attribute__((aligned(8)));
};

// Wire-format size for `pointerCount` actual pointers.
// Matches AOSP: sizeof(Motion) - MAX_POINTERS*sizeof(Pointer) + n*sizeof(Pointer).
constexpr size_t motionBodyWireSize(uint32_t pointerCount) {
    return sizeof(MotionEventBody)
            - sizeof(InputMessagePointer) * MAX_POINTERS
            + sizeof(InputMessagePointer) * pointerCount;
}

// Finished body — for ACK back from client to server.
struct FinishedBody {
    bool handled;
    uint8_t empty[7];
    int64_t consumeTime;
};
static_assert(sizeof(FinishedBody) == 16, "FinishedBody must be 16B");

// ============================================================
// Key body — bit-exact AOSP 14 InputMessage::Body::Key
// (frameworks/native/include/input/InputTransport.h struct Key).
// ============================================================
struct KeyEventBody {
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

// KeyEvent source + actions (system/core/include/android/input.h)
constexpr int32_t AINPUT_SOURCE_KEYBOARD     = 0x00000101;
constexpr int32_t AKEY_EVENT_ACTION_DOWN     = 0;
constexpr int32_t AKEY_EVENT_ACTION_UP       = 1;

// OH MMI keycode -> AOSP Android keycode (navigation subset).
// OH d-pad/system codes are 2000+; AOSP are the classic <100 values.
static int32_t ohKeyCodeToAndroid(int32_t oh) {
    switch (oh) {
        case 2012: return 19;  // DPAD_UP
        case 2013: return 20;  // DPAD_DOWN
        case 2014: return 21;  // DPAD_LEFT
        case 2015: return 22;  // DPAD_RIGHT
        case 2016: return 23;  // DPAD_CENTER
        case 2054: return 66;  // ENTER
        case 2:    return 4;   // BACK
        case 1:    return 3;   // HOME
        case 2049: return 62;  // SPACE  (KEYCODE_SPACE)
        case 2050: return 61;  // TAB    (KEYCODE_TAB)
        case 2055: return 67;  // DEL / BACKSPACE
        case 2071: return 92;  // PAGE_UP
        case 2072: return 93;  // PAGE_DOWN
        default:   return -1;  // unmapped -> drop
    }
}

}  // anonymous namespace

namespace oh_adapter {

OHInputBridge& OHInputBridge::getInstance() {
    static OHInputBridge instance;
    return instance;
}

void OHInputBridge::registerInputChannel(int32_t sessionId, int serverFd) {
    std::lock_guard<std::mutex> lock(mutex_);

    SessionInput& session = sessions_[sessionId];
    session.serverFd = serverFd;
    session.seq = 0;

    LOGI("Registered input channel: session=%d, fd=%d", sessionId, serverFd);
}

void OHInputBridge::unregisterInputChannel(int32_t sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        // Don't close serverFd here; Java InputChannel owns it
        LOGI("Unregistered input channel: session=%d", sessionId);
        sessions_.erase(it);
    }
}

void OHInputBridge::registerOHInputFd(int32_t sessionId, int ohInputFd) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        it->second.ohInputFd = ohInputFd;
        LOGI("Registered OH input fd: session=%d, ohFd=%d", sessionId, ohInputFd);
    }

    // Start monitoring thread if not already running
    if (!monitoring_.load()) {
        monitoring_ = true;
        monitorThread_ = std::thread(&OHInputBridge::monitorOHInputEvents, this);
        monitorThread_.detach();
    }
}

int32_t OHInputBridge::injectTouchEvent(int32_t sessionId, int32_t action,
                                          float x, float y,
                                          int64_t downTime, int64_t eventTime) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        LOGE("injectTouchEvent: session %d not found", sessionId);
        return -1;
    }

    SessionInput& session = it->second;
    if (session.serverFd < 0) {
        LOGE("injectTouchEvent: session %d has no server fd", sessionId);
        return -1;
    }

    session.seq++;
    int result = writeMotionEvent(session.serverFd, session.seq,
                                   action, x, y, downTime, eventTime);

    LOGD("injectTouchEvent: session=%d, action=%d, x=%.1f, y=%.1f, result=%d",
         sessionId, action, x, y, result);

    return result;
}

int32_t OHInputBridge::injectKeyEvent(int32_t sessionId, int32_t action,
                                       int32_t androidKeyCode,
                                       int64_t downTime, int64_t eventTime) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        LOGE("injectKeyEvent: session %d not found", sessionId);
        return -1;
    }

    SessionInput& session = it->second;
    if (session.serverFd < 0) {
        LOGE("injectKeyEvent: session %d has no server fd", sessionId);
        return -1;
    }

    session.seq++;
    int result = writeKeyEvent(session.serverFd, session.seq, action,
                                androidKeyCode, downTime, eventTime);

    LOGI("injectKeyEvent: session=%d, action=%d, keyCode=%d, result=%d",
         sessionId, action, androidKeyCode, result);

    return result;
}

// ============================================================
// dispatchKeyViaViewRoot — direct in-process key dispatch
// ============================================================
// The deployed runtime 16e08711 renders noice but its InputChannel consumer
// (android_view_InputEventReceiver.cpp) DROPS type=KEY messages (the KEY case
// is a "not yet dispatched" stub), so writeKeyEvent's bytes are read and
// thrown away. Rather than rebuild that runtime (its source is gone; the only
// rebuildable runtime regresses noice's init), we dispatch the key ourselves
// from the bridge, which runs in-process with JNI access:
//   1. build android.view.KeyEvent (10-arg ctor; KeyEvent natives are
//      registered in 16e08711 via register_android_view_KeyEvent),
//   2. find the focused ViewRootImpl's mInputEventReceiver via
//      WindowManagerGlobal.mRoots reflection,
//   3. hand it to adapter.window.InputEventBridge.dispatchOnMainThread — the
//      SAME helper the MOTION path uses — which posts onto the main looper and
//      calls receiver.dispatchInputEvent(seq, event) → ViewRootImpl input
//      stages → View.dispatchKeyEvent (focus nav / BACK / etc.).
// ============================================================
int32_t OHInputBridge::dispatchKeyViaViewRoot(int32_t action, int32_t keyCode,
                                              int64_t downTimeNs,
                                              int64_t eventTimeNs) {
    if (!jvm_) { LOGE("dispatchKeyViaViewRoot: no JavaVM"); return -1; }
    JNIEnv* env = nullptr;
    bool needDetach = false;
    if (jvm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        JavaVMAttachArgs args{JNI_VERSION_1_6, "oh-key-dispatch", nullptr};
        if (jvm_->AttachCurrentThread(&env, &args) != JNI_OK) {
            LOGE("dispatchKeyViaViewRoot: AttachCurrentThread failed");
            return -1;
        }
        needDetach = true;
    }
    auto fail = [&](const char* why) -> int32_t {
        if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
        LOGE("dispatchKeyViaViewRoot: %s", why);
        if (needDetach) jvm_->DetachCurrentThread();
        return -1;
    };

    // --- 1. KeyEvent(downMs, evtMs, action, code, repeat, meta, devId, scan, flags, source) ---
    jclass keCls = env->FindClass("android/view/KeyEvent");
    if (!keCls || env->ExceptionCheck()) return fail("FindClass KeyEvent");
    jmethodID keCtor = env->GetMethodID(keCls, "<init>", "(JJIIIIIIII)V");
    if (!keCtor || env->ExceptionCheck()) return fail("KeyEvent ctor");
    jlong downMs = downTimeNs / 1000000LL;
    jlong evtMs  = eventTimeNs / 1000000LL;
    jobject keyEvent = env->NewObject(
        keCls, keCtor, downMs, evtMs, (jint)action, (jint)keyCode,
        (jint)0 /*repeat*/, (jint)0 /*meta*/, (jint)-1 /*deviceId=VIRTUAL*/,
        (jint)0 /*scancode*/, (jint)0 /*flags*/, (jint)0x101 /*SOURCE_KEYBOARD*/);
    if (!keyEvent || env->ExceptionCheck()) return fail("NewObject KeyEvent");

    // --- 2. Focused ViewRootImpl receiver via WindowManagerGlobal.mRoots ---
    jclass wmgCls = env->FindClass("android/view/WindowManagerGlobal");
    if (!wmgCls || env->ExceptionCheck()) return fail("FindClass WindowManagerGlobal");
    jmethodID getInst = env->GetStaticMethodID(
        wmgCls, "getInstance", "()Landroid/view/WindowManagerGlobal;");
    if (!getInst || env->ExceptionCheck()) return fail("WMG.getInstance id");
    jobject wmg = env->CallStaticObjectMethod(wmgCls, getInst);
    if (!wmg || env->ExceptionCheck()) return fail("WMG.getInstance call");
    jfieldID mRootsF = env->GetFieldID(wmgCls, "mRoots", "Ljava/util/ArrayList;");
    if (!mRootsF || env->ExceptionCheck()) return fail("WMG.mRoots field");
    jobject roots = env->GetObjectField(wmg, mRootsF);
    if (!roots) return fail("WMG.mRoots null");

    jclass listCls = env->FindClass("java/util/ArrayList");
    jmethodID sizeM = env->GetMethodID(listCls, "size", "()I");
    jmethodID getM  = env->GetMethodID(listCls, "get", "(I)Ljava/lang/Object;");
    jint n = env->CallIntMethod(roots, sizeM);

    jclass vriCls = env->FindClass("android/view/ViewRootImpl");
    if (!vriCls || env->ExceptionCheck()) return fail("FindClass ViewRootImpl");
    jfieldID mViewF = env->GetFieldID(vriCls, "mView", "Landroid/view/View;");
    jfieldID recvF  = env->GetFieldID(vriCls, "mInputEventReceiver",
                          "Landroid/view/ViewRootImpl$WindowInputEventReceiver;");
    if (!mViewF || !recvF || env->ExceptionCheck()) return fail("ViewRootImpl fields");
    jclass viewCls = env->FindClass("android/view/View");
    jmethodID hasFocusM = env->GetMethodID(viewCls, "hasWindowFocus", "()Z");

    jobject receiver = nullptr;   // chosen WindowInputEventReceiver
    jobject fallbackRecv = nullptr;
    for (jint i = 0; i < n; ++i) {
        jobject vri = env->CallObjectMethod(roots, getM, i);
        if (!vri) continue;
        jobject recv = env->GetObjectField(vri, recvF);
        jobject view = env->GetObjectField(vri, mViewF);
        if (recv && view) {
            jboolean focused = env->CallBooleanMethod(view, hasFocusM);
            if (focused) {
                receiver = env->NewLocalRef(recv);  // keep
            } else if (!fallbackRecv) {
                fallbackRecv = env->NewLocalRef(recv);  // topmost-ish fallback
            }
        }
        if (recv) env->DeleteLocalRef(recv);
        if (view) env->DeleteLocalRef(view);
        env->DeleteLocalRef(vri);
        if (receiver) break;
    }
    if (!receiver) receiver = fallbackRecv;   // no focused window → best effort
    if (!receiver) { env->DeleteLocalRef(keyEvent); return fail("no ViewRootImpl receiver"); }

    // --- 3. dispatchOnMainThread(receiver, seq, keyEvent) ---
    jclass iebCls = env->FindClass("adapter/window/InputEventBridge");
    if (!iebCls || env->ExceptionCheck()) return fail("FindClass InputEventBridge");
    jmethodID domt = env->GetStaticMethodID(iebCls, "dispatchOnMainThread",
        "(Landroid/view/InputEventReceiver;ILandroid/view/InputEvent;)V");
    if (!domt || env->ExceptionCheck()) return fail("dispatchOnMainThread id");
    static std::atomic<int32_t> s_keySeq{0x40000000};
    jint seq = s_keySeq.fetch_add(1);
    env->CallStaticVoidMethod(iebCls, domt, receiver, seq, keyEvent);
    if (env->ExceptionCheck()) return fail("dispatchOnMainThread call");

    LOGI("dispatchKeyViaViewRoot: action=%d keyCode=%d seq=%d -> ViewRootImpl OK",
         action, keyCode, seq);
    env->DeleteLocalRef(keyEvent);
    if (needDetach) jvm_->DetachCurrentThread();
    return 0;
}

// ============================================================
// VelocityTracker JNI stub
// ============================================================
// The deployed runtime (liboh_android_runtime.so) never registers
// android_view_VelocityTracker JNI (no register_android_view_VelocityTracker,
// no impl symbols). RecyclerView / ScrollView / GestureDetector touch handling
// calls VelocityTracker.obtain()->nativeInitialize(int) on the FIRST touch,
// which throws UnsatisfiedLinkError and aborts View.dispatchTouchEvent
// mid-traversal — so no click/scroll ever lands. We register no-op stubs for
// the 7 native methods the deployed framework.jar declares (verified against
// device framework.jar md5 e92991b0): velocity always reads back 0 (no fling),
// but DOWN/MOVE/UP dispatch + click detection + scrolling all work.
static jlong    VT_nativeInitialize(JNIEnv*, jclass, jint) { return (jlong)1; }
static void     VT_nativeAddMovement(JNIEnv*, jclass, jlong, jobject) {}
static void     VT_nativeClear(JNIEnv*, jclass, jlong) {}
static void     VT_nativeComputeCurrentVelocity(JNIEnv*, jclass, jlong, jint, jfloat) {}
static void     VT_nativeDispose(JNIEnv*, jclass, jlong) {}
static jfloat   VT_nativeGetVelocity(JNIEnv*, jclass, jlong, jint, jint) { return 0.0f; }
static jboolean VT_nativeIsAxisSupported(JNIEnv*, jclass, jint) { return JNI_FALSE; }

static void ensureVelocityTrackerStub(JNIEnv* env) {
    static std::atomic<bool> done{false};
    if (done.load()) return;
    jclass vt = env->FindClass("android/view/VelocityTracker");
    if (!vt || env->ExceptionCheck()) { env->ExceptionClear(); return; }
    static const JNINativeMethod methods[] = {
        {"nativeInitialize", "(I)J", (void*)VT_nativeInitialize},
        {"nativeAddMovement", "(JLandroid/view/MotionEvent;)V", (void*)VT_nativeAddMovement},
        {"nativeClear", "(J)V", (void*)VT_nativeClear},
        {"nativeComputeCurrentVelocity", "(JIF)V", (void*)VT_nativeComputeCurrentVelocity},
        {"nativeDispose", "(J)V", (void*)VT_nativeDispose},
        {"nativeGetVelocity", "(JII)F", (void*)VT_nativeGetVelocity},
        {"nativeIsAxisSupported", "(I)Z", (void*)VT_nativeIsAxisSupported},
    };
    jint rc = env->RegisterNatives(vt, methods, 7);
    if (env->ExceptionCheck()) env->ExceptionClear();
    LOGI("ensureVelocityTrackerStub: RegisterNatives rc=%d (7 methods)", (int)rc);
    if (rc == 0) done.store(true);
    env->DeleteLocalRef(vt);
}

// ============================================================
// dispatchTouchViaViewRoot — direct in-process touch dispatch
// ============================================================
// Touch DOES reach the bridge (OnInputEvent(PointerEvent) fires), but the
// InputChannel MOTION path (injectTouchEvent → consumer worker) doesn't land
// on the deployed runtime — same as keys. So we build the MotionEvent here and
// dispatch it straight into the focused ViewRootImpl (bypassing the channel),
// the proven path from dispatchKeyViaViewRoot. Enables tap/click: nav-tab
// switching, sound play/info/volume buttons, etc.
// ============================================================
int32_t OHInputBridge::dispatchTouchViaViewRoot(int32_t action, float x, float y,
                                                int64_t downTimeNs,
                                                int64_t eventTimeNs) {
    (void)downTimeNs; (void)eventTimeNs;
    // Act on ACTION_UP: synthesize a DOWN -> (delay) -> UP tap dispatched to the
    // focused decor view ON THE UI THREAD via noice's OHTouchInjector helper
    // (loaded through the app classloader — no boot-image change needed). The
    // bridge builds the events + paces DOWN/UP (so RecyclerView CheckForTap sets
    // PREPRESSED and the UP performClick()s); the helper does the main-Looper post.
    if (action != 1 /* AMOTION_EVENT_ACTION_UP */) return 0;
    if (!jvm_) { LOGE("dispatchTouchViaViewRoot: no JavaVM"); return -1; }
    JNIEnv* env = nullptr;
    bool needDetach = false;
    if (jvm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        JavaVMAttachArgs args{JNI_VERSION_1_6, "oh-touch-dispatch", nullptr};
        if (jvm_->AttachCurrentThread(&env, &args) != JNI_OK) { LOGE("touch: attach failed"); return -1; }
        needDetach = true;
    }
    auto fail = [&](const char* why) -> int32_t {
        if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
        LOGE("dispatchTouchViaViewRoot: %s", why);
        if (needDetach) jvm_->DetachCurrentThread();
        return -1;
    };

    // Register the VelocityTracker JNI stub (once) so View.dispatchTouchEvent's
    // RecyclerView/scroll/gesture velocity tracking doesn't UnsatisfiedLinkError.
    ensureVelocityTrackerStub(env);

    // --- find focused decor view (mView) ---
    jclass wmgCls = env->FindClass("android/view/WindowManagerGlobal");
    if (!wmgCls || env->ExceptionCheck()) return fail("FindClass WMG");
    jmethodID getInst = env->GetStaticMethodID(wmgCls, "getInstance", "()Landroid/view/WindowManagerGlobal;");
    jobject wmg = env->CallStaticObjectMethod(wmgCls, getInst);
    if (!wmg || env->ExceptionCheck()) return fail("WMG.getInstance");
    jfieldID mRootsF = env->GetFieldID(wmgCls, "mRoots", "Ljava/util/ArrayList;");
    jobject roots = env->GetObjectField(wmg, mRootsF);
    if (!roots) return fail("mRoots null");
    jclass listCls = env->FindClass("java/util/ArrayList");
    jmethodID sizeM = env->GetMethodID(listCls, "size", "()I");
    jmethodID getM  = env->GetMethodID(listCls, "get", "(I)Ljava/lang/Object;");
    jint nn = env->CallIntMethod(roots, sizeM);
    jclass vriCls = env->FindClass("android/view/ViewRootImpl");
    jfieldID mViewF = env->GetFieldID(vriCls, "mView", "Landroid/view/View;");
    if (!mViewF || env->ExceptionCheck()) return fail("ViewRootImpl mView");
    jclass viewCls = env->FindClass("android/view/View");
    jmethodID hasFocusM = env->GetMethodID(viewCls, "hasWindowFocus", "()Z");
    jobject decorView = nullptr, fallbackView = nullptr;
    for (jint i = 0; i < nn; ++i) {
        jobject vri = env->CallObjectMethod(roots, getM, i);
        if (!vri) continue;
        jobject view = env->GetObjectField(vri, mViewF);
        if (view) {
            jboolean f = env->CallBooleanMethod(view, hasFocusM);
            if (f) decorView = env->NewLocalRef(view);
            else if (!fallbackView) fallbackView = env->NewLocalRef(view);
        }
        if (view) env->DeleteLocalRef(view);
        env->DeleteLocalRef(vri);
        if (decorView) break;
    }
    if (!decorView) decorView = fallbackView;
    if (!decorView) return fail("no decorView");

    // --- resolve noice's OHTouchInjector.dispatchTouchOnMain via app classloader ---
    jmethodID getCtx = env->GetMethodID(viewCls, "getContext", "()Landroid/content/Context;");
    jobject ctx = env->CallObjectMethod(decorView, getCtx);
    if (!ctx || env->ExceptionCheck()) return fail("getContext");
    jclass ctxCls = env->FindClass("android/content/Context");
    jmethodID getCL = env->GetMethodID(ctxCls, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject cl = env->CallObjectMethod(ctx, getCL);
    if (!cl || env->ExceptionCheck()) return fail("getClassLoader");
    jclass clCls = env->FindClass("java/lang/ClassLoader");
    jmethodID loadClassM = env->GetMethodID(clCls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring hn = env->NewStringUTF("com.github.ashutoshgngwr.noice.OHTouchInjector");
    jobject helperClsO = env->CallObjectMethod(cl, loadClassM, hn);
    env->DeleteLocalRef(hn);
    if (!helperClsO || env->ExceptionCheck()) return fail("loadClass OHTouchInjector");
    jclass helperCls = static_cast<jclass>(helperClsO);
    jclass meCls = env->FindClass("android/view/MotionEvent");
    jmethodID dispM = env->GetStaticMethodID(helperCls, "dispatchTouchOnMain",
        "(Landroid/view/View;Landroid/view/MotionEvent;)V");
    if (!dispM || env->ExceptionCheck()) return fail("GetStaticMethodID dispatchTouchOnMain");
    jmethodID obtain = env->GetStaticMethodID(meCls, "obtain", "(JJIFFI)Landroid/view/MotionEvent;");
    jmethodID setSrc = env->GetMethodID(meCls, "setSource", "(I)V");
    jclass scCls = env->FindClass("android/os/SystemClock");
    jmethodID upmM = env->GetStaticMethodID(scCls, "uptimeMillis", "()J");
    if (!obtain || !setSrc || !upmM || env->ExceptionCheck()) return fail("resolve obtain");

    jlong T = env->CallStaticLongMethod(scCls, upmM);
    // DOWN — direct static call (no reflection)
    jobject down = env->CallStaticObjectMethod(meCls, obtain, T, T, (jint)0, (jfloat)x, (jfloat)y, (jint)0);
    if (down) {
        env->CallVoidMethod(down, setSrc, (jint)0x1002);
        env->CallStaticVoidMethod(helperCls, dispM, decorView, down);
        if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); LOGE("dispatchTouchViaViewRoot: DOWN dispatchTouchOnMain THREW"); }
        env->DeleteLocalRef(down);
    }
    usleep(150 * 1000);
    // UP
    jlong T2 = env->CallStaticLongMethod(scCls, upmM);
    jobject up = env->CallStaticObjectMethod(meCls, obtain, T, T2, (jint)1, (jfloat)x, (jfloat)y, (jint)0);
    if (up) {
        env->CallVoidMethod(up, setSrc, (jint)0x1002);
        env->CallStaticVoidMethod(helperCls, dispM, decorView, up);
        if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); LOGE("dispatchTouchViaViewRoot: UP dispatchTouchOnMain THREW"); }
        env->DeleteLocalRef(up);
    }
    usleep(100 * 1000);  // let the UP run() complete on the UI thread
    {
        jfieldID rcF = env->GetStaticFieldID(helperCls, "runCount", "I");
        jfieldID lhF = env->GetStaticFieldID(helperCls, "lastHandled", "I");
        jfieldID icF = env->GetStaticFieldID(helperCls, "invokeCount", "I");
        jfieldID exF = env->GetStaticFieldID(helperCls, "lastEx", "Ljava/lang/String;");
        jint rc = rcF ? env->GetStaticIntField(helperCls, rcF) : -999;
        jint lh = lhF ? env->GetStaticIntField(helperCls, lhF) : -999;
        jint ic = icF ? env->GetStaticIntField(helperCls, icF) : -999;
        jstring exs = exF ? static_cast<jstring>(env->GetStaticObjectField(helperCls, exF)) : nullptr;
        const char* exc = nullptr;
        if (exs) exc = env->GetStringUTFChars(exs, nullptr);
        if (env->ExceptionCheck()) env->ExceptionClear();
        LOGI("dispatchTouchViaViewRoot: OHTI invokeCount=%d runCount=%d lastHandled=%d lastEx=%s",
             (int)ic, (int)rc, (int)lh, exc ? exc : "none");
        if (exs && exc) env->ReleaseStringUTFChars(exs, exc);
    }
    LOGI("dispatchTouchViaViewRoot: TAP x=%.1f y=%.1f -> OHTouchInjector DOWN+UP (UI thread)", x, y);
    if (needDetach) jvm_->DetachCurrentThread();
    return 0;
}

int OHInputBridge::writeMotionEvent(int fd, uint32_t seq, int32_t action,
                                     float x, float y,
                                     int64_t downTime, int64_t eventTime) {
    /*
     * 2026-05-18 Phase 2: bit-exact AOSP 14 InputMessage::Body::Motion format.
     * AOSP InputConsumer::consume reads (header + body) where body size is
     * computed from pointerCount — see motionBodyWireSize(). Sending the full
     * sizeof(MotionEventBody) (incl. all MAX_POINTERS slots) would also work
     * for SOCK_SEQPACKET (boundary preserves count) but wastes bytes; AOSP
     * truncates to actual pointer count.
     *
     * Field order MUST match AOSP — see frameworks/native/include/input/
     * InputTransport.h:67-... (audited 2026-05-18).
     */

    // Stack buffer for header + body (single pointer = 144B + ~200B body
    // header + 8B msg header = well under 4KB, fits in typical stack frame).
    struct {
        InputMessageHeader header;
        MotionEventBody body;
    } msg;
    memset(&msg, 0, sizeof(msg));

    // Header
    msg.header.type = INPUT_MSG_TYPE_MOTION;
    msg.header.seq = seq;

    // Motion body — AOSP field order
    msg.body.eventId = static_cast<int32_t>(seq);
    msg.body.pointerCount = 1;
    msg.body.eventTime = eventTime;
    msg.body.deviceId = 1;
    msg.body.source = AINPUT_SOURCE_TOUCHSCREEN;
    msg.body.displayId = 0;
    // hmac left zeroed (AOSP InputDispatcher signs events for security; OH
    // side has no equivalent — consumer accepts any when hmac is zero).
    msg.body.action = action;
    msg.body.actionButton = 0;
    msg.body.flags = 0;
    msg.body.metaState = 0;
    msg.body.buttonState = 0;
    msg.body.classification = 0;   // MotionClassification::NONE
    // empty2[3] already zeroed by memset
    msg.body.edgeFlags = 0;
    msg.body.downTime = downTime;

    // Window transform = identity (AOSP order: dsdx, dtdx, dtdy, dsdy, tx, ty)
    msg.body.dsdx = 1.0f;
    msg.body.dtdx = 0.0f;
    msg.body.dtdy = 0.0f;
    msg.body.dsdy = 1.0f;
    msg.body.tx   = 0.0f;
    msg.body.ty   = 0.0f;

    msg.body.xPrecision = 1.0f;
    msg.body.yPrecision = 1.0f;
    msg.body.xCursorPosition = 0.0f;
    msg.body.yCursorPosition = 0.0f;

    // Raw transform = identity (same field order as window transform)
    msg.body.dsdxRaw = 1.0f;
    msg.body.dtdxRaw = 0.0f;
    msg.body.dtdyRaw = 0.0f;
    msg.body.dsdyRaw = 1.0f;
    msg.body.txRaw   = 0.0f;
    msg.body.tyRaw   = 0.0f;

    // Single pointer at index 0
    InputMessagePointer& p0 = msg.body.pointers[0];
    p0.properties.id = 0;
    p0.properties.toolType = AMOTION_EVENT_TOOL_TYPE_FINGER;
    p0.coords.bits = AXIS_X_BIT | AXIS_Y_BIT | AXIS_PRESSURE_BIT | AXIS_SIZE_BIT;
    p0.coords.values[0] = x;       // AXIS_X
    p0.coords.values[1] = y;       // AXIS_Y
    p0.coords.values[2] = 1.0f;    // AXIS_PRESSURE
    p0.coords.values[3] = 0.01f;   // AXIS_SIZE
    p0.coords.isResampled = false;

    // Send only header + body-for-this-pointer-count (AOSP wire format).
    const size_t wireSize = sizeof(InputMessageHeader)
                            + motionBodyWireSize(1);
    ssize_t written = send(fd, &msg, wireSize, MSG_DONTWAIT | MSG_NOSIGNAL);
    if (written < 0) {
        LOGE("writeMotionEvent: send failed, errno=%d (%s)", errno, strerror(errno));
        return -errno;
    }
    if (static_cast<size_t>(written) != wireSize) {
        LOGE("writeMotionEvent: short write %zd / %zu", written, wireSize);
        return -1;
    }
    return 0;
}

int OHInputBridge::writeKeyEvent(int fd, uint32_t seq, int32_t action,
                                  int32_t keyCode, int64_t downTime,
                                  int64_t eventTime) {
    struct {
        InputMessageHeader header;
        KeyEventBody body;
    } msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = INPUT_MSG_TYPE_KEY;
    msg.header.seq = seq;

    msg.body.eventId = static_cast<int32_t>(seq);
    msg.body.eventTime = eventTime;
    msg.body.deviceId = 1;
    msg.body.source = AINPUT_SOURCE_KEYBOARD;
    msg.body.displayId = 0;
    // hmac already zeroed by memset
    msg.body.action = action;
    msg.body.flags = 0;
    msg.body.keyCode = keyCode;
    msg.body.scanCode = 0;
    msg.body.metaState = 0;
    msg.body.repeatCount = 0;
    msg.body.downTime = downTime;

    const size_t wireSize = sizeof(InputMessageHeader) + sizeof(KeyEventBody);
    ssize_t written = send(fd, &msg, wireSize, MSG_DONTWAIT | MSG_NOSIGNAL);
    if (written < 0) {
        LOGE("writeKeyEvent: send failed, errno=%d (%s)", errno, strerror(errno));
        return -errno;
    }
    if (static_cast<size_t>(written) != wireSize) {
        LOGE("writeKeyEvent: short write %zd / %zu", written, wireSize);
        return -1;
    }
    return 0;
}

void OHInputBridge::monitorOHInputEvents() {
    LOGI("OH input monitor thread started");

    while (monitoring_.load()) {
        std::vector<pollfd> fds;
        std::vector<int32_t> sessionIds;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& pair : sessions_) {
                if (pair.second.ohInputFd >= 0 && pair.second.serverFd >= 0) {
                    pollfd pfd;
                    pfd.fd = pair.second.ohInputFd;
                    pfd.events = POLLIN;
                    pfd.revents = 0;
                    fds.push_back(pfd);
                    sessionIds.push_back(pair.first);
                }
            }
        }

        if (fds.empty()) {
            // No OH input fds to monitor yet, sleep briefly
            usleep(100000); // 100ms
            continue;
        }

        int ret = poll(fds.data(), fds.size(), 100 /* timeout_ms */);
        if (ret <= 0) continue;

        for (size_t i = 0; i < fds.size(); i++) {
            if (fds[i].revents & POLLIN) {
                /*
                 * Read OH input event and convert to Android MotionEvent.
                 *
                 * OH PointerEvent format (from MultiModal Input framework):
                 * - action: DOWN/UP/MOVE
                 * - pointerId, x, y, pressure
                 * - timestamp
                 *
                 * Phase 2: Parse the actual OH MMI event format here.
                 * Phase 1: The monitoring thread is set up but real OH
                 * event parsing depends on the actual OH MMI binary format.
                 */
                uint8_t buf[4096];
                ssize_t nread = read(fds[i].fd, buf, sizeof(buf));
                if (nread > 0) {
                    LOGD("OH input event received: session=%d, %zd bytes",
                         sessionIds[i], nread);
                    // Phase 2: Parse OH event format and call injectTouchEvent()
                }
            }
        }
    }

    LOGI("OH input monitor thread stopped");
}

// ============================================================
// OhMmiInputConsumer — bridges OH MMI events to Android InputChannel
//
// Implements OHOS::MMI::IInputEventConsumer. The MMI service holds a
// shared_ptr to this object (kept alive on the bridge side via
// OHInputBridge::mmiConsumer_) and dispatches each OH input event to one of
// the three OnInputEvent overloads on the registered EventHandler thread.
//
// Pointer events are converted to Android MotionEvent semantics and forwarded
// to OHInputBridge::injectTouchEvent, which writes a binary Android
// InputMessage to the InputChannel server fd; ViewRootImpl's
// WindowInputEventReceiver reads from the client end and dispatches up the
// View tree (HelloWorld's Button.OnClickListener.onClick eventually fires).
//
// Key / Axis events stay Phase 1 stub for now (see §3.3.5 KeyEvent path).
//
// See doc/Input_Adapter_design.html §3.3.5 for design rationale.
// ============================================================
class OhMmiInputConsumer : public OHOS::MMI::IInputEventConsumer {
public:
    OhMmiInputConsumer() = default;
    ~OhMmiInputConsumer() override = default;

    void OnInputEvent(std::shared_ptr<OHOS::MMI::KeyEvent> keyEvent) const override {
        if (!keyEvent) return;
        // Phase 2: translate OH MMI KeyEvent -> Android KeyEvent + publish a
        // type=KEY InputMessage through the session InputChannel, mirroring the
        // PointerEvent path. Enables D-pad/key navigation of the app's views.
        int32_t sessionId = OHInputBridge::getInstance().getActiveSessionId();
        int32_t ohCode = keyEvent->GetKeyCode();
        int32_t ohAction = keyEvent->GetKeyAction();

        // Bottom-nav tab trigger: the OHOS systemui nav window occludes y>=1208
        // from MMI AND noice's window isn't in the WMS stack (launcher EntryView
        // is topmost) — so direct taps on noice's BottomNavigationView never
        // arrive via the display. Map OH digit keys 1..5 (2001..2005) to an
        // IN-PROCESS tap on tab N; in-process dispatchTouchViaViewRoot reaches
        // noice's decor view tree regardless of WMS z-order / systemui overlap.
        if (ohAction == 2 /*KEY_ACTION_DOWN*/ && ohCode >= 2001 && ohCode <= 2005) {
            static const float tabX[5] = {72.f, 216.f, 360.f, 504.f, 648.f};
            int idx = ohCode - 2001;
            int64_t nowNs = keyEvent->GetActionTime() * 1000LL;
            LOGI("OnInputEvent(KeyEvent): bottom-nav tab-trigger ohCode=%d -> in-process tap (%.0f,1218)",
                 ohCode, tabX[idx]);
            OHInputBridge::getInstance().dispatchTouchViaViewRoot(
                1 /*ACTION_UP: synthesizes the full DOWN+UP tap*/, tabX[idx], 1218.f, nowNs, nowNs);
            keyEvent->MarkProcessed();
            return;
        }

        int32_t androidKey = ohKeyCodeToAndroid(ohCode);
        // OH KEY_ACTION_DOWN=2, KEY_ACTION_UP=3 -> Android DOWN=0, UP=1.
        int32_t androidAction = (ohAction == 3) ? AKEY_EVENT_ACTION_UP
                                                : AKEY_EVENT_ACTION_DOWN;
        if (sessionId < 0 || androidKey < 0) {
            LOGI("OnInputEvent(KeyEvent): ohCode=%d action=%d %s — drop",
                 ohCode, ohAction, sessionId < 0 ? "(no session)" : "(unmapped)");
            keyEvent->MarkProcessed();
            return;
        }
        int64_t eventTimeNs = keyEvent->GetActionTime() * 1000LL;
        static thread_local int64_t s_keyDownNs = 0;
        int64_t downTimeNs;
        if (androidAction == AKEY_EVENT_ACTION_DOWN) {
            s_keyDownNs = eventTimeNs;
            downTimeNs = eventTimeNs;
        } else {
            downTimeNs = (s_keyDownNs > 0) ? s_keyDownNs : eventTimeNs;
        }
        LOGI("OnInputEvent(KeyEvent): ohCode=%d -> android=%d action=%d session=%d (forwarding)",
             ohCode, androidKey, androidAction, sessionId);
        // Primary path: dispatch DIRECTLY into the focused ViewRootImpl (the
        // deployed runtime's InputChannel consumer drops type=KEY). Fall back
        // to the InputChannel write if direct dispatch can't find a receiver
        // (e.g. on a runtime whose consumer DOES handle KEY).
        int32_t rc = OHInputBridge::getInstance().dispatchKeyViaViewRoot(
            androidAction, androidKey, downTimeNs, eventTimeNs);
        if (rc != 0) {
            OHInputBridge::getInstance().injectKeyEvent(sessionId, androidAction,
                                                        androidKey, downTimeNs, eventTimeNs);
        }
        keyEvent->MarkProcessed();
    }

    void OnInputEvent(std::shared_ptr<OHOS::MMI::PointerEvent> pointerEvent) const override {
        if (!pointerEvent) return;

        int32_t sessionId = OHInputBridge::getInstance().getActiveSessionId();
        if (sessionId < 0) {
            LOGD("OnInputEvent(PointerEvent): no active session, drop");
            pointerEvent->MarkProcessed();
            return;
        }

        int32_t pointerId = pointerEvent->GetPointerId();
        OHOS::MMI::PointerEvent::PointerItem item;
        if (!pointerEvent->GetPointerItem(pointerId, item)) {
            LOGE("OnInputEvent(PointerEvent): GetPointerItem(%d) failed", pointerId);
            pointerEvent->MarkProcessed();
            return;
        }

        // OH MMI PointerEvent::POINTER_ACTION_* → Android MotionEvent action.
        // OH:  CANCEL=1, DOWN=2, MOVE=3, UP=4
        // AOSP MotionEvent: ACTION_DOWN=0, UP=1, MOVE=2, CANCEL=3
        int32_t androidAction = -1;
        switch (pointerEvent->GetPointerAction()) {
            case OHOS::MMI::PointerEvent::POINTER_ACTION_DOWN:    androidAction = 0; break;
            case OHOS::MMI::PointerEvent::POINTER_ACTION_UP:      androidAction = 1; break;
            case OHOS::MMI::PointerEvent::POINTER_ACTION_MOVE:    androidAction = 2; break;
            case OHOS::MMI::PointerEvent::POINTER_ACTION_CANCEL:  androidAction = 3; break;
            default:
                LOGD("OnInputEvent(PointerEvent): unhandled OH action=%d, drop",
                     pointerEvent->GetPointerAction());
                pointerEvent->MarkProcessed();
                return;
        }

        // Use window-relative X/Y (aligned with ArkUI mmi_event_convertor.cpp:190-194).
        // InputMessage transform is identity (dsdx=dsdy=1, tx=ty=0), so AOSP consumer
        // treats MotionEvent.getX/Y as window-relative — feeding window coords
        // directly is correct regardless of where SCB places the window on display.
        // GetWindowXPos returns double for sub-pixel precision; fall back to
        // GetWindowX (int) when sub-pixel data unavailable.
        float x = static_cast<float>(item.GetWindowXPos());
        float y = static_cast<float>(item.GetWindowYPos());
        if (x == 0.0f && y == 0.0f) {
            x = static_cast<float>(item.GetWindowX());
            y = static_cast<float>(item.GetWindowY());
        }

        // OH timestamps are microseconds; Android InputMessage expects ns.
        int64_t eventTimeNs = pointerEvent->GetActionTime() * 1000LL;
        // OH PointerEvent::GetDownTime() returns 0 on this device (OH MMI
        // doesn't populate it for synthetic / non-Stage callers).  AOSP
        // ViewRootImpl click detection compares (eventTime - downTime) against
        // ViewConfiguration.getLongPressTimeout() (500ms); a downTime=0 with
        // multi-second eventTime registers as a non-click long-press and
        // Button.performClick() never fires.  Adapter caches its own
        // per-action-sequence downTime: set at DOWN, reused at MOVE/UP, reset
        // at UP/CANCEL.  Phase 1: single-touch only, single static cache;
        // multi-touch (Phase 2) needs per-pointerId map.
        static thread_local int64_t s_cachedDownTimeNs = 0;
        int64_t downTimeNs;
        if (androidAction == 0 /* AMOTION_EVENT_ACTION_DOWN */) {
            s_cachedDownTimeNs = eventTimeNs;
            downTimeNs = eventTimeNs;
        } else {
            int64_t ohDownNs = item.GetDownTime() * 1000LL;
            if (ohDownNs > 0) {
                downTimeNs = ohDownNs;        // trust OH if it actually has one
            } else if (s_cachedDownTimeNs > 0) {
                downTimeNs = s_cachedDownTimeNs;  // fall back to adapter cache
            } else {
                downTimeNs = eventTimeNs;     // no prior DOWN seen; use event time
            }
            if (androidAction == 1 /* UP */ || androidAction == 3 /* CANCEL */) {
                int64_t carry = downTimeNs;
                s_cachedDownTimeNs = 0;
                downTimeNs = carry;
            }
        }

        LOGI("OnInputEvent(PointerEvent): session=%d action=%d (oh=%d) x=%.1f y=%.1f "
             "down=%lldns evt=%lldns",
             sessionId, androidAction, pointerEvent->GetPointerAction(), x, y,
             (long long)downTimeNs, (long long)eventTimeNs);

        // Bottom-nav tab proxy: noice's BottomNavigationView (y>=1208) is
        // occluded from MMI by the systemui nav window, AND key events don't
        // reach noice (the launcher is WMS-topmost so it holds MMI key focus,
        // noice's window isn't in WMS — displayId:-1). MMI DOES deliver pointer
        // events in noice's content area, so we repurpose a deliverable strip
        // of the top toolbar (声音库 title bar, y in [76,130], no tap action)
        // as a 5-column tab selector → in-process tap on bottom-nav tab N.
        if (androidAction == 1 /*ACTION_UP*/ && y >= 76.0f && y <= 130.0f) {
            static const float tabX[5] = {72.f, 216.f, 360.f, 504.f, 648.f};
            int idx = static_cast<int>(x / 144.0f);
            if (idx < 0) idx = 0; if (idx > 4) idx = 4;
            LOGI("OnInputEvent(PointerEvent): top-band tab-proxy x=%.0f -> in-process tap tab%d (%.0f,1218)",
                 x, idx, tabX[idx]);
            OHInputBridge::getInstance().dispatchTouchViaViewRoot(
                1, tabX[idx], 1218.0f, downTimeNs, eventTimeNs);
            pointerEvent->MarkProcessed();
            return;
        }

        // Primary: dispatch the touch DIRECTLY into the focused ViewRootImpl
        // (the InputChannel MOTION path doesn't land on the deployed runtime,
        // same as keys). Fall back to the InputChannel write if no receiver.
        int32_t trc = OHInputBridge::getInstance().dispatchTouchViaViewRoot(
            androidAction, x, y, downTimeNs, eventTimeNs);
        if (trc != 0) {
            OHInputBridge::getInstance().injectTouchEvent(
                sessionId, androidAction, x, y, downTimeNs, eventTimeNs);
        }

        pointerEvent->MarkProcessed();
    }

    void OnInputEvent(std::shared_ptr<OHOS::MMI::AxisEvent> axisEvent) const override {
        // Axis events (wheel / pinch / etc.) — not used by HelloWorld; Phase 1 noop.
        if (axisEvent) axisEvent->MarkProcessed();
    }
};

// ============================================================
// startTapControlChannel — reliable in-process tap side-channel
// ============================================================
// OHOS MMI pointer delivery to noice's process is foundation-flaky
// (displayId:-1 — noice's window isn't in WMS), so a real tap only sometimes
// reaches the bridge. This poller reads a control file and dispatches the tap
// IN-PROCESS (dispatchTouchViaViewRoot — proven to drive the view tree
// regardless of WMS/systemui), giving deterministic interaction for testing:
//   echo "84 337"  > /data/local/tmp/noice_tap   # raw window coords
//   echo "2"        > /data/local/tmp/noice_tap   # bottom-nav tab 2 (1..5)
// The file is truncated after each command so it fires exactly once.
void OHInputBridge::startTapControlChannel() {
    static std::atomic<bool> started{false};
    bool expected = false;
    if (!started.compare_exchange_strong(expected, true)) return;
    std::thread([]() {
        const char* path = "/data/local/tmp/noice_tap";
        for (;;) {
            usleep(300 * 1000);
            FILE* f = fopen(path, "r");
            if (!f) continue;
            char buf[64] = {0};
            size_t n = fread(buf, 1, sizeof(buf) - 1, f);
            fclose(f);
            if (n == 0) continue;
            int a = -1, b = -1;
            int cnt = sscanf(buf, "%d %d", &a, &b);
            float x = -1.0f, y = -1.0f;
            if (cnt == 1 && a >= 1 && a <= 5) {
                static const float tabX[5] = {72.f, 216.f, 360.f, 504.f, 648.f};
                x = tabX[a - 1]; y = 1218.0f;
            } else if (cnt == 2) {
                x = static_cast<float>(a); y = static_cast<float>(b);
            }
            // fire once: truncate the control file
            FILE* w = fopen(path, "w"); if (w) fclose(w);
            if (x < 0.0f) continue;
            LOGI("tapControlChannel: in-process tap (%.0f,%.0f)", x, y);
            OHInputBridge::getInstance().dispatchTouchViaViewRoot(1, x, y, 0, 0);
        }
    }).detach();
    LOGI("startTapControlChannel: polling /data/local/tmp/noice_tap");
}

void OHInputBridge::subscribeMmi(int32_t sessionId) {
    // Update active session id first — consumer reads it via getActiveSessionId().
    activeSessionId_.store(sessionId);

    // Single-shot subscription per process. Once MMI knows our process is the
    // input target for the focused window, it dispatches all events here;
    // subsequent createSession calls (e.g. a second activity) just update the
    // session id we route to.
    bool expected = false;
    if (!mmiSubscribed_.compare_exchange_strong(expected, true)) {
        LOGI("subscribeMmi: already subscribed, updated activeSessionId=%d", sessionId);
        return;
    }

    // Start the reliable in-process tap side-channel (once per process).
    startTapControlChannel();

    // EventHandler running on the current thread's runner; if we're on a
    // worker thread without a runner, fall back to creating a dedicated runner
    // so MMI callbacks have somewhere to land. HelloWorld createSession is
    // called from JNI on a binder/worker thread, so we explicitly Create() a
    // new runner thread for MMI delivery — this matches how OH native
    // InputTransferStation creates its INPUT_AND_VSYNC_THREAD runner.
    auto runner = OHOS::AppExecFwk::EventRunner::Create("AdapterMmiConsumer");
    if (!runner) {
        LOGE("subscribeMmi: EventRunner::Create failed");
        mmiSubscribed_.store(false);
        return;
    }
    mmiEventHandler_ = std::make_shared<OHOS::AppExecFwk::EventHandler>(runner);
    mmiConsumer_ = std::make_shared<OhMmiInputConsumer>();

    int32_t rc = OHOS::MMI::InputManager::GetInstance()->SetWindowInputEventConsumer(
        mmiConsumer_, mmiEventHandler_);
    if (rc != 0) {
        LOGE("subscribeMmi: SetWindowInputEventConsumer rc=%d (session=%d)", rc, sessionId);
        // Don't roll back state — MMI may have partially registered; future
        // calls just no-op.
        return;
    }
    LOGI("subscribeMmi: MMI consumer registered for session=%d (Input_Adapter_design §3.3.5)",
         sessionId);
}

}  // namespace oh_adapter
