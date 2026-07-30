// ============================================================================
// android_view_MotionEvent_aosp.cpp
//
// 2026-05-18 (Plan A): adapter-private JNI binding for android.view.MotionEvent.
//
// Mirrors AOSP frameworks/base/core/jni/android_view_MotionEvent.cpp's 55
// native-method table verbatim (method names + signatures) so Java side sees
// identical JNI surface.  Implementation routes to adapter-private
// android::MotionEvent (in include/input/Input.h) rather than AOSP's
// MotionEvent class — adapter controls class layout, avoiding ABI mismatch
// with OH device libraries that previously caused SIGBUS in libicu_jni.so
// (see compile_oh_android_runtime.sh comment for full story).
//
// PHASE 1 NOTES:
//   - nativeReadFromParcel / nativeWriteToParcel are stubs (return -1) —
//     HelloWorld doesn't cross-process MotionEvent
//   - nativeAxisToString / nativeAxisFromString return null/-1 — labels
//     are stubbed in InputEventLookup
//   - nativeTransform / nativeApplyTransform are no-ops — Phase 2 will wire
//     real matrix composition
//
// PHASE 2 EXTENSION POINTS: same as Input.h header comment.
// ============================================================================

// WESTLAKE §393: do NOT pull AOSP's attestation/HmacKeyManager.h here.  This TU now compiles against
// the project's own input/Input.h (which matches the MotionEvent ABI this bridge links) and that
// header already defines INVALID_HMAC as `inline constexpr`; AOSP's defines the same symbol
// non-inline, so including both is a redefinition error.  Fixing this file is what finally supplies
// MotionEvent.nativeInitialize — without it every injected touch died with
// `UnsatisfiedLinkError: No implementation found for long android.view.MotionEvent.nativeInitialize`.
// #include "attestation/HmacKeyManager.h"


#include "input/Input.h"

#include <jni.h>
#include <nativehelper/JNIHelp.h>
#include <nativehelper/ScopedUtfChars.h>

#include <android/log.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

// WESTLAKE §395: define the two PointerCoords/PointerProperties helpers this TU needs.
// They are DECLARED in the project's input/Input.h but defined nowhere we link, so once this file
// started compiling (§393) the bridge gained exactly three new undefined symbols —
// `android::PointerCoords::clear()`, `android::PointerProperties::clear()` and `abort` — and
// **dlopen of liboh_adapter_bridge.so then failed outright** (`[B35.A] dlopen(...) returned null`).
// That is what broke the child: with the bridge unloaded NOTHING registers, which surfaced as the
// misleading `UnsatisfiedLinkError: android.os.Process.setArgV0Native`.
// ★Diagnosis recipe: diff `llvm-readelf --dyn-syms | grep UND` between the working and failing .so.
// WESTLAKE §441: android_input_Input_aosp.cpp is now part of this link and defines both of these
// for real, so the §395 stand-ins became `duplicate symbol` link errors. Marking them WEAK keeps
// both worlds working: the strong AOSP definition wins when Input.cpp is linked, and these still
// satisfy the link if it is ever dropped again.
namespace android {
__attribute__((weak)) void PointerCoords::clear() {
    bits = 0;
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        values[i] = 0;
    }
}
__attribute__((weak)) void PointerProperties::clear() {
    id = -1;
    toolType = static_cast<ToolType>(0);
}
}  // namespace android

// Phase 1 (Plan A): nativeTransform / nativeApplyTransform extract the 3x3
// matrix from android.graphics.Matrix via the public Java API
// Matrix.getValues(float[9]).  This avoids:
//   (a) linking against libhwui.so's AMatrix_getContents NDK helper
//       (libhwui's cross-compile in this project does not export it);
//   (b) reaching into Matrix.mNativeInstance (SkMatrix*) which would
//       impose an ABI lock on the device's libskia version — the very
//       class of mismatch that caused 2026-05-18 SIGBUS.
// The Java-side getValues() path is stable across every Android version
// and survives Phase 2 expansion (real matrix composition / multi-touch /
// rotation) unchanged.

#define LOG_TAG "MotionEvent-aosp"

