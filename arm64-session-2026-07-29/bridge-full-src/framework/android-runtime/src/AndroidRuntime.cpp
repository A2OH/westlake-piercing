// ============================================================================
// AndroidRuntime.cpp
//
// OH-Adapter's replacement of frameworks/base/core/jni/AndroidRuntime.cpp's
// startReg() dispatch. Kept intentionally small: each entry is a register_*
// function that has already been implemented in this project.
//
// Adding a new JNI module:
//   1. Write src/android_<area>_<Class>.cpp with a `register_android_*` fn.
//   2. Declare it in include/AndroidRuntime.h.
//   3. Add a line to `kRegJNI[]` below.
//   4. Rebuild liboh_android_runtime.so.
// ============================================================================

#include <atomic>
#include <map>
#include <set>
#include <unistd.h>
#include "AndroidRuntime.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <netdb.h>
#include <poll.h>
#include <string>
#include <vector>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <hilog/log.h>
#include <nativehelper/JNIHelp.h>  // jniRegisterNativeMethods (L2)

namespace android {

// Cached JavaVM so ApkAssets.cpp's AndroidRuntime::getJNIEnv() works.
static JavaVM* g_vm = nullptr;

void AndroidRuntime::setJavaVM(JavaVM* vm) { g_vm = vm; }

JNIEnv* AndroidRuntime::getJNIEnv() {
    if (g_vm == nullptr) return nullptr;
    JNIEnv* env = nullptr;
    jint rc = g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (rc == JNI_OK && env != nullptr) return env;
    // Attach if needed (not used in HelloWorld bring-up; defensive)
    if (rc == JNI_EDETACHED) {
        JavaVMAttachArgs args = { JNI_VERSION_1_6, "AndroidRuntime", nullptr };
        if (g_vm->AttachCurrentThread(&env, &args) == JNI_OK) return env;
    }
    return nullptr;
}

// 2026-05-18 (L2): AOSP-compatible static helper used by frameworks/base/
// core/jni/core_jni_helpers.h when AOSP JNI source (e.g., MotionEvent.cpp)
// registers its native methods.  Trivial passthrough to libnativehelper —
// mirrors frameworks/base/core/jni/AndroidRuntime.cpp.
/*static*/ int AndroidRuntime::registerNativeMethods(JNIEnv* env,
        const char* className, const JNINativeMethod* gMethods, int numMethods) {
    return jniRegisterNativeMethods(env, className, gMethods, numMethods);
}

// G2.4 (2026-04-30): graphics JNI compat shim — last-wins overrides for
// methods whose libhwui impl aborts (fid==null) or whose AOSP libandroid_runtime
// impl is missing (we don't cross-compile that .so).  Spec: doc/graphics_jni_inventory.html §4.1.
extern int register_android_graphics_compat_shim(JNIEnv* env);

// G2.14k (2026-05-01) + 2026-05-02 audit: NAR applyFreeFunction guard.
// User flagged for removal as "defensive hack" but empirical removal breaks
// initChild Java entry on main thread (SIGSEGV pc=0).  TODO P1: identify the
// implicit class-init dependency this provides, then replace.  See header
// comment in libcore_util_NativeAllocationRegistry_guard.cpp.
extern int register_libcore_util_NativeAllocationRegistry_guard(JNIEnv* env);

// G2.14n (2026-05-01): PropertyValuesHolder JNI cache for animation framework.
// View.setContentView path triggers Animator/PropertyValuesHolder static init
// which calls nGetFloatMethod / nGetIntMethod via JNI.
extern int register_android_animation_PropertyValuesHolder(JNIEnv* env);

// B.15: register_android_graphics_Typeface — minimal in-runtime impl.
// Earlier dlopen("libhwui.so") attempt hung (libhwui has heavy GPU init not
// safe in init service ctx).  Compiled directly into liboh_android_runtime.so.
extern int register_android_graphics_Typeface(JNIEnv* env);

// G2.14u (2026-05-07): AOSP-ported android_os_Parcel.cpp.  Provides ~30
// Parcel native methods needed by SurfaceControl / Bundle / Intent / Binder
// IPC paths that ViewRootImpl.relayoutWindow follows on first frame.
// Source: frameworks/base/core/jni/android_os_Parcel.cpp (918 lines), pulled
// in via SRCS_AOSP (compile_oh_android_runtime.sh).
extern int register_android_os_Parcel(JNIEnv* env);

// G2.14w (2026-05-08): adapter-rewritten android_view_KeyCharacterMap.cpp.
// Provides 13 native methods for android.view.KeyCharacterMap.  KCM.load
// fallback path (KCM.obtainEmptyMap → nativeObtainEmptyKeyCharacterMap)
// previously hit UnsatisfiedLinkError because no register_X covered it,
// blocking PhoneWindow.preparePanel after InputManagerAdapter VIRTUAL_KEYBOARD
// double mapping landed.  Source pattern: frameworks/base/core/jni/
// android_view_KeyCharacterMap.cpp (AOSP 14, 284 lines), adapter-adapted to
// strip <binder/Parcel.h> + <input/*.h> deps; mMap permanently nullptr in
// Phase 1 (Phase 3 will populate from OH MMI keymap data).
extern int register_android_view_KeyCharacterMap(JNIEnv* env);

// 2026-05-18: adapter-rewritten android_view_MotionEvent_aosp.cpp.  Provides
// all 51 native methods for android.view.MotionEvent.  Final dependency for
// Input_Adapter_design §3.3.5 Phase 2 — without it, MotionEvent.obtain
// throws UnsatisfiedLinkError on nativeInitialize, blocking helloworld
// touch dispatch (CHANGE COLOR button click never reaches APK onClick).
// Source pattern: AOSP 14 frameworks/base/core/jni/android_view_MotionEvent.cpp
// (917 lines), but reimplemented with a self-contained OhMotionEvent struct
// to avoid pulling libinput.so / libui.so / ui::Transform / HmacKeyManager /
// IInputConstants AIDL chain into liboh_android_runtime.
extern int register_android_view_MotionEvent(JNIEnv* env);

// B.20.r7: register_android_os_GraphicsEnvironment — 11 natives, all no-op
// safe defaults. setupGraphicsSupport line 6680 calls isDebuggable() then
// (within setup) calls layer/driver/ANGLE configuration natives.  AOSP
// registers these in libandroid_servers.so (system_server-side); on OH the
// child process needs them resolved in liboh_android_runtime.so.
extern int register_android_os_GraphicsEnvironment(JNIEnv* env);

// Phase 2 r27 (2026-04-28): all graphics register_X functions
// (Paint/Canvas/RenderNode/HardwareRenderer/Matrix/Path/...) come from real
// cross-compiled libhwui.so via dlopen+dlsym in startReg below.  Stub
// register_android_graphics_Canvas / HardwareRenderer / RenderNode / Paint
// are retired (no longer compiled into liboh_android_runtime).

struct RegJNIRec {
    const char* name;
    int (*proc)(JNIEnv*);
};

// Dispatch table. Grows as we port more AOSP register_* functions.
extern int register_android_database_SQLiteConnection(JNIEnv* env);
extern int register_android_database_CursorWindow(JNIEnv* env);
static const RegJNIRec kRegJNI[] = {
    { "register_android_util_Log",            register_android_util_Log },
    { "register_android_util_EventLog",       register_android_util_EventLog },
    { "register_android_app_Activity",        register_android_app_Activity },
    { "register_android_os_SystemProperties", register_android_os_SystemProperties },
    { "register_android_os_Trace",            register_android_os_Trace },
    { "register_android_os_Process",          register_android_os_Process },
    { "register_android_os_SystemClock",      register_android_os_SystemClock },
    { "register_android_os_Binder",           register_android_os_Binder },
    // 2026-05-07 G2.14u: AOSP-ported android_os_Parcel.cpp providing
    // ~30 Parcel native methods (nativeCreate / nativeWriteToParcel /
    // nativeMarshall / etc).  HelloWorld TextView path → ViewRootImpl
    // .performTraversals → relayoutWindow → adapter WindowSessionAdapter
    // .relayout → SurfaceControl.<init> → Parcel.obtain → Parcel.<init>
    // → Parcel.nativeCreate UnsatisfiedLinkError before this entry.
    { "register_android_os_Parcel",           register_android_os_Parcel },
    { "register_android_view_SurfaceControl", register_android_view_SurfaceControl },
    { "register_android_view_SurfaceSession", register_android_view_SurfaceSession },
    { "register_android_view_DisplayEventReceiver", register_android_view_DisplayEventReceiver },
    { "register_android_view_InputChannel",   register_android_view_InputChannel },
    { "register_android_view_KeyCharacterMap", register_android_view_KeyCharacterMap },
    // 2026-05-18: MotionEvent JNI must register BEFORE InputEventReceiver
    // so the worker-thread MotionEvent.obtain JNI lookup (cached in
    // ensureJavaRefs at first dispatch) finds nativeInitialize already
    // bound.  Both register_X are last-wins-safe regardless of order.
    //
    // 2026-05-18 (Plan A): restored.  Now backed by adapter-private
    // android_view_MotionEvent_aosp.cpp (replaces AOSP direct-reference
    // MotionEvent.cpp; class layout under adapter control, no ABI drift).
    { "register_android_view_MotionEvent",    register_android_view_MotionEvent },
    { "register_android_view_InputEventReceiver", register_android_view_InputEventReceiver },
    { "register_android_content_AssetManager", register_android_content_AssetManager },
    { "register_android_os_MessageQueue",     register_android_os_MessageQueue },
    { "register_android_content_res_ApkAssets", register_android_content_res_ApkAssets },
    { "register_android_content_StringBlock", register_android_content_StringBlock },
    { "register_android_content_XmlBlock",    register_android_content_XmlBlock },
    { "register_com_android_internal_os_ClassLoaderFactory", register_com_android_internal_os_ClassLoaderFactory },
    { "register_com_android_internal_util_VirtualRefBasePtr", register_com_android_internal_util_VirtualRefBasePtr },
    { "register_android_graphics_Typeface",   register_android_graphics_Typeface },
    { "register_android_os_GraphicsEnvironment", register_android_os_GraphicsEnvironment },
    // 2026-05-02 audit: kept; see libcore_util_NativeAllocationRegistry_guard.cpp
    // header.  TODO P1: identify implicit dep then replace + remove.
    { "register_libcore_util_NativeAllocationRegistry_guard",
      register_libcore_util_NativeAllocationRegistry_guard },
    { "register_android_animation_PropertyValuesHolder",
      register_android_animation_PropertyValuesHolder },
    // Phase 2 (r27): graphics natives provided by real libhwui.so via dlopen
    // block in startReg below — see kHwuiRegFns table.
    // WESTLAKE 2026-07-22: SQLite JNI (Room/AppIntro DB). Appended LAST so a failure
    // cannot affect earlier modules. sqlite3 amalgamation is bundled (board libsqlite.z.so
    // exports only 3 sqlite3_* symbols - none usable).
    { "register_android_database_SQLiteConnection", register_android_database_SQLiteConnection },
    { "register_android_database_CursorWindow", register_android_database_CursorWindow },
};

static constexpr size_t kRegJNICount = sizeof(kRegJNI) / sizeof(kRegJNI[0]);

// 2026-05-01 G2.14n: hook RegisterNatives to dump every (class, method, sig, fnPtr)
// registration. Goal: identify which Java method's ArtMethod entry_point is being
// set to a corrupted address that lands in lib .bss when later invoked via
// art_quick_invoke_stub_internal blx r12 → SIGILL.
namespace {
using OrigRegisterNatives = jint (*)(JNIEnv*, jclass, const JNINativeMethod*, jint);
static OrigRegisterNatives g_orig_register_natives = nullptr;

static jint hooked_RegisterNatives(JNIEnv* env, jclass clazz,
                                    const JNINativeMethod* methods, jint nMethods) {
    // Resolve class name for logging. Use stack buffer to avoid heap allocation
    // here (we re-enter JNI carefully — no recursion into RegisterNatives).
    char className[256] = "?";
    if (clazz) {
        // Use original FindClass/GetMethodID/CallObjectMethod/GetStringUTFChars
        // through env->functions, but those are unaffected by our hook (only
        // RegisterNatives is patched).
        jclass cls = env->GetObjectClass(clazz);  // returns Class.class
        if (cls) {
            jmethodID mGetName = env->GetMethodID(cls, "getName", "()Ljava/lang/String;");
            if (mGetName) {
                jstring nameStr = (jstring)env->CallObjectMethod(clazz, mGetName);
                if (nameStr) {
                    const char* utf = env->GetStringUTFChars(nameStr, nullptr);
                    if (utf) {
                        snprintf(className, sizeof(className), "%s", utf);
                        env->ReleaseStringUTFChars(nameStr, utf);
                    }
                    env->DeleteLocalRef(nameStr);
                }
            }
            env->DeleteLocalRef(cls);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    for (jint i = 0; i < nMethods; i++) {
        // 2026-05-02 G2.14n+: enhanced flagging for suspicious fnPtr.
        // SIGILL crash signature: PC ends in f98 (ARM) or f99 (Thumb bit set)
        // and falls inside libskia_canvaskit.z.so .text. Flag both criteria.
        uintptr_t fp = (uintptr_t)methods[i].fnPtr;
        uintptr_t pageOff = fp & 0xFFFu;
        bool offHit = (pageOff == 0xf98u) || (pageOff == 0xf99u);
        bool libHit = false;
        const char* libName = "?";
        Dl_info info;
        if (methods[i].fnPtr && dladdr(methods[i].fnPtr, &info) && info.dli_fname) {
            libName = info.dli_fname;
            if (strstr(info.dli_fname, "libskia_canvaskit") != nullptr) libHit = true;
        }
        const char* tag = (offHit || libHit) ? "OH_RegHook_BAD" : "OH_RegHook";
        HiLogPrint(LOG_CORE, LOG_INFO, 0xD000F00u, tag,
            "%{public}s%{public}s::%{public}s%{public}s -> fn=%{public}p (lib=%{public}s pageOff=0x%{public}x)",
            (offHit || libHit) ? "[!!! SUSPECT] " : "",
            className,
            methods[i].name ? methods[i].name : "?",
            methods[i].signature ? methods[i].signature : "?",
            methods[i].fnPtr, libName, pageOff);
    }
    return g_orig_register_natives(env, clazz, methods, nMethods);
}

static void install_register_natives_hook(JNIEnv* env) {
    if (g_orig_register_natives) return;  // already installed
    // env->functions is `const struct JNINativeInterface*`. We allocate a copy
    // we can patch, then point env->functions at the copy.
    static struct JNINativeInterface patched;
    const struct JNINativeInterface* orig =
        *(const struct JNINativeInterface**)env;
    memcpy(&patched, orig, sizeof(patched));
    g_orig_register_natives = orig->RegisterNatives;
    patched.RegisterNatives = hooked_RegisterNatives;
    // Cast away const and overwrite the JNIEnv's functions pointer.
    *(const struct JNINativeInterface**)env = &patched;
    HiLogPrint(LOG_CORE, LOG_INFO, 0xD000F00u, "OH_RegHook",
        "RegisterNatives hook INSTALLED orig=%{public}p hook=%{public}p",
        (void*)g_orig_register_natives, (void*)hooked_RegisterNatives);
}
}  // namespace

// WESTLAKE (2026-07-22): register the bring-up JSSE shim.
// This BCP has NO concrete SSLContextSpi (Conscrypt absent), so SSLContext.getInstance("TLS")
// throws NoSuchAlgorithmException inside OkHttpClient.<init>, which Hilt builds during
// MainActivity.onCreate -> the Activity dies before setContentView and no frame is drawn.
// adapter.compat.WestlakeJsseProvider ships as classes2.dex in oh-adapter-framework.jar; we
// instantiate and install it here because there is no Java call site we can edit surgically.
static void WestlakeRegisterJsseShim(JNIEnv* env) {
    jclass provCls = env->FindClass("adapter/compat/WestlakeJsseProvider");
    if (provCls == nullptr) {
        env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-JSSE] adapter.compat.WestlakeJsseProvider NOT FOUND\n");
        fflush(stderr);
        return;
    }
    jmethodID ctor = env->GetMethodID(provCls, "<init>", "()V");
    jobject prov = (ctor != nullptr) ? env->NewObject(provCls, ctor) : nullptr;
    if (prov == nullptr) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-JSSE] could not construct provider\n");
        fflush(stderr);
        return;
    }
    jclass secCls = env->FindClass("java/security/Security");
    jmethodID add = (secCls != nullptr)
            ? env->GetStaticMethodID(secCls, "addProvider", "(Ljava/security/Provider;)I")
            : nullptr;
    if (add == nullptr) {
        env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-JSSE] Security.addProvider not found\n");
        fflush(stderr);
        return;
    }
    jint rc = env->CallStaticIntMethod(secCls, add, prov);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
    fprintf(stderr, "[WESTLAKE-JSSE] Security.addProvider(WestlakeJSSE) rc=%d\n", (int) rc);
    fflush(stderr);
}

// WESTLAKE §258: point ICU at its data as EARLY as possible.
// §257 set the data dir from createSession and the text-measure crash was unchanged; §257b then
// showed `icudt66_dat` inside libicuuc.so is only 56 bytes -- a STUB -- so ICU really must load the
// external icudt66l.dat (shipped to /data/local/tmp/asx in §257). The remaining suspicion is that our
// call came too late: ICU caches its data state on first use, and text/locale work happens long
// before any window is created. Do it here, at runtime registration, before any Java code runs.
// The child resets `environ`, so the ICU_DATA exported by run_asx.sh never reaches us (§194/§208).
static void wl_icu_early_init() {
    static bool wl_done = false;
    if (wl_done) { return; }
    wl_done = true;
    void* h = dlopen("libicuuc.so", RTLD_NOW);
    if (h == nullptr) {
        fprintf(stderr, "[WESTLAKE-ICU] early: dlopen(libicuuc.so) failed\n"); fflush(stderr);
        return;
    }
    using SetDirFn = void (*)(const char*);
    SetDirFn setDir = reinterpret_cast<SetDirFn>(dlsym(h, "u_setDataDirectory_66"));
    if (setDir == nullptr) { setDir = reinterpret_cast<SetDirFn>(dlsym(h, "u_setDataDirectory")); }
    if (setDir != nullptr) {
        setDir("/data/local/tmp/asx");
        fprintf(stderr, "[WESTLAKE-ICU] early u_setDataDirectory(/data/local/tmp/asx)\n");
    } else {
        fprintf(stderr, "[WESTLAKE-ICU] early: u_setDataDirectory not found\n");
    }
    fflush(stderr);
}

