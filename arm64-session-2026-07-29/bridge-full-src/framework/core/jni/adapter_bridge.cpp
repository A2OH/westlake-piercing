/*
 * adapter_bridge.cpp
 *
 * JNI Bridge entry point.  Connects Android Java layer with OpenHarmony C++
 * IPC framework.  Per-adapter native methods now live in dedicated cpp files
 * registered via RegisterNatives — see register_* functions below.
 *
 * This file's responsibilities (post 2026-05-19 startReg refactor):
 *   - AdapterBridge singleton (LifecycleAdapter JNI cache, DataShare init)
 *   - JNI_OnLoad: skia codec registration + 7 register_* calls
 *   - Direction-2 classloader cache (adapter_bridge_set_class_loader/load_class)
 *   - AppSpawnXInit.nativeHiLog best-effort RegisterNatives (PathClassLoader
 *     non-BCP class — FindClass may return null at JNI_OnLoad time)
 *   - libmali RTLD_GLOBAL preload constructor
 *
 * Per-class native impls and JNINativeMethod tables:
 *   - adapter/core/OHEnvironment              → oh_environment.cpp
 *   - adapter/activity/ActivityManagerAdapter → activity_manager_adapter.cpp
 *   - adapter/activity/ActivityTaskManagerAdapter
 *                                             → activity_task_manager_adapter.cpp
 *   - adapter/window/WindowManagerAdapter     → window_manager_adapter.cpp
 *   - adapter/window/WindowSessionAdapter     → window_session_adapter.cpp
 *   - adapter/window/InputEventBridge         → input_event_bridge.cpp
 *
 * Historical context (kept for grep traceability):
 *   - 2026-04-22 B.1: original Java_adapter_OHEnvironment_* /
 *     Java_adapter_bridge_* / Java_adapter_client_* exports were patched
 *     with adapter_activity_/adapter_window_/adapter_core_ forwarder shims
 *     after the Java package rename to adapter.{core,activity,window}.
 *   - 2026-05-19 startReg refactor: every Java native is now registered via
 *     RegisterNatives + JNINativeMethod tables in sibling cpp files, mirroring
 *     liboh_android_runtime.so's startReg() dispatch model.  All Java_* dlsym
 *     long-name exports retired in this file.  The "OAT cache" rationale cited
 *     by the original forwarder shims was a 2026-04 misjudgment (ART JNI is
 *     lazy resolve, OAT does not cache dlsym).
 */
#include "adapter_bridge.h"
#include "oh_callback_handler.h"
#include "oh_datashare_client.h"

#include <android/log.h>
#include <jni.h>
#include <string>
#include <unistd.h>
#include <dlfcn.h>
#include <cstdint>

// initDataShareJniCache lives in framework/contentprovider/jni/oh_datashare_client.cpp
// inside `namespace oh_adapter {}`. Must declare extern in matching namespace
// so the C++ mangled name matches the definition:
//   defined:  _ZN10oh_adapter21initDataShareJniCacheEP7_JNIEnv
//   (global extern would look for _Z21initDataShareJniCacheP7_JNIEnv → mismatch)
namespace oh_adapter { extern bool initDataShareJniCache(JNIEnv* env); }

#define LOG_TAG "OH_JNI_Bridge"
// B.37 (2026-04-29 EOD+2): direct HiLogPrint bypass for child diagnostics.
// __android_log_print → liblog → hilog bridge is broken in fork child, so
// LOGI/LOGE silently drop in child.  Route to HiLogPrint directly.
extern "C" int HiLogPrint(int type, int level, unsigned int domain,
                          const char* tag, const char* fmt, ...)
    __attribute__((__format__(printf, 5, 6)));
#define LOGI(fmt, ...) HiLogPrint(3, 4, 0xD000F00u, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) HiLogPrint(3, 6, 0xD000F00u, LOG_TAG, fmt, ##__VA_ARGS__)