namespace android {

namespace {

constexpr float kInvalidCursorPosition = NAN;

struct {
    jclass clazz;
    jmethodID obtain;
    jmethodID recycle;
    jfieldID mNativePtr;
} gMotionEventClassInfo;

struct {
    jfieldID mPackedAxisBits;
    jfieldID mPackedAxisValues;
    jfieldID x;
    jfieldID y;
    jfieldID pressure;
    jfieldID size;
    jfieldID touchMajor;
    jfieldID touchMinor;
    jfieldID toolMajor;
    jfieldID toolMinor;
    jfieldID orientation;
    jfieldID relativeX;
    jfieldID relativeY;
    jfieldID isResampled;
} gPointerCoordsClassInfo;

struct {
    jfieldID id;
    jfieldID toolType;
} gPointerPropertiesClassInfo;

struct {
    jclass clazz;
    jmethodID getValues;  // void getValues(float[9])
} gMatrixClassInfo;

// Adapter replacement for AOSP's libhwui AMatrix_getContents.  Calls the
// public Java API Matrix.getValues(float[9]) — works identically on every
// Android version, immune to OH libskia / libhwui ABI changes.
//
// **Lazy cache**: GetMethodID is deferred to first call.  Caching at
// register_android_view_MotionEvent time would force Matrix.<clinit> to
// run before libhwui's register_Matrix had a chance — Matrix.<clinit>
// references Matrix.nCreate which is implemented in libhwui.so, and
// libhwui is dlopen'd later in startReg.  Lazy cache avoids that
// dependency-order trap.
static bool extractMatrixContents(JNIEnv* env, jobject matrixObj, float values[9]) {
    if (!matrixObj) return false;
    if (!gMatrixClassInfo.getValues) {
        jclass mx = env->FindClass("android/graphics/Matrix");
        if (!mx) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            return false;
        }
        gMatrixClassInfo.clazz = static_cast<jclass>(env->NewGlobalRef(mx));
        gMatrixClassInfo.getValues = env->GetMethodID(mx, "getValues", "([F)V");
        env->DeleteLocalRef(mx);
        if (!gMatrixClassInfo.getValues) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            return false;
        }
    }
    jfloatArray arr = env->NewFloatArray(9);
    if (!arr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return false;
    }
    env->CallVoidMethod(matrixObj, gMatrixClassInfo.getValues, arr);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(arr);
        return false;
    }
    env->GetFloatArrayRegion(arr, 0, 9, values);
    env->DeleteLocalRef(arr);
    return true;
}

// --- Helper: die-on-null wrappers (replace AOSP core_jni_helpers macros) ---

static jclass findClassOrDie(JNIEnv* env, const char* name) {
    jclass clazz = env->FindClass(name);
    if (!clazz) {
        __android_log_print(ANDROID_LOG_FATAL, LOG_TAG,
                            "FindClass(%s) returned null", name);
        std::abort();
    }
    return clazz;
}

static jmethodID getMethodIDOrDie(JNIEnv* env, jclass clazz, const char* name,
                                  const char* sig) {
    jmethodID id = env->GetMethodID(clazz, name, sig);
    if (!id) {
        __android_log_print(ANDROID_LOG_FATAL, LOG_TAG,
                            "GetMethodID(%s, %s) returned null", name, sig);
        std::abort();
    }
    return id;
}

static jmethodID getStaticMethodIDOrDie(JNIEnv* env, jclass clazz, const char* name,
                                        const char* sig) {
    jmethodID id = env->GetStaticMethodID(clazz, name, sig);
    if (!id) {
        __android_log_print(ANDROID_LOG_FATAL, LOG_TAG,
                            "GetStaticMethodID(%s, %s) returned null", name, sig);
        std::abort();
    }
    return id;
}

static jfieldID getFieldIDOrDie(JNIEnv* env, jclass clazz, const char* name, const char* sig) {
    jfieldID id = env->GetFieldID(clazz, name, sig);
    if (!id) {
        __android_log_print(ANDROID_LOG_FATAL, LOG_TAG,
                            "GetFieldID(%s, %s) returned null", name, sig);
        std::abort();
    }
    return id;
}

// --- Validation helpers ---

static bool validatePointerCount(JNIEnv* env, jint pointerCount) {
    if (pointerCount < 1) {
        jniThrowException(env, "java/lang/IllegalArgumentException",
                          "pointerCount must be >= 1");
        return false;
    }
    return true;
}

static bool validatePointerPropertiesArray(JNIEnv* env, jobjectArray a, jint expected) {
    if (!a) {
        jniThrowException(env, "java/lang/NullPointerException", "pointerProperties");
        return false;
    }
    if (env->GetArrayLength(a) < expected) {
        jniThrowException(env, "java/lang/IllegalArgumentException",
                          "pointerProperties array too small");
        return false;
    }
    return true;
}

static bool validatePointerCoordsObjArray(JNIEnv* env, jobjectArray a, jint expected) {
    if (!a) {
        jniThrowException(env, "java/lang/NullPointerException", "pointerCoords");
        return false;
    }
    if (env->GetArrayLength(a) < expected) {
        jniThrowException(env, "java/lang/IllegalArgumentException",
                          "pointerCoords array too small");
        return false;
    }
    return true;
}

static bool validatePointerIndex(JNIEnv* env, jint pointerIndex, const MotionEvent& event) {
    if (pointerIndex < 0 || static_cast<size_t>(pointerIndex) >= event.getPointerCount()) {
        jniThrowException(env, "java/lang/IllegalArgumentException", "pointerIndex out of range");
        return false;
    }
    return true;
}

