/*
 * window_session_adapter.cpp
 *
 * JNI registration for adapter.window.WindowSessionAdapter via RegisterNatives.
 * Replaces the legacy Java_adapter_window_WindowSessionAdapter_* exports
 * previously in framework/core/jni/adapter_bridge.cpp.
 *
 * Class:  adapter/window/WindowSessionAdapter  (BCP - oh-adapter-framework.jar)
 * Registered from adapter_bridge.cpp's JNI_OnLoad via
 *   register_WindowSessionAdapter(env).
 *
 * 15 natives: 9 session + 6 surface/buffer management.
 */

#include "adapter_bridge.h"
#include "oh_window_manager_client.h"
#include "oh_input_bridge.h"
#include "oh_surface_bridge.h"
#include "oh_graphic_buffer_producer.h"

#include <android/log.h>
#include <jni.h>
#include <dlfcn.h>
#include <string>
#include <surface.h>   // OHOS::Surface — explicit include needed so sptr<Surface>
                        // dtor (instantiated through oh_window_manager_client.h
                        // refbase chain) sees the complete type; without this
                        // refbase.h:928 errors with "incomplete type OHOS::Surface".

#define TAG "OH_WSAJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
// __android_log_print is a no-op on OH; use HiLogPrint (innerAPI) directly for visible G3.6 diag.
extern "C" int HiLogPrint(int, int, unsigned int, const char*, const char*, ...) __attribute__((__format__(printf, 5, 6)));
#define HLOG(...) HiLogPrint(3, 4, 0xD000F00u, "OH_WSAJNI_G36", __VA_ARGS__)

using namespace oh_adapter;