// 2026-05-08 G2.14x: preload chipset Mali GPU driver into RTLD_GLOBAL scope so
// helloworld matches RS process loading: RS DT_NEEDED libmali.so.0 (eager link,
// global by default); App side OH wrapper EglWrapperLoader::Load uses RTLD_LOCAL
// for libEGL_impl.so (= libmali symlink), which keeps mali symbols private.
// This mismatch was identified as a candidate root cause for the EGLDisplay
// invalid issue seen in helloworld but not in RS. Pre-loading libmali.so.0 into
// the global scope before any hwui PLT resolution puts mali's eglXxx symbols
// into the global lookup chain, matching RS behavior.
__attribute__((constructor))
static void preload_libmali_global() {
    void* h = dlopen("libmali.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (h == nullptr) {
        LOGE("preload libmali.so.0 failed: %{public}s", dlerror());
    } else {
        LOGI("preload libmali.so.0 -> RTLD_GLOBAL OK (handle=%p)", h);
    }
}

using namespace oh_adapter;

// ==================== AdapterBridge Implementation ====================

AdapterBridge& AdapterBridge::getInstance() {
    static AdapterBridge instance;
    return instance;
}

bool AdapterBridge::initialize(JNIEnv* env) {
    LOGI("AdapterBridge::initialize()");

    // Cache LifecycleAdapter JNI method IDs.  The class moved from
    // adapter.* to adapter.activity.*; FindClass on the legacy path returns
    // null (which propagated as a bare NoClassDefFoundError to the caller).
    jclass clazz = env->FindClass("adapter/activity/LifecycleAdapter");
    if (clazz == nullptr) {
        // Legacy path fallback in case some build still ships the old package
        if (env->ExceptionCheck()) env->ExceptionClear();
        clazz = env->FindClass("adapter/LifecycleAdapter");
    }
    if (clazz == nullptr) {
        LOGE("Failed to find adapter/activity/LifecycleAdapter class");
        if (env->ExceptionCheck()) env->ExceptionClear();
        return false;
    }
    lifecycle_adapter_class_ = (jclass)env->NewGlobalRef(clazz);
    on_oh_lifecycle_callback_ = env->GetMethodID(clazz, "onOHLifecycleCallback", "(II)V");
    if (on_oh_lifecycle_callback_ == nullptr) {
        LOGE("Failed to find onOHLifecycleCallback method");
        return false;
    }

    // Initialize DataShare JNI cache (MatrixCursor class/method IDs)
    if (!initDataShareJniCache(env)) {
        LOGE("Failed to initialize DataShare JNI cache");
        // Non-fatal: ContentProvider bridge won't work, but other bridges are fine
    }

    return true;
}

void AdapterBridge::shutdown() {
    LOGI("AdapterBridge::shutdown()");
    JNIEnv* env = getEnv();
    if (env && lifecycle_adapter_ref_) {
        env->DeleteGlobalRef(lifecycle_adapter_ref_);
        lifecycle_adapter_ref_ = nullptr;
    }
    if (env && lifecycle_adapter_class_) {
        env->DeleteGlobalRef(lifecycle_adapter_class_);
        lifecycle_adapter_class_ = nullptr;
    }
}

void AdapterBridge::setLifecycleAdapterRef(JNIEnv* env, jobject obj) {
    if (lifecycle_adapter_ref_) {
        env->DeleteGlobalRef(lifecycle_adapter_ref_);
    }
    lifecycle_adapter_ref_ = env->NewGlobalRef(obj);
}

JNIEnv* AdapterBridge::getEnv() {
    JNIEnv* env = nullptr;
    if (jvm_) {
        int status = jvm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
        if (status == JNI_EDETACHED) {
            jvm_->AttachCurrentThread(&env, nullptr);
        }
    }
    return env;
}

void AdapterBridge::callbackLifecycleChange(int abilityToken, int ohState) {
    JNIEnv* env = getEnv();
    if (!env || !lifecycle_adapter_ref_) {
        LOGE("Cannot callback: JNI env or LifecycleAdapter ref is null");
        return;
    }

    // Call LifecycleAdapter directly using the stored singleton reference
    env->CallVoidMethod(lifecycle_adapter_ref_, on_oh_lifecycle_callback_,
                        abilityToken, ohState);
}