static bool validateHistoryPos(JNIEnv* env, jint historyPos, const MotionEvent& event) {
    if (historyPos < 0 || static_cast<size_t>(historyPos) >= event.getHistorySize()) {
        jniThrowException(env, "java/lang/IllegalArgumentException",
                          "historyPos out of range");
        return false;
    }
    return true;
}

static bool validatePointerProperties(JNIEnv* env, jobject obj) {
    if (!obj) {
        jniThrowException(env, "java/lang/NullPointerException", "pointerProperties");
        return false;
    }
    return true;
}

static bool validatePointerCoords(JNIEnv* env, jobject obj) {
    if (!obj) {
        jniThrowException(env, "java/lang/NullPointerException", "pointerCoords");
        return false;
    }
    return true;
}

// --- Java <-> native conversions ---

static void pointerPropertiesToNative(JNIEnv* env, jobject obj, PointerProperties* out) {
    out->clear();
    out->id = env->GetIntField(obj, gPointerPropertiesClassInfo.id);
    out->toolType = static_cast<ToolType>(env->GetIntField(obj, gPointerPropertiesClassInfo.toolType));
}

static void pointerPropertiesFromNative(JNIEnv* env, const PointerProperties* in, jobject obj) {
    env->SetIntField(obj, gPointerPropertiesClassInfo.id, in->id);
    env->SetIntField(obj, gPointerPropertiesClassInfo.toolType, static_cast<jint>(in->toolType));
}

static void pointerCoordsToNative(JNIEnv* env, jobject obj, float xOffset, float yOffset,
                                  PointerCoords* out) {
    out->clear();
    out->setAxisValue(AMOTION_EVENT_AXIS_X,
                      env->GetFloatField(obj, gPointerCoordsClassInfo.x) - xOffset);
    out->setAxisValue(AMOTION_EVENT_AXIS_Y,
                      env->GetFloatField(obj, gPointerCoordsClassInfo.y) - yOffset);
    out->setAxisValue(AMOTION_EVENT_AXIS_PRESSURE,
                      env->GetFloatField(obj, gPointerCoordsClassInfo.pressure));
    out->setAxisValue(AMOTION_EVENT_AXIS_SIZE,
                      env->GetFloatField(obj, gPointerCoordsClassInfo.size));
    out->setAxisValue(AMOTION_EVENT_AXIS_TOUCH_MAJOR,
                      env->GetFloatField(obj, gPointerCoordsClassInfo.touchMajor));
    out->setAxisValue(AMOTION_EVENT_AXIS_TOUCH_MINOR,
                      env->GetFloatField(obj, gPointerCoordsClassInfo.touchMinor));
    out->setAxisValue(AMOTION_EVENT_AXIS_TOOL_MAJOR,
                      env->GetFloatField(obj, gPointerCoordsClassInfo.toolMajor));
    out->setAxisValue(AMOTION_EVENT_AXIS_TOOL_MINOR,
                      env->GetFloatField(obj, gPointerCoordsClassInfo.toolMinor));
    out->setAxisValue(AMOTION_EVENT_AXIS_ORIENTATION,
                      env->GetFloatField(obj, gPointerCoordsClassInfo.orientation));
    out->setAxisValue(AMOTION_EVENT_AXIS_RELATIVE_X,
                      env->GetFloatField(obj, gPointerCoordsClassInfo.relativeX));
    out->setAxisValue(AMOTION_EVENT_AXIS_RELATIVE_Y,
                      env->GetFloatField(obj, gPointerCoordsClassInfo.relativeY));
    out->isResampled = env->GetBooleanField(obj, gPointerCoordsClassInfo.isResampled);
}

static void pointerCoordsFromNative(JNIEnv* env, const PointerCoords* in, jobject obj) {
    // Phase 1: write the 11 standard axes back to Java fields.  Packed-axis
    // (mPackedAxisBits / mPackedAxisValues) path is not exercised by
    // HelloWorld and is left as Phase 2 work.
    env->SetFloatField(obj, gPointerCoordsClassInfo.x, in->getAxisValue(AMOTION_EVENT_AXIS_X));
    env->SetFloatField(obj, gPointerCoordsClassInfo.y, in->getAxisValue(AMOTION_EVENT_AXIS_Y));
    env->SetFloatField(obj, gPointerCoordsClassInfo.pressure, in->getAxisValue(AMOTION_EVENT_AXIS_PRESSURE));
    env->SetFloatField(obj, gPointerCoordsClassInfo.size, in->getAxisValue(AMOTION_EVENT_AXIS_SIZE));
    env->SetFloatField(obj, gPointerCoordsClassInfo.touchMajor, in->getAxisValue(AMOTION_EVENT_AXIS_TOUCH_MAJOR));
    env->SetFloatField(obj, gPointerCoordsClassInfo.touchMinor, in->getAxisValue(AMOTION_EVENT_AXIS_TOUCH_MINOR));
    env->SetFloatField(obj, gPointerCoordsClassInfo.toolMajor, in->getAxisValue(AMOTION_EVENT_AXIS_TOOL_MAJOR));
    env->SetFloatField(obj, gPointerCoordsClassInfo.toolMinor, in->getAxisValue(AMOTION_EVENT_AXIS_TOOL_MINOR));
    env->SetFloatField(obj, gPointerCoordsClassInfo.orientation, in->getAxisValue(AMOTION_EVENT_AXIS_ORIENTATION));
    env->SetFloatField(obj, gPointerCoordsClassInfo.relativeX, in->getAxisValue(AMOTION_EVENT_AXIS_RELATIVE_X));
    env->SetFloatField(obj, gPointerCoordsClassInfo.relativeY, in->getAxisValue(AMOTION_EVENT_AXIS_RELATIVE_Y));
    env->SetBooleanField(obj, gPointerCoordsClassInfo.isResampled, in->isResampled);
    env->SetLongField(obj, gPointerCoordsClassInfo.mPackedAxisBits, 0);
}

}  // anonymous namespace