namespace {

std::string jstr(JNIEnv* env, jstring s) {
    if (!s) return "";
    const char* raw = env->GetStringUTFChars(s, nullptr);
    std::string result(raw);
    env->ReleaseStringUTFChars(s, raw);
    return result;
}

// -------- session (9) --------

jlong nativeGetOHSessionService_impl(JNIEnv*, jclass) {
    return (jlong)&OHWindowManagerClient::getInstance();
}

// Spec: doc/window_manager_ipc_adapter_design.html §3.1.5.6.2
// Returns int[6]: {sessionId, surfaceNodeId, displayId, w, h, wsErrCode}.
jintArray nativeCreateSession_impl(JNIEnv* env, jclass,
                                   jobject androidWindow,
                                   jstring bundleNameJ, jstring abilityNameJ,
                                   jstring moduleNameJ, jstring windowNameJ,
                                   jint androidWindowType, jint displayId,
                                   jint requestedWidth, jint requestedHeight,
                                   jlong ohTokenAddrJ) {
    std::string bundleName  = jstr(env, bundleNameJ);
    std::string abilityName = jstr(env, abilityNameJ);
    std::string moduleName  = jstr(env, moduleNameJ);
    std::string windowName  = jstr(env, windowNameJ);
    if (moduleName.empty()) moduleName = "entry";          // HAP default
    if (abilityName.empty()) abilityName = "MainAbility";
    if (windowName.empty()) windowName = "AndroidWindow";

    JavaVM* jvm = AdapterBridge::getInstance().getJavaVM();
    OHWindowSession session = OHWindowManagerClient::getInstance().createSession(
            jvm, androidWindow,
            bundleName, abilityName, moduleName, windowName,
            androidWindowType, displayId,
            requestedWidth, requestedHeight,
            static_cast<uint64_t>(ohTokenAddrJ));

    jintArray result = env->NewIntArray(6);
    jint info[6] = {
        session.sessionId,
        session.surfaceNodeId,
        session.displayId,
        session.width,
        session.height,
        session.wsErr,
    };
    env->SetIntArrayRegion(result, 0, 6, info);
    return result;
}

jint nativeUpdateSessionRect_impl(JNIEnv*, jclass,
                                  jint sessionId, jint x, jint y,
                                  jint width, jint height) {
    { static int wl_n = 0; if (wl_n < 8) { wl_n++;
        fprintf(stderr, "[WESTLAKE-WSA] nativeUpdateSessionRect_impl ENTER #%d\n", wl_n); fflush(stderr); } }

    return OHWindowManagerClient::getInstance().updateSessionRect(
            sessionId, x, y, width, height);
}

jint nativeNotifyDrawingCompleted_impl(JNIEnv*, jclass, jint sessionId) {
    { static int wl_n = 0; if (wl_n < 8) { wl_n++;
        fprintf(stderr, "[WESTLAKE-WSA] nativeNotifyDrawingCompleted_impl ENTER #%d\n", wl_n); fflush(stderr); } }

    return OHWindowManagerClient::getInstance().notifyDrawingCompleted(sessionId);
}

void nativeDestroySession_impl(JNIEnv*, jclass, jint sessionId) {
    OHWindowManagerClient::getInstance().destroySession(sessionId);
}

// 2026-05-19: visibility transition helpers — counterpart to AddWindow at
// session creation.  See oh_window_manager_client.cpp::hideWindow / showWindow
// for design alternatives (A/B/C/D) — adapter uses in-App-process C++ cache
// (Option D), idempotent at native layer.
jint nativeHideWindow_impl(JNIEnv*, jclass, jint sessionId) {
    return OHWindowManagerClient::getInstance().hideWindow(sessionId);
}

jint nativeShowWindow_impl(JNIEnv*, jclass, jint sessionId) {
    { static int wl_n = 0; if (wl_n < 8) { wl_n++;
        fprintf(stderr, "[WESTLAKE-WSA] nativeShowWindow_impl ENTER #%d\n", wl_n); fflush(stderr); } }

    return OHWindowManagerClient::getInstance().showWindow(sessionId);
}

extern "C" void wl_send_app_visible(JNIEnv* env);   // WESTLAKE §253 (oh_window_manager_client.cpp)

// WESTLAKE §280: window-path throw gate. Set just BEFORE the SurfaceControl block in
// WindowSessionAdapter.relayout (nativeGetSurfaceNodeId is its first statement) and cleared just
// AFTER it (nativeAttachSessionToSc). Thread::SetException prints whatever is thrown in between --
// that is the exception the adapter's `catch (Exception e) { Log.e(...) }` swallows invisibly (§279).
// Resolved from libart at runtime (it is DEFINED there, §280).
static bool* wl_scgate() {
    static bool* p = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        void* h = dlopen("libart.so", RTLD_NOW | RTLD_NOLOAD);
        if (h == nullptr) { h = dlopen("libart.so", RTLD_NOW); }
        if (h != nullptr) { p = reinterpret_cast<bool*>(dlsym(h, "g_wl_in_surfacectl")); }
        fprintf(stderr, "[WESTLAKE-SCGATE] libart gate symbol=%p\n", (void*) p);
        fflush(stderr);
    }
    return p;
}

// WESTLAKE §283: defined in oh_window_manager_client.cpp (same .so, extern "C", default
// visibility).  Declared here rather than dlsym'd because it is a plain intra-library call.
extern "C" void oh_wm_set_last_session(int32_t sessionId);
// WESTLAKE §283d: per-thread relayout-session stamp consumed by alloc_sc()
// (android_view_SurfaceControl.cpp).  Same .so, extern "C", default visibility.
extern "C" void wl_set_relayout_session(int32_t sessionId);