// Forward decl outside extern "C" so the C++ mangled name matches the
// definition in skia_codec_register.cpp.
namespace adapter { void RegisterAllSkiaCodecs(); }

// ==================== Hilog logging helpers for AppSpawnX side ====================

#define ZYG_LOG_INFO(fmt, ...)  HiLogPrint(3, 4, 0xD000F00u, "AppSpawnX", fmt, ##__VA_ARGS__)
#define ZYG_LOG_ERR(fmt, ...)   HiLogPrint(3, 6, 0xD000F00u, "AppSpawnX", fmt, ##__VA_ARGS__)

#define B32_LOG_TYPE_CORE  3       // OH LOG_CORE
#define B32_LOG_INFO       4       // OH LOG_INFO
#define B32_LOG_DOMAIN     0xD000F00u  // matches APPSPAWNX_LOG_DOMAIN in spawn_msg.h

// ==================== Direction-2 classloader cache ====================
//
// 2026-04-30: cache PathClassLoader for cross-thread access.  Set time: at
// JNI_OnLoad time, FindClass(AppSpawnXInit) often returns null (system CL
// cannot find PathClassLoader classes), so appspawnx_runtime.cpp injects the
// classloader at B.35.A time via dlsym → adapter_bridge_set_class_loader.
// OH IPC native threads (attached, system CL only) use adapter_bridge_load_class
// to load classes that moved into oh-adapter-runtime.jar (e.g.,
// AppSchedulerBridge).

static JavaVM* g_adapter_jvm = nullptr;
static jobject g_adapter_class_loader = nullptr;   // global ref to PathClassLoader
static jmethodID g_load_class_method = nullptr;    // ClassLoader.loadClass(String)

extern "C" void adapter_bridge_set_class_loader(JavaVM* vm, jobject classLoader,
                                                jmethodID loadClassMethod) {
    g_adapter_jvm = vm;
    if (!classLoader) {
        ZYG_LOG_ERR("[Direction2] adapter_bridge_set_class_loader called with null cl");
        return;
    }
    JNIEnv* env = nullptr;
    if (vm && vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK
        && env != nullptr) {
        if (g_adapter_class_loader) {
            env->DeleteGlobalRef(g_adapter_class_loader);
        }
        g_adapter_class_loader = env->NewGlobalRef(classLoader);
        g_load_class_method = loadClassMethod;
        ZYG_LOG_INFO("[Direction2] adapter_bridge_set_class_loader cached cl=%{public}p loadClass=%{public}p",
            g_adapter_class_loader, g_load_class_method);
    } else {
        ZYG_LOG_ERR("[Direction2] adapter_bridge_set_class_loader: GetEnv failed");
    }
}

extern "C" jclass adapter_bridge_load_class(JNIEnv* env, const char* binaryName) {
    if (!env || !binaryName) return nullptr;
    if (g_adapter_class_loader && g_load_class_method) {
        jstring jName = env->NewStringUTF(binaryName);
        jobject cls = env->CallObjectMethod(g_adapter_class_loader,
                                            g_load_class_method, jName);
        env->DeleteLocalRef(jName);
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            return nullptr;
        }
        if (cls) return reinterpret_cast<jclass>(cls);
    }
    // Fallback: FindClass with '/' separator
    std::string jniName(binaryName);
    for (char& c : jniName) if (c == '.') c = '/';
    jclass cls = env->FindClass(jniName.c_str());
    if (env->ExceptionCheck()) env->ExceptionClear();
    return cls;
}