// =====================================================================
// JNI native methods (mirror gMotionEventMethods in AOSP MotionEvent.cpp)
// =====================================================================

extern "C" {

static jlong nativeInitialize(JNIEnv* env, jclass /*clazz*/, jlong nativePtr,
                              jint deviceId, jint source, jint displayId, jint action,
                              jint flags, jint edgeFlags, jint metaState, jint buttonState,
                              jint classification, jfloat xOffset, jfloat yOffset,
                              jfloat xPrecision, jfloat yPrecision, jlong downTimeNanos,
                              jlong eventTimeNanos, jint pointerCount,
                              jobjectArray pointerPropertiesObjArray,
                              jobjectArray pointerCoordsObjArray) {
    if (!validatePointerCount(env, pointerCount) ||
        !validatePointerPropertiesArray(env, pointerPropertiesObjArray, pointerCount) ||
        !validatePointerCoordsObjArray(env, pointerCoordsObjArray, pointerCount)) {
        return 0;
    }

    std::unique_ptr<MotionEvent> event(nativePtr ? reinterpret_cast<MotionEvent*>(nativePtr)
                                                  : new MotionEvent());

    std::vector<PointerProperties> pps(pointerCount);
    std::vector<PointerCoords> pcs(pointerCount);

    for (jint i = 0; i < pointerCount; ++i) {
        jobject ppo = env->GetObjectArrayElement(pointerPropertiesObjArray, i);
        if (!ppo) return 0;
        pointerPropertiesToNative(env, ppo, &pps[i]);
        env->DeleteLocalRef(ppo);

        jobject pco = env->GetObjectArrayElement(pointerCoordsObjArray, i);
        if (!pco) return 0;
        pointerCoordsToNative(env, pco, xOffset, yOffset, &pcs[i]);
        env->DeleteLocalRef(pco);
    }

    event->initialize(InputEvent::nextId(), deviceId, source, displayId, INVALID_HMAC, action,
                      0 /*actionButton*/, flags, edgeFlags, metaState, buttonState,
                      static_cast<MotionClassification>(classification), xOffset, yOffset,
                      xPrecision, yPrecision, kInvalidCursorPosition, kInvalidCursorPosition,
                      downTimeNanos, eventTimeNanos, pointerCount, pps.data(), pcs.data());

    return reinterpret_cast<jlong>(event.release());
}

static void nativeDispose(JNIEnv* /*env*/, jclass /*clazz*/, jlong nativePtr) {
    delete reinterpret_cast<MotionEvent*>(nativePtr);
}

static void nativeAddBatch(JNIEnv* env, jclass /*clazz*/, jlong nativePtr, jlong eventTimeNanos,
                           jobjectArray pointerCoordsObjArray, jint metaState) {
    MotionEvent* event = reinterpret_cast<MotionEvent*>(nativePtr);
    size_t pointerCount = event->getPointerCount();
    if (!validatePointerCoordsObjArray(env, pointerCoordsObjArray,
                                       static_cast<jint>(pointerCount))) {
        return;
    }
    std::vector<PointerCoords> pcs(pointerCount);
    for (size_t i = 0; i < pointerCount; ++i) {
        jobject pco = env->GetObjectArrayElement(pointerCoordsObjArray, i);
        if (!pco) return;
        pointerCoordsToNative(env, pco, event->getXOffset(), event->getYOffset(), &pcs[i]);
        env->DeleteLocalRef(pco);
    }
    event->addSample(eventTimeNanos, pcs.data());
    event->setMetaState(event->getMetaState() | metaState);
}

static jlong nativeReadFromParcel(JNIEnv* env, jclass /*clazz*/, jlong /*nativePtr*/,
                                  jobject /*parcelObj*/) {
    jniThrowException(env, "java/lang/UnsupportedOperationException",
                      "MotionEvent Parcel I/O not implemented in Phase 1");
    return 0;
}

static void nativeWriteToParcel(JNIEnv* env, jclass /*clazz*/, jlong /*nativePtr*/,
                                jobject /*parcelObj*/) {
    jniThrowException(env, "java/lang/UnsupportedOperationException",
                      "MotionEvent Parcel I/O not implemented in Phase 1");
}

static jstring nativeAxisToString(JNIEnv* env, jclass /*clazz*/, jint axis) {
    const char* label = MotionEvent::getLabel(axis);
    return label ? env->NewStringUTF(label) : nullptr;
}

static jint nativeAxisFromString(JNIEnv* env, jclass /*clazz*/, jstring labelStr) {
    if (!labelStr) return -1;
    ScopedUtfChars chars(env, labelStr);
    return MotionEvent::getAxisFromLabel(chars.c_str()).value_or(-1);
}

static void nativeGetPointerProperties(JNIEnv* env, jclass /*clazz*/, jlong nativePtr,
                                       jint pointerIndex, jobject outObj) {
    MotionEvent* event = reinterpret_cast<MotionEvent*>(nativePtr);
    if (!validatePointerIndex(env, pointerIndex, *event) ||
        !validatePointerProperties(env, outObj)) return;
    pointerPropertiesFromNative(env, event->getPointerProperties(pointerIndex), outObj);
}

static void nativeGetPointerCoords(JNIEnv* env, jclass /*clazz*/, jlong nativePtr,
                                   jint pointerIndex, jint historyPos, jobject outObj) {
    MotionEvent* event = reinterpret_cast<MotionEvent*>(nativePtr);
    if (!validatePointerIndex(env, pointerIndex, *event) ||
        !validatePointerCoords(env, outObj)) return;
    if (historyPos != HISTORY_CURRENT && !validateHistoryPos(env, historyPos, *event)) return;

    const PointerCoords* pc = (historyPos == HISTORY_CURRENT)
                                  ? event->getRawPointerCoords(pointerIndex)
                                  : event->getHistoricalRawPointerCoords(pointerIndex, historyPos);
    if (pc) pointerCoordsFromNative(env, pc, outObj);
}

// --- @FastNative / @CriticalNative getters & setters ---

static jint nativeGetPointerId(JNIEnv* env, jclass /*clazz*/, jlong nativePtr, jint idx) {
    MotionEvent* event = reinterpret_cast<MotionEvent*>(nativePtr);
    if (!validatePointerIndex(env, idx, *event)) return -1;
    return event->getPointerId(idx);
}

static jint nativeGetToolType(JNIEnv* env, jclass /*clazz*/, jlong nativePtr, jint idx) {
    MotionEvent* event = reinterpret_cast<MotionEvent*>(nativePtr);
    if (!validatePointerIndex(env, idx, *event)) return -1;
    return static_cast<jint>(event->getToolType(idx));
}

static jlong nativeGetEventTimeNanos(JNIEnv* env, jclass /*clazz*/, jlong nativePtr,
                                     jint historyPos) {
    MotionEvent* event = reinterpret_cast<MotionEvent*>(nativePtr);
    if (historyPos == HISTORY_CURRENT) return event->getEventTime();
    if (!validateHistoryPos(env, historyPos, *event)) return 0;
    return event->getHistoricalEventTime(historyPos);
}

static jfloat nativeGetRawAxisValue(JNIEnv* env, jclass /*clazz*/, jlong nativePtr, jint axis,
                                    jint idx, jint historyPos) {
    MotionEvent* event = reinterpret_cast<MotionEvent*>(nativePtr);
    if (!validatePointerIndex(env, idx, *event)) return 0.f;
    if (historyPos == HISTORY_CURRENT) return event->getRawAxisValue(axis, idx);
    if (!validateHistoryPos(env, historyPos, *event)) return 0.f;
    return event->getHistoricalRawAxisValue(axis, idx, historyPos);
}

static jfloat nativeGetAxisValue(JNIEnv* env, jclass /*clazz*/, jlong nativePtr, jint axis,
                                 jint idx, jint historyPos) {
    MotionEvent* event = reinterpret_cast<MotionEvent*>(nativePtr);
    if (!validatePointerIndex(env, idx, *event)) return 0.f;
    jfloat v;
    if (historyPos == HISTORY_CURRENT) {
        v = event->getAxisValue(axis, idx);
    } else if (!validateHistoryPos(env, historyPos, *event)) {
        v = 0.f;
    } else {
        v = event->getHistoricalAxisValue(axis, idx, historyPos);
    }
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
        "[DIAG] nativeGetAxisValue axis=%d idx=%d hist=%d -> %.1f (action=%d)",
        axis, idx, historyPos, v, event->getAction());
    return v;
}