// ============================================================
// WESTLAKE §404 — repair java.lang.invoke classes that arrive INITIALISED but EMPTY.
//
// §202 established that some classes reach this child already marked initialised even though
// their <clinit> never ran in any process, so their `static final` reference fields stay null
// forever and ART never reports an initialisation failure.  libart carries a hardcoded repair
// for java.io.BufferedInputStream (class_linker.cc `[WESTLAKE-REPAIR]`); this is the same repair,
// done from the bridge so it can cover a list of classes without rebuilding libart.
//
// The class that matters here is java.lang.invoke.MethodType: `internTable` is null, and
// MethodType.makeImpl()'s very first statement is `internTable.get(...)`, so EVERY
// invokedynamic / Kotlin-lambda path throws
//   NullPointerException: Attempt to invoke InvokeType(2) method
//   'java.lang.Object java.lang.invoke.MethodType$ConcurrentWeakInternSet.get(java.lang.Object)'
//   on a null object reference
// (§403 counted ~3100 of these in one run).  noice's click listeners are all Kotlin lambdas, so
// this is what makes every delivered touch abort the child.
//
// Detection is allow-listed on purpose: a null static reference field is perfectly legal in
// general, so it cannot be used as a global test (same reasoning as the libart repair).
//
// Repair = invoke <clinit> directly.  ART's GetStaticMethodID resolves "<clinit>" like any other
// direct static method, which is exactly what libart's repair does natively.
static bool wl_class_has_null_static_ref(JNIEnv* env, jclass cls, const char* desc) {
    // Enumerate static fields reflectively: JNI has no field iteration.
    jclass clsCls = env->FindClass("java/lang/Class");
    jclass fieldCls = env->FindClass("java/lang/reflect/Field");
    jclass modCls = env->FindClass("java/lang/reflect/Modifier");
    if (!clsCls || !fieldCls || !modCls) { if (env->ExceptionCheck()) env->ExceptionClear(); return false; }
    jmethodID getDF = env->GetMethodID(clsCls, "getDeclaredFields", "()[Ljava/lang/reflect/Field;");
    jmethodID getMod = env->GetMethodID(fieldCls, "getModifiers", "()I");
    jmethodID getType = env->GetMethodID(fieldCls, "getType", "()Ljava/lang/Class;");
    jmethodID getName = env->GetMethodID(fieldCls, "getName", "()Ljava/lang/String;");
    jmethodID setAcc = env->GetMethodID(fieldCls, "setAccessible", "(Z)V");
    jmethodID fGet = env->GetMethodID(fieldCls, "get", "(Ljava/lang/Object;)Ljava/lang/Object;");
    jmethodID isPrim = env->GetMethodID(clsCls, "isPrimitive", "()Z");
    jmethodID isStatic = env->GetStaticMethodID(modCls, "isStatic", "(I)Z");
    if (!getDF || !getMod || !getType || !getName || !setAcc || !fGet || !isPrim || !isStatic) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return false;
    }
    jobjectArray fields = static_cast<jobjectArray>(env->CallObjectMethod(cls, getDF));
    if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
    if (fields == nullptr) return false;
    bool needs = false;
    const jsize n = env->GetArrayLength(fields);
    for (jsize i = 0; i < n && !needs; ++i) {
        jobject f = env->GetObjectArrayElement(fields, i);
        if (f == nullptr) continue;
        const jint mods = env->CallIntMethod(f, getMod);
        if (!env->CallStaticBooleanMethod(modCls, isStatic, mods)) { env->DeleteLocalRef(f); continue; }
        jobject ftype = env->CallObjectMethod(f, getType);
        const bool prim = ftype ? (env->CallBooleanMethod(ftype, isPrim) == JNI_TRUE) : true;
        if (ftype) env->DeleteLocalRef(ftype);
        if (prim) { env->DeleteLocalRef(f); continue; }
        env->CallVoidMethod(f, setAcc, JNI_TRUE);
        if (env->ExceptionCheck()) env->ExceptionClear();
        jobject val = env->CallObjectMethod(f, fGet, nullptr);
        if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(f); continue; }
        if (val == nullptr) {
            jstring jn = static_cast<jstring>(env->CallObjectMethod(f, getName));
            const char* nm = jn ? env->GetStringUTFChars(jn, nullptr) : nullptr;
            fprintf(stderr, "[WESTLAKE-404] %s: static ref field '%s' is NULL\n",
                    desc, nm ? nm : "?");
            if (jn && nm) env->ReleaseStringUTFChars(jn, nm);
            if (jn) env->DeleteLocalRef(jn);
            needs = true;
        } else {
            env->DeleteLocalRef(val);
        }
        env->DeleteLocalRef(f);
    }
    env->DeleteLocalRef(fields);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return needs;
}

static void wl_repair_invoke_classes(JNIEnv* env) {
    static bool wl_done = false;
    if (wl_done) { return; }
    wl_done = true;
    // MethodType first: everything else in java.lang.invoke depends on it.
    static const char* kClasses[] = {
        "java/lang/invoke/MethodType",
        "java/lang/invoke/MethodTypeForm",
        "java/lang/invoke/MethodHandleStatics",
        "java/lang/invoke/MethodHandleNatives",
        "java/lang/invoke/MethodHandles",
        "java/lang/invoke/MethodHandleImpl",
        "java/lang/invoke/LambdaMetafactory",
        "java/lang/invoke/InnerClassLambdaMetafactory",
        "java/lang/invoke/Invokers",
        "java/lang/invoke/CallSite",
    };
    for (const char* desc : kClasses) {
        if (env->PushLocalFrame(64) < 0) { continue; }
        jclass cls = env->FindClass(desc);
        if (cls == nullptr || env->ExceptionCheck()) {
            env->ExceptionClear();
            fprintf(stderr, "[WESTLAKE-404] %s: not found\n", desc);
            env->PopLocalFrame(nullptr);
            continue;
        }
        if (wl_class_has_null_static_ref(env, cls, desc)) {
            jmethodID clinit = env->GetStaticMethodID(cls, "<clinit>", "()V");
            if (env->ExceptionCheck()) { env->ExceptionClear(); clinit = nullptr; }
            if (clinit != nullptr) {
                env->CallStaticVoidMethod(cls, clinit);
                if (env->ExceptionCheck()) {
                    fprintf(stderr, "[WESTLAKE-404] %s: <clinit> THREW:\n", desc);
                    env->ExceptionDescribe();
                    env->ExceptionClear();
                }
                const bool still = wl_class_has_null_static_ref(env, cls, desc);
                fprintf(stderr, "[WESTLAKE-404] %s: direct <clinit> done, stillNull=%d\n",
                        desc, still ? 1 : 0);
            } else {
                fprintf(stderr, "[WESTLAKE-404] %s: no <clinit> method to invoke\n", desc);
            }
        } else {
            fprintf(stderr, "[WESTLAKE-404] %s: statics OK\n", desc);
        }
        env->PopLocalFrame(nullptr);
    }
    // Last resort for the one field that actually blocks input: build the intern set by hand.
    // MethodType.makeImpl() only needs `internTable` to be non-null to stop NPE-ing.
    if (env->PushLocalFrame(16) >= 0) {
        jclass mt = env->FindClass("java/lang/invoke/MethodType");
        if (env->ExceptionCheck()) env->ExceptionClear();
        jfieldID it = mt ? env->GetStaticFieldID(mt, "internTable",
            "Ljava/lang/invoke/MethodType$ConcurrentWeakInternSet;") : nullptr;
        if (env->ExceptionCheck()) { env->ExceptionClear(); it = nullptr; }
        jobject cur = it ? env->GetStaticObjectField(mt, it) : nullptr;
        fprintf(stderr, "[WESTLAKE-404] MethodType.internTable fid=%d null=%d\n",
                it ? 1 : 0, cur == nullptr ? 1 : 0);
        if (it != nullptr && cur == nullptr) {
            jclass setCls = env->FindClass("java/lang/invoke/MethodType$ConcurrentWeakInternSet");
            if (env->ExceptionCheck()) env->ExceptionClear();
            jmethodID ctor = setCls ? env->GetMethodID(setCls, "<init>", "()V") : nullptr;
            if (env->ExceptionCheck()) { env->ExceptionClear(); ctor = nullptr; }
            jobject inst = ctor ? env->NewObject(setCls, ctor) : nullptr;
            if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); inst = nullptr; }
            if (inst != nullptr) {
                env->SetStaticObjectField(mt, it, inst);
                if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
                jobject chk = env->GetStaticObjectField(mt, it);
                fprintf(stderr, "[WESTLAKE-404] internTable INSTALLED by hand, nowNull=%d\n",
                        chk == nullptr ? 1 : 0);
            } else {
                fprintf(stderr, "[WESTLAKE-404] could not construct ConcurrentWeakInternSet\n");
            }
        }
        env->PopLocalFrame(nullptr);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    fflush(stderr);
}

// WESTLAKE §413 — route the app's HTTP(S) through the host proxy.
// The board has no network of its own (only `lo`), so noice's OkHttp client cannot reach
// api.trynoice.com and the Library page shows "Oops! We couldn't fetch the sound library."
// `hdc rport tcp:8888 tcp:8888` tunnels board:8888 to a host proxy over USB; OkHttp's default
// ProxySelector honours the standard http/https proxy system properties, so setting them here —
// before any app code runs — is all that is needed.  Gated on WL_PROXY (host:port, or "1" for the
// default 127.0.0.1:8888) so a networkless run is still possible.
static void wl_install_proxy_props(JNIEnv* env) {
    const char* spec = getenv("WL_PROXY");
    if (spec == nullptr || spec[0] == '\0') { return; }
    char host[128] = "127.0.0.1";
    char port[16]  = "8888";
    if (strcmp(spec, "1") != 0) {
        const char* colon = strrchr(spec, ':');
        if (colon != nullptr) {
            const size_t hl = static_cast<size_t>(colon - spec);
            if (hl > 0 && hl < sizeof(host)) { memcpy(host, spec, hl); host[hl] = '\0'; }
            snprintf(port, sizeof(port), "%s", colon + 1);
        } else {
            snprintf(host, sizeof(host), "%s", spec);
        }
    }
    jclass sysCls = env->FindClass("java/lang/System");
    jmethodID setProp = sysCls ? env->GetStaticMethodID(sysCls, "setProperty",
        "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;") : nullptr;
    if (setProp == nullptr) { if (env->ExceptionCheck()) env->ExceptionClear(); return; }
    const char* keys[][2] = {
        {"http.proxyHost", host}, {"http.proxyPort", port},
        {"https.proxyHost", host}, {"https.proxyPort", port},
    };
    for (auto& kv : keys) {
        jstring k = env->NewStringUTF(kv[0]);
        jstring v = env->NewStringUTF(kv[1]);
        jobject old = env->CallStaticObjectMethod(sysCls, setProp, k, v);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (old) env->DeleteLocalRef(old);
        env->DeleteLocalRef(k); env->DeleteLocalRef(v);
    }
    fprintf(stderr, "[WESTLAKE-413] proxy props set: %s:%s\n", host, port);
    fflush(stderr);
}

// ============================================================
// WESTLAKE §412 — survive an uncaught exception on the main thread
// ============================================================
// AppSpawnXInit.initChild() wraps ActivityThread.main() in `catch (Throwable) { ...
// System.exit(1); }`.  So ANY uncaught exception on the main looper takes the whole app down
// (`[INITCHILD-FAIL] ...` then `[WESTLAKE-REAP] child N exited(1)`).  On this adapter that happens
// for every service we do not provide — e.g. tapping the Presets tab hits
// `NullPointerException: ShortcutManager.getDynamicShortcuts() on a null object reference`, and the
// whole process dies instead of just that one screen misbehaving.
//
// java.lang.Runtime.nativeExit is an ordinary JNI-registered native (libart's openjdk_stub owns it),
// and RegisterNatives is last-wins, so the bridge can take it over.  When the exit comes from
// initChild's catch block we re-enter Looper.loop() instead of exiting: ActivityThread, the windows
// and the view hierarchy are all still intact, so the app carries on and only the failed action is
// lost.  Every other exit proceeds normally.
static void WL_Runtime_nativeExit(JNIEnv* env, jclass /*clazz*/, jint status) {
    static std::atomic<int> wl_resumes{0};
    const int kMaxResumes = 32;   // do not spin forever if the loop rethrows immediately

    bool fromInitChild = false;
    if (env != nullptr && !env->ExceptionCheck()) {
        jclass thCls = env->FindClass("java/lang/Thread");
        jmethodID curM = thCls ? env->GetStaticMethodID(thCls, "currentThread",
            "()Ljava/lang/Thread;") : nullptr;
        jobject cur = curM ? env->CallStaticObjectMethod(thCls, curM) : nullptr;
        jmethodID stM = thCls ? env->GetMethodID(thCls, "getStackTrace",
            "()[Ljava/lang/StackTraceElement;") : nullptr;
        jobjectArray st = (cur && stM)
            ? static_cast<jobjectArray>(env->CallObjectMethod(cur, stM)) : nullptr;
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (st != nullptr) {
            jclass steCls = env->FindClass("java/lang/StackTraceElement");
            jmethodID cnM = steCls ? env->GetMethodID(steCls, "getClassName",
                "()Ljava/lang/String;") : nullptr;
            const jsize n = env->GetArrayLength(st);
            for (jsize i = 0; i < n && !fromInitChild; ++i) {
                jobject e = env->GetObjectArrayElement(st, i);
                if (!e) continue;
                jstring js = cnM ? static_cast<jstring>(env->CallObjectMethod(e, cnM)) : nullptr;
                const char* cs = js ? env->GetStringUTFChars(js, nullptr) : nullptr;
                if (cs != nullptr && strstr(cs, "AppSpawnXInit") != nullptr) fromInitChild = true;
                if (js && cs) env->ReleaseStringUTFChars(js, cs);
                if (js) env->DeleteLocalRef(js);
                env->DeleteLocalRef(e);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
    }

    if (fromInitChild && wl_resumes.load() < kMaxResumes) {
        const int nth = wl_resumes.fetch_add(1) + 1;
        fprintf(stderr, "[WESTLAKE-412] Runtime.nativeExit(%d) from initChild — NOT exiting; "
                        "re-entering Looper.loop() (resume #%d)\n", (int)status, nth);
        fflush(stderr);
        if (env != nullptr) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            jclass loopCls = env->FindClass("android/os/Looper");
            jmethodID myLooper = loopCls ? env->GetStaticMethodID(loopCls, "myLooper",
                "()Landroid/os/Looper;") : nullptr;
            jobject lp = myLooper ? env->CallStaticObjectMethod(loopCls, myLooper) : nullptr;
            if (env->ExceptionCheck()) env->ExceptionClear();
            jmethodID loopM = loopCls ? env->GetStaticMethodID(loopCls, "loop", "()V") : nullptr;
            if (lp != nullptr && loopM != nullptr) {
                // Keep re-entering: a screen whose every frame rethrows (e.g. a text view that
                // re-parses broken HTML on each tick) would otherwise burn the single resume and
                // die anyway.  Bounded so a truly wedged app still terminates.
                for (int i = nth; i <= kMaxResumes; ++i) {
                    env->CallStaticVoidMethod(loopCls, loopM);   // normally never returns
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    fprintf(stderr, "[WESTLAKE-412] Looper.loop() threw again — resume #%d\n", i);
                    fflush(stderr);
                    wl_resumes.store(i);
                }
                fprintf(stderr, "[WESTLAKE-412] resume budget exhausted (%d)\n", kMaxResumes);
                fflush(stderr);
                return;   // let the caller unwind
            }
            fprintf(stderr, "[WESTLAKE-412] no Looper on this thread — cannot resume\n");
            fflush(stderr);
        }
        return;
    }
    fprintf(stderr, "[WESTLAKE-412] Runtime.nativeExit(%d) proceeding (fromInitChild=%d resumes=%d)\n",
            (int)status, fromInitChild ? 1 : 0, wl_resumes.load());
    fflush(stderr);
    _exit(status);
}

// ============================================================
// WESTLAKE §416 — give the app a non-null UserManager (and friends)
// ============================================================
// The Settings screen cannot inflate its MaterialSwitch preference:
//   InflateException: layout/preference_widget_material_switch
//     caused by NullPointerException: UserManager.isUserUnlocked(int) on a null object reference
// The same null UserManager also kills ViewRootImpl.enqueueInputEvent (autofill -> DeviceConfig ->
// Settings$NameValueCache).  It is null because SystemServiceRegistry's fetcher does
//   new UserManager(ctx, IUserManager.Stub.asInterface(ServiceManager.getServiceOrThrow("user")))
// and ServiceManager has no "user" service here, so getServiceOrThrow throws
// ServiceNotFoundException, the CachedServiceFetcher records STATE_NOT_FOUND and
// getSystemService(USER_SERVICE) returns null forever.
//
// ServiceManager.getService(name) consults a static `sCache` map BEFORE asking the real service
// manager, so seeding that map with a plain android.os.Binder makes the whole normal path work:
// asInterface() sees no local interface and builds an AIDL Stub.Proxy; a transact() on a local
// Binder returns false with an untouched reply Parcel, so readException() reads 0 (no exception)
// and the result reads back as the type's zero value.  isUserUnlocked() therefore returns false
// instead of throwing — which is all the inflate path needs.
//
// Deliberately narrow: seeding a service changes app behaviour, so only names we have diagnosed.
static void wl_seed_service_manager_cache(JNIEnv* env) {
    // "user"     — UserManager.isUserUnlocked, needed by MaterialSwitch inflate + autofill (§416)
    // "shortcut"  — ShortcutManager.getDynamicShortcuts, thrown on entering the Presets tab (§419)
    static const char* kServices[] = { "user", "shortcut" };

    jclass smCls = env->FindClass("android/os/ServiceManager");
    if (!smCls || env->ExceptionCheck()) {
        env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-416] android/os/ServiceManager not found\n");
        return;
    }
    // AOSP declares `private static HashMap<String, IBinder> sCache`; accept a Map too.
    jfieldID cacheF = env->GetStaticFieldID(smCls, "sCache", "Ljava/util/HashMap;");
    if (!cacheF || env->ExceptionCheck()) {
        env->ExceptionClear();
        cacheF = env->GetStaticFieldID(smCls, "sCache", "Ljava/util/Map;");
        if (env->ExceptionCheck()) { env->ExceptionClear(); cacheF = nullptr; }
    }
    if (cacheF == nullptr) {
        fprintf(stderr, "[WESTLAKE-416] ServiceManager.sCache not found — cannot seed\n");
        fflush(stderr);
        return;
    }
    jobject cache = env->GetStaticObjectField(smCls, cacheF);
    if (cache == nullptr || env->ExceptionCheck()) {
        env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-416] ServiceManager.sCache is null\n");
        fflush(stderr);
        return;
    }
    jclass mapCls = env->FindClass("java/util/Map");
    jmethodID putM = mapCls ? env->GetMethodID(mapCls, "put",
        "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;") : nullptr;
    jclass binderCls = env->FindClass("android/os/Binder");
    jmethodID bCtor = binderCls ? env->GetMethodID(binderCls, "<init>", "()V") : nullptr;
    if (putM == nullptr || bCtor == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-416] Map.put / Binder.<init> unresolved\n");
        fflush(stderr);
        return;
    }
    for (const char* name : kServices) {
        jobject binder = env->NewObject(binderCls, bCtor);
        if (binder == nullptr || env->ExceptionCheck()) {
            env->ExceptionClear();
            fprintf(stderr, "[WESTLAKE-416] new Binder() failed for '%s'\n", name);
            continue;
        }
        jstring key = env->NewStringUTF(name);
        jobject old = env->CallObjectMethod(cache, putM, key, binder);
        const bool threw = env->ExceptionCheck();
        if (threw) env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-416] seeded ServiceManager.sCache['%s'] (replaced=%d threw=%d)\n",
                name, old != nullptr ? 1 : 0, threw ? 1 : 0);
        if (old) env->DeleteLocalRef(old);
        env->DeleteLocalRef(key);
        env->DeleteLocalRef(binder);
    }
    fflush(stderr);
    env->DeleteLocalRef(cache);
    env->DeleteLocalRef(smCls);
}

// WESTLAKE §414 — android.view.KeyEvent.nativeNextId().
// AOSP implements it in android_view_KeyEvent.cpp (returns InputEvent::nextId()); we never port
// that file, so `new KeyEvent(...)` throws UnsatisfiedLinkError and NO key event can be built —
// which is why injected BACK never worked.  The id only has to be unique-ish per event.
static jint WL_KeyEvent_nativeNextId(JNIEnv*, jclass) {
    static std::atomic<int32_t> next{1};
    return static_cast<jint>(next.fetch_add(1));
}

// §416b: KeyEvent.nativeKeyCodeToString / nativeKeyCodeFromString are in the same unported AOSP
// file.  The "Enable media buttons" preference formats key codes, and the missing native threw
// UnsatisfiedLinkError on the main thread.  AOSP returns names like "KEYCODE_A"; the exact spelling
// only matters for display, so synthesise "KEYCODE_<n>" and parse that form back.
static jstring WL_KeyEvent_nativeKeyCodeToString(JNIEnv* env, jclass, jint keyCode) {
    char buf[32];
    snprintf(buf, sizeof(buf), "KEYCODE_%d", static_cast<int>(keyCode));
    return env->NewStringUTF(buf);
}

static jint WL_KeyEvent_nativeKeyCodeFromString(JNIEnv* env, jclass, jstring s) {
    if (s == nullptr) return 0;
    const char* cs = env->GetStringUTFChars(s, nullptr);
    jint rc = 0;
    if (cs != nullptr) {
        const char* p = strncmp(cs, "KEYCODE_", 8) == 0 ? cs + 8 : cs;
        rc = static_cast<jint>(atoi(p));
        env->ReleaseStringUTFChars(s, cs);
    }
    return rc;
}