// ==================== AppSpawnXInit.nativeHiLog (PathClassLoader, best-effort) ====================
//
// 2026-04-29 B.32: direct HiLogPrint bridge for Java side.  Android Log →
// liblog.so → hilog bridge constructor path is broken in spawned child
// (B.31 verified: zero D002000 entries in child hilog).  This native bypasses
// all of that and calls HiLogPrint directly.  Lets us see Java init
// checkpoints without depending on the liblog.so → hilog bridge state
// surviving fork.  Filter via:
//     hdc shell hilog | grep "AppSpawnXJava"
//
// 2026-04-29 B.34.1: registered via RegisterNatives because AppSpawnXInit
// lives in oh-adapter-runtime.jar (PathClassLoader, non-BCP).  ART's
// auto-dlsym JNI lookup is classloader-scoped: a native method on a
// PathClassLoader class is searched in libraries loaded by the same
// PathClassLoader (or its ancestors via parent delegation).  bridge.so,
// bound to bootstrap, is NOT in PathClassLoader's library set, so any
// Java_com_android_internal_os_AppSpawnXInit_nativeHiLog symbol exported
// here would still auto-dlsym fail.  RegisterNatives bypasses scoping.
//
// Best-effort: if FindClass returns null at JNI_OnLoad (PathClassLoader not
// yet created), registration silently skips and appLog falls back to
// System.err in that window.  appspawnx_runtime.cpp's B.35.A path (PathClass-
// Loader-aware retry) re-registers via dlsym of the symbol below, so the
// impl is exposed with extern "C" linkage as `adapter_appspawnx_native_hilog`
// (same naming style as adapter_bridge_set_class_loader / load_class) — see
// framework/appspawn-x/src/appspawnx_runtime.cpp B.35.A path:
//     dlsym(bridge, "adapter_appspawnx_native_hilog")

extern "C" JNIEXPORT void JNICALL adapter_appspawnx_native_hilog(
        JNIEnv* env, jclass /*clazz*/, jstring jtag, jstring jmsg) {
    const char* tag = jtag ? env->GetStringUTFChars(jtag, nullptr) : "Java";
    const char* msg = jmsg ? env->GetStringUTFChars(jmsg, nullptr) : "(null)";
    HiLogPrint(B32_LOG_TYPE_CORE, B32_LOG_INFO, B32_LOG_DOMAIN,
               tag ? tag : "Java", "%{public}s", msg ? msg : "(null)");
    if (jtag && tag) env->ReleaseStringUTFChars(jtag, tag);
    if (jmsg && msg) env->ReleaseStringUTFChars(jmsg, msg);
}

namespace {

int register_AppSpawnXInit(JNIEnv* env) {
    jclass cls = env->FindClass("com/android/internal/os/AppSpawnXInit");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (cls == nullptr) {
        ZYG_LOG_ERR("[B34.1] FindClass(AppSpawnXInit) returned null at JNI_OnLoad "
                    "— appLog will fall back to stderr until later resolution");
        return JNI_ERR;
    }
    JNINativeMethod m = {
        const_cast<char*>("nativeHiLog"),
        const_cast<char*>("(Ljava/lang/String;Ljava/lang/String;)V"),
        reinterpret_cast<void*>(adapter_appspawnx_native_hilog)
    };
    jint rc = env->RegisterNatives(cls, &m, 1);
    env->DeleteLocalRef(cls);
    if (rc != JNI_OK) {
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        ZYG_LOG_ERR("[B34.1] RegisterNatives(AppSpawnXInit.nativeHiLog) failed rc=%{public}d", rc);
    } else {
        ZYG_LOG_INFO("[B34.1] RegisterNatives(AppSpawnXInit.nativeHiLog) OK");
    }
    return rc;
}

}  // namespace

// Forward decl for the 6 BCP-class register functions defined in sibling cpp files
extern int register_OHEnvironment(JNIEnv*);
extern int register_ActivityManagerAdapter(JNIEnv*);
extern int register_ActivityTaskManagerAdapter(JNIEnv*);
extern int register_WindowManagerAdapter(JNIEnv*);
extern int register_WindowSessionAdapter(JNIEnv*);
extern int register_InputEventBridge(JNIEnv*);
extern int register_InputMethodBridge(JNIEnv*);