static void nativeTransform(JNIEnv* env, jclass /*clazz*/, jlong nativePtr, jobject matrixObj) {
    MotionEvent* event = reinterpret_cast<MotionEvent*>(nativePtr);
    std::array<float, 9> m;
    if (!extractMatrixContents(env, matrixObj, m.data())) return;
    event->transform(m);
}

static void nativeApplyTransform(JNIEnv* env, jclass /*clazz*/, jlong nativePtr,
                                 jobject matrixObj) {
    MotionEvent* event = reinterpret_cast<MotionEvent*>(nativePtr);
    std::array<float, 9> m;
    if (!extractMatrixContents(env, matrixObj, m.data())) return;
    event->applyTransform(m);
}

// --- @CriticalNative (no JNIEnv*) ---

static jlong nativeCopy(jlong destPtr, jlong srcPtr, jboolean keepHistory) {
    MotionEvent* dest = reinterpret_cast<MotionEvent*>(destPtr);
    if (!dest) dest = new MotionEvent();
    MotionEvent* src = reinterpret_cast<MotionEvent*>(srcPtr);
    if (src) dest->copyFrom(src, keepHistory);
    return reinterpret_cast<jlong>(dest);
}

#define ME(p) (reinterpret_cast<MotionEvent*>(p))

static jint nativeGetId(jlong p) { return ME(p)->getId(); }
static jint nativeGetDeviceId(jlong p) { return ME(p)->getDeviceId(); }
static jint nativeGetSource(jlong p) { return ME(p)->getSource(); }
static void nativeSetSource(jlong p, jint v) { ME(p)->setSource(v); }
static jint nativeGetDisplayId(jlong p) { return ME(p)->getDisplayId(); }
static void nativeSetDisplayId(jlong p, jint v) { ME(p)->setDisplayId(v); }
static jint nativeGetAction(jlong p) {
    jint a = ME(p)->getAction();
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
        "[DIAG] nativeGetAction -> %d (ptrs=%zu)", a, ME(p)->getPointerCount());
    return a;
}
static void nativeSetAction(jlong p, jint v) { ME(p)->setAction(v); }
static jint nativeGetActionButton(jlong p) { return ME(p)->getActionButton(); }
static void nativeSetActionButton(jlong p, jint v) { ME(p)->setActionButton(v); }
static jboolean nativeIsTouchEvent(jlong p) { return ME(p)->isTouchEvent() ? JNI_TRUE : JNI_FALSE; }
static jint nativeGetFlags(jlong p) { return ME(p)->getFlags(); }
static void nativeSetFlags(jlong p, jint v) { ME(p)->setFlags(v); }
static jint nativeGetEdgeFlags(jlong p) { return ME(p)->getEdgeFlags(); }
static void nativeSetEdgeFlags(jlong p, jint v) { ME(p)->setEdgeFlags(v); }
static jint nativeGetMetaState(jlong p) { return ME(p)->getMetaState(); }
static jint nativeGetButtonState(jlong p) { return ME(p)->getButtonState(); }
static void nativeSetButtonState(jlong p, jint v) { ME(p)->setButtonState(v); }
static jint nativeGetClassification(jlong p) { return static_cast<jint>(ME(p)->getClassification()); }
static void nativeOffsetLocation(jlong p, jfloat dx, jfloat dy) { ME(p)->offsetLocation(dx, dy); }
static jfloat nativeGetXOffset(jlong p) { return ME(p)->getXOffset(); }
static jfloat nativeGetYOffset(jlong p) { return ME(p)->getYOffset(); }
static jfloat nativeGetXPrecision(jlong p) { return ME(p)->getXPrecision(); }
static jfloat nativeGetYPrecision(jlong p) { return ME(p)->getYPrecision(); }
static jfloat nativeGetXCursorPosition(jlong p) { return ME(p)->getXCursorPosition(); }
static jfloat nativeGetYCursorPosition(jlong p) { return ME(p)->getYCursorPosition(); }
static void nativeSetCursorPosition(jlong p, jfloat x, jfloat y) { ME(p)->setCursorPosition(x, y); }
static jlong nativeGetDownTimeNanos(jlong p) { return ME(p)->getDownTime(); }
static void nativeSetDownTimeNanos(jlong p, jlong v) { ME(p)->setDownTime(v); }
static jint nativeGetPointerCount(jlong p) {
    jint n = static_cast<jint>(ME(p)->getPointerCount());
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
        "[DIAG] nativeGetPointerCount -> %d (action=%d)", n, ME(p)->getAction());
    return n;
}
static jint nativeFindPointerIndex(jlong p, jint id) { return static_cast<jint>(ME(p)->findPointerIndex(id)); }
static jint nativeGetHistorySize(jlong p) { return static_cast<jint>(ME(p)->getHistorySize()); }
static void nativeScale(jlong p, jfloat s) { ME(p)->scale(s); }
static jint nativeGetSurfaceRotation(jlong p) { return ME(p)->getSurfaceRotation(); }