static void wl_register_keyevent_natives(JNIEnv* env) {
    jclass keCls = env->FindClass("android/view/KeyEvent");
    if (!keCls || env->ExceptionCheck()) {
        env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-414] android/view/KeyEvent not found\n");
        return;
    }
    static const JNINativeMethod m[] = {
        {"nativeNextId", "()I", reinterpret_cast<void*>(WL_KeyEvent_nativeNextId)},
        {"nativeKeyCodeToString", "(I)Ljava/lang/String;",
         reinterpret_cast<void*>(WL_KeyEvent_nativeKeyCodeToString)},
        {"nativeKeyCodeFromString", "(Ljava/lang/String;)I",
         reinterpret_cast<void*>(WL_KeyEvent_nativeKeyCodeFromString)},
    };
    const jint rc = env->RegisterNatives(keCls, m, 3);
    if (env->ExceptionCheck()) env->ExceptionClear();
    fprintf(stderr, "[WESTLAKE-414] KeyEvent.nativeNextId RegisterNatives rc=%d\n", (int)rc);
    fflush(stderr);
    env->DeleteLocalRef(keCls);
}

// ============================================================
// WESTLAKE §423 — libcore.io.Linux.android_getaddrinfo (DNS)
// ============================================================
// THIS is why noice always said "The network is unreachable" even with working WiFi:
//   [WESTLAKE-JNIMISS] java.net.InetAddress[] libcore.io.Linux.android_getaddrinfo(
//                        java.lang.String, android.system.StructAddrinfo, int)
// the native is UNREGISTERED, so every DNS lookup throws UnsatisfiedLinkError *before* a socket is
// ever created. That matches the evidence exactly: an LD_PRELOAD trace of socket/connect/getaddrinfo
// recorded ZERO calls from the app, and /proc/net/tcp never showed a socket for its uid, while a
// probe running as that same uid resolved and connected fine.
//
// AOSP implements this in libcore's Linux.cpp on top of bionic's android_getaddrinfofornet(); that
// symbol does not exist in musl, which is presumably why it was never wired up here. The netId only
// selects a network on multi-network Android, and this board has exactly one, so plain getaddrinfo()
// is equivalent. Addresses come back through the public InetAddress.getByAddress(String, byte[]).
static jobjectArray WL_Linux_android_getaddrinfo(JNIEnv* env, jobject /*thiz*/,
                                                 jstring jnode, jobject /*hints*/, jint /*netId*/) {
    if (jnode == nullptr) return nullptr;
    const char* node = env->GetStringUTFChars(jnode, nullptr);
    if (node == nullptr) return nullptr;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = nullptr;
    const int rc = getaddrinfo(node, nullptr, &hints, &res);
    if (rc != 0 || res == nullptr) {
        fprintf(stderr, "[WESTLAKE-423] getaddrinfo(%s) failed rc=%d (%s)\n",
                node, rc, gai_strerror(rc));
        fflush(stderr);
        env->ReleaseStringUTFChars(jnode, node);
        if (res) freeaddrinfo(res);
        return nullptr;   // libcore turns a null result into UnknownHostException
    }

    int n = 0;
    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
        if (p->ai_family == AF_INET || p->ai_family == AF_INET6) n++;
    }
    jclass iaCls = env->FindClass("java/net/InetAddress");
    jmethodID byAddr = iaCls ? env->GetStaticMethodID(iaCls, "getByAddress",
        "(Ljava/lang/String;[B)Ljava/net/InetAddress;") : nullptr;
    if (byAddr == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-423] InetAddress.getByAddress unresolved\n"); fflush(stderr);
        env->ReleaseStringUTFChars(jnode, node);
        freeaddrinfo(res);
        return nullptr;
    }
    jobjectArray out = env->NewObjectArray(n, iaCls, nullptr);
    int i = 0;
    for (struct addrinfo* p = res; p != nullptr && i < n; p = p->ai_next) {
        const unsigned char* raw = nullptr;
        int len = 0;
        if (p->ai_family == AF_INET) {
            raw = (const unsigned char*)&((struct sockaddr_in*)p->ai_addr)->sin_addr; len = 4;
        } else if (p->ai_family == AF_INET6) {
            raw = (const unsigned char*)&((struct sockaddr_in6*)p->ai_addr)->sin6_addr; len = 16;
        } else {
            continue;
        }
        jbyteArray ba = env->NewByteArray(len);
        env->SetByteArrayRegion(ba, 0, len, (const jbyte*)raw);
        jobject ia = env->CallStaticObjectMethod(iaCls, byAddr, jnode, ba);
        if (env->ExceptionCheck()) { env->ExceptionClear(); }
        else if (ia != nullptr) { env->SetObjectArrayElement(out, i++, ia); }
        if (ia) env->DeleteLocalRef(ia);
        env->DeleteLocalRef(ba);
    }
    fprintf(stderr, "[WESTLAKE-423] getaddrinfo(%s) -> %d address(es)\n", node, i);
    fflush(stderr);
    env->ReleaseStringUTFChars(jnode, node);
    freeaddrinfo(res);
    return out;
}

// WESTLAKE §424 — the rest of the missing libcore.io.Linux natives.
// With DNS fixed (§423) the app got as far as creating a socket and hit the NEXT unregistered
// native: `java.io.FileDescriptor libcore.io.Linux.socket(int,int,int)`. gettid/statvfs were
// unregistered too. Same cause as the DNS one: libjavacore never bound them here.
static jobject wl_new_fd(JNIEnv* env, int fd) {
    jclass fdCls = env->FindClass("java/io/FileDescriptor");
    if (!fdCls) { if (env->ExceptionCheck()) env->ExceptionClear(); return nullptr; }
    jmethodID ctor = env->GetMethodID(fdCls, "<init>", "()V");
    jfieldID descF = env->GetFieldID(fdCls, "descriptor", "I");
    if (!ctor || !descF) { if (env->ExceptionCheck()) env->ExceptionClear(); return nullptr; }
    jobject o = env->NewObject(fdCls, ctor);
    if (o != nullptr) env->SetIntField(o, descF, fd);
    return o;
}

static jobject WL_Linux_socket(JNIEnv* env, jobject, jint domain, jint type, jint protocol) {
    const int fd = socket(domain, type, protocol);
    if (fd < 0) {
        fprintf(stderr, "[WESTLAKE-424] socket(%d,%d,%d) failed errno=%d\n",
                (int)domain, (int)type, (int)protocol, errno);
        fflush(stderr);
        return nullptr;
    }
    return wl_new_fd(env, fd);
}

static jint WL_Linux_gettid(JNIEnv*, jobject) {
    return static_cast<jint>(syscall(SYS_gettid));
}

static jobject WL_Linux_statvfs(JNIEnv* env, jobject, jstring jpath) {
    if (jpath == nullptr) return nullptr;
    const char* path = env->GetStringUTFChars(jpath, nullptr);
    struct statvfs st;
    const int rc = path ? statvfs(path, &st) : -1;
    if (path) env->ReleaseStringUTFChars(jpath, path);
    if (rc != 0) return nullptr;
    jclass c = env->FindClass("android/system/StructStatVfs");
    jmethodID ctor = c ? env->GetMethodID(c, "<init>", "(JJJJJJJJJJJ)V") : nullptr;
    if (ctor == nullptr) { if (env->ExceptionCheck()) env->ExceptionClear(); return nullptr; }
    return env->NewObject(c, ctor,
        (jlong)st.f_bsize, (jlong)st.f_frsize, (jlong)st.f_blocks, (jlong)st.f_bfree,
        (jlong)st.f_bavail, (jlong)st.f_files, (jlong)st.f_ffree, (jlong)st.f_favail,
        (jlong)st.f_fsid, (jlong)st.f_flag, (jlong)st.f_namemax);
}

// ============================================================
// WESTLAKE §425 — android.system.OsConstants was never initialised
// ============================================================
// With DNS fixed (§423) the app finally reached socket creation and asked for **socket(0,0,0)**,
// which fails with EAFNOSUPPORT(97):
//     [WESTLAKE-424] socket(0,0,0) failed errno=97
//       at java.net.PlainSocketImpl.socketCreate / okhttp3.internal.connection.RealConnection
// Those three arguments are `OsConstants.AF_INET6 / SOCK_STREAM / 0`. They are ALL ZERO because
// android.system.OsConstants' statics were never populated: the class has no field initialisers,
// its values are assigned by a native `initConstants()` invoked from <clinit>, and that never ran
// here (the log has ZERO OsConstants mentions). Same shape as §404's MethodType.
//
// Repair: force <clinit>; if that leaves AF_INET at 0, call initConstants() directly; and if the
// native itself is unbound, fall back to writing the constants the socket/file paths actually need.
struct WlConst { const char* name; int value; };

static void wl_repair_os_constants(JNIEnv* env) {
    jclass cls = env->FindClass("android/system/OsConstants");
    if (!cls || env->ExceptionCheck()) {
        env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-425] android/system/OsConstants not found\n"); fflush(stderr);
        return;
    }
    jfieldID afInet = env->GetStaticFieldID(cls, "AF_INET", "I");
    if (!afInet || env->ExceptionCheck()) {
        env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-425] AF_INET field missing\n"); fflush(stderr);
        return;
    }
    jint v = env->GetStaticIntField(cls, afInet);
    fprintf(stderr, "[WESTLAKE-425] AF_INET before = %d\n", (int)v); fflush(stderr);
    if (v != 0) return;   // already populated

    // 1. run <clinit> (JNI FindClass does not)
    jmethodID clinit = env->GetStaticMethodID(cls, "<clinit>", "()V");
    if (env->ExceptionCheck()) { env->ExceptionClear(); clinit = nullptr; }
    if (clinit != nullptr) {
        env->CallStaticVoidMethod(cls, clinit);
        if (env->ExceptionCheck()) { env->ExceptionClear(); }
        v = env->GetStaticIntField(cls, afInet);
        fprintf(stderr, "[WESTLAKE-425] after <clinit> AF_INET = %d\n", (int)v); fflush(stderr);
    }
    // 2. call the native initialiser directly
    if (v == 0) {
        jmethodID init = env->GetStaticMethodID(cls, "initConstants", "()V");
        if (env->ExceptionCheck()) { env->ExceptionClear(); init = nullptr; }
        if (init != nullptr) {
            env->CallStaticVoidMethod(cls, init);
            if (env->ExceptionCheck()) { env->ExceptionClear(); }
            v = env->GetStaticIntField(cls, afInet);
            fprintf(stderr, "[WESTLAKE-425] after initConstants AF_INET = %d\n", (int)v);
            fflush(stderr);
        }
    }
    if (v != 0) return;

    // 3. last resort: write the constants the socket / file paths depend on (Linux aarch64 values)
    static const WlConst kConsts[] = {
        {"AF_UNSPEC", 0}, {"AF_UNIX", 1}, {"AF_INET", 2}, {"AF_INET6", 10}, {"AF_NETLINK", 16},
        {"SOCK_STREAM", 1}, {"SOCK_DGRAM", 2}, {"SOCK_RAW", 3}, {"SOCK_SEQPACKET", 5},
        {"SOCK_CLOEXEC", 02000000}, {"SOCK_NONBLOCK", 04000},
        {"IPPROTO_IP", 0}, {"IPPROTO_ICMP", 1}, {"IPPROTO_TCP", 6}, {"IPPROTO_UDP", 17},
        {"IPPROTO_IPV6", 41}, {"IPPROTO_RAW", 255},
        {"SOL_SOCKET", 1},
        {"SO_REUSEADDR", 2}, {"SO_KEEPALIVE", 9}, {"SO_BROADCAST", 6}, {"SO_LINGER", 13},
        {"SO_SNDBUF", 7}, {"SO_RCVBUF", 8}, {"SO_SNDTIMEO", 21}, {"SO_RCVTIMEO", 20},
        {"SO_ERROR", 4}, {"SO_OOBINLINE", 10}, {"SO_BINDTODEVICE", 25},
        {"TCP_NODELAY", 1},
        {"IPV6_MULTICAST_HOPS", 18}, {"IPV6_TCLASS", 67}, {"IPV6_V6ONLY", 26},
        {"IP_TOS", 1}, {"IP_MULTICAST_TTL", 33},
        {"SHUT_RD", 0}, {"SHUT_WR", 1}, {"SHUT_RDWR", 2},
        {"MSG_PEEK", 2}, {"MSG_OOB", 1}, {"MSG_DONTROUTE", 4},
        {"O_RDONLY", 0}, {"O_WRONLY", 1}, {"O_RDWR", 2}, {"O_CREAT", 0100},
        {"O_EXCL", 0200}, {"O_TRUNC", 01000}, {"O_APPEND", 02000}, {"O_NONBLOCK", 04000},
        {"O_ACCMODE", 3}, {"O_CLOEXEC", 02000000},
        {"S_IFMT", 0170000}, {"S_IFDIR", 040000}, {"S_IFREG", 0100000}, {"S_IFLNK", 0120000},
        {"S_IFSOCK", 0140000}, {"S_IFIFO", 010000}, {"S_IFCHR", 020000}, {"S_IFBLK", 060000},
        {"S_IRUSR", 0400}, {"S_IWUSR", 0200}, {"S_IXUSR", 0100},
        {"F_GETFL", 3}, {"F_SETFL", 4}, {"F_GETFD", 1}, {"F_SETFD", 2}, {"F_DUPFD", 0},
        {"FD_CLOEXEC", 1},
        {"EAGAIN", 11}, {"EWOULDBLOCK", 11}, {"EINTR", 4}, {"EINPROGRESS", 115},
        {"ECONNRESET", 104}, {"EPIPE", 32}, {"EBADF", 9}, {"ENOENT", 2}, {"EACCES", 13},
        {"EEXIST", 17}, {"EINVAL", 22}, {"ETIMEDOUT", 110}, {"ECONNREFUSED", 111},
        {"EHOSTUNREACH", 113}, {"ENETUNREACH", 101}, {"EAFNOSUPPORT", 97}, {"EISCONN", 106},
        {"EALREADY", 114}, {"ENOTCONN", 107}, {"EADDRINUSE", 98},
        {"POLLIN", 1}, {"POLLOUT", 4}, {"POLLERR", 8}, {"POLLHUP", 16}, {"POLLNVAL", 32},
        {"SEEK_SET", 0}, {"SEEK_CUR", 1}, {"SEEK_END", 2},
        {"MAP_PRIVATE", 2}, {"MAP_SHARED", 1}, {"MAP_ANONYMOUS", 040000},
        {"PROT_READ", 1}, {"PROT_WRITE", 2}, {"PROT_EXEC", 4}, {"PROT_NONE", 0},
        {"AI_ADDRCONFIG", 0x20}, {"AI_ALL", 0x10}, {"AI_CANONNAME", 0x2}, {"AI_NUMERICHOST", 0x4},
        {"AI_PASSIVE", 0x1}, {"AI_V4MAPPED", 0x8},
        {"NI_NUMERICHOST", 1}, {"NI_NUMERICSERV", 2},
        {"EAI_AGAIN", -3}, {"EAI_NODATA", -5}, {"EAI_NONAME", -2}, {"EAI_SYSTEM", -11},
    };
    int set = 0, missing = 0;
    for (const WlConst& c : kConsts) {
        jfieldID f = env->GetStaticFieldID(cls, c.name, "I");
        if (!f || env->ExceptionCheck()) { env->ExceptionClear(); missing++; continue; }
        env->SetStaticIntField(cls, f, (jint)c.value);
        if (env->ExceptionCheck()) { env->ExceptionClear(); missing++; continue; }
        set++;
    }
    fprintf(stderr, "[WESTLAKE-425] wrote %d constants by hand (%d absent); AF_INET now %d\n",
            set, missing, (int)env->GetStaticIntField(cls, afInet));
    fflush(stderr);
    env->DeleteLocalRef(cls);
}

// WESTLAKE §426 — the remaining unregistered socket natives.
// Each fix reveals the next one, because the JNIMISS list only shows natives the app has actually
// HIT. After §425 the app created a socket and immediately needed getsockoptInt/socketpair.
static int wl_fd_of(JNIEnv* env, jobject fdObj) {
    if (fdObj == nullptr) return -1;
    jclass c = env->GetObjectClass(fdObj);
    jfieldID f = c ? env->GetFieldID(c, "descriptor", "I") : nullptr;
    if (!f) { if (env->ExceptionCheck()) env->ExceptionClear(); return -1; }
    return (int)env->GetIntField(fdObj, f);
}

static void wl_set_fd(JNIEnv* env, jobject fdObj, int fd) {
    if (fdObj == nullptr) return;
    jclass c = env->GetObjectClass(fdObj);
    jfieldID f = c ? env->GetFieldID(c, "descriptor", "I") : nullptr;
    if (f) env->SetIntField(fdObj, f, fd);
    else if (env->ExceptionCheck()) env->ExceptionClear();
}

/* Throw android.system.ErrnoException(functionName, errno) the way libcore callers expect. */
static void wl_throw_errno(JNIEnv* env, const char* fn, int err) {
    jclass c = env->FindClass("android/system/ErrnoException");
    if (!c) { if (env->ExceptionCheck()) env->ExceptionClear(); return; }
    jmethodID ctor = env->GetMethodID(c, "<init>", "(Ljava/lang/String;I)V");
    if (!ctor) { if (env->ExceptionCheck()) env->ExceptionClear(); return; }
    jstring n = env->NewStringUTF(fn);
    jobject e = env->NewObject(c, ctor, n, (jint)err);
    if (e) env->Throw((jthrowable)e);
}

static jint WL_Linux_getsockoptInt(JNIEnv* env, jobject, jobject jfd, jint level, jint option) {
    int fd = wl_fd_of(env, jfd);
    int value = 0;
    socklen_t len = sizeof(value);
    if (getsockopt(fd, level, option, &value, &len) < 0) {
        wl_throw_errno(env, "getsockopt", errno);
        return 0;
    }
    if (level == SOL_SOCKET && option == 4 /*SO_ERROR*/) {
        fprintf(stderr, "[WESTLAKE-429] getsockopt(fd=%d, SO_ERROR) = %d (%s)\n",
                fd, value, value ? strerror(value) : "no error");
        fflush(stderr);
    }
    return (jint)value;
}

static void WL_Linux_setsockoptInt(JNIEnv* env, jobject, jobject jfd, jint level, jint option, jint value) {
    int fd = wl_fd_of(env, jfd);
    int v = (int)value;
    if (setsockopt(fd, level, option, &v, sizeof(v)) < 0) wl_throw_errno(env, "setsockopt", errno);
}

static void WL_Linux_socketpair(JNIEnv* env, jobject, jint domain, jint type, jint protocol,
                                jobject jfd1, jobject jfd2) {
    int fds[2] = {-1, -1};
    if (socketpair(domain, type, protocol, fds) < 0) { wl_throw_errno(env, "socketpair", errno); return; }
    wl_set_fd(env, jfd1, fds[0]);
    wl_set_fd(env, jfd2, fds[1]);
}

static jint WL_Linux_fcntlInt(JNIEnv* env, jobject, jobject jfd, jint cmd, jint arg) {
    int fd = wl_fd_of(env, jfd);
    int r = fcntl(fd, cmd, arg);
    if (r < 0) { wl_throw_errno(env, "fcntl", errno); return 0; }
    return (jint)r;
}

static void WL_Linux_connect(JNIEnv* env, jobject, jobject jfd, jobject jaddr, jint port) {
    int fd = wl_fd_of(env, jfd);
    if (fd < 0 || jaddr == nullptr) { wl_throw_errno(env, "connect", EBADF); return; }
    jclass iaCls = env->FindClass("java/net/InetAddress");
    jmethodID getAddr = iaCls ? env->GetMethodID(iaCls, "getAddress", "()[B") : nullptr;
    jbyteArray ba = getAddr ? (jbyteArray)env->CallObjectMethod(jaddr, getAddr) : nullptr;
    if (ba == nullptr) { if (env->ExceptionCheck()) env->ExceptionClear();
                         wl_throw_errno(env, "connect", EINVAL); return; }
    const jsize n = env->GetArrayLength(ba);
    jbyte raw[16];
    env->GetByteArrayRegion(ba, 0, n > 16 ? 16 : n, raw);
    // §426b: Java opens AF_INET6 dual-stack sockets, so a 4-byte (IPv4) address must be sent as an
    // IPv4-MAPPED v6 address (::ffff:a.b.c.d). Handing a sockaddr_in to an AF_INET6 socket fails
    // with EINVAL — that was the "connect(35.94.160.101:443) = -1 Invalid argument".
    int domain = AF_INET6;
    {
        int d = 0; socklen_t dl = sizeof(d);
        if (getsockopt(fd, SOL_SOCKET, 39 /*SO_DOMAIN*/, &d, &dl) == 0 && d != 0) domain = d;
    }
    int rc;
    if (n == 4 && domain == AF_INET) {
        struct sockaddr_in a; memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET; a.sin_port = htons((uint16_t)port);
        memcpy(&a.sin_addr, raw, 4);
        rc = connect(fd, (struct sockaddr*)&a, sizeof(a));
    } else if (n == 4) {
        struct sockaddr_in6 a; memset(&a, 0, sizeof(a));
        a.sin6_family = AF_INET6; a.sin6_port = htons((uint16_t)port);
        a.sin6_addr.s6_addr[10] = 0xff; a.sin6_addr.s6_addr[11] = 0xff;
        memcpy(&a.sin6_addr.s6_addr[12], raw, 4);
        rc = connect(fd, (struct sockaddr*)&a, sizeof(a));
    } else {
        struct sockaddr_in6 a; memset(&a, 0, sizeof(a));
        a.sin6_family = AF_INET6; a.sin6_port = htons((uint16_t)port);
        memcpy(&a.sin6_addr, raw, 16);
        rc = connect(fd, (struct sockaddr*)&a, sizeof(a));
    }
    fprintf(stderr, "[WESTLAKE-427] connect(fd=%d, %u bytes, port=%d, domain=%d) = %d%s%s\n",
            fd, (unsigned)n, (int)port, domain, rc,
            rc < 0 ? " errno=" : "", rc < 0 ? strerror(errno) : "");
    fflush(stderr);
    // §428: throw on EINPROGRESS TOO. AOSP's Linux_connect reports every failure, and
    // IoBridge.connectErrno relies on catching EINPROGRESS to then poll() for completion.
    // Swallowing it made Java believe a non-blocking connect had already finished, so it used a
    // socket whose handshake was still in flight.
    if (rc < 0) wl_throw_errno(env, "connect", errno);
}