// ---- BLASTBufferQueue ABI fixup (app-owned SurfaceView: Unity/NativeActivity) ----
// The deployed Android-13 framework.jar declares nativeUpdate(JJJJI)V + transaction/
// sync natives the (older-AOSP) runtime never bound, so SurfaceView crashes on
// UnsatisfiedLinkError. Bind a real nativeUpdate (mirrors the runtime's BBQ_nativeUpdate,
// resolving sessionId -> OHNativeWindow via oh_wm_get_*) + no-op the transaction methods.
// Struct layout copied from android_graphics_compat_shim.cpp so the runtime's
// BBQ_nativeGetSurface reads ohNativeWindow at the right offset.
extern "C" void* oh_wm_get_native_window(int32_t sessionId);
extern "C" int32_t oh_wm_get_last_session(void);
namespace {
struct BbqFix { int32_t magic; char name[64]; int32_t width; int32_t height; int32_t format; void* ohNativeWindow; int32_t sessionId; };
constexpr int32_t kBbqFixMagic = 0x4F484251;  // 'OHBQ'
void BbqUpdateFix(JNIEnv*, jclass, jlong p, jlong, jlong w, jlong h, jint fmt) {
    BbqFix* b = reinterpret_cast<BbqFix*>(static_cast<uintptr_t>(p));
    if (!b || b->magic != kBbqFixMagic) return;
    b->width = static_cast<int32_t>(w); b->height = static_cast<int32_t>(h); b->format = fmt;
    int32_t sid = oh_wm_get_last_session();
    b->sessionId = sid;
    b->ohNativeWindow = sid ? oh_wm_get_native_window(sid) : nullptr;
}
void     BbqVJL  (JNIEnv*, jclass, jlong, jobject) {}
void     BbqVJJ  (JNIEnv*, jclass, jlong, jlong) {}
void     BbqVJ   (JNIEnv*, jclass, jlong) {}
jobject  BbqOJJ  (JNIEnv*, jclass, jlong, jlong) { return nullptr; }
jboolean BbqBJJ  (JNIEnv*, jclass, jlong, jlong) { return JNI_FALSE; }
jboolean BbqBJLZ (JNIEnv*, jclass, jlong, jobject, jboolean) { return JNI_FALSE; }
}  // namespace
static int register_BLASTBufferQueue_fixup(JNIEnv* env) {
    jclass c = env->FindClass("android/graphics/BLASTBufferQueue");
    if (!c) { if (env->ExceptionCheck()) env->ExceptionClear();
        ZYG_LOG_ERR("[BBQ-FIX] FindClass(BLASTBufferQueue) null"); return -1; }
    const JNINativeMethod ms[] = {
        {const_cast<char*>("nativeUpdate"), const_cast<char*>("(JJJJI)V"), reinterpret_cast<void*>(BbqUpdateFix)},
        {const_cast<char*>("nativeSetTransactionHangCallback"), const_cast<char*>("(JLandroid/graphics/BLASTBufferQueue$TransactionHangCallback;)V"), reinterpret_cast<void*>(BbqVJL)},
        {const_cast<char*>("nativeApplyPendingTransactions"), const_cast<char*>("(JJ)V"), reinterpret_cast<void*>(BbqVJJ)},
        {const_cast<char*>("nativeClearSyncTransaction"), const_cast<char*>("(J)V"), reinterpret_cast<void*>(BbqVJ)},
        {const_cast<char*>("nativeStopContinuousSyncTransaction"), const_cast<char*>("(J)V"), reinterpret_cast<void*>(BbqVJ)},
        {const_cast<char*>("nativeGatherPendingTransactions"), const_cast<char*>("(JJ)Landroid/view/SurfaceControl$Transaction;"), reinterpret_cast<void*>(BbqOJJ)},
        {const_cast<char*>("nativeIsSameSurfaceControl"), const_cast<char*>("(JJ)Z"), reinterpret_cast<void*>(BbqBJJ)},
        {const_cast<char*>("nativeSyncNextTransaction"), const_cast<char*>("(JLjava/util/function/Consumer;Z)Z"), reinterpret_cast<void*>(BbqBJLZ)},
    };
    int ok = 0;
    for (size_t i = 0; i < sizeof(ms) / sizeof(ms[0]); i++) {
        jint rc = env->RegisterNatives(c, &ms[i], 1);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (rc == 0) ok++;
    }
    ZYG_LOG_INFO("[BBQ-FIX] registered %{public}d/8 BLASTBufferQueue natives", ok);
    return 0;
}