#undef ME

}  // extern "C"

// =====================================================================
// JNI method table + register
// =====================================================================

static const JNINativeMethod gMotionEventMethods[] = {
    {"nativeInitialize",
     "(JIIIIIIIIIFFFFJJI[Landroid/view/MotionEvent$PointerProperties;"
     "[Landroid/view/MotionEvent$PointerCoords;)J",
     reinterpret_cast<void*>(nativeInitialize)},
    {"nativeDispose", "(J)V", reinterpret_cast<void*>(nativeDispose)},
    {"nativeAddBatch", "(JJ[Landroid/view/MotionEvent$PointerCoords;I)V",
     reinterpret_cast<void*>(nativeAddBatch)},
    {"nativeReadFromParcel", "(JLandroid/os/Parcel;)J",
     reinterpret_cast<void*>(nativeReadFromParcel)},
    {"nativeWriteToParcel", "(JLandroid/os/Parcel;)V",
     reinterpret_cast<void*>(nativeWriteToParcel)},
    {"nativeAxisToString", "(I)Ljava/lang/String;", reinterpret_cast<void*>(nativeAxisToString)},
    {"nativeAxisFromString", "(Ljava/lang/String;)I",
     reinterpret_cast<void*>(nativeAxisFromString)},
    {"nativeGetPointerProperties", "(JILandroid/view/MotionEvent$PointerProperties;)V",
     reinterpret_cast<void*>(nativeGetPointerProperties)},
    {"nativeGetPointerCoords", "(JIILandroid/view/MotionEvent$PointerCoords;)V",
     reinterpret_cast<void*>(nativeGetPointerCoords)},
    // @FastNative
    {"nativeGetPointerId", "(JI)I", reinterpret_cast<void*>(nativeGetPointerId)},
    {"nativeGetToolType", "(JI)I", reinterpret_cast<void*>(nativeGetToolType)},
    {"nativeGetEventTimeNanos", "(JI)J", reinterpret_cast<void*>(nativeGetEventTimeNanos)},
    {"nativeGetRawAxisValue", "(JIII)F", reinterpret_cast<void*>(nativeGetRawAxisValue)},
    {"nativeGetAxisValue", "(JIII)F", reinterpret_cast<void*>(nativeGetAxisValue)},
    {"nativeTransform", "(JLandroid/graphics/Matrix;)V", reinterpret_cast<void*>(nativeTransform)},
    {"nativeApplyTransform", "(JLandroid/graphics/Matrix;)V",
     reinterpret_cast<void*>(nativeApplyTransform)},
    // @CriticalNative
    {"nativeCopy", "(JJZ)J", reinterpret_cast<void*>(nativeCopy)},
    {"nativeGetId", "(J)I", reinterpret_cast<void*>(nativeGetId)},
    {"nativeGetDeviceId", "(J)I", reinterpret_cast<void*>(nativeGetDeviceId)},
    {"nativeGetSource", "(J)I", reinterpret_cast<void*>(nativeGetSource)},
    {"nativeSetSource", "(JI)V", reinterpret_cast<void*>(nativeSetSource)},
    {"nativeGetDisplayId", "(J)I", reinterpret_cast<void*>(nativeGetDisplayId)},
    {"nativeSetDisplayId", "(JI)V", reinterpret_cast<void*>(nativeSetDisplayId)},
    {"nativeGetAction", "(J)I", reinterpret_cast<void*>(nativeGetAction)},
    {"nativeSetAction", "(JI)V", reinterpret_cast<void*>(nativeSetAction)},
    {"nativeGetActionButton", "(J)I", reinterpret_cast<void*>(nativeGetActionButton)},
    {"nativeSetActionButton", "(JI)V", reinterpret_cast<void*>(nativeSetActionButton)},
    {"nativeIsTouchEvent", "(J)Z", reinterpret_cast<void*>(nativeIsTouchEvent)},
    {"nativeGetFlags", "(J)I", reinterpret_cast<void*>(nativeGetFlags)},
    {"nativeSetFlags", "(JI)V", reinterpret_cast<void*>(nativeSetFlags)},
    {"nativeGetEdgeFlags", "(J)I", reinterpret_cast<void*>(nativeGetEdgeFlags)},
    {"nativeSetEdgeFlags", "(JI)V", reinterpret_cast<void*>(nativeSetEdgeFlags)},
    {"nativeGetMetaState", "(J)I", reinterpret_cast<void*>(nativeGetMetaState)},
    {"nativeGetButtonState", "(J)I", reinterpret_cast<void*>(nativeGetButtonState)},
    {"nativeSetButtonState", "(JI)V", reinterpret_cast<void*>(nativeSetButtonState)},
    {"nativeGetClassification", "(J)I", reinterpret_cast<void*>(nativeGetClassification)},
    {"nativeOffsetLocation", "(JFF)V", reinterpret_cast<void*>(nativeOffsetLocation)},
    {"nativeGetXOffset", "(J)F", reinterpret_cast<void*>(nativeGetXOffset)},
    {"nativeGetYOffset", "(J)F", reinterpret_cast<void*>(nativeGetYOffset)},
    {"nativeGetXPrecision", "(J)F", reinterpret_cast<void*>(nativeGetXPrecision)},
    {"nativeGetYPrecision", "(J)F", reinterpret_cast<void*>(nativeGetYPrecision)},
    {"nativeGetXCursorPosition", "(J)F", reinterpret_cast<void*>(nativeGetXCursorPosition)},
    {"nativeGetYCursorPosition", "(J)F", reinterpret_cast<void*>(nativeGetYCursorPosition)},
    {"nativeSetCursorPosition", "(JFF)V", reinterpret_cast<void*>(nativeSetCursorPosition)},
    {"nativeGetDownTimeNanos", "(J)J", reinterpret_cast<void*>(nativeGetDownTimeNanos)},
    {"nativeSetDownTimeNanos", "(JJ)V", reinterpret_cast<void*>(nativeSetDownTimeNanos)},
    {"nativeGetPointerCount", "(J)I", reinterpret_cast<void*>(nativeGetPointerCount)},
    {"nativeFindPointerIndex", "(JI)I", reinterpret_cast<void*>(nativeFindPointerIndex)},
    {"nativeGetHistorySize", "(J)I", reinterpret_cast<void*>(nativeGetHistorySize)},
    {"nativeScale", "(JF)V", reinterpret_cast<void*>(nativeScale)},
    {"nativeGetSurfaceRotation", "(J)I", reinterpret_cast<void*>(nativeGetSurfaceRotation)},
};