static void WL_Linux_shutdown(JNIEnv* env, jobject, jobject jfd, jint how) {
    int fd = wl_fd_of(env, jfd);
    if (shutdown(fd, how) < 0 && errno != ENOTCONN) wl_throw_errno(env, "shutdown", errno);
}

static jobject WL_Linux_getsockname(JNIEnv* env, jobject, jobject jfd) {
    int fd = wl_fd_of(env, jfd);
    struct sockaddr_storage ss; memset(&ss, 0, sizeof(ss));
    socklen_t len = sizeof(ss);
    if (getsockname(fd, (struct sockaddr*)&ss, &len) < 0) { wl_throw_errno(env, "getsockname", errno); return nullptr; }
    const unsigned char* raw = nullptr; int rawLen = 0, port = 0;
    if (ss.ss_family == AF_INET) {
        struct sockaddr_in* a = (struct sockaddr_in*)&ss;
        raw = (const unsigned char*)&a->sin_addr; rawLen = 4; port = ntohs(a->sin_port);
    } else if (ss.ss_family == AF_INET6) {
        struct sockaddr_in6* a = (struct sockaddr_in6*)&ss;
        raw = (const unsigned char*)&a->sin6_addr; rawLen = 16; port = ntohs(a->sin6_port);
    } else {
        return nullptr;
    }
    jclass iaCls = env->FindClass("java/net/InetAddress");
    jmethodID byAddr = iaCls ? env->GetStaticMethodID(iaCls, "getByAddress", "([B)Ljava/net/InetAddress;") : nullptr;
    if (!byAddr) { if (env->ExceptionCheck()) env->ExceptionClear(); return nullptr; }
    jbyteArray ba = env->NewByteArray(rawLen);
    env->SetByteArrayRegion(ba, 0, rawLen, (const jbyte*)raw);
    jobject ia = env->CallStaticObjectMethod(iaCls, byAddr, ba);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
    jclass isaCls = env->FindClass("java/net/InetSocketAddress");
    jmethodID ctor = isaCls ? env->GetMethodID(isaCls, "<init>", "(Ljava/net/InetAddress;I)V") : nullptr;
    if (!ctor) { if (env->ExceptionCheck()) env->ExceptionClear(); return nullptr; }
    return env->NewObject(isaCls, ctor, ia, (jint)port);
}

// §429: libcore.io.Linux.poll — IoBridge.connectErrno catches the EINPROGRESS thrown by connect()
// and then poll()s the fd for writability to learn when the handshake finished. Without it the
// non-blocking connect can never be completed.
static jint WL_Linux_poll(JNIEnv* env, jobject, jobjectArray jfds, jint timeoutMs) {
    const jsize n = jfds ? env->GetArrayLength(jfds) : 0;
    if (n <= 0) return 0;
    struct pollfd* pfds = (struct pollfd*)calloc((size_t)n, sizeof(struct pollfd));
    if (pfds == nullptr) { wl_throw_errno(env, "poll", ENOMEM); return -1; }

    jclass pfCls = env->FindClass("android/system/StructPollfd");
    jfieldID fFd = pfCls ? env->GetFieldID(pfCls, "fd", "Ljava/io/FileDescriptor;") : nullptr;
    jfieldID fEv = pfCls ? env->GetFieldID(pfCls, "events", "S") : nullptr;
    jfieldID fRe = pfCls ? env->GetFieldID(pfCls, "revents", "S") : nullptr;
    if (!fFd || !fEv || !fRe) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        free(pfds); wl_throw_errno(env, "poll", EINVAL); return -1;
    }
    for (jsize i = 0; i < n; ++i) {
        jobject e = env->GetObjectArrayElement(jfds, i);
        if (e == nullptr) { pfds[i].fd = -1; continue; }
        jobject fdo = env->GetObjectField(e, fFd);
        pfds[i].fd = fdo ? wl_fd_of(env, fdo) : -1;
        pfds[i].events = (short)env->GetShortField(e, fEv);
        if (fdo) env->DeleteLocalRef(fdo);
        env->DeleteLocalRef(e);
    }
    int rc;
    do { rc = poll(pfds, (nfds_t)n, (int)timeoutMs); } while (rc < 0 && errno == EINTR);
    fprintf(stderr, "[WESTLAKE-429] poll(n=%d, timeout=%d) = %d fd0=%d events=0x%x revents=0x%x%s\n",
            (int)n, (int)timeoutMs, rc, pfds[0].fd, (unsigned)pfds[0].events,
            (unsigned)pfds[0].revents, rc < 0 ? strerror(errno) : "");
    fflush(stderr);
    if (rc < 0) { free(pfds); wl_throw_errno(env, "poll", errno); return -1; }
    for (jsize i = 0; i < n; ++i) {
        jobject e = env->GetObjectArrayElement(jfds, i);
        if (e == nullptr) continue;
        env->SetShortField(e, fRe, (jshort)pfds[i].revents);
        env->DeleteLocalRef(e);
    }
    free(pfds);
    return (jint)rc;
}