// ---- Audio native stubs (Unity/AudioTrack init hits unimplemented AudioSystem/AudioTrack
// natives → UnsatisfiedLinkError thrown through libunity's native audio init, which can leave a
// libunity pthread mutex locked → UnityPlayer ctor deadlocks). Provide sane-default stubs so
// audio init doesn't throw. Static methods: (JNIEnv*, jclass[, args]). ----
namespace {
jint AS_i_void(JNIEnv*, jclass) { return 0; }
jint AS_maxChan(JNIEnv*, jclass) { return 8; }       // native_getMaxChannelCount (FCC_8)
jint AS_maxRate(JNIEnv*, jclass) { return 192000; }  // native_getMaxSampleRate
jint AS_minRate(JNIEnv*, jclass) { return 4000; }    // native_getMinSampleRate
jint AS_primRate(JNIEnv*, jclass) { return 48000; }  // getPrimaryOutputSamplingRate
jint AS_primFrames(JNIEnv*, jclass) { return 1024; } // getPrimaryOutputFrameCount
jint AS_outLatency(JNIEnv*, jclass, jint) { return 0; }              // getOutputLatency(I)
jint AT_minBuf(JNIEnv*, jclass, jint, jint, jint) { return 8192; }   // native_get_min_buff_size(III)
jint AT_outRate(JNIEnv*, jclass, jint) { return 48000; }            // native_get_output_sample_rate(I)
jint AS_newSessionId(JNIEnv*, jclass) { static int s = 1000; return ++s; }  // AudioSystem.newAudioSessionId() -> unique >0
static void reg(JNIEnv* env, const char* cls, const char* name, const char* sig, void* fn) {
    jclass c = env->FindClass(cls);
    if (!c) { if (env->ExceptionCheck()) env->ExceptionClear(); return; }
    JNINativeMethod m = { const_cast<char*>(name), const_cast<char*>(sig), fn };
    env->RegisterNatives(c, &m, 1);
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(c);
}
}  // namespace
// [FIX-AUDIO 2026-06-30] real AudioTrack -> OH_AudioRenderer shim (oh_audiotrack_shim.cpp)
extern "C" int register_AudioTrack_shim(JNIEnv* env);
// [FIX-AUDIO 2026-07-02] MediaCodec (MP3 decode) -> OH_AudioCodec shim (oh_mediacodec_shim.cpp)
extern "C" int register_MediaCodec_shim(JNIEnv* env);
static int register_Audio_fixup(JNIEnv* env) {
    reg(env, "android/media/AudioSystem", "native_getMaxChannelCount", "()I", (void*)AS_maxChan);
    reg(env, "android/media/AudioSystem", "native_getMaxSampleRate", "()I", (void*)AS_maxRate);
    reg(env, "android/media/AudioSystem", "native_getMinSampleRate", "()I", (void*)AS_minRate);
    reg(env, "android/media/AudioSystem", "getPrimaryOutputSamplingRate", "()I", (void*)AS_primRate);
    reg(env, "android/media/AudioSystem", "getPrimaryOutputFrameCount", "()I", (void*)AS_primFrames);
    reg(env, "android/media/AudioSystem", "getOutputLatency", "(I)I", (void*)AS_outLatency);
    reg(env, "android/media/AudioSystem", "newAudioSessionId", "()I", (void*)AS_newSessionId);
    reg(env, "android/media/AudioTrack", "native_get_min_buff_size", "(III)I", (void*)AT_minBuf);
    reg(env, "android/media/AudioTrack", "native_get_output_sample_rate", "(I)I", (void*)AT_outRate);
    ZYG_LOG_INFO("[AUDIO-FIX] AudioSystem/AudioTrack native stubs registered");
    register_AudioTrack_shim(env);   // overrides min_buff_size stub + adds setup/start/write/...
    return 0;
}

