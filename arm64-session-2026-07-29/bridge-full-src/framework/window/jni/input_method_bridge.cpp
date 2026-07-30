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

#include <cstdio>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>

#define IMB_TAG "OH_IMEBridge"
// Log to BOTH hilog and stderr so the chain is visible in the per-child stderr
// (hilog tag filtering is unreliable on this build).
#define IMBI(...) do { __android_log_print(ANDROID_LOG_INFO,  IMB_TAG, __VA_ARGS__); \
    fprintf(stderr, "[OH_IMEBridge] " __VA_ARGS__); fprintf(stderr, "\n"); fflush(stderr); } while (0)
#define IMBE(...) do { __android_log_print(ANDROID_LOG_ERROR, IMB_TAG, __VA_ARGS__); \
    fprintf(stderr, "[OH_IMEBridge][E] " __VA_ARGS__); fprintf(stderr, "\n"); fflush(stderr); } while (0)

// 2026-06-26 IME-focus fix — defined in oh_window_manager_client.cpp (same .so).
// Re-asserts OH WMS input focus on the foreground main (Activity) window so the
// IMSA IsFocused gate passes and OnFocused(appPid) is a no-op (see that file).
extern "C" int32_t oh_wm_request_focus_current();
// Tell the window-manager client the IME is active so it won't RemoveWindow the
// foreground activity on the SearchView/IME-resize relayout (the real teardown
// cause: hidden window -> lost focus -> keyboard hidden).
extern "C" void oh_wm_set_ime_active(int active);

namespace {

JavaVM* g_jvm = nullptr;
std::mutex g_mutex;

typedef int  (*oh_ime_show_fn)();
typedef int  (*oh_ime_hide_fn)();
typedef void (*oh_ime_set_vm_fn)(JavaVM*);
typedef void (*oh_ime_hidden_cb_fn)();
typedef void (*oh_ime_set_hidden_cb_fn)(oh_ime_hidden_cb_fn);

void* g_imeHelper = nullptr;
oh_ime_show_fn g_imeShow = nullptr;
oh_ime_hide_fn g_imeHide = nullptr;

// True between a show request and the keyboard being hidden — the keyboard is
// "wanted"/up.  Drives whether the hideWindow KEEP guard holds the activity
// window.  Cleared on an explicit hide (Android hideSoftInput), on the OHOS
// keyboard reporting it hid (de-stick callback), and by the hard timeout.
std::atomic<bool> g_imeWanted{false};
// Generation counter: each show bumps it; the one-shot focus re-assert and the
// auto-hide-timeout capture the value at launch and bail if superseded.
std::atomic<uint32_t> g_imeGen{0};
// Wall-clock (ms, steady) of the last show request — used to debounce a
// spurious HIDE arriving immediately after a show (the show/hide race), so we
// don't tear our own keyboard down a few ms after summoning it.
std::atomic<int64_t> g_lastShowMs{0};

static int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Clear all IME-up state and release the hideWindow KEEP guard. Idempotent.
void ClearImeState(const char* why) {
    bool was = g_imeWanted.exchange(false);
    ++g_imeGen;                  // supersede any in-flight timeout / re-assert
    oh_wm_set_ime_active(0);     // let the activity window hide/relayout again
    if (was) IMBI("ClearImeState(%s): IME no longer active", why);
}

// 2026-06-26 IME de-stick — the OHOS keyboard told us it hid (user dismissed it
// via back / the keyboard's own down-arrow, or the IMSA tore it down).  Clear
// our IME-up state so the KEEP guard releases and the keyboard can never be
// stuck up.  Registered with the helper via oh_ime_set_hidden_cb.  Debounced:
// ignore a HIDE within 500ms of a show (that's the show/hide race, not a real
// user dismiss).
void OnKeyboardHidden() {
    if (!g_imeWanted.load()) return;     // already down — nothing to do
    int64_t since = NowMs() - g_lastShowMs.load();
    if (since >= 0 && since < 500) {
        IMBI("OnKeyboardHidden: ignoring HIDE %lldms after show (race debounce)",
             (long long)since);
        return;
    }
    ClearImeState("keyboard-hidden-cb");
}

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
    // Register the de-stick "keyboard hidden" callback so an OHOS-level dismiss
    // clears our IME-up state. (No-op if the deployed helper predates it.)
    auto setHiddenCb = reinterpret_cast<oh_ime_set_hidden_cb_fn>(
        dlsym(g_imeHelper, "oh_ime_set_hidden_cb"));
    if (setHiddenCb) setHiddenCb(&OnKeyboardHidden);
    else IMBE("EnsureImeHelper: oh_ime_set_hidden_cb absent (old helper) — "
              "de-stick relies on hideSoftInput + timeout only");
    IMBI("EnsureImeHelper: liboh_ime_helper.so loaded OK");
    return true;
}