int register_android_view_MotionEvent(JNIEnv* env) {
    jclass me = findClassOrDie(env, "android/view/MotionEvent");
    if (env->RegisterNatives(me, gMotionEventMethods,
                             sizeof(gMotionEventMethods) / sizeof(gMotionEventMethods[0])) < 0) {
        __android_log_print(ANDROID_LOG_FATAL, LOG_TAG, "RegisterNatives failed");
        env->DeleteLocalRef(me);
        return -1;
    }

    gMotionEventClassInfo.clazz = static_cast<jclass>(env->NewGlobalRef(me));
    gMotionEventClassInfo.obtain = getStaticMethodIDOrDie(env, me, "obtain",
                                                         "()Landroid/view/MotionEvent;");
    gMotionEventClassInfo.recycle = getMethodIDOrDie(env, me, "recycle", "()V");
    gMotionEventClassInfo.mNativePtr = getFieldIDOrDie(env, me, "mNativePtr", "J");
    env->DeleteLocalRef(me);

    jclass pc = findClassOrDie(env, "android/view/MotionEvent$PointerCoords");
    gPointerCoordsClassInfo.mPackedAxisBits = getFieldIDOrDie(env, pc, "mPackedAxisBits", "J");
    gPointerCoordsClassInfo.mPackedAxisValues = getFieldIDOrDie(env, pc, "mPackedAxisValues", "[F");
    gPointerCoordsClassInfo.x = getFieldIDOrDie(env, pc, "x", "F");
    gPointerCoordsClassInfo.y = getFieldIDOrDie(env, pc, "y", "F");
    gPointerCoordsClassInfo.pressure = getFieldIDOrDie(env, pc, "pressure", "F");
    gPointerCoordsClassInfo.size = getFieldIDOrDie(env, pc, "size", "F");
    gPointerCoordsClassInfo.touchMajor = getFieldIDOrDie(env, pc, "touchMajor", "F");
    gPointerCoordsClassInfo.touchMinor = getFieldIDOrDie(env, pc, "touchMinor", "F");
    gPointerCoordsClassInfo.toolMajor = getFieldIDOrDie(env, pc, "toolMajor", "F");
    gPointerCoordsClassInfo.toolMinor = getFieldIDOrDie(env, pc, "toolMinor", "F");
    gPointerCoordsClassInfo.orientation = getFieldIDOrDie(env, pc, "orientation", "F");
    gPointerCoordsClassInfo.relativeX = getFieldIDOrDie(env, pc, "relativeX", "F");
    gPointerCoordsClassInfo.relativeY = getFieldIDOrDie(env, pc, "relativeY", "F");
    gPointerCoordsClassInfo.isResampled = getFieldIDOrDie(env, pc, "isResampled", "Z");
    env->DeleteLocalRef(pc);

    jclass pp = findClassOrDie(env, "android/view/MotionEvent$PointerProperties");
    gPointerPropertiesClassInfo.id = getFieldIDOrDie(env, pp, "id", "I");
    gPointerPropertiesClassInfo.toolType = getFieldIDOrDie(env, pp, "toolType", "I");
    env->DeleteLocalRef(pp);

    // android.graphics.Matrix cache is deferred to first nativeTransform/
    // nativeApplyTransform call — see extractMatrixContents comment.
    // Doing it here triggers Matrix.<clinit> -> nCreate, but Matrix.nCreate
    // lives in libhwui.so which is dlopen'd later in startReg.
    gMatrixClassInfo.clazz = nullptr;
    gMatrixClassInfo.getValues = nullptr;

    return 0;
}

}  // namespace android
