/*
 * input_method_bridge.cpp
 *
 * Android IMM -> OHOS inputMethod bridge (loader half, lives in
 * liboh_adapter_bridge.so).
 *
 * Registers the two JNI natives that adapter.window.OhImeBridge declares:
 *     private static native boolean nativeShowKeyboard();
 *     private static native boolean nativeHideKeyboard();
 * (OhImeBridge is called by adapter.window.InputMethodManagerAdapter — the
 * registered "input_method" IInputMethodManager$Stub — on showSoftInput /
 * startInputOrWindowGainedFocus / hideSoftInput.)
 *
 * The actual InputMethodController calls + the OnTextChangedListener that
 * receives the keyboard's text events live in a SEPARATE .so,
 * liboh_ime_helper.so, which links libinputmethod_client.z.so. We dlopen that
 * helper LAZILY on the first ShowKeyboard — in a forked app process — and call
 * its C-ABI entry points (oh_ime_show / oh_ime_hide / oh_ime_set_vm).
 *
 * WHY the split: liboh_adapter_bridge.so is loaded by appspawn-x in the PREFORK
 * process. libinputmethod_client.z.so has a load-time INIT_ARRAY and pulls in
 * libclang_rt.ubsan_minimal.so + libinputmethod_common/imf_hisysevent. Making
 * it a DT_NEEDED of the bridge forces all of that into the prefork process,
 * where it aborts libart at .so load:
 *   FATAL appspawn-x [primitive.h] Primitive char conversion on invalid type
 * Loading it lazily (dlopen) from a forked app process avoids the prefork
 * pollution entirely.
 */

#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>

#include <mutex>

#define IMB_TAG "OH_IMEBridge"
#define IMBI(...) __android_log_print(ANDROID_LOG_INFO,  IMB_TAG, __VA_ARGS__)
#define IMBE(...) __android_log_print(ANDROID_LOG_ERROR, IMB_TAG, __VA_ARGS__)

namespace {

JavaVM* g_jvm = nullptr;
std::mutex g_mutex;

typedef int  (*oh_ime_show_fn)();
typedef int  (*oh_ime_hide_fn)();
typedef void (*oh_ime_set_vm_fn)(JavaVM*);

void* g_imeHelper = nullptr;
oh_ime_show_fn g_imeShow = nullptr;
oh_ime_hide_fn g_imeHide = nullptr;

// Lazily dlopen liboh_ime_helper.so (which links libinputmethod_client.z.so)
// and resolve its C-ABI entry points. Runs in the forked app process on first
// keyboard request — never in the appspawn-x prefork.
bool EnsureImeHelper() {
    if (g_imeHelper) return (g_imeShow != nullptr && g_imeHide != nullptr);
    g_imeHelper = dlopen("liboh_ime_helper.so", RTLD_NOW | RTLD_GLOBAL);
    if (!g_imeHelper) {
        IMBE("EnsureImeHelper: dlopen(liboh_ime_helper.so) failed: %s", dlerror());
        return false;
    }
    auto setVm = reinterpret_cast<oh_ime_set_vm_fn>(dlsym(g_imeHelper, "oh_ime_set_vm"));
    g_imeShow  = reinterpret_cast<oh_ime_show_fn>(dlsym(g_imeHelper, "oh_ime_show"));
    g_imeHide  = reinterpret_cast<oh_ime_hide_fn>(dlsym(g_imeHelper, "oh_ime_hide"));
    if (!g_imeShow || !g_imeHide) {
        IMBE("EnsureImeHelper: dlsym failed (show=%p hide=%p)",
             reinterpret_cast<void*>(g_imeShow), reinterpret_cast<void*>(g_imeHide));
        return false;
    }
    if (setVm && g_jvm) setVm(g_jvm);
    IMBI("EnsureImeHelper: liboh_ime_helper.so loaded OK");
    return true;
}

jboolean IMB_nativeShowKeyboard(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!EnsureImeHelper()) return JNI_FALSE;
    int s = g_imeShow();
    IMBI("nativeShowKeyboard -> helper rc=%d", s);
    return (s == 0) ? JNI_TRUE : JNI_FALSE;
}

jboolean IMB_nativeHideKeyboard(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!EnsureImeHelper()) return JNI_FALSE;
    int r = g_imeHide();
    IMBI("nativeHideKeyboard -> helper rc=%d", r);
    return (r == 0) ? JNI_TRUE : JNI_FALSE;
}

const JNINativeMethod kMethods[] = {
    {"nativeShowKeyboard", "()Z", reinterpret_cast<void*>(IMB_nativeShowKeyboard)},
    {"nativeHideKeyboard", "()Z", reinterpret_cast<void*>(IMB_nativeHideKeyboard)},
};

} // namespace

// Called from adapter_bridge.cpp JNI_OnLoad. Plain C++ linkage (NOT extern "C")
// to match the `extern int register_InputMethodBridge(JNIEnv*);` declaration in
// adapter_bridge.cpp — mirroring register_InputEventBridge etc. (extern "C"
// here would emit an unmangled symbol the mangled caller can't resolve →
// MUSL-LDSO relocation failure at load).
int register_InputMethodBridge(JNIEnv* env) {
    IMBI("register_InputMethodBridge: ENTER");
    if (g_jvm == nullptr) {
        env->GetJavaVM(&g_jvm);
    }
    IMBI("register_InputMethodBridge: before FindClass(OhImeBridge)");
    jclass cls = env->FindClass("adapter/window/OhImeBridge");
    if (!cls || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        IMBE("register_InputMethodBridge: FindClass(OhImeBridge) null");
        return -1;
    }
    IMBI("register_InputMethodBridge: FindClass OK, before RegisterNatives");
    jint rc = env->RegisterNatives(cls, kMethods, sizeof(kMethods) / sizeof(kMethods[0]));
    IMBI("register_InputMethodBridge: RegisterNatives returned rc=%d", (int)rc);
    if (rc != JNI_OK) {
        if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
        IMBE("register_InputMethodBridge: RegisterNatives failed rc=%d", (int)rc);
        env->DeleteLocalRef(cls);
        return -1;
    }
    IMBI("register_InputMethodBridge: OK (%zu natives)", sizeof(kMethods)/sizeof(kMethods[0]));
    env->DeleteLocalRef(cls);
    return 0;
}