// ===================== WESTLAKE §441: REAL TLS (was a passthrough stub) =====================
// adapter.compat.WestlakeSSLSocketFactory used to hand OkHttp back the *plain* socket, so the app
// spoke cleartext HTTP to port 443; the server hung up and OkHttp reported
// "EOFException: \n not found: limit=0". This implements the TLS client at the ABI boundary
// (the westlake way) on top of OHOS's own OpenSSL 3.x, which ships on the device:
//   /system/lib64/platformsdk/libssl_openssl.z.so  +  libcrypto_openssl.z.so
// Certificates are really verified against /etc/ssl/certs/cacert.pem, and the hostname is checked
// by OpenSSL itself via SSL_set1_host (OkHttp's OkHostnameVerifier then checks it a second time
// using the leaf certificate we hand back through getPeerCertificates()).
namespace {

struct WlSslApi {
    void* libssl = nullptr;
    void* libcrypto = nullptr;
    const void* (*TLS_client_method)();
    void* (*SSL_CTX_new)(const void*);
    int   (*SSL_CTX_load_verify_locations)(void*, const char*, const char*);
    void  (*SSL_CTX_set_verify)(void*, int, void*);
    void* (*SSL_new)(void*);
    int   (*SSL_set_fd)(void*, int);
    long  (*SSL_ctrl)(void*, int, long, void*);
    int   (*SSL_set1_host)(void*, const char*);
    int   (*SSL_connect)(void*);
    int   (*SSL_read)(void*, void*, int);
    int   (*SSL_write)(void*, const void*, int);
    int   (*SSL_get_error)(const void*, int);
    long  (*SSL_get_verify_result)(const void*);
    void* (*SSL_get1_peer_certificate)(const void*);
    const char* (*SSL_get_version)(const void*);
    const void* (*SSL_get_current_cipher)(const void*);
    const char* (*SSL_CIPHER_get_name)(const void*);
    int   (*SSL_shutdown)(void*);
    void  (*SSL_free)(void*);
    int   (*i2d_X509)(void*, unsigned char**);
    void  (*X509_free)(void*);
    void* ctx = nullptr;
    bool  ready = false;
    bool  tried = false;
};
static WlSslApi g_tls;
static pthread_mutex_t g_tls_lock = PTHREAD_MUTEX_INITIALIZER;

static const char* kCaFile = "/etc/ssl/certs/cacert.pem";

static void* wl_tls_dlopen(const char* const* names) {
    for (int i = 0; names[i] != nullptr; i++) {
        void* h = dlopen(names[i], RTLD_NOW | RTLD_GLOBAL);
        if (h != nullptr) {
            fprintf(stderr, "[WESTLAKE-441] dlopen %s OK\n", names[i]);
            return h;
        }
    }
    return nullptr;
}

#define WL_TLS_SYM(handle, field)                                                   \
    do {                                                                            \
        g_tls.field = reinterpret_cast<decltype(g_tls.field)>(dlsym(handle, #field)); \
        if (g_tls.field == nullptr) {                                               \
            fprintf(stderr, "[WESTLAKE-441] MISSING symbol %s\n", #field);          \
            missing++;                                                              \
        }                                                                           \
    } while (0)

static bool wl_tls_init() {
    pthread_mutex_lock(&g_tls_lock);
    if (g_tls.tried) { pthread_mutex_unlock(&g_tls_lock); return g_tls.ready; }
    g_tls.tried = true;

    static const char* kSslNames[] = {
        "libssl_openssl.z.so",
        "/system/lib64/platformsdk/libssl_openssl.z.so",
        "/system/lib64/chipset-sdk/libssl_openssl.z.so", nullptr };
    static const char* kCryptoNames[] = {
        "libcrypto_openssl.z.so",
        "/system/lib64/platformsdk/libcrypto_openssl.z.so",
        "/system/lib64/chipset-sdk-sp/libcrypto_openssl.z.so", nullptr };
    g_tls.libssl    = wl_tls_dlopen(kSslNames);
    g_tls.libcrypto = wl_tls_dlopen(kCryptoNames);
    if (g_tls.libssl == nullptr || g_tls.libcrypto == nullptr) {
        fprintf(stderr, "[WESTLAKE-441] dlopen FAILED ssl=%p crypto=%p err=%s\n",
                g_tls.libssl, g_tls.libcrypto, dlerror());
        fflush(stderr); pthread_mutex_unlock(&g_tls_lock); return false;
    }

    int missing = 0;
    void* s = g_tls.libssl;
    WL_TLS_SYM(s, TLS_client_method);      WL_TLS_SYM(s, SSL_CTX_new);
    WL_TLS_SYM(s, SSL_CTX_load_verify_locations); WL_TLS_SYM(s, SSL_CTX_set_verify);
    WL_TLS_SYM(s, SSL_new);                WL_TLS_SYM(s, SSL_set_fd);
    WL_TLS_SYM(s, SSL_ctrl);               WL_TLS_SYM(s, SSL_set1_host);
    WL_TLS_SYM(s, SSL_connect);            WL_TLS_SYM(s, SSL_read);
    WL_TLS_SYM(s, SSL_write);              WL_TLS_SYM(s, SSL_get_error);
    WL_TLS_SYM(s, SSL_get_verify_result);  WL_TLS_SYM(s, SSL_get1_peer_certificate);
    WL_TLS_SYM(s, SSL_get_version);        WL_TLS_SYM(s, SSL_get_current_cipher);
    WL_TLS_SYM(s, SSL_CIPHER_get_name);    WL_TLS_SYM(s, SSL_shutdown);
    WL_TLS_SYM(s, SSL_free);
    void* c = g_tls.libcrypto;
    WL_TLS_SYM(c, i2d_X509);               WL_TLS_SYM(c, X509_free);
    if (missing > 0) {
        fprintf(stderr, "[WESTLAKE-441] %d symbol(s) missing — TLS unavailable\n", missing);
        fflush(stderr); pthread_mutex_unlock(&g_tls_lock); return false;
    }

    g_tls.ctx = g_tls.SSL_CTX_new(g_tls.TLS_client_method());
    if (g_tls.ctx == nullptr) {
        fprintf(stderr, "[WESTLAKE-441] SSL_CTX_new FAILED\n");
        fflush(stderr); pthread_mutex_unlock(&g_tls_lock); return false;
    }
    const int loaded = g_tls.SSL_CTX_load_verify_locations(g_tls.ctx, kCaFile, nullptr);
    // SSL_VERIFY_PEER = 1. Keep verification ON: a silently-insecure client is worse than none.
    g_tls.SSL_CTX_set_verify(g_tls.ctx, 1, nullptr);
    fprintf(stderr, "[WESTLAKE-441] SSL_CTX ready ca=%s loaded=%d verify=PEER\n", kCaFile, loaded);
    fflush(stderr);
    g_tls.ready = (loaded == 1);
    if (!g_tls.ready) {
        fprintf(stderr, "[WESTLAKE-441] CA bundle did NOT load — refusing to run unverified\n");
        fflush(stderr);
    }
    pthread_mutex_unlock(&g_tls_lock);
    return g_tls.ready;
}

static void wl_tls_throw_io(JNIEnv* env, const char* what, int err) {
    char buf[192];
    snprintf(buf, sizeof(buf), "WestlakeTLS: %s failed (ssl_err=%d)", what, err);
    jclass ioe = env->FindClass("javax/net/ssl/SSLException");
    if (ioe == nullptr) { env->ExceptionClear(); ioe = env->FindClass("java/io/IOException"); }
    if (ioe != nullptr) env->ThrowNew(ioe, buf);
}

// Drive a would-block SSL op. Returns true to retry, false if it really failed/timed out.
static bool wl_tls_wait(int fd, int sslErr, int timeoutMs) {
    if (sslErr != 2 /*WANT_READ*/ && sslErr != 3 /*WANT_WRITE*/) return false;
    struct pollfd p;
    p.fd = fd;
    p.events = (sslErr == 2) ? POLLIN : POLLOUT;
    p.revents = 0;
    const int r = poll(&p, 1, timeoutMs);
    return r > 0;
}

// §447: dump the first few HTTP bytes each way so we can see the actual request/response.
static int g_tls_dump_count = 0;
static void wl_tls_dump(const char* op, const jbyte* data, int n) {
    if (g_tls_dump_count >= 6 || n <= 0) return;
    g_tls_dump_count++;
    const int cap = (n < 420) ? n : 420;
    std::string out;
    out.reserve((size_t)cap + 8);
    for (int i = 0; i < cap; i++) {
        const unsigned char c = (unsigned char)data[i];
        if (c == '\r') { out += "\\r"; }
        else if (c == '\n') { out += "\\n"; }
        else if (c >= 32 && c < 127) { out += (char)c; }
        else { out += '.'; }
    }
    fprintf(stderr, "[WESTLAKE-447] %s %d bytes: %s%s\n", op, n, out.c_str(),
            (n > cap) ? " ...(truncated)" : "");
    fflush(stderr);
}

// §446: bounded I/O tracing so a premature EOF can be told apart from a real one.
static int g_tls_io_logged = 0;
static void wl_tls_log_io(const char* op, const char* what, int n, int sslErr, int fd) {
    const bool interesting = (strcmp(what, "ok") != 0);
    if (!interesting && g_tls_io_logged >= 12) return;
    if (g_tls_io_logged >= 200) return;
    g_tls_io_logged++;
    fprintf(stderr, "[WESTLAKE-446] tls %s %s fd=%d n=%d ssl_err=%d errno=%s\n",
            op, what, fd, n, sslErr, (errno != 0) ? strerror(errno) : "-");
    fflush(stderr);
}

static jlong WL_TLS_handshake(JNIEnv* env, jclass, jint fd, jstring jhost, jint timeoutMs) {
    if (!wl_tls_init()) { wl_tls_throw_io(env, "init", 0); return 0; }
    const char* host = (jhost != nullptr) ? env->GetStringUTFChars(jhost, nullptr) : nullptr;

    void* ssl = g_tls.SSL_new(g_tls.ctx);
    if (ssl == nullptr) {
        if (host) env->ReleaseStringUTFChars(jhost, host);
        wl_tls_throw_io(env, "SSL_new", 0); return 0;
    }
    g_tls.SSL_set_fd(ssl, (int)fd);
    if (host != nullptr) {
        // SNI: SSL_CTRL_SET_TLSEXT_HOSTNAME=55, TLSEXT_NAMETYPE_host_name=0
        g_tls.SSL_ctrl(ssl, 55, 0, const_cast<char*>(host));
        g_tls.SSL_set1_host(ssl, host);   // OpenSSL-side hostname verification
    }

    const int deadline = (timeoutMs > 0) ? timeoutMs : 30000;
    int rc;
    for (;;) {
        rc = g_tls.SSL_connect(ssl);
        if (rc == 1) break;
        const int e = g_tls.SSL_get_error(ssl, rc);
        if (!wl_tls_wait((int)fd, e, deadline)) {
            fprintf(stderr, "[WESTLAKE-441] handshake FAILED host=%s rc=%d ssl_err=%d errno=%s\n",
                    host ? host : "?", rc, e, strerror(errno));
            fflush(stderr);
            g_tls.SSL_free(ssl);
            if (host) env->ReleaseStringUTFChars(jhost, host);
            wl_tls_throw_io(env, "handshake", e);
            return 0;
        }
    }
    const long vr = g_tls.SSL_get_verify_result(ssl);
    if (vr != 0 /*X509_V_OK*/) {
        fprintf(stderr, "[WESTLAKE-441] CERT VERIFY FAILED host=%s result=%ld\n",
                host ? host : "?", vr);
        fflush(stderr);
        g_tls.SSL_free(ssl);
        if (host) env->ReleaseStringUTFChars(jhost, host);
        wl_tls_throw_io(env, "certificate verification", (int)vr);
        return 0;
    }
    const char* ver = g_tls.SSL_get_version(ssl);
    const void* cip = g_tls.SSL_get_current_cipher(ssl);
    fprintf(stderr, "[WESTLAKE-441] HANDSHAKE OK host=%s proto=%s cipher=%s verify=OK\n",
            host ? host : "?", ver ? ver : "?",
            cip ? g_tls.SSL_CIPHER_get_name(cip) : "?");
    fflush(stderr);
    if (host) env->ReleaseStringUTFChars(jhost, host);
    return (jlong)(uintptr_t)ssl;
}

static jint WL_TLS_read(JNIEnv* env, jclass, jlong handle, jint fd, jbyteArray buf,
                        jint off, jint len, jint timeoutMs) {
    void* ssl = (void*)(uintptr_t)handle;
    if (ssl == nullptr || buf == nullptr) return -1;
    if (len <= 0) return 0;
    jbyte* tmp = (jbyte*)malloc((size_t)len);
    if (tmp == nullptr) return -1;
    const int deadline = (timeoutMs > 0) ? timeoutMs : 60000;
    int n;
    for (;;) {
        errno = 0;
        n = g_tls.SSL_read(ssl, tmp, len);
        if (n > 0) break;
        const int e = g_tls.SSL_get_error(ssl, n);
        if (e == 6 /*SSL_ERROR_ZERO_RETURN*/) {          // peer closed cleanly
            wl_tls_log_io("read", "peer closed (ZERO_RETURN)", n, e, fd);
            free(tmp); return -1;
        }
        if (e == 2 /*WANT_READ*/ || e == 3 /*WANT_WRITE*/) {
            if (!wl_tls_wait((int)fd, e, deadline)) {
                wl_tls_log_io("read", "poll timeout/error", n, e, fd);
                free(tmp); return -1;
            }
            continue;
        }
        // §446: SSL_ERROR_SYSCALL with a retryable errno is NOT end-of-stream. Treating it as EOF
        // is what produced OkHttp's "EOFException: \n not found: limit=0" on a healthy connection.
        if (e == 5 /*SSL_ERROR_SYSCALL*/ && n < 0 &&
            (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
            if (!wl_tls_wait((int)fd, 2 /*poll readable*/, deadline)) {
                wl_tls_log_io("read", "poll after EAGAIN failed", n, e, fd);
                free(tmp); return -1;
            }
            continue;
        }
        wl_tls_log_io("read", "fatal", n, e, fd);
        free(tmp); return -1;
    }
    env->SetByteArrayRegion(buf, off, n, tmp);
    wl_tls_dump("<-- recv", tmp, n);
    free(tmp);
    wl_tls_log_io("read", "ok", n, 0, fd);
    return n;
}

static jint WL_TLS_write(JNIEnv* env, jclass, jlong handle, jint fd, jbyteArray buf,
                         jint off, jint len, jint timeoutMs) {
    void* ssl = (void*)(uintptr_t)handle;
    if (ssl == nullptr || buf == nullptr) return -1;
    if (len <= 0) return 0;
    jbyte* tmp = (jbyte*)malloc((size_t)len);
    if (tmp == nullptr) return -1;
    env->GetByteArrayRegion(buf, off, len, tmp);
    wl_tls_dump("--> send", tmp, len);
    const int deadline = (timeoutMs > 0) ? timeoutMs : 60000;
    int done = 0;
    while (done < len) {
        errno = 0;
        const int n = g_tls.SSL_write(ssl, tmp + done, len - done);
        if (n > 0) { done += n; continue; }
        const int e = g_tls.SSL_get_error(ssl, n);
        if (e == 2 /*WANT_READ*/ || e == 3 /*WANT_WRITE*/) {
            if (!wl_tls_wait((int)fd, e, deadline)) {
                wl_tls_log_io("write", "poll timeout/error", n, e, fd);
                free(tmp); return -1;
            }
            continue;
        }
        if (e == 5 /*SSL_ERROR_SYSCALL*/ && n < 0 &&
            (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
            if (!wl_tls_wait((int)fd, 3 /*poll writable*/, deadline)) {
                wl_tls_log_io("write", "poll after EAGAIN failed", n, e, fd);
                free(tmp); return -1;
            }
            continue;
        }
        wl_tls_log_io("write", "fatal", n, e, fd);
        free(tmp); return -1;
    }
    free(tmp);
    wl_tls_log_io("write", "ok", done, 0, fd);
    return done;
}

static jbyteArray WL_TLS_peerCert(JNIEnv* env, jclass, jlong handle) {
    void* ssl = (void*)(uintptr_t)handle;
    if (ssl == nullptr) return nullptr;
    void* x = g_tls.SSL_get1_peer_certificate(ssl);
    if (x == nullptr) return nullptr;
    unsigned char* der = nullptr;
    const int n = g_tls.i2d_X509(x, &der);
    jbyteArray out = nullptr;
    if (n > 0 && der != nullptr) {
        out = env->NewByteArray(n);
        if (out != nullptr) env->SetByteArrayRegion(out, 0, n, (const jbyte*)der);
    }
    g_tls.X509_free(x);
    return out;
}

static jstring WL_TLS_info(JNIEnv* env, jclass, jlong handle, jint which) {
    void* ssl = (void*)(uintptr_t)handle;
    if (ssl == nullptr) return nullptr;
    if (which == 0) {
        const char* v = g_tls.SSL_get_version(ssl);
        return v ? env->NewStringUTF(v) : nullptr;
    }
    const void* c = g_tls.SSL_get_current_cipher(ssl);
    const char* n = c ? g_tls.SSL_CIPHER_get_name(c) : nullptr;
    return n ? env->NewStringUTF(n) : nullptr;
}

static void WL_TLS_close(JNIEnv*, jclass, jlong handle) {
    void* ssl = (void*)(uintptr_t)handle;
    if (ssl == nullptr) return;
    g_tls.SSL_shutdown(ssl);
    g_tls.SSL_free(ssl);
}

}  // namespace


// WESTLAKE §442: OkHttp's DiskLruCache rejected a perfectly legal cache key
//   IllegalArgumentException: keys must match regex [a-z0-9_-]{1,120}: "61d4dfc9...ae1b"
// which can only happen if Matcher.matches() lies. Characterise java.util.regex here so we know
// whether the engine is broken in general or only for some construct.
static void wl_probe_regex(JNIEnv* env) {
    jclass pat = env->FindClass("java/util/regex/Pattern");
    if (pat == nullptr) { env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-442] java/util/regex/Pattern NOT FOUND\n"); fflush(stderr); return; }
    jmethodID matches = env->GetStaticMethodID(pat, "matches",
        "(Ljava/lang/String;Ljava/lang/CharSequence;)Z");
    if (matches == nullptr) { env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-442] Pattern.matches NOT FOUND\n"); fflush(stderr); return; }
    struct { const char* re; const char* in; int expect; } cases[] = {
        {"[a-z0-9_-]{1,120}", "61d4dfc91e8f736cb250faa8afd1ae1b", 1},  // the real failing key
        {"[a-z0-9_-]+",       "61d4dfc91e8f736cb250faa8afd1ae1b", 1},  // same class, unbounded
        {"[a-z0-9]{1,120}",   "61d4dfc91e8f736cb250faa8afd1ae1b", 1},  // no '-' in the class
        {"[a-z0-9_-]{1,120}", "abc",                              1},  // short input
        {"a{1,3}",            "aa",                               1},  // bounded quantifier alone
        {"[a-z]+",            "abc",                              1},  // plain class
        {"[a-z]+",            "ABC",                              0},  // must NOT match
        {"abc",               "abc",                              1},  // pure literal
        {"a",                 "a",                                1},  // single literal char
        {"a+",                "aa",                               1},  // greedy on a literal
        {".",                 "a",                                1},  // any-char
        {"[a]",               "a",                                1},  // single-member class
        {"\\d+",              "123",                              1},  // predefined class
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        jstring re = env->NewStringUTF(cases[i].re);
        jstring in = env->NewStringUTF(cases[i].in);
        jboolean r = env->CallStaticBooleanMethod(pat, matches, re, in);
        if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
        fprintf(stderr, "[WESTLAKE-442] /%s/ =~ \"%s\" -> %d (expect %d)%s\n",
                cases[i].re, cases[i].in, (int)r, cases[i].expect,
                ((int)r == cases[i].expect) ? "" : "   <<<< WRONG");
        env->DeleteLocalRef(re); env->DeleteLocalRef(in);
    }
    // Narrow it down: is the pattern wrong, or does Matcher think the input is empty?
    jmethodID compile = env->GetStaticMethodID(pat, "compile",
        "(Ljava/lang/String;)Ljava/util/regex/Pattern;");
    jmethodID matcherOf = env->GetMethodID(pat, "matcher",
        "(Ljava/lang/CharSequence;)Ljava/util/regex/Matcher;");
    jclass mat = env->FindClass("java/util/regex/Matcher");
    if (compile && matcherOf && mat) {
        jstring re = env->NewStringUTF("[a-z]+");
        jstring in = env->NewStringUTF("abc");
        jobject pobj = env->CallStaticObjectMethod(pat, compile, re);
        if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
        jobject mobj = pobj ? env->CallObjectMethod(pobj, matcherOf, in) : nullptr;
        if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
        if (mobj != nullptr) {
            jmethodID regionEnd = env->GetMethodID(mat, "regionEnd", "()I");
            jmethodID regionStart = env->GetMethodID(mat, "regionStart", "()I");
            jmethodID find = env->GetMethodID(mat, "find", "()Z");
            jint rs = regionStart ? env->CallIntMethod(mobj, regionStart) : -1;
            jint re2 = regionEnd ? env->CallIntMethod(mobj, regionEnd) : -1;
            jboolean f = find ? env->CallBooleanMethod(mobj, find) : 0;
            if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
            fprintf(stderr, "[WESTLAKE-442] matcher(\"abc\") regionStart=%d regionEnd=%d "
                    "(expect 0..3)  find=%d (expect 1)\n", (int)rs, (int)re2, (int)f);
        }
        // §404 recipe: does the class merely have un-run static initialisers?
        jmethodID clinit = env->GetStaticMethodID(pat, "<clinit>", "()V");
        if (clinit != nullptr) {
            env->CallStaticVoidMethod(pat, clinit);
            if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
            jstring re3 = env->NewStringUTF("[a-z]+");
            jstring in3 = env->NewStringUTF("abc");
            jboolean r = env->CallStaticBooleanMethod(pat, matches, re3, in3);
            if (env->ExceptionCheck()) { env->ExceptionClear(); }
            fprintf(stderr, "[WESTLAKE-442] after forcing Pattern.<clinit>: /[a-z]+/ =~ \"abc\" -> %d\n",
                    (int)r);
        } else {
            env->ExceptionClear();
            fprintf(stderr, "[WESTLAKE-442] Pattern has no reachable <clinit>\n");
        }
    }
    // §442b: same logic through four call shapes, from a BCP dex, to isolate desugared lambdas.
    jclass probe = env->FindClass("adapter/compat/WlProbe");
    if (probe == nullptr) { env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-442] adapter/compat/WlProbe NOT FOUND\n"); }
    else {
        jmethodID run = env->GetStaticMethodID(probe, "run", "()Ljava/lang/String;");
        if (run != nullptr) {
            jstring r = (jstring)env->CallStaticObjectMethod(probe, run);
            if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
            if (r != nullptr) {
                const char* c = env->GetStringUTFChars(r, nullptr);
                fprintf(stderr, "[WESTLAKE-442] BCP dispatch: %s\n", c ? c : "(null)");
                if (c) env->ReleaseStringUTFChars(r, c);
            }
        } else { env->ExceptionClear(); fprintf(stderr, "[WESTLAKE-442] WlProbe.run NOT FOUND\n"); }
        env->DeleteLocalRef(probe);
    }
    fflush(stderr);
    env->DeleteLocalRef(pat);
}

// ==================== WESTLAKE §443: a REAL java.util.regex ====================
// libart ships a bring-up stub for the ICU regex natives (`Westlake_PatternNative_compileImpl`
// just stores the pattern STRING; `WestlakeRegexFind` is a literal substring search), so the whole
// of java.util.regex behaved as if every pattern were compiled with UREGEX_LITERAL:
//   /abc/ =~ "abc" -> true      but   /[abc]/ =~ "[abc]" -> true, groupCount("(a)(b)") -> 0
// That silently breaks anything relying on regex — it surfaced as OkHttp's DiskLruCache rejecting
// a perfectly legal cache key, which killed noice's sound-library fetch.
//
// libart is not rebuildable from source (§435), so we override the natives here instead:
// RegisterNatives is last-wins and the bridge registers after libart. The implementation mirrors
// AOSP's android_icu_util_regex_{Pattern,Matcher}Native.cpp on ICU's uregex_* API, taken from the
// libicui18n.so we already ship (symbols carry the ICU-66 `_66` suffix).
namespace {

struct WlReApi {
    void* lib = nullptr;
    void* (*open)(const uint16_t*, int32_t, uint32_t, void*, int*);
    void* (*clone)(const void*, int*);
    void  (*close)(void*);
    void  (*setText)(void*, const uint16_t*, int32_t, int*);
    void  (*setRegion)(void*, int32_t, int32_t, int*);
    int8_t (*matches)(void*, int32_t, int*);
    int8_t (*lookingAt)(void*, int32_t, int*);
    int8_t (*find)(void*, int32_t, int*);
    int8_t (*findNext)(void*, int*);
    int32_t (*groupCount)(void*, int*);
    int32_t (*start)(void*, int32_t, int*);
    int32_t (*end)(void*, int32_t, int*);
    int8_t (*hitEnd)(const void*, int*);
    int8_t (*requireEnd)(const void*, int*);
    void  (*useAnchoringBounds)(void*, int8_t, int*);
    void  (*useTransparentBounds)(void*, int8_t, int*);
    int32_t (*groupNumberFromName)(void*, const uint16_t*, int32_t, int*);
    bool ok = false;
    bool tried = false;
};
static WlReApi g_re;

// ICU exports are version-suffixed (uregex_open_66). Try plain, then the versions we might ship.
static void* wl_re_sym(void* h, const char* base) {
    static const char* kSuffix[] = { "", "_66", "_74", "_72", "_70", nullptr };
    char buf[128];
    for (int i = 0; kSuffix[i] != nullptr; i++) {
        snprintf(buf, sizeof(buf), "%s%s", base, kSuffix[i]);
        void* p = dlsym(h, buf);
        if (p != nullptr) return p;
    }
    return nullptr;
}

#define WL_RE_SYM(field, name)                                                       \
    do {                                                                             \
        g_re.field = reinterpret_cast<decltype(g_re.field)>(wl_re_sym(g_re.lib, name)); \
        if (g_re.field == nullptr) {                                                 \
            fprintf(stderr, "[WESTLAKE-443] MISSING %s\n", name);                    \
            missing++;                                                               \
        }                                                                            \
    } while (0)

static bool wl_re_init() {
    if (g_re.tried) return g_re.ok;
    g_re.tried = true;
    static const char* kNames[] = { "libicui18n.so", "/data/local/tmp/asx/libicui18n.so",
                                    "/system/lib64/platformsdk/libhmicui18n.z.so", nullptr };
    for (int i = 0; kNames[i] != nullptr && g_re.lib == nullptr; i++) {
        g_re.lib = dlopen(kNames[i], RTLD_NOW | RTLD_GLOBAL);
        if (g_re.lib) fprintf(stderr, "[WESTLAKE-443] dlopen %s OK\n", kNames[i]);
    }
    if (g_re.lib == nullptr) {
        fprintf(stderr, "[WESTLAKE-443] no libicui18n — regex stays stubbed (%s)\n", dlerror());
        fflush(stderr); return false;
    }
    int missing = 0;
    WL_RE_SYM(open, "uregex_open");            WL_RE_SYM(clone, "uregex_clone");
    WL_RE_SYM(close, "uregex_close");          WL_RE_SYM(setText, "uregex_setText");
    WL_RE_SYM(matches, "uregex_matches");      WL_RE_SYM(lookingAt, "uregex_lookingAt");
    WL_RE_SYM(find, "uregex_find");            WL_RE_SYM(findNext, "uregex_findNext");
    WL_RE_SYM(groupCount, "uregex_groupCount");WL_RE_SYM(start, "uregex_start");
    WL_RE_SYM(end, "uregex_end");              WL_RE_SYM(hitEnd, "uregex_hitEnd");
    WL_RE_SYM(requireEnd, "uregex_requireEnd");
    WL_RE_SYM(useAnchoringBounds, "uregex_useAnchoringBounds");
    WL_RE_SYM(useTransparentBounds, "uregex_useTransparentBounds");
    WL_RE_SYM(groupNumberFromName, "uregex_groupNumberFromName");
    // optional
    g_re.setRegion = reinterpret_cast<decltype(g_re.setRegion)>(wl_re_sym(g_re.lib, "uregex_setRegion"));
    if (missing > 0) {
        fprintf(stderr, "[WESTLAKE-443] %d symbols missing — regex stays stubbed\n", missing);
        fflush(stderr); return false;
    }
    g_re.ok = true;
    fprintf(stderr, "[WESTLAKE-443] ICU regex bound (setRegion=%s)\n",
            g_re.setRegion ? "yes" : "no");
    fflush(stderr);
    return true;
}

struct WlReMatcher {
    void* re = nullptr;
    std::u16string text;
};

static void wl_re_pattern_free(void* p) { if (p && g_re.ok) g_re.close(p); }
static void wl_re_matcher_free(void* p) {
    WlReMatcher* m = reinterpret_cast<WlReMatcher*>(p);
    if (m == nullptr) return;
    if (m->re && g_re.ok) g_re.close(m->re);
    delete m;
}

static void wl_re_offsets(JNIEnv* env, void* re, jintArray offsets) {
    if (offsets == nullptr || re == nullptr) return;
    int st = 0;
    const int32_t gc = g_re.groupCount(re, &st);
    const jsize len = env->GetArrayLength(offsets);
    std::vector<jint> tmp((size_t)len, -1);
    for (int32_t i = 0; i <= gc && (2 * i + 1) < len; i++) {
        st = 0; tmp[2 * i]     = g_re.start(re, i, &st);
        st = 0; tmp[2 * i + 1] = g_re.end(re, i, &st);
    }
    env->SetIntArrayRegion(offsets, 0, len, tmp.data());
}

static jlong WL_Pattern_compileImpl(JNIEnv* env, jclass, jstring pattern, jint flags) {
    if (!wl_re_init()) return 0;
    const jchar* chars = env->GetStringChars(pattern, nullptr);
    const jsize len = env->GetStringLength(pattern);
    int st = 0;
    void* re = g_re.open(reinterpret_cast<const uint16_t*>(chars), len, (uint32_t)flags, nullptr, &st);
    env->ReleaseStringChars(pattern, chars);
    if (st > 0 || re == nullptr) {   // U_FAILURE
        jclass ex = env->FindClass("java/util/regex/PatternSyntaxException");
        if (ex != nullptr) {
            jmethodID c = env->GetMethodID(ex, "<init>", "(Ljava/lang/String;Ljava/lang/String;I)V");
            if (c != nullptr) {
                jobject o = env->NewObject(ex, c, env->NewStringUTF("ICU compile failed"), pattern, -1);
                if (o != nullptr) env->Throw((jthrowable)o);
            } else { env->ExceptionClear(); }
        } else { env->ExceptionClear(); }
        return 0;
    }
    return (jlong)(uintptr_t)re;
}

static jlong WL_Pattern_openMatcherImpl(JNIEnv*, jclass, jlong addr) {
    if (!g_re.ok || addr == 0) return 0;
    int st = 0;
    void* c = g_re.clone((void*)(uintptr_t)addr, &st);
    if (st > 0 || c == nullptr) return 0;
    WlReMatcher* m = new WlReMatcher();
    m->re = c;
    return (jlong)(uintptr_t)m;
}

static jint WL_Pattern_getMatchedGroupIndexImpl(JNIEnv* env, jclass, jlong addr, jstring name) {
    if (!g_re.ok || addr == 0 || name == nullptr) return -1;
    const jchar* chars = env->GetStringChars(name, nullptr);
    const jsize len = env->GetStringLength(name);
    int st = 0;
    const int32_t idx = g_re.groupNumberFromName((void*)(uintptr_t)addr,
            reinterpret_cast<const uint16_t*>(chars), len, &st);
    env->ReleaseStringChars(name, chars);
    return (st > 0) ? -1 : (jint)idx;
}

static jlong WL_Pattern_getNativeFinalizer(JNIEnv*, jclass) {
    return (jlong)(uintptr_t)&wl_re_pattern_free;
}
static jlong WL_Matcher_getNativeFinalizer(JNIEnv*, jclass) {
    return (jlong)(uintptr_t)&wl_re_matcher_free;
}

static void WL_Matcher_setInputImpl(JNIEnv* env, jclass, jlong addr, jstring text,
                                    jint start, jint end) {
    WlReMatcher* m = reinterpret_cast<WlReMatcher*>((uintptr_t)addr);
    if (m == nullptr || !g_re.ok) return;
    const jchar* chars = env->GetStringChars(text, nullptr);
    const jsize len = env->GetStringLength(text);
    m->text.assign(reinterpret_cast<const char16_t*>(chars), (size_t)len);  // must outlive setText
    env->ReleaseStringChars(text, chars);
    int st = 0;
    g_re.setText(m->re, reinterpret_cast<const uint16_t*>(m->text.data()),
                 (int32_t)m->text.size(), &st);
    if (g_re.setRegion != nullptr) { st = 0; g_re.setRegion(m->re, start, end, &st); }
}

static jboolean WL_Matcher_matchesImpl(JNIEnv* env, jclass, jlong addr, jintArray offsets) {
    WlReMatcher* m = reinterpret_cast<WlReMatcher*>((uintptr_t)addr);
    if (m == nullptr || !g_re.ok) return JNI_FALSE;
    int st = 0;
    const bool r = g_re.matches(m->re, -1, &st) != 0 && st <= 0;
    if (r) wl_re_offsets(env, m->re, offsets);
    return r ? JNI_TRUE : JNI_FALSE;
}

static jboolean WL_Matcher_lookingAtImpl(JNIEnv* env, jclass, jlong addr, jintArray offsets) {
    WlReMatcher* m = reinterpret_cast<WlReMatcher*>((uintptr_t)addr);
    if (m == nullptr || !g_re.ok) return JNI_FALSE;
    int st = 0;
    const bool r = g_re.lookingAt(m->re, -1, &st) != 0 && st <= 0;
    if (r) wl_re_offsets(env, m->re, offsets);
    return r ? JNI_TRUE : JNI_FALSE;
}

static jboolean WL_Matcher_findImpl(JNIEnv* env, jclass, jlong addr, jint startIndex,
                                    jintArray offsets) {
    WlReMatcher* m = reinterpret_cast<WlReMatcher*>((uintptr_t)addr);
    if (m == nullptr || !g_re.ok) return JNI_FALSE;
    int st = 0;
    const bool r = g_re.find(m->re, startIndex, &st) != 0 && st <= 0;
    if (r) wl_re_offsets(env, m->re, offsets);
    return r ? JNI_TRUE : JNI_FALSE;
}

static jboolean WL_Matcher_findNextImpl(JNIEnv* env, jclass, jlong addr, jintArray offsets) {
    WlReMatcher* m = reinterpret_cast<WlReMatcher*>((uintptr_t)addr);
    if (m == nullptr || !g_re.ok) return JNI_FALSE;
    int st = 0;
    const bool r = g_re.findNext(m->re, &st) != 0 && st <= 0;
    if (r) wl_re_offsets(env, m->re, offsets);
    return r ? JNI_TRUE : JNI_FALSE;
}

static jint WL_Matcher_groupCountImpl(JNIEnv*, jclass, jlong addr) {
    WlReMatcher* m = reinterpret_cast<WlReMatcher*>((uintptr_t)addr);
    if (m == nullptr || !g_re.ok) return 0;
    int st = 0;
    return (jint)g_re.groupCount(m->re, &st);
}

static jboolean WL_Matcher_hitEndImpl(JNIEnv*, jclass, jlong addr) {
    WlReMatcher* m = reinterpret_cast<WlReMatcher*>((uintptr_t)addr);
    if (m == nullptr || !g_re.ok) return JNI_FALSE;
    int st = 0;
    return g_re.hitEnd(m->re, &st) ? JNI_TRUE : JNI_FALSE;
}

static jboolean WL_Matcher_requireEndImpl(JNIEnv*, jclass, jlong addr) {
    WlReMatcher* m = reinterpret_cast<WlReMatcher*>((uintptr_t)addr);
    if (m == nullptr || !g_re.ok) return JNI_FALSE;
    int st = 0;
    return g_re.requireEnd(m->re, &st) ? JNI_TRUE : JNI_FALSE;
}

static void WL_Matcher_useAnchoringBoundsImpl(JNIEnv*, jclass, jlong addr, jboolean v) {
    WlReMatcher* m = reinterpret_cast<WlReMatcher*>((uintptr_t)addr);
    if (m == nullptr || !g_re.ok) return;
    int st = 0; g_re.useAnchoringBounds(m->re, v ? 1 : 0, &st);
}

static void WL_Matcher_useTransparentBoundsImpl(JNIEnv*, jclass, jlong addr, jboolean v) {
    WlReMatcher* m = reinterpret_cast<WlReMatcher*>((uintptr_t)addr);
    if (m == nullptr || !g_re.ok) return;
    int st = 0; g_re.useTransparentBounds(m->re, v ? 1 : 0, &st);
}

}  // namespace


// WESTLAKE §444: java.lang.String arrives marked initialised with NULL statics — the §202/§404
// pattern again. OkHttp's Headers sorts header names with String.CASE_INSENSITIVE_ORDER, so the
// library fetch died on "NullPointerException: CASE_INSENSITIVE_ORDER must not be null".
// ⚠️Do NOT force String.<clinit> to repair this (§404's recipe): re-running it kills the child at
// startup — A/B proven, the app never reaches a frame. Construct the comparator and assign it.
// Only acts when the field really is null, so it is a no-op on a healthy runtime.
// WESTLAKE §450: a native log sink the APP can call. Android routes System.err to the log
// framework, so an injected printStackTrace() never reaches our child stderr; and libart's
// SCTHROW probe caps at 40 throws and is saturated by benign ones (28 Gson ctor probes etc.),
// which is exactly why noice's real failure was invisible.
static void WL_Probe_nativeLog(JNIEnv* env, jclass, jstring s) {
    if (s == nullptr) return;
    const char* c = env->GetStringUTFChars(s, nullptr);
    if (c != nullptr) {
        fprintf(stderr, "[WESTLAKE-450] APP THROWABLE:\n%s\n", c);
        fflush(stderr);
        env->ReleaseStringUTFChars(s, c);
    }
}

// WESTLAKE §454: replace §416's bare `new Binder()` for "shortcut" with a real local
// IShortcutService. A bare Binder satisfies Stub.asInterface() but every call then transacts into
// nothing and returns a null ParceledListSlice, which NPE'd
// ShortcutManager.getDynamicShortcuts() and killed noice's PresetsFragment.onViewCreated before it
// could query the preset table ("No Presets" with 4 presets sitting in the DB).
// WESTLAKE §458: wire bindService to the in-process service creator (the audio "in-app bind" gate).
// AppSpawnXInit's IActivityManager stub returns a type default, so bindServiceInstance() answers 0
// and noice's SoundPlaybackService is never created — tapping play does nothing. The adapter already
// has adapter.activity.InProcessServiceBinder (instantiate + attach + onCreate + onBind + connected);
// this just routes the bind through it. Must run AFTER AppSpawnXInit installs its stub.
static void wl_install_ams_bind(JNIEnv* env) {
    jclass cls = env->FindClass("adapter/compat/WlAmsBind");
    if (cls == nullptr) {
        env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-458] adapter/compat/WlAmsBind NOT FOUND\n");
        fflush(stderr);
        return;
    }
    jmethodID m = env->GetStaticMethodID(cls, "install", "()Ljava/lang/String;");
    if (m == nullptr) {
        env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-458] WlAmsBind.install not found\n");
        fflush(stderr);
        env->DeleteLocalRef(cls);
        return;
    }
    jstring r = (jstring) env->CallStaticObjectMethod(cls, m);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
    if (r != nullptr) {
        const char* c = env->GetStringUTFChars(r, nullptr);
        fprintf(stderr, "[WESTLAKE-458] ams bind: %s\n", c ? c : "(null)");
        if (c) env->ReleaseStringUTFChars(r, c);
    }
    fflush(stderr);
    env->DeleteLocalRef(cls);
}

// WESTLAKE §459: grant audio focus (gate 2). AudioManager.sService is a Proxy stub returning 0 =
// AUDIOFOCUS_REQUEST_FAILED, so the player never starts.
static void wl_install_audio_focus(JNIEnv* env) {
    jclass cls = env->FindClass("adapter/compat/WlAudioFocus");
    if (cls == nullptr) { env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-459] WlAudioFocus NOT FOUND\n"); fflush(stderr); return; }
    jmethodID m = env->GetStaticMethodID(cls, "install", "()Ljava/lang/String;");
    if (m == nullptr) { env->ExceptionClear(); env->DeleteLocalRef(cls); return; }
    jstring r = (jstring) env->CallStaticObjectMethod(cls, m);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
    if (r != nullptr) {
        const char* c = env->GetStringUTFChars(r, nullptr);
        fprintf(stderr, "[WESTLAKE-459] audio focus: %s\n", c ? c : "(null)");
        if (c) env->ReleaseStringUTFChars(r, c);
    }
    fflush(stderr);
    env->DeleteLocalRef(cls);
}

// WESTLAKE §467: provide "media_session". noice's SoundPlaybackService creates a MediaSession and
// MediaSessionManager's ISessionManager was null -> NPE inside onStartCommand.
// WESTLAKE §468: implement the two audio-policy list natives.
// Both are UNBOUND here, and AudioAttributes.setLegacyStreamType reaches
// AudioProductStrategy.initializeAudioProductStrategies -> native_list_audio_product_strategies,
// so the throw aborted com.github.ashutoshgngwr.noice.engine.SoundPlayerManager.<clinit> half-way
// and left its two AudioAttributesCompat statics NULL (§466). AOSP semantics: return a status int
// and fill the caller's ArrayList; an EMPTY list plus SUCCESS(0) is exactly what a device with no
// audio-policy engine should report.
static jint WL_AudioPolicy_listEmpty(JNIEnv*, jclass, jobject /*ArrayList*/) {
    return 0;   // AudioSystem.SUCCESS — leave the list empty
}

static void wl_register_audiopolicy_natives(JNIEnv* env) {
    struct Entry { const char* cls; const char* name; };
    static const Entry kEntries[] = {
        {"android/media/audiopolicy/AudioProductStrategy", "native_list_audio_product_strategies"},
        {"android/media/audiopolicy/AudioVolumeGroup",     "native_list_audio_volume_groups"},
    };
    for (const Entry& e : kEntries) {
        jclass cls = env->FindClass(e.cls);
        if (cls == nullptr || env->ExceptionCheck()) {
            env->ExceptionClear();
            fprintf(stderr, "[WESTLAKE-468] %s not found\n", e.cls);
            continue;
        }
        const JNINativeMethod m[] = {
            {e.name, "(Ljava/util/ArrayList;)I", reinterpret_cast<void*>(WL_AudioPolicy_listEmpty)},
        };
        const jint rc = env->RegisterNatives(cls, m, 1);
        if (env->ExceptionCheck()) env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-468] RegisterNatives(%s.%s) rc=%d\n", e.cls, e.name, (int)rc);
        env->DeleteLocalRef(cls);
    }
    fflush(stderr);
}

static void wl_install_media_session(JNIEnv* env) {
    jclass cls = env->FindClass("adapter/compat/WlMediaSession");
    if (cls == nullptr) { env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-467] WlMediaSession NOT FOUND\n"); fflush(stderr); return; }
    jmethodID m = env->GetStaticMethodID(cls, "install", "()Ljava/lang/String;");
    if (m == nullptr) { env->ExceptionClear(); env->DeleteLocalRef(cls); return; }
    jstring r = (jstring) env->CallStaticObjectMethod(cls, m);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
    if (r != nullptr) {
        const char* c = env->GetStringUTFChars(r, nullptr);
        fprintf(stderr, "[WESTLAKE-467] media session: %s\n", c ? c : "(null)");
        if (c) env->ReleaseStringUTFChars(r, c);
    }
    fflush(stderr);
    env->DeleteLocalRef(cls);
}

static void wl_install_shortcut_service(JNIEnv* env) {
    jclass cls = env->FindClass("adapter/compat/WlShortcutService");
    if (cls == nullptr) {
        env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-454] adapter/compat/WlShortcutService NOT FOUND\n");
        fflush(stderr);
        return;
    }
    jmethodID m = env->GetStaticMethodID(cls, "install", "()Ljava/lang/String;");
    if (m == nullptr) {
        env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-454] WlShortcutService.install not found\n");
        fflush(stderr);
        env->DeleteLocalRef(cls);
        return;
    }
    jstring r = (jstring) env->CallStaticObjectMethod(cls, m);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
    if (r != nullptr) {
        const char* c = env->GetStringUTFChars(r, nullptr);
        fprintf(stderr, "[WESTLAKE-454] shortcut service: %s\n", c ? c : "(null)");
        if (c) env->ReleaseStringUTFChars(r, c);
    }
    fflush(stderr);
    env->DeleteLocalRef(cls);
}