// ==================== JNI_OnLoad ====================

// [PROXY 2026-07-01] Opt-in: if /data/local/tmp/oh_proxy exists (contents
// "host:port"), point this app's JVM HTTP(S) proxy at it so ExoPlayer's
// DefaultHttpDataSource (HttpURLConnection) tunnels to the host CONNECT proxy
// (hdc rport). The proxy resolves hostnames host-side, so no device DNS is
// needed. Gated by the file so normal boots (no proxy running) are unaffected.
static void oh_set_proxy_props(JNIEnv* env) {
    FILE* f = fopen("/data/local/tmp/oh_proxy", "r");
    if (!f) return;
    char host[64] = {0}, port[16] = {0};
    int n = fscanf(f, "%63[^:]:%15s", host, port);
    fclose(f);
    if (n != 2) return;
    jclass sys = env->FindClass("java/lang/System");
    if (!sys) { if (env->ExceptionCheck()) env->ExceptionClear(); return; }
    jmethodID setProp = env->GetStaticMethodID(sys, "setProperty",
        "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
    if (!setProp) { if (env->ExceptionCheck()) env->ExceptionClear(); return; }
    const char* hkeys[] = {"https.proxyHost", "http.proxyHost"};
    const char* pkeys[] = {"https.proxyPort", "http.proxyPort"};
    jstring jhost = env->NewStringUTF(host);
    jstring jport = env->NewStringUTF(port);
    for (int i = 0; i < 2; i++) {
        jstring k = env->NewStringUTF(hkeys[i]);
        jobject r = env->CallStaticObjectMethod(sys, setProp, k, jhost);
        if (r) env->DeleteLocalRef(r);
        env->DeleteLocalRef(k);
        jstring pk = env->NewStringUTF(pkeys[i]);
        jobject r2 = env->CallStaticObjectMethod(sys, setProp, pk, jport);
        if (r2) env->DeleteLocalRef(r2);
        env->DeleteLocalRef(pk);
    }
    env->DeleteLocalRef(jhost); env->DeleteLocalRef(jport);
    if (env->ExceptionCheck()) env->ExceptionClear();
    ZYG_LOG_INFO("[PROXY] JVM http(s) proxy -> %s:%s", host, port);
}

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    ZYG_LOG_INFO("[B34.1] JNI_OnLoad: oh_adapter_bridge entered");
    AdapterBridge::getInstance().setJavaVM(vm);

    // Bridge to OH libskia_canvaskit's SkCodecs::Register — without this the
    // 4-arg SkCodec::MakeFromStream wrapper sees an empty decoder list and
    // returns kUnimplemented for every PNG/JPEG/WEBP load (HelloWorld
    // setContentView -> ImageDecoder failure root cause).
    adapter::RegisterAllSkiaCodecs();

    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }

    // Best-effort: AppSpawnXInit is a PathClassLoader class (non-BCP). If it's
    // not loaded yet, register_AppSpawnXInit logs and returns; appLog falls
    // back to System.err in that window.  Logged but not fatal.
    register_AppSpawnXInit(env);

    // BCP classes: all of these live in oh-adapter-framework.jar which is
    // loaded into the boot classpath before this library; FindClass must
    // succeed.  Any rc != JNI_OK is a programmer error (signature mismatch
    // most likely) and logged as ERROR inside each register_* function.
    register_OHEnvironment(env);
    register_ActivityManagerAdapter(env);
    register_ActivityTaskManagerAdapter(env);
    register_WindowManagerAdapter(env);
    register_WindowSessionAdapter(env);
    register_InputEventBridge(env);
    register_InputMethodBridge(env);
    register_BLASTBufferQueue_fixup(env);
    register_Audio_fixup(env);
    register_MediaCodec_shim(env);

    oh_set_proxy_props(env);

    return JNI_VERSION_1_6;
}