jlong nativeGetSurfaceNodeId_impl(JNIEnv* env, jclass, jint sessionId) {
    { static int wl_n = 0; if (wl_n < 8) { wl_n++;
        fprintf(stderr, "[WESTLAKE-WSA] nativeGetSurfaceNodeId_impl ENTER #%d\n", wl_n); fflush(stderr); } }

    // WESTLAKE §253: WindowSessionAdapter.relayout() calls this, i.e. AFTER ViewRootImpl.setView()
    // has completed -- the right moment to deliver IWindow.dispatchAppVisibility(true). Without it
    // `mAppVisible` stays false, getHostVisibility() returns GONE and performDraw never runs (§251).
    // §252 sent it from createSession (inside addToDisplay) which was too early.
    fprintf(stderr, "[WESTLAKE-WSA] gsni:a pre wl_send_app_visible\n"); fflush(stderr);
    // §283t probe REMOVED: it dlopen'd libEGL/bridge/libnative_window on the relayout path,
    // which perturbs the very startup race being measured.  Result recorded: all three
    // resolve fine in-process (dlsym(eglCreateWindowSurface)=0x7f286d999c), so hwui's
    // internal EGL wrapper is NOT failing to resolve.
    wl_send_app_visible(env);
    fprintf(stderr, "[WESTLAKE-WSA] gsni:b post wl_send_app_visible\n"); fflush(stderr);
    fprintf(stderr, "[WESTLAKE-WSA] gsni:c pre scgate\n"); fflush(stderr);
    { bool* g = wl_scgate(); if (g) *g = true; }
    fprintf(stderr, "[WESTLAKE-WSA] gsni:d post scgate\n"); fflush(stderr);   // WESTLAKE §280: arm the throw gate
    // WESTLAKE §283 — THE INVALID-Surface ROOT CAUSE.
    // ViewRootImpl.relayoutWindow() -> Surface.copyFrom(mSurfaceControl) -> JNI
    // nativeGetFromSurfaceControl -> sc_to_oh_native_window(sc), which resolves the OH
    // NativeWindow from a session id obtained as:
    //     oh_sc_get_session(sc)  ||  oh_wm_get_last_session()
    // Measured on device (hilog, privacy off):
    //     sc_to_oh_native_window: sc=0x7f0c720dc0 scSessionId=0 lastSessionId=0 -> 0
    // BOTH are 0, so it returns 0, so Surface.mNativeObject stays 0x0, so mSurface is
    // invalid, so ViewRootImpl.draw() returns before syncAndDrawFrame/drawSoftware and
    // NOTHING is ever composited -> black window.
    // Why lastSessionId is 0: only OHWindowManagerClient::createSession stamps it, and on
    // this arm64 port the window sessions are established through a different path (its
    // "createSession: success" never appears in hilog), so the hint is never set.  And
    // alloc_sc() seeds sc->sessionId from that same 0 hint, which is why scSessionId is 0 too.
    // relayout() calls THIS function with the authoritative sessionId immediately before the
    // SurfaceControl is built/copied, so stamping the hint here fixes both reads at once --
    // and it stays a bridge-only change (nativeAttachSessionToSc, which would have done this
    // from Java, is not declared in the deployed BCP jar, so it can never bind).
    // ★Stamp only on CHANGE: oh_wm_set_last_session() opens an OH_BR_IPC_SCOPE (HiLog +
    // IPC tracing) on every call, and relayout runs per frame.  Calling it unconditionally
    // measurably slowed the UI thread (performTraversals collapsed 22 -> 1 per run).
    // §283c A/B: DISABLED pending investigation.  With the stamp enabled the app stalled at
    // performTraversals=1 / draw=0 in 3/3 runs, versus performTraversals=22 / draw=29 without
    // it.  Stamping the "last session" hint evidently changes which OH NativeWindow downstream
    // code binds to (alloc_sc seeds sc->sessionId from it, and BBQ/EGL resolve through it), so
    // it must be re-introduced together with the correct per-window session, not globally.
    // §283d: per-thread stamp (safe -- does NOT touch the global hint that §283c showed is harmful).
    wl_set_relayout_session(static_cast<int32_t>(sessionId));
#if 0
    {
        static int32_t wl_stamped = 0;
        const int32_t wl_sid = static_cast<int32_t>(sessionId);
        if (wl_sid != 0 && wl_sid != wl_stamped) {
            wl_stamped = wl_sid;
            oh_wm_set_last_session(wl_sid);
        }
    }
#endif
    fprintf(stderr, "[WESTLAKE-WSA] gsni:e pre getSurfaceNodeId\n"); fflush(stderr);
    jlong wl_r = OHWindowManagerClient::getInstance().getSurfaceNodeId(sessionId);
    fprintf(stderr, "[WESTLAKE-WSA] gsni:f post getSurfaceNodeId\n"); fflush(stderr);
    return wl_r;
}