static void wl_register_probe_log(JNIEnv* env) {
    jclass cls = env->FindClass("adapter/compat/WlProbe");
    if (cls == nullptr) { env->ExceptionClear(); return; }
    static const JNINativeMethod m[] = {
        {"nativeLog", "(Ljava/lang/String;)V", reinterpret_cast<void*>(WL_Probe_nativeLog)},
    };
    const jint rc = env->RegisterNatives(cls, m, 1);
    if (env->ExceptionCheck()) env->ExceptionClear();
    fprintf(stderr, "[WESTLAKE-450] RegisterNatives(WlProbe.nativeLog) rc=%d\n", (int)rc);
    fflush(stderr);
    env->DeleteLocalRef(cls);
}

static void wl_repair_string_statics(JNIEnv* env) {
    jclass cls = env->FindClass("java/lang/String");
    if (cls == nullptr) { env->ExceptionClear(); return; }
    jfieldID fid = env->GetStaticFieldID(cls, "CASE_INSENSITIVE_ORDER", "Ljava/util/Comparator;");
    if (fid == nullptr) { env->ExceptionClear(); env->DeleteLocalRef(cls); return; }
    jobject v = env->GetStaticObjectField(cls, fid);
    if (v != nullptr) {
        env->DeleteLocalRef(v); env->DeleteLocalRef(cls);
        return;   // healthy runtime: nothing to do
    }
    jclass cmp = env->FindClass("java/lang/String$CaseInsensitiveComparator");
    jmethodID ctor = (cmp != nullptr) ? env->GetMethodID(cmp, "<init>", "()V") : nullptr;
    jobject obj = (ctor != nullptr) ? env->NewObject(cmp, ctor) : nullptr;
    if (obj == nullptr) {
        if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
        fprintf(stderr, "[WESTLAKE-444] could not build String$CaseInsensitiveComparator\n");
        fflush(stderr);
        if (cmp != nullptr) env->DeleteLocalRef(cmp);
        env->DeleteLocalRef(cls);
        return;
    }
    env->SetStaticObjectField(cls, fid, obj);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
    v = env->GetStaticObjectField(cls, fid);
    fprintf(stderr, "[WESTLAKE-444] String.CASE_INSENSITIVE_ORDER repaired -> %s\n",
            v != nullptr ? "SET" : "STILL NULL");
    if (v != nullptr) env->DeleteLocalRef(v);
    env->DeleteLocalRef(obj); env->DeleteLocalRef(cmp); env->DeleteLocalRef(cls);
    fflush(stderr);
}