// One show attempt with a fresh focus re-assert first.  Caller holds g_mutex.
int DoShowWithFocus(const char* why) {
    int fr = oh_wm_request_focus_current();
    int s = g_imeShow();
    IMBI("show[%s]: requestFocus rc=%d, helper showrc=%d", why, fr, s);
    return s;
}

// 2026-06-26 IME de-stick — ONE short delayed focus-reassert + re-show.  The
// keep-window guard (oh_window_manager_client.cpp) keeps the activity window
// shown so it retains WMS focus, which is what actually keeps the keyboard up;
// this single pass only covers the brief initial race where SceneBoard's
// EntryView momentarily re-grabs focus right after the first show.  Unlike the
// reverted multi-pass watchdog this fires AT MOST ONCE and never re-shows after
// that, so it cannot stick the keyboard up.  Gated by g_imeWanted + generation.
void StartSingleReassert(uint32_t myGen) {
    std::thread([myGen]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        if (!g_imeWanted.load() || g_imeGen.load() != myGen) return;
        std::lock_guard<std::mutex> lk(g_mutex);
        if (!g_imeWanted.load() || g_imeGen.load() != myGen || !g_imeShow) return;
        DoShowWithFocus("reassert-once");
    }).detach();
}

// 2026-06-26 IME de-stick — hard auto-hide backstop.  If nothing clears the
// IME-up state within the timeout (Android never called hideSoftInput AND the
// OHOS keyboard never reported a hide), force a hide so the keyboard can NEVER
// be permanently stuck on the grid and the KEEP guard always releases.  Each
// show resets it via the generation; a real hide supersedes it.
void StartAutoHideTimeout(uint32_t myGen) {
    std::thread([myGen]() {
        // Generous: this is only a last-resort safety net. The normal de-stick
        // is the keyboard-hidden callback (BACK / dismiss) + Android
        // hideSoftInput, both of which clear state and supersede this via the
        // generation. 20s is long enough not to bite a demo's type-a-few-chars
        // flow, short enough that a truly stuck keyboard self-clears.
        std::this_thread::sleep_for(std::chrono::milliseconds(20000));
        if (!g_imeWanted.load() || g_imeGen.load() != myGen) return;
        std::lock_guard<std::mutex> lk(g_mutex);
        if (!g_imeWanted.load() || g_imeGen.load() != myGen) return;
        IMBI("auto-hide timeout fired (gen=%u) — forcing keyboard hide", myGen);
        if (g_imeHide) g_imeHide();
        ClearImeState("auto-hide-timeout");
    }).detach();
}

jboolean IMB_nativeShowKeyboard(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!EnsureImeHelper()) return JNI_FALSE;
    g_imeWanted.store(true);
    g_lastShowMs.store(NowMs());
    oh_wm_set_ime_active(1);   // stop relayout from hiding the activity window
    uint32_t gen = ++g_imeGen;
    int s = DoShowWithFocus("initial");
    IMBI("nativeShowKeyboard -> helper rc=%d (gen=%u)", s, gen);
    StartSingleReassert(gen);
    StartAutoHideTimeout(gen);
    // Return TRUE even if the first show races (rc!=0): the single re-assert
    // makes it stick, and returning TRUE keeps the Android IMM bookkeeping
    // consistent (the field stays in the "IME shown" state).
    return JNI_TRUE;
}

jboolean IMB_nativeHideKeyboard(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lk(g_mutex);
    ClearImeState("nativeHideKeyboard");   // clears g_imeWanted + ime-active + bumps gen
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