// [G3.6-MULTIWINDOW 2026-06-02] Stamp the SurfaceControl's session so each window's SC
// resolves to ITS OWN OH NativeWindow instead of the thread-local last-attached session
// (which collides for multi-window apps like noice: AppIntroActivity + MainActivity -> the
// 2nd window's eglCreateWindowSurface collided on the 1st window's already-bound nw -> abort).
// Calls oh_sc_attach_session (exported by liboh_android_runtime.so) via dlsym at RUNTIME — a
// link-time reference would leave bridge.so with an unresolved symbol and break its dlopen
// (the exact reason the original nativeAttachSessionToSc was removed, G2.14r). oh_sc_attach_session
// itself HiLogPrints "attach_session: sc=.. sessionId=..", so the stamp is observable in hilog.
void nativeAttachSessionToSc_impl(JNIEnv* env, jclass, jobject scObj, jint sessionId) {
    // WESTLAKE §279: this runs IMMEDIATELY AFTER `outSurfaceControl.copyFrom(sc,"OH_relayout")` in
    // WindowSessionAdapter.relayout. It is the one place that can answer, without touching HiLog
    // (empty here) or relying on the libart interpreter probe (blind to JIT'd calls), whether the
    // copyFrom actually completed:
    //   prints  -> copyFrom succeeded, outSurfaceControl IS populated -> defect is downstream, in
    //              ViewRootImpl building mSurface from that SurfaceControl (§275: mSurface invalid);
    //   silent  -> copyFrom threw and the Binder/AIDL stub swallowed it.
    {
        static int wl_atn = 0;
        if (wl_atn < 6) {
            wl_atn++;
            fprintf(stderr, "[WESTLAKE-SCATTACH] nativeAttachSessionToSc #%d sessionId=%d sc=%p\n",
                    wl_atn, (int) sessionId, (void*) scObj);
            fflush(stderr);
        }
        { bool* g = wl_scgate(); if (g) *g = false; }   // WESTLAKE §280: region completed OK
    }
    if (!scObj) { HLOG("attach: scObj=null session=%{public}d", sessionId); return; }
    jclass scCls = env->GetObjectClass(scObj);
    jfieldID fid = scCls ? env->GetFieldID(scCls, "mNativeObject", "J") : nullptr;
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (!fid) { HLOG("attach: mNativeObject field NOT FOUND session=%{public}d", sessionId); return; }
    jlong nativePtr = env->GetLongField(scObj, fid);
    if (!nativePtr) { HLOG("attach: nativePtr=0 session=%{public}d", sessionId); return; }
    typedef int (*attach_fn_t)(jlong, int32_t);
    static attach_fn_t fn = nullptr;
    if (!fn) {
        fn = reinterpret_cast<attach_fn_t>(dlsym(RTLD_DEFAULT, "oh_sc_attach_session"));
        if (!fn) {
            // liboh_android_runtime.so may be loaded RTLD_LOCAL (not in the global scope),
            // so RTLD_DEFAULT can't see oh_sc_attach_session — grab its handle explicitly.
            void* h = dlopen("liboh_android_runtime.so", RTLD_NOLOAD | RTLD_GLOBAL);
            if (!h) h = dlopen("liboh_android_runtime.so", RTLD_NOW | RTLD_GLOBAL);
            if (h) fn = reinterpret_cast<attach_fn_t>(dlsym(h, "oh_sc_attach_session"));
        }
    }
    HLOG("attach: session=%{public}d nativePtr=%{public}p fn=%{public}p", sessionId, (void*)nativePtr, (void*)fn);
    if (fn) fn(nativePtr, static_cast<int32_t>(sessionId));
}

jint nativeInjectTouchEvent_impl(JNIEnv*, jclass,
                                 jint sessionId, jint action,
                                 jfloat x, jfloat y,
                                 jlong downTime, jlong eventTime) {
    return OHInputBridge::getInstance().injectTouchEvent(
            sessionId, action, x, y, downTime, eventTime);
}