// =============== WESTLAKE §451: a REAL charset converter (was a Latin-1 stub) ===============
// libart's stubs/icu_jni_stub.c implements com.android.icu.charset.NativeConverter.decode/encode as
// a byte<->char 1:1 copy AND writes ABSOLUTE offsets back into data[]. AOSP writes DELTAS (bytes
// consumed / chars produced), so CharsetDecoderICU kept advancing the buffer positions by the whole
// absolute offset each round:
//     java.nio.charset.CoderMalfunctionError:
//       java.lang.IllegalArgumentException: newPosition > limit: (15360 > 8192)
// Short ASCII strings survive (1:1 is right and one round is enough); a 565KB UTF-8 JSON stream does
// not — which is what killed noice's sound-library parse.
// libart is not rebuildable, so we override the natives here (RegisterNatives is last-wins), backed
// by the real ICU converter API from the libicuuc.so we already ship.
namespace {

struct WlCnvApi {
    void* lib = nullptr;
    void* (*open)(const char*, int*);
    void  (*close)(void*);
    void  (*toUnicode)(void*, uint16_t**, const uint16_t*, const char**, const char*, int*, int8_t, int*);
    void  (*fromUnicode)(void*, char**, const char*, const uint16_t**, const uint16_t*, int*, int8_t, int*);
    int8_t (*getMaxCharSize)(const void*);
    int8_t (*getMinCharSize)(const void*);
    void  (*resetToUnicode)(void*);
    void  (*resetFromUnicode)(void*);
    bool ok = false;
    bool tried = false;
};
static WlCnvApi g_cnv;

// §470: the child SIGSEGVs inside ucnv_fromUnicode_66 at addr=0x5c. Root cause, from the ordering in
// the zygote log: libart's own icu_jni stub registers com.android.icu.charset.NativeConverter FIRST
// (its JNI_OnLoad, log line ~337) and we re-register the same class much later (~2131). Anything that
// opens a charset in between gets a handle from the STUB's openConverter -- which returns a fake
// handle, `index + 1` into its private CHARSETS table, i.e. the integer 1. That value is cached inside
// the Java Charset/CharsetEncoder, so once our real encode() takes over it is handed 0x1 and passes it
// straight to ICU as a UConverter* -> deref at a tiny address -> SIGSEGV at 0x5c.
//
// RegisterNatives is last-wins per method, so we cannot rely on winning the race. Instead we ADOPT the
// stub's handles: its table is fixed and libart is a non-rebuildable binary here, so index->charset is
// stable. A foreign small-integer handle is mapped to the charset it denotes and backed by a real ICU
// converter, opened once and cached. ★A standalone probe (native-tools/wlicu.c) proved real converters
// stay valid across fork, so genuine pointers -- including ones the parent opened pre-fork -- are
// simply accepted; do NOT invalidate on pid change.
static pthread_mutex_t g_cnv_lk = PTHREAD_MUTEX_INITIALIZER;
static std::set<uintptr_t> g_cnv_live;      // handles WL_Cnv_open issued
static std::map<uintptr_t, void*> g_cnv_stub;  // libart fake handle -> real converter we opened for it

// libart/stubs/icu_jni_stub.c CHARSETS[], in order. Handle value is (index + 1).
static const char* const kStubCharsets[] = {
    "UTF-8", "US-ASCII", "ISO-8859-1", "UTF-16", "UTF-16BE", "UTF-16LE",
    "UTF-32", "UTF-32BE", "UTF-32LE",
};
static const size_t kStubCharsetCount = sizeof(kStubCharsets) / sizeof(kStubCharsets[0]);

// Anything at or above this is a real mapped address on this target; below it can only be a fake
// handle. Real ICU converters live on the heap, far above the first page.
static const uintptr_t kMinRealPointer = 0x10000;

static void wl_cnv_track(void* c) {
    pthread_mutex_lock(&g_cnv_lk);
    g_cnv_live.insert((uintptr_t)c);
    pthread_mutex_unlock(&g_cnv_lk);
}
static void wl_cnv_untrack(void* c) {
    pthread_mutex_lock(&g_cnv_lk);
    g_cnv_live.erase((uintptr_t)c);
    pthread_mutex_unlock(&g_cnv_lk);
}

// Turn whatever Java handed us into a usable UConverter*, or nullptr if it cannot be resolved.
static void* wl_cnv_resolve(jlong h, const char* who) {
    const uintptr_t v = (uintptr_t)h;
    if (v == 0 || !g_cnv.ok) return nullptr;
    if (v >= kMinRealPointer) return (void*)v;      // a real converter (ours, or inherited pre-fork)

    if (v > kStubCharsetCount) {
        fprintf(stderr, "[WESTLAKE-470] %s: handle %p is neither a pointer nor a known stub handle\n",
                who, (void*)v);
        fflush(stderr);
        return nullptr;
    }
    pthread_mutex_lock(&g_cnv_lk);
    auto it = g_cnv_stub.find(v);
    if (it != g_cnv_stub.end()) { void* c = it->second; pthread_mutex_unlock(&g_cnv_lk); return c; }
    pthread_mutex_unlock(&g_cnv_lk);

    const char* name = kStubCharsets[v - 1];
    int st = 0;
    void* real = g_cnv.open(name, &st);
    if (real == nullptr || st > 0) {
        fprintf(stderr, "[WESTLAKE-470] %s: cannot back stub handle %p (%s) st=%d\n",
                who, (void*)v, name, st);
        fflush(stderr);
        return nullptr;
    }
    pthread_mutex_lock(&g_cnv_lk);
    auto ins = g_cnv_stub.emplace(v, real);
    void* winner = ins.first->second;
    bool we_won = ins.second;
    pthread_mutex_unlock(&g_cnv_lk);
    if (!we_won) { g_cnv.close(real); return winner; }   // lost a race; keep the first one
    fprintf(stderr, "[WESTLAKE-470] adopted libart stub handle %p as %s -> real converter %p\n",
            (void*)v, name, real);
    fflush(stderr);
    return winner;
}

static void* wl_cnv_sym(void* h, const char* base) {
    static const char* kSuffix[] = { "", "_66", "_74", "_72", "_70", nullptr };
    char buf[128];
    for (int i = 0; kSuffix[i] != nullptr; i++) {
        snprintf(buf, sizeof(buf), "%s%s", base, kSuffix[i]);
        void* p = dlsym(h, buf);
        if (p != nullptr) return p;
    }
    return nullptr;
}

#define WL_CNV_SYM(field, name)                                                        \
    do {                                                                               \
        g_cnv.field = reinterpret_cast<decltype(g_cnv.field)>(wl_cnv_sym(g_cnv.lib, name)); \
        if (g_cnv.field == nullptr) { fprintf(stderr, "[WESTLAKE-451] MISSING %s\n", name); missing++; } \
    } while (0)

static bool wl_cnv_init() {
    if (g_cnv.tried) return g_cnv.ok;
    g_cnv.tried = true;
    static const char* kNames[] = { "libicuuc.so", "/data/local/tmp/asx/libicuuc.so",
                                    "/system/lib64/platformsdk/libhmicuuc.z.so", nullptr };
    for (int i = 0; kNames[i] != nullptr && g_cnv.lib == nullptr; i++)
        g_cnv.lib = dlopen(kNames[i], RTLD_NOW | RTLD_GLOBAL);
    if (g_cnv.lib == nullptr) {
        fprintf(stderr, "[WESTLAKE-451] no libicuuc — charset stays stubbed\n"); fflush(stderr);
        return false;
    }
    int missing = 0;
    WL_CNV_SYM(open, "ucnv_open");                 WL_CNV_SYM(close, "ucnv_close");
    WL_CNV_SYM(toUnicode, "ucnv_toUnicode");       WL_CNV_SYM(fromUnicode, "ucnv_fromUnicode");
    WL_CNV_SYM(getMaxCharSize, "ucnv_getMaxCharSize");
    WL_CNV_SYM(getMinCharSize, "ucnv_getMinCharSize");
    WL_CNV_SYM(resetToUnicode, "ucnv_resetToUnicode");
    WL_CNV_SYM(resetFromUnicode, "ucnv_resetFromUnicode");
    if (missing > 0) { fflush(stderr); return false; }
    g_cnv.ok = true;
    fprintf(stderr, "[WESTLAKE-451] ICU charset converter bound\n"); fflush(stderr);
    return true;
}

// This is handed to Java as the NativeAllocationRegistry finalizer (getNativeFinalizer), so the GC
// calls it with whatever handle the Java object holds -- including one of libart's fake integers.
// ucnv_close(0x1) would fault exactly like the encode path did, on a GC thread. Only ever close a
// real converter, and route a stub handle to the converter we adopted for it.
static void wl_cnv_free(void* p) {
    if (p == nullptr || !g_cnv.ok) return;
    const uintptr_t v = (uintptr_t)p;
    if (v >= kMinRealPointer) { wl_cnv_untrack(p); g_cnv.close(p); return; }
    pthread_mutex_lock(&g_cnv_lk);
    auto it = g_cnv_stub.find(v);
    void* real = (it == g_cnv_stub.end()) ? nullptr : it->second;
    if (it != g_cnv_stub.end()) g_cnv_stub.erase(it);
    pthread_mutex_unlock(&g_cnv_lk);
    if (real != nullptr) g_cnv.close(real);
}

static jlong WL_Cnv_open(JNIEnv* env, jclass, jstring jname) {
    if (!wl_cnv_init() || jname == nullptr) return 0;
    const char* name = env->GetStringUTFChars(jname, nullptr);
    int st = 0;
    void* c = g_cnv.open(name, &st);
    if (st > 0 || c == nullptr) {
        fprintf(stderr, "[WESTLAKE-451] ucnv_open(%s) FAILED st=%d\n", name ? name : "?", st);
        fflush(stderr);
        env->ReleaseStringUTFChars(jname, name);
        return 0;
    }
    fprintf(stderr, "[WESTLAKE-470] ucnv_open(%s) -> %p pid=%d\n", name, c, (int)getpid());
    fflush(stderr);
    env->ReleaseStringUTFChars(jname, name);
    wl_cnv_track(c);
    return (jlong)(uintptr_t)c;
}

static void WL_Cnv_close(JNIEnv*, jclass, jlong h) {
    wl_cnv_untrack((void*)(uintptr_t)h); wl_cnv_free((void*)(uintptr_t)h);
}

// decode(long, byte[] in, int inEnd, char[] out, int outEnd, int[] data, boolean flush)
static jint WL_Cnv_decode(JNIEnv* env, jclass, jlong h, jbyteArray in, jint inEnd,
                          jcharArray out, jint outEnd, jintArray data, jboolean flush) {
    void* cnv = (void*)(uintptr_t)h;
    if (cnv == nullptr || !g_cnv.ok) return 1;   // CoderResult error
    cnv = wl_cnv_resolve(h, "decode");
    if (cnv == nullptr) return 1;
    jbyte* src = env->GetByteArrayElements(in, nullptr);
    jchar* dst = env->GetCharArrayElements(out, nullptr);
    jint*  d   = env->GetIntArrayElements(data, nullptr);
    const jint inPos = d[0], outPos = d[1];

    const char* mySource      = reinterpret_cast<const char*>(src) + inPos;
    const char* mySourceLimit = reinterpret_cast<const char*>(src) + inEnd;
    uint16_t* myTarget        = reinterpret_cast<uint16_t*>(dst) + outPos;
    const uint16_t* myTargetLimit = reinterpret_cast<uint16_t*>(dst) + outEnd;

    int st = 0;
    g_cnv.toUnicode(cnv, &myTarget, myTargetLimit, &mySource, mySourceLimit,
                    nullptr, flush ? 1 : 0, &st);
    // ★AOSP semantics: data[] receives DELTAS, not absolute offsets. The stub's absolute write-back
    // is exactly what produced "newPosition > limit".
    d[0] = (jint)((mySource - reinterpret_cast<const char*>(src)) - inPos);
    d[1] = (jint)((myTarget - reinterpret_cast<uint16_t*>(dst)) - outPos);

    env->ReleaseIntArrayElements(data, d, 0);
    env->ReleaseCharArrayElements(out, dst, 0);
    env->ReleaseByteArrayElements(in, src, JNI_ABORT);
    if (st == 15 /*U_BUFFER_OVERFLOW_ERROR*/) return 0;   // OVERFLOW -> caller loops
    return (st > 0) ? 1 : 0;
}

// encode(long, char[] in, int inEnd, byte[] out, int outEnd, int[] data, boolean flush)
static jint WL_Cnv_encode(JNIEnv* env, jclass, jlong h, jcharArray in, jint inEnd,
                          jbyteArray out, jint outEnd, jintArray data, jboolean flush) {
    void* cnv = (void*)(uintptr_t)h;
    if (cnv == nullptr || !g_cnv.ok) return 1;
    cnv = wl_cnv_resolve(h, "encode");
    if (cnv == nullptr) return 1;
    jchar* src = env->GetCharArrayElements(in, nullptr);
    jbyte* dst = env->GetByteArrayElements(out, nullptr);
    jint*  d   = env->GetIntArrayElements(data, nullptr);
    const jint inPos = d[0], outPos = d[1];

    const uint16_t* mySource      = reinterpret_cast<uint16_t*>(src) + inPos;
    const uint16_t* mySourceLimit = reinterpret_cast<uint16_t*>(src) + inEnd;
    char* myTarget                = reinterpret_cast<char*>(dst) + outPos;
    const char* myTargetLimit     = reinterpret_cast<char*>(dst) + outEnd;

    int st = 0;
    g_cnv.fromUnicode(cnv, &myTarget, myTargetLimit, &mySource, mySourceLimit,
                      nullptr, flush ? 1 : 0, &st);
    d[0] = (jint)((mySource - reinterpret_cast<uint16_t*>(src)) - inPos);
    d[1] = (jint)((myTarget - reinterpret_cast<char*>(dst)) - outPos);

    env->ReleaseIntArrayElements(data, d, 0);
    env->ReleaseByteArrayElements(out, dst, 0);
    env->ReleaseCharArrayElements(in, src, JNI_ABORT);
    if (st == 15) return 0;
    return (st > 0) ? 1 : 0;
}

static jint  WL_Cnv_maxBytes(JNIEnv*, jclass, jlong h) {
    void* c=(void*)(uintptr_t)h; return ((c = wl_cnv_resolve(h,"maxBytes")) != nullptr) ? (jint)g_cnv.getMaxCharSize(c) : 1;
}
static jfloat WL_Cnv_aveBytes(JNIEnv*, jclass, jlong h) {
    void* c=(void*)(uintptr_t)h; return ((c = wl_cnv_resolve(h,"aveBytes")) != nullptr) ? (jfloat)g_cnv.getMaxCharSize(c) : 1.0f;
}
static jfloat WL_Cnv_aveChars(JNIEnv*, jclass, jlong h) {
    void* c=(void*)(uintptr_t)h;
    c = wl_cnv_resolve(h, "aveChars");
    if (!c) return 1.0f;
    const int8_t mn = g_cnv.getMinCharSize(c);
    return (mn > 0) ? (1.0f / (jfloat)mn) : 1.0f;
}
static void WL_Cnv_resetToUni(JNIEnv*, jclass, jlong h) {
    void* c=(void*)(uintptr_t)h; if ((c = wl_cnv_resolve(h,"resetToUni")) != nullptr) g_cnv.resetToUnicode(c);
}
static void WL_Cnv_resetFromUni(JNIEnv*, jclass, jlong h) {
    void* c=(void*)(uintptr_t)h; if ((c = wl_cnv_resolve(h,"resetFromUni")) != nullptr) g_cnv.resetFromUnicode(c);
}
static jbyteArray WL_Cnv_substBytes(JNIEnv* env, jclass, jlong) {
    jbyteArray a = env->NewByteArray(1);
    if (a != nullptr) { const jbyte q = '?'; env->SetByteArrayRegion(a, 0, 1, &q); }
    return a;
}
static jlong WL_Cnv_finalizer(JNIEnv*, jclass) { return (jlong)(uintptr_t)&wl_cnv_free; }
static void WL_Cnv_cbDecode(JNIEnv*, jclass, jlong, jint, jint, jstring) {}
static void WL_Cnv_cbEncode(JNIEnv*, jclass, jlong, jint, jint, jbyteArray) {}

}  // namespace

static void wl_register_charset_natives(JNIEnv* env) {
    if (!wl_cnv_init()) return;   // leave the stub rather than half-break charsets
    jclass cls = env->FindClass("com/android/icu/charset/NativeConverter");
    if (cls == nullptr) { env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-451] NativeConverter not found\n"); fflush(stderr); return; }
    static const JNINativeMethod m[] = {
        {"openConverter", "(Ljava/lang/String;)J", reinterpret_cast<void*>(WL_Cnv_open)},
        {"closeConverter", "(J)V", reinterpret_cast<void*>(WL_Cnv_close)},
        {"decode", "(J[BI[CI[IZ)I", reinterpret_cast<void*>(WL_Cnv_decode)},
        {"encode", "(J[CI[BI[IZ)I", reinterpret_cast<void*>(WL_Cnv_encode)},
        {"getMaxBytesPerChar", "(J)I", reinterpret_cast<void*>(WL_Cnv_maxBytes)},
        {"getAveBytesPerChar", "(J)F", reinterpret_cast<void*>(WL_Cnv_aveBytes)},
        {"getAveCharsPerByte", "(J)F", reinterpret_cast<void*>(WL_Cnv_aveChars)},
        {"resetByteToChar", "(J)V", reinterpret_cast<void*>(WL_Cnv_resetToUni)},
        {"resetCharToByte", "(J)V", reinterpret_cast<void*>(WL_Cnv_resetFromUni)},
        {"getSubstitutionBytes", "(J)[B", reinterpret_cast<void*>(WL_Cnv_substBytes)},
        {"getNativeFinalizer", "()J", reinterpret_cast<void*>(WL_Cnv_finalizer)},
        {"setCallbackDecode", "(JIILjava/lang/String;)V", reinterpret_cast<void*>(WL_Cnv_cbDecode)},
        {"setCallbackEncode", "(JII[B)V", reinterpret_cast<void*>(WL_Cnv_cbEncode)},
    };
    const jint rc = env->RegisterNatives(cls, m, 13);
    if (env->ExceptionCheck()) env->ExceptionClear();
    fprintf(stderr, "[WESTLAKE-451] RegisterNatives(NativeConverter) rc=%d (real ICU charsets)\n",
            (int)rc);
    fflush(stderr);
    env->DeleteLocalRef(cls);
}

static void wl_register_regex_natives(JNIEnv* env) {
    if (!wl_re_init()) return;   // leave libart's stub in place rather than break regex further
    jclass pn = env->FindClass("com/android/icu/util/regex/PatternNative");
    if (pn == nullptr) { env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-443] PatternNative not found\n"); fflush(stderr); return; }
    static const JNINativeMethod pm[] = {
        {"compileImpl", "(Ljava/lang/String;I)J", reinterpret_cast<void*>(WL_Pattern_compileImpl)},
        {"openMatcherImpl", "(J)J", reinterpret_cast<void*>(WL_Pattern_openMatcherImpl)},
        {"getMatchedGroupIndexImpl", "(JLjava/lang/String;)I",
         reinterpret_cast<void*>(WL_Pattern_getMatchedGroupIndexImpl)},
        {"getNativeFinalizer", "()J", reinterpret_cast<void*>(WL_Pattern_getNativeFinalizer)},
    };
    const jint rc1 = env->RegisterNatives(pn, pm, 4);
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(pn);

    jclass mn = env->FindClass("com/android/icu/util/regex/MatcherNative");
    if (mn == nullptr) { env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-443] MatcherNative not found\n"); fflush(stderr); return; }
    static const JNINativeMethod mm[] = {
        {"setInputImpl", "(JLjava/lang/String;II)V", reinterpret_cast<void*>(WL_Matcher_setInputImpl)},
        {"matchesImpl", "(J[I)Z", reinterpret_cast<void*>(WL_Matcher_matchesImpl)},
        {"lookingAtImpl", "(J[I)Z", reinterpret_cast<void*>(WL_Matcher_lookingAtImpl)},
        {"findImpl", "(JI[I)Z", reinterpret_cast<void*>(WL_Matcher_findImpl)},
        {"findNextImpl", "(J[I)Z", reinterpret_cast<void*>(WL_Matcher_findNextImpl)},
        {"groupCountImpl", "(J)I", reinterpret_cast<void*>(WL_Matcher_groupCountImpl)},
        {"hitEndImpl", "(J)Z", reinterpret_cast<void*>(WL_Matcher_hitEndImpl)},
        {"requireEndImpl", "(J)Z", reinterpret_cast<void*>(WL_Matcher_requireEndImpl)},
        {"useAnchoringBoundsImpl", "(JZ)V",
         reinterpret_cast<void*>(WL_Matcher_useAnchoringBoundsImpl)},
        {"useTransparentBoundsImpl", "(JZ)V",
         reinterpret_cast<void*>(WL_Matcher_useTransparentBoundsImpl)},
        {"getNativeFinalizer", "()J", reinterpret_cast<void*>(WL_Matcher_getNativeFinalizer)},
    };
    const jint rc2 = env->RegisterNatives(mn, mm, 11);
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(mn);
    fprintf(stderr, "[WESTLAKE-443] RegisterNatives Pattern=%d Matcher=%d (ICU regex ACTIVE)\n",
            (int)rc1, (int)rc2);
    fflush(stderr);
}

static void wl_register_tls_natives(JNIEnv* env) {
    jclass cls = env->FindClass("adapter/compat/WestlakeSSLSocket");
    if (cls == nullptr || env->ExceptionCheck()) {
        env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-441] adapter/compat/WestlakeSSLSocket NOT FOUND — TLS off\n");
        fflush(stderr);
        return;
    }
    static const JNINativeMethod m[] = {
        {"nativeHandshake", "(ILjava/lang/String;I)J", reinterpret_cast<void*>(WL_TLS_handshake)},
        {"nativeRead",  "(JI[BIII)I", reinterpret_cast<void*>(WL_TLS_read)},
        {"nativeWrite", "(JI[BIII)I", reinterpret_cast<void*>(WL_TLS_write)},
        {"nativePeerCert", "(J)[B",   reinterpret_cast<void*>(WL_TLS_peerCert)},
        {"nativeInfo", "(JI)Ljava/lang/String;", reinterpret_cast<void*>(WL_TLS_info)},
        {"nativeClose", "(J)V",       reinterpret_cast<void*>(WL_TLS_close)},
    };
    const jint rc = env->RegisterNatives(cls, m, 6);
    if (env->ExceptionCheck()) env->ExceptionClear();
    fprintf(stderr, "[WESTLAKE-441] RegisterNatives(WestlakeSSLSocket) rc=%d\n", (int)rc);
    fflush(stderr);
    env->DeleteLocalRef(cls);
}

static void wl_register_dns_native(JNIEnv* env) {
    jclass linuxCls = env->FindClass("libcore/io/Linux");
    if (!linuxCls || env->ExceptionCheck()) {
        env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-423] libcore/io/Linux not found — DNS native NOT installed\n");
        fflush(stderr);
        return;
    }
    static const JNINativeMethod m[] = {
        {"android_getaddrinfo",
         "(Ljava/lang/String;Landroid/system/StructAddrinfo;I)[Ljava/net/InetAddress;",
         reinterpret_cast<void*>(WL_Linux_android_getaddrinfo)},
        {"socket", "(III)Ljava/io/FileDescriptor;", reinterpret_cast<void*>(WL_Linux_socket)},
        {"gettid", "()I", reinterpret_cast<void*>(WL_Linux_gettid)},
        {"statvfs", "(Ljava/lang/String;)Landroid/system/StructStatVfs;",
         reinterpret_cast<void*>(WL_Linux_statvfs)},
        {"getsockoptInt", "(Ljava/io/FileDescriptor;II)I",
         reinterpret_cast<void*>(WL_Linux_getsockoptInt)},
        {"setsockoptInt", "(Ljava/io/FileDescriptor;III)V",
         reinterpret_cast<void*>(WL_Linux_setsockoptInt)},
        {"socketpair", "(IIILjava/io/FileDescriptor;Ljava/io/FileDescriptor;)V",
         reinterpret_cast<void*>(WL_Linux_socketpair)},
        {"fcntlInt", "(Ljava/io/FileDescriptor;II)I",
         reinterpret_cast<void*>(WL_Linux_fcntlInt)},
        {"connect", "(Ljava/io/FileDescriptor;Ljava/net/InetAddress;I)V",
         reinterpret_cast<void*>(WL_Linux_connect)},
        {"shutdown", "(Ljava/io/FileDescriptor;I)V", reinterpret_cast<void*>(WL_Linux_shutdown)},
        {"getsockname", "(Ljava/io/FileDescriptor;)Ljava/net/SocketAddress;",
         reinterpret_cast<void*>(WL_Linux_getsockname)},
        {"poll", "([Landroid/system/StructPollfd;I)I", reinterpret_cast<void*>(WL_Linux_poll)},
    };
    const jint rc = env->RegisterNatives(linuxCls, m, 12);
    if (env->ExceptionCheck()) env->ExceptionClear();
    fprintf(stderr, "[WESTLAKE-423] RegisterNatives(Linux.android_getaddrinfo) rc=%d\n", (int)rc);
    fflush(stderr);
    env->DeleteLocalRef(linuxCls);
}

static void wl_install_exit_guard(JNIEnv* env) {
    jclass rtCls = env->FindClass("java/lang/Runtime");
    if (!rtCls || env->ExceptionCheck()) {
        env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-412] java/lang/Runtime not found — exit guard NOT installed\n");
        return;
    }
    static const JNINativeMethod m[] = {
        {"nativeExit", "(I)V", reinterpret_cast<void*>(WL_Runtime_nativeExit)},
    };
    const jint rc = env->RegisterNatives(rtCls, m, 1);
    if (env->ExceptionCheck()) env->ExceptionClear();
    fprintf(stderr, "[WESTLAKE-412] exit guard RegisterNatives(Runtime.nativeExit) rc=%d\n", (int)rc);
    fflush(stderr);
    env->DeleteLocalRef(rtCls);
}