// -------- surface / buffer (6) --------

jboolean nativeCreateOHSurface_impl(JNIEnv* env, jclass,
                                    jint sessionId, jstring windowName,
                                    jint width, jint height, jint format) {
    { static int wl_n = 0; if (wl_n < 8) { wl_n++;
        fprintf(stderr, "[WESTLAKE-WSA] nativeCreateOHSurface_impl ENTER #%d\n", wl_n); fflush(stderr); } }

    std::string name = jstr(env, windowName);
    bool result = OHSurfaceBridge::getInstance().createSurface(
            sessionId, name.c_str(), width, height, format);
    return (jboolean)result;
}

jlong nativeGetSurfaceHandle_impl(JNIEnv*, jclass,
                                  jint sessionId, jint width, jint height,
                                  jint format) {
    { static int wl_n = 0; if (wl_n < 8) { wl_n++;
        fprintf(stderr, "[WESTLAKE-WSA] nativeGetSurfaceHandle_impl ENTER #%d\n", wl_n); fflush(stderr); } }

    return OHSurfaceBridge::getInstance().getSurfaceHandle(
            sessionId, width, height, format);
}

void nativeNotifySurfaceDrawingCompleted_impl(JNIEnv*, jclass, jint sessionId) {
    { static int wl_n = 0; if (wl_n < 8) { wl_n++;
        fprintf(stderr, "[WESTLAKE-WSA] nativeNotifySurfaceDrawingCompleted_impl ENTER #%d\n", wl_n); fflush(stderr); } }

    OHSurfaceBridge::getInstance().notifyDrawingCompleted(sessionId);
}

void nativeUpdateSurfaceSize_impl(JNIEnv*, jclass,
                                  jint sessionId, jint width, jint height) {
    { static int wl_n = 0; if (wl_n < 8) { wl_n++;
        fprintf(stderr, "[WESTLAKE-WSA] nativeUpdateSurfaceSize_impl ENTER #%d\n", wl_n); fflush(stderr); } }

    OHSurfaceBridge::getInstance().updateSurfaceSize(sessionId, width, height);
}

void nativeDestroyOHSurface_impl(JNIEnv*, jclass, jint sessionId) {
    OHSurfaceBridge::getInstance().destroySurface(sessionId);
}

jintArray nativeDequeueBuffer_impl(JNIEnv* env, jclass,
                                   jlong producerHandle, jint width, jint height,
                                   jint format, jlong usage) {
    { static int wl_n = 0; if (wl_n < 8) { wl_n++;
        fprintf(stderr, "[WESTLAKE-WSA] nativeDequeueBuffer_impl ENTER #%d\n", wl_n); fflush(stderr); } }

    auto* producer = reinterpret_cast<OHGraphicBufferProducer*>(producerHandle);
    if (!producer) {
        LOGE("nativeDequeueBuffer: null producer handle");
        return nullptr;
    }
    int slot = -1, fenceFd = -1;
    int ret = producer->dequeueBuffer(&slot, &fenceFd, width, height, format, usage);
    if (ret != 0) {
        LOGE("nativeDequeueBuffer: dequeueBuffer failed (ret=%d)", ret);
        return nullptr;
    }
    int32_t bufWidth = 0, bufHeight = 0, stride = 0, bufFormat = 0;
    producer->getBufferInfo(slot, &bufWidth, &bufHeight, &stride, &bufFormat);
    int dmabufFd = producer->getBufferFd(slot);

    jintArray result = env->NewIntArray(6);
    jint info[6] = { slot, fenceFd, dmabufFd, bufWidth, bufHeight, stride };
    env->SetIntArrayRegion(result, 0, 6, info);
    return result;
}

jint nativeQueueBuffer_impl(JNIEnv*, jclass,
                            jlong producerHandle, jint slot, jint fenceFd,
                            jlong timestamp,
                            jint cropLeft, jint cropTop, jint cropRight,
                            jint cropBottom) {
    { static int wl_n = 0; if (wl_n < 8) { wl_n++;
        fprintf(stderr, "[WESTLAKE-WSA] nativeQueueBuffer_impl ENTER #%d\n", wl_n); fflush(stderr); } }

    auto* producer = reinterpret_cast<OHGraphicBufferProducer*>(producerHandle);
    if (!producer) {
        LOGE("nativeQueueBuffer: null producer handle");
        return -1;
    }
    return producer->queueBuffer(slot, fenceFd, timestamp,
                                 cropLeft, cropTop, cropRight, cropBottom);
}

jint nativeCancelBuffer_impl(JNIEnv*, jclass,
                             jlong producerHandle, jint slot, jint fenceFd) {
    auto* producer = reinterpret_cast<OHGraphicBufferProducer*>(producerHandle);
    if (!producer) {
        LOGE("nativeCancelBuffer: null producer handle");
        return -1;
    }
    return producer->cancelBuffer(slot, fenceFd);
}

const JNINativeMethod kMethods[] = {
    // session
    {"nativeGetOHSessionService", "()J", (void*)nativeGetOHSessionService_impl},
    {"nativeCreateSession",
        "(Ljava/lang/Object;Ljava/lang/String;Ljava/lang/String;"
        "Ljava/lang/String;Ljava/lang/String;IIIIJ)[I",
        (void*)nativeCreateSession_impl},
    {"nativeUpdateSessionRect",        "(IIIII)I",   (void*)nativeUpdateSessionRect_impl},
    {"nativeNotifyDrawingCompleted",   "(I)I",       (void*)nativeNotifyDrawingCompleted_impl},
    {"nativeDestroySession",           "(I)V",       (void*)nativeDestroySession_impl},
    {"nativeHideWindow",               "(I)I",       (void*)nativeHideWindow_impl},
    {"nativeShowWindow",               "(I)I",       (void*)nativeShowWindow_impl},
    {"nativeGetSurfaceNodeId",         "(I)J",       (void*)nativeGetSurfaceNodeId_impl},
    {"nativeAttachSessionToSc", "(Landroid/view/SurfaceControl;I)V", (void*)nativeAttachSessionToSc_impl},
    {"nativeInjectTouchEvent",         "(IIFFJJ)I",  (void*)nativeInjectTouchEvent_impl},

    // surface / buffer
    {"nativeCreateOHSurface",
        "(ILjava/lang/String;III)Z",
        (void*)nativeCreateOHSurface_impl},
    {"nativeGetSurfaceHandle",         "(IIII)J",    (void*)nativeGetSurfaceHandle_impl},
    {"nativeNotifySurfaceDrawingCompleted", "(I)V",  (void*)nativeNotifySurfaceDrawingCompleted_impl},
    {"nativeUpdateSurfaceSize",        "(III)V",     (void*)nativeUpdateSurfaceSize_impl},
    {"nativeDestroyOHSurface",         "(I)V",       (void*)nativeDestroyOHSurface_impl},
    {"nativeDequeueBuffer",            "(JIIIJ)[I",  (void*)nativeDequeueBuffer_impl},
    {"nativeQueueBuffer",              "(JIIJIIII)I",(void*)nativeQueueBuffer_impl},
    {"nativeCancelBuffer",             "(JII)I",     (void*)nativeCancelBuffer_impl},
};

}  // namespace

int register_WindowSessionAdapter(JNIEnv* env) {
    jclass clazz = env->FindClass("adapter/window/WindowSessionAdapter");
    if (!clazz) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        LOGE("register_WindowSessionAdapter: FindClass returned null");
        return JNI_ERR;
    }
    jint rc = env->RegisterNatives(clazz, kMethods,
                                   sizeof(kMethods) / sizeof(kMethods[0]));
    env->DeleteLocalRef(clazz);
    if (rc != JNI_OK) {
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        LOGE("register_WindowSessionAdapter: RegisterNatives failed rc=%d", (int)rc);
    } else {
        LOGI("register_WindowSessionAdapter: OK 17 methods");
    }
    return rc;
}