int AndroidRuntime::startReg(JNIEnv* env) {
    wl_icu_early_init();   // WESTLAKE §258 -- before ANY module registers
    fprintf(stderr, "[liboh_android_runtime] startReg entering (%zu modules)\n",
            kRegJNICount);
    WestlakeRegisterJsseShim(env);

    // Cache JavaVM so AOSP-ported sources (ApkAssets.cpp et al.) can call
    // AndroidRuntime::getJNIEnv() from background threads.
    JavaVM* vm = nullptr;
    if (env->GetJavaVM(&vm) == JNI_OK) {
        setJavaVM(vm);
        fprintf(stderr, "[liboh_android_runtime] cached JavaVM=%p\n", (void*)vm);
    }

    // 2026-05-02 G2.14n+: install RegisterNatives audit hook unconditionally
    // for this debug iteration. Hook flags fnPtrs whose page offset is f98/f99
    // OR whose containing lib is libskia_canvaskit (per SIGILL signature).
    // Tag: OH_RegHook (normal) / OH_RegHook_BAD (suspicious).
    install_register_natives_hook(env);

    // 2026-05-02 G2.14r: bootstrap libjavacore.so by invoking its JNI_OnLoad
    // ONCE before our own kRegJNI loop runs.  Adapter never calls
    // System.loadLibrary("javacore") on the Java side, so absent this hook
    // libjavacore's 12 register_libcore_* / register_sun_misc_Unsafe /
    // register_java_lang_invoke_* / register_android_system_OsConstants
    // never run, leaving the corresponding native methods unbound.  ART can
    // intrinsify some (sun.misc.Unsafe atomics) but not e.g.
    // NativeAllocationRegistry.applyFreeFunction — first GC cycle calls it
    // and SEGV pc=0 follows (G2.14r root cause).  Calling JNI_OnLoad here
    // restores the canonical AOSP init flow once for the parent process;
    // child forks inherit the registrations.
    {
        void* libjc = dlopen("libjavacore.so", RTLD_NOW);
        if (libjc != nullptr) {
            using OnLoadFn = jint (*)(JavaVM*, void*);
            OnLoadFn onload = reinterpret_cast<OnLoadFn>(
                dlsym(libjc, "JNI_OnLoad"));
            if (onload != nullptr && vm != nullptr) {
                jint rc = onload(vm, nullptr);
                fprintf(stderr,
                    "[liboh_android_runtime] libjavacore JNI_OnLoad rc=0x%x %s\n",
                    rc, (rc == JNI_VERSION_1_6) ? "(OK)" : "(unexpected)");
                if (env->ExceptionCheck()) {
                    fprintf(stderr,
                        "[liboh_android_runtime] libjavacore JNI_OnLoad raised exception:\n");
                    env->ExceptionDescribe();
                    env->ExceptionClear();
                }
            } else {
                fprintf(stderr,
                    "[liboh_android_runtime] libjavacore JNI_OnLoad symbol missing or vm=null\n");
            }
        } else {
            fprintf(stderr,
                "[liboh_android_runtime] libjavacore.so dlopen FAILED: %s\n",
                dlerror() ? dlerror() : "(null)");
        }
    }

    // Bound a local-ref frame generously; each register_* may create a handful
    // of class / method references that won't be released until frame pop.
    if (env->PushLocalFrame(200) < 0) {
        fprintf(stderr, "[liboh_android_runtime] PushLocalFrame failed\n");
        return -1;
    }

    // 2026-07-11: do NOT bail on the first module failure. A single non-essential
    // module (e.g. register_android_view_MotionEvent, which fails on arm64) must not
    // block the registration of every module AFTER it — notably android.os.MessageQueue
    // (Looper core) and android.content.res.ApkAssets, whose absence kills
    // ActivityThread.main with UnsatisfiedLinkError. Skip the failed one (clearing any
    // pending JNI exception so it doesn't poison the next registration) and continue.
    int failures = 0;
    for (size_t i = 0; i < kRegJNICount; ++i) {
        int rc = kRegJNI[i].proc(env);
        // 2026-07-11: log WHY a module failed (its pending exception) before clearing, so
        // missing-class / missing-field / signature-mismatch registration failures are
        // diagnosable instead of silently skipped.
        if (env->ExceptionCheck()) {
            jthrowable exc = env->ExceptionOccurred();
            env->ExceptionClear();
            if (exc != nullptr) {
                jclass excCls = env->GetObjectClass(exc);
                jmethodID toStr = env->GetMethodID(excCls,
                    "toString", "()Ljava/lang/String;");
                jstring js = toStr ? (jstring)env->CallObjectMethod(exc, toStr) : nullptr;
                if (env->ExceptionCheck()) env->ExceptionClear();
                const char* cs = js ? env->GetStringUTFChars(js, nullptr) : "<no msg>";
                fprintf(stderr, "[liboh_android_runtime]   %s EXC: %s\n", kRegJNI[i].name, cs);
                if (js) env->ReleaseStringUTFChars(js, cs);
            }
            if (rc >= 0) rc = -1;  // an exception means failure regardless of returned rc
        }
        if (rc < 0) {
            fprintf(stderr, "[liboh_android_runtime] %s failed (rc=%d) — skipping, continuing\n",
                    kRegJNI[i].name, rc);
            ++failures;
            continue;
        }
        fprintf(stderr, "[liboh_android_runtime]   ok %s\n", kRegJNI[i].name);
    }
    fprintf(stderr, "[liboh_android_runtime] kRegJNI loop done: %zu ok, %d failed\n",
            kRegJNICount - failures, failures);

    env->PopLocalFrame(nullptr);

    // ============================================================
    // Phase 2 (r27) — dlopen real libhwui.so + invoke its 27 register_X
    // functions to register Paint/Canvas/RenderNode/HardwareRenderer/...
    // graphics natives with REAL Skia-backed implementations.  No stubs.
    //
    // libhwui.so is OH-cross-built (DT_NEEDED: liboh_hwui_shim.so /
    // liboh_skia_rtti_shim.so / libskia_canvaskit.z.so / libEGL.so
    // / libGLESv3.so / libutils.so etc., all available on device).
    // Earlier r15 diag confirmed libhwui dlopen alone succeeds in JVM ctx.
    // ============================================================
    if (env->PushLocalFrame(50) >= 0) {
        const char* kHwuiPaths[] = {
            "/system/android/lib/libhwui.so",
            "libhwui.so",
        };
        void* hwui = nullptr;
        for (const char* p : kHwuiPaths) {
            hwui = dlopen(p, RTLD_NOW | RTLD_GLOBAL);
            if (hwui) {
                fprintf(stderr, "[liboh_android_runtime] dlopen %s OK\n", p);
                break;
            } else {
                fprintf(stderr, "[liboh_android_runtime] dlopen %s FAIL: %s\n",
                        p, dlerror());
            }
        }
        if (hwui) {
            // 45 register_X mangled symbol names extracted from libhwui.so
            // dynsym table (`llvm-readelf --dyn-syms ... | grep register_android`).
            // Mangled names use Itanium C++ ABI:
            //   - `_Z<len>register_android_*P7_JNIEnv` for global-namespace fns
            //   - `_ZN7android<len>register_android_*EP7_JNIEnv` for android:: fns
            using RegFn = int (*)(JNIEnv*);
            struct HwuiReg { const char* name; const char* sym; };
            static const HwuiReg kHwuiRegFns[] = {
                // 2026-05-07 G2.14s: Graphics is registered below, AFTER ColorSpace,
                // matching AOSP frameworks/base/libs/hwui/apex/jni_runtime.cpp:104-109
                // canonical order (Canvas, ColorSpace, Graphics, Bitmap, ...).
                // History blame for the prior SKIP: see the larger comment at the
                // ColorSpace → Graphics block below.
                {"BitmapFactory",                "_Z39register_android_graphics_BitmapFactoryP7_JNIEnv"},
                {"Matrix",                       "_ZN7android32register_android_graphics_MatrixEP7_JNIEnv"},
                {"BitmapRegionDecoder",          "_Z45register_android_graphics_BitmapRegionDecoderP7_JNIEnv"},
                {"Interpolator",                 "_Z38register_android_graphics_InterpolatorP7_JNIEnv"},
                {"CreateJavaOutputStreamAdaptor","_Z55register_android_graphics_CreateJavaOutputStreamAdaptorP7_JNIEnv"},
                {"PathMeasure",                  "_ZN7android37register_android_graphics_PathMeasureEP7_JNIEnv"},
                {"GraphicsStatsService",         "_Z46register_android_graphics_GraphicsStatsServiceP7_JNIEnv"},
                {"Picture",                      "_ZN7android33register_android_graphics_PictureEP7_JNIEnv"},
                {"ColorFilter",                  "_ZN7android37register_android_graphics_ColorFilterEP7_JNIEnv"},
                {"Camera",                       "_Z32register_android_graphics_CameraP7_JNIEnv"},
                {"Gainmap",                      "_ZN7android33register_android_graphics_GainmapEP7_JNIEnv"},
                {"Region",                       "_ZN7android32register_android_graphics_RegionEP7_JNIEnv"},
                {"Paint",                        "_ZN7android31register_android_graphics_PaintEP7_JNIEnv"},
                {"DisplayListCanvas",            "_ZN7android39register_android_view_DisplayListCanvasEP7_JNIEnv"},
                {"ByteBufferStreamAdaptor",      "_Z49register_android_graphics_ByteBufferStreamAdaptorP7_JNIEnv"},
                {"Movie",                        "_Z31register_android_graphics_MovieP7_JNIEnv"},
                {"Mesh",                         "_ZN7android30register_android_graphics_MeshEP7_JNIEnv"},
                {"ThreadedRenderer",             "_ZN7android38register_android_view_ThreadedRendererEP7_JNIEnv"},
                {"PathIterator",                 "_ZN7android38register_android_graphics_PathIteratorEP7_JNIEnv"},
                // 2026-04-30 G2.4 (graphics_jni_inventory §3.3): ColorSpace
                // must register BEFORE ImageDecoder/Bitmap/HardwareBufferRenderer.
                // ImageDecoder's register_X caches a jfieldID for ColorSpace
                // static field (e.g. SRGB).  If ColorSpace class isn't loaded
                // when ImageDecoder.cpp does GetStaticFieldID, the cached fid
                // stays null → first nGetColorSpace call CheckJNI-aborts with
                // "JNI DETECTED ERROR IN APPLICATION: fid == null".
                // (Was previously after ImageDecoder; moved up here.)
                {"ColorSpace",                   "_ZN7android36register_android_graphics_ColorSpaceEP7_JNIEnv"},
                // 2026-05-07 G2.14s: Graphics MUST be registered here — after ColorSpace
                // (which Graphics's register fn reads via gColorSpace_Named_class for
                // SRGB/EXTENDED_SRGB/etc fields) and before ANY Paint/Bitmap/Region/
                // Canvas native gets first invoked at runtime.
                //
                // register_android_graphics_Graphics initializes the file-static
                // globals gFontMetricsInt_class / gFontMetrics_class / gRect_class /
                // gRectF_class / gPoint_class / gPointF_class / gBitmapConfig_class /
                // gCanvas_class / gPicture_class / gRegion_class / gByte_class /
                // gVMRuntime_class / gColorSpace_class / gColorSpaceRGB_class /
                // gTransferParameters_class plus their associated fieldIDs in
                // libhwui's own translation units.  Paint.cpp's nGetFontMetricsInt
                // does `IsInstanceOf(metrics, gFontMetricsInt_class)` and SetIntField
                // on gFontMetricsInt_top/ascent/descent/bottom/leading — without this
                // call those globals stay NULL → JNI DETECTED ERROR aborts in CheckJNI.
                //
                // History blame: 2026-05-06 SKIPPED on the false-attribution theory
                // that adding Graphics required a boot image rebuild, which itself
                // had failed with `Class mismatch String objectSize 467 vs 459`
                // dex2oat ABI mismatch.  That ABI mismatch is an INDEPENDENT issue
                // (ART build flags drift vs device libart) and has nothing to do
                // with whether libhwui's register_X is invoked at startReg time.
                // Verification (2026-05-07):
                //   nm -D libhwui.so | grep register_android_graphics_Graphics
                //   → 000dedcd T _Z34register_android_graphics_GraphicsP7_JNIEnv
                // The symbol IS exported; dlsym at startReg succeeds; the call
                // initializes Graphics's LOCAL g_* globals (LOCAL is fine — same .so
                // internal access, see jni_Graphics.o objdump showing 'b' BSS for
                // _ZL18gFontMetrics_class etc).  Paint native then sees init'd globals.
                {"Graphics",                     "_Z34register_android_graphics_GraphicsP7_JNIEnv"},
                {"AnimatedImageDrawable",        "_Z56register_android_graphics_drawable_AnimatedImageDrawableP7_JNIEnv"},
                {"PathParser",                   "_ZN7android32register_android_util_PathParserEP7_JNIEnv"},
                {"TextureLayer",                 "_ZN7android38register_android_graphics_TextureLayerEP7_JNIEnv"},
                {"AnimatedVectorDrawable",       "_ZN7android57register_android_graphics_drawable_AnimatedVectorDrawableEP7_JNIEnv"},
                {"NativeInterpolatorFactory",    "_ZN7android61register_android_graphics_animation_NativeInterpolatorFactoryEP7_JNIEnv"},
                {"ImageDecoder",                 "_Z38register_android_graphics_ImageDecoderP7_JNIEnv"},
                {"RenderNode",                   "_ZN7android32register_android_view_RenderNodeEP7_JNIEnv"},
                {"DrawFilter",                   "_ZN7android36register_android_graphics_DrawFilterEP7_JNIEnv"},
                {"RenderEffect",                 "_Z38register_android_graphics_RenderEffectP7_JNIEnv"},
                {"NinePatch",                    "_Z35register_android_graphics_NinePatchP7_JNIEnv"},
                {"Canvas",                       "_ZN7android32register_android_graphics_CanvasEP7_JNIEnv"},
                {"HardwareBufferRenderer",       "_ZN7android48register_android_graphics_HardwareBufferRendererEP7_JNIEnv"},
                {"Bitmap",                       "_Z32register_android_graphics_BitmapP7_JNIEnv"},
                {"HardwareRendererObserver",     "_ZN7android50register_android_graphics_HardwareRendererObserverEP7_JNIEnv"},
                {"Path",                         "_ZN7android30register_android_graphics_PathEP7_JNIEnv"},
                {"Shader",                       "_Z32register_android_graphics_ShaderP7_JNIEnv"},
                {"VectorDrawable",               "_ZN7android49register_android_graphics_drawable_VectorDrawableEP7_JNIEnv"},
                {"MaskFilter",                   "_Z36register_android_graphics_MaskFilterP7_JNIEnv"},
                {"PathEffect",                   "_Z36register_android_graphics_PathEffectP7_JNIEnv"},
                {"RenderNodeAnimator",           "_ZN7android54register_android_graphics_animation_RenderNodeAnimatorEP7_JNIEnv"},
                {"CanvasProperty",               "_ZN7android40register_android_graphics_CanvasPropertyEP7_JNIEnv"},
                {"YuvImage",                     "_Z34register_android_graphics_YuvImageP7_JNIEnv"},
                {"FontFamily",                   "_ZN7android36register_android_graphics_FontFamilyEP7_JNIEnv"},
                {"MeshSpecification",            "_ZN7android43register_android_graphics_MeshSpecificationEP7_JNIEnv"},
                // 2026-05-02 G2.14r: NEW font API (android.graphics.fonts.*)
                // — required by AOSP SystemFonts.buildSystemFallback chain.
                // Without these, Font$Builder.nInitBuilder() throws
                // UnsatisfiedLinkError → setSystemFontMap NPE → handleBindApplication
                // fails → mInitialApplication = null → ConfigurationController NPE.
                // libhwui exports them via fonts/Font.cpp + fonts/FontFamily.cpp
                // (compiled into libhwui.so in G2.14q Path A).
                {"fonts.Font",                   "_ZN7android36register_android_graphics_fonts_FontEP7_JNIEnv"},
                {"fonts.FontFamily",             "_ZN7android42register_android_graphics_fonts_FontFamilyEP7_JNIEnv"},
                // 2026-05-07 G2.14t: text/* register fns required by AOSP
                // apex/jni_runtime.cpp:149-152.  Order kept identical to AOSP
                // (MeasuredText → LineBreaker → TextShaper → GraphemeBreak).
                //
                // Without these registered, HelloWorld TextView.onMeasure path
                // → StaticLayout.generate → LineBreaker$Builder.build →
                // LineBreaker.<clinit>:450 → nGetReleaseFunc() throws
                // UnsatisfiedLinkError (No implementation found for ...) →
                // ART runtime exception → AMS schedulerDied → kill -9 child.
                //
                // Build dependency: build/compile_libhwui_jni.sh must compile
                // frameworks/base/libs/hwui/jni/text/*.cpp (4 files); the main
                // loop in that script globs jni/*.cpp (top level only) so
                // jni/text/ subdir was previously missed.  G2.14t patched both
                // sides simultaneously.
                // mangled name lengths verified against actual nm -D libhwui.so output:
                //   MeasuredText = 43 chars, LineBreaker = 42, TextShaper = 41, GraphemeBreak = 44
                {"text.MeasuredText",            "_ZN7android43register_android_graphics_text_MeasuredTextEP7_JNIEnv"},
                {"text.LineBreaker",             "_ZN7android42register_android_graphics_text_LineBreakerEP7_JNIEnv"},
                {"text.TextShaper",              "_ZN7android41register_android_graphics_text_TextShaperEP7_JNIEnv"},
                {"text.GraphemeBreak",           "_ZN7android44register_android_graphics_text_GraphemeBreakEP7_JNIEnv"},
                // 2026-05-01 G2.14n: Typeface real-impl from libhwui DEFERRED
                // — libhwui register_android_graphics_Typeface aborts during
                // startReg (SIGABRT in parent appspawn-x).  Likely needs
                // init_FontUtils / GraphicsJNI prior init that we haven't wired.
                // Keep stub for now; revisit after addressing init path.
            };
            int hwui_ok = 0, hwui_fail = 0;
            for (const auto& r : kHwuiRegFns) {
                RegFn fn = reinterpret_cast<RegFn>(dlsym(hwui, r.sym));
                if (!fn) {
                    fprintf(stderr, "[liboh_android_runtime]   dlsym %s FAIL: %s\n",
                            r.name, dlerror());
                    hwui_fail++;
                    continue;
                }
                int rc = fn(env);
                if (env->ExceptionCheck()) {
                    env->ExceptionDescribe();
                    env->ExceptionClear();
                    hwui_fail++;
                    continue;
                }
                if (rc == 0) {
                    fprintf(stderr, "[liboh_android_runtime]   libhwui:register_%s OK\n", r.name);
                    hwui_ok++;
                } else {
                    fprintf(stderr, "[liboh_android_runtime]   libhwui:register_%s rc=%d\n", r.name, rc);
                    hwui_fail++;
                }
            }
            fprintf(stderr, "[liboh_android_runtime] libhwui register: %d ok / %d fail\n",
                    hwui_ok, hwui_fail);
        } else {
            fprintf(stderr, "[liboh_android_runtime] WARN: libhwui not loaded — graphics natives unbound\n");
        }

        // G2.4 (2026-04-30): apply graphics JNI compat shim — last-wins
        // RegisterNatives that override libhwui impls known to abort
        // (e.g., ImageDecoder.nGetColorSpace fid==null) and fill in
        // framework JNI methods we don't have a real libandroid_runtime
        // for (Surface / BLASTBufferQueue / DisplayEventReceiver).
        // MUST run AFTER libhwui's register loop above so our overrides win.
        register_android_graphics_compat_shim(env);

        env->PopLocalFrame(nullptr);
    }

    // WESTLAKE §404: repair java.lang.invoke statics AFTER libjavacore's
    // register_java_lang_invoke_* natives are bound (they run in its JNI_OnLoad above) and after
    // every module is registered, but still before any app code runs.
    wl_repair_invoke_classes(env);
    wl_install_exit_guard(env);   // WESTLAKE §412 — last-wins override of Runtime.nativeExit
    wl_install_proxy_props(env);  // WESTLAKE §413 — outbound HTTP(S) via the hdc reverse tunnel
    wl_register_keyevent_natives(env);  // WESTLAKE §414 — KeyEvent.nativeNextId (BACK key)
    wl_seed_service_manager_cache(env); // WESTLAKE §416 — non-null UserManager for the Settings page
    wl_register_dns_native(env);        // WESTLAKE §423 — DNS: libcore.io.Linux.android_getaddrinfo
    wl_repair_os_constants(env);        // WESTLAKE §425 — OsConstants statics were all zero
    wl_register_tls_natives(env);       // WESTLAKE §441 — real TLS over OHOS OpenSSL
    wl_register_probe_log(env);         // WESTLAKE §450 — native log sink for app code
    wl_register_audiopolicy_natives(env); // WESTLAKE §468 — unblock AudioAttributes/<clinit>
    wl_install_media_session(env);      // WESTLAKE §467 — media_session for MediaSession
    wl_install_shortcut_service(env);   // WESTLAKE §454 — real IShortcutService (Presets page)
    wl_install_ams_bind(env);           // WESTLAKE §458 — bindService -> InProcessServiceBinder
    wl_install_audio_focus(env);        // WESTLAKE §459 — requestAudioFocus -> GRANTED
    wl_repair_string_statics(env);      // WESTLAKE §444 — String.CASE_INSENSITIVE_ORDER was null
    wl_register_charset_natives(env);   // WESTLAKE §451 — real ICU charsets over libart's Latin-1 stub
    wl_register_regex_natives(env);     // WESTLAKE §443 — real ICU regex over libart's stub
    wl_probe_regex(env);                // WESTLAKE §442 — verify the regex engine

    fprintf(stderr, "[liboh_android_runtime] startReg exiting (ok)\n");
    return 0;
}

}  // namespace android
