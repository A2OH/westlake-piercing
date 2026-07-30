/*
 * oh_ime_helper.cpp  ->  liboh_ime_helper.so
 *
 * The OHOS-inputMethod-touching half of the IME bridge, isolated into its own
 * .so so that libinputmethod_client.z.so (and its load-time INIT_ARRAY +
 * libclang_rt.ubsan_minimal.so transitive chain) is ONLY loaded when the
 * keyboard is first summoned — in a forked app process — NOT pulled into the
 * appspawn-x prefork process (which aborts libart at .so load:
 * "primitive.h: Primitive char conversion on invalid type").
 *
 * liboh_adapter_bridge.so dlopen()s this lazily on first ShowKeyboard and calls
 * the C-ABI entry points below. This file:
 *   - subclasses OHOS::MiscServices::OnTextChangedListener (22-virtual ABI,
 *     matching the deployed libinputmethod_client.z.so — see ABI NOTE),
 *   - Attaches it to InputMethodController + Show/Hide the soft keyboard,
 *   - routes the keyboard's text events back to Java OhImeBridge.nativeOn*
 *     (which commit into the focused Android InputConnection on the UI thread).
 *
 * ABI NOTE: identical to input_method_bridge.cpp's former local decls. The
 * deployed .so's InputMethodController::Attach/ShowSoftKeyboard take a trailing
 * ClientType (INNER_KIT=3) and OnTextChangedListener has 22 virtuals (the old
 * v3.1 on-box header has only 16 — 6 appended: ReceivePrivateCommand,
 * SetPreviewText, FinishTextPreview, OnDetach, IsFromTs, GetEventHandler;
 * GetEventHandler() is called on every text event). RefBase/sptr from real
 * c_utils refbase.h.
 */

#include <jni.h>
#include <android/log.h>

#include <string>
#include <mutex>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <variant>

#include "refbase.h"
#include "event_handler.h"
#include "event_runner.h"

#include <cstdio>
#define IMB_TAG "OH_IMEHelper"
// Log to BOTH hilog and stderr — stderr reliably surfaces via AppSpawnXJava
// in the per-child stderr/hilog, hilog tag filtering has been unreliable.
#define IMBI(...) do { __android_log_print(ANDROID_LOG_INFO,  IMB_TAG, __VA_ARGS__); \
    fprintf(stderr, "[OH_IMEHelper] " __VA_ARGS__); fprintf(stderr, "\n"); fflush(stderr); } while (0)
#define IMBE(...) do { __android_log_print(ANDROID_LOG_ERROR, IMB_TAG, __VA_ARGS__); \
    fprintf(stderr, "[OH_IMEHelper][E] " __VA_ARGS__); fprintf(stderr, "\n"); fflush(stderr); } while (0)

// ===========================================================================
// ABI declarations matching the DEPLOYED libinputmethod_client.z.so
// ===========================================================================
namespace OHOS {
namespace MiscServices {

enum class ClientType : uint32_t {
    CAPI = 0, JS = 1, CJ = 2, INNER_KIT = 3, INNER_KIT_ARKUI = 4, CLIENT_TYPE_END = 5,
};

enum class EnterKeyType : int32_t {
    UNSPECIFIED = 0, NONE = 1, GO = 2, SEARCH = 3, SEND = 4,
    NEXT = 5, DONE = 6, PREVIOUS = 7, NEW_LINE = 8,
};
class FunctionKey {
public:
    EnterKeyType GetEnterKeyType() const { return enterKeyType_; }
    EnterKeyType enterKeyType_ = EnterKeyType::UNSPECIFIED;
};
enum class KeyboardStatus : int32_t { NONE = 0, HIDE = 1, SHOW = 2 };
enum class Direction : int32_t { NONE = 0, UP = 1, DOWN = 2, LEFT = 3, RIGHT = 4 };
struct KeyEvent { char _opaque[256]; };
// PanelStatusInfo layout matches base/inputmethod/imf input_method_utils.h:
//   struct PanelStatusInfo { PanelInfo panelInfo; bool visible; Trigger trigger; }
//   PanelInfo { PanelType panelType; PanelFlag panelFlag; }  (two int32 enums)
//   enum Trigger : int32_t { IME_APP, IMF, END };
// We only read .visible, but lay the whole thing out so the field offset is
// correct (over-sized tail padding is harmless — the caller constructs it).
enum class PanelType : int32_t { SOFT_KEYBOARD = 0, STATUS_BAR = 1 };
enum class PanelFlag : int32_t { FLG_FIXED = 0, FLG_FLOATING = 1, FLG_CANDIDATE_COLUMN = 2 };
enum Trigger : int32_t { IME_APP, IMF, END };
struct PanelInfo { PanelType panelType = PanelType::SOFT_KEYBOARD; PanelFlag panelFlag = PanelFlag::FLG_FIXED; };
struct PanelStatusInfo {
    PanelInfo panelInfo;
    bool visible = false;
    Trigger trigger = END;
};
struct Range { int32_t start = 0; int32_t end = 0; };

// OnTextChangedListener — EXACT 22-virtual layout (load-bearing order).
class OnTextChangedListener : public virtual RefBase {
public:
    OnTextChangedListener() = default;
    virtual ~OnTextChangedListener() = default;
    virtual void InsertText(const std::u16string &text) = 0;                 // 2
    virtual void DeleteForward(int32_t length) = 0;                          // 3
    virtual void DeleteBackward(int32_t length) = 0;                         // 4
    virtual void SendKeyEventFromInputMethod(const KeyEvent &event) = 0;     // 5
    virtual void SendKeyboardStatus(const KeyboardStatus &status) = 0;       // 6
    virtual void NotifyPanelStatusInfo(const PanelStatusInfo &info) {}       // 7
    virtual void NotifyKeyboardHeight(uint32_t height) {}                    // 8
    virtual void SendFunctionKey(const FunctionKey &functionKey) = 0;        // 9
    virtual void SetKeyboardStatus(bool status) = 0;                         // 10
    virtual void MoveCursor(const Direction direction) = 0;                  // 11
    virtual void HandleSetSelection(int32_t start, int32_t end) = 0;         // 12
    virtual void HandleExtendAction(int32_t action) = 0;                     // 13
    virtual void HandleSelect(int32_t keyCode, int32_t cursorMoveSkip) = 0;  // 14
    virtual std::u16string GetLeftTextOfCursor(int32_t number) = 0;          // 15
    virtual std::u16string GetRightTextOfCursor(int32_t number) = 0;         // 16
    virtual int32_t GetTextIndexAtCursor() = 0;                             // 17
    virtual int32_t ReceivePrivateCommand(
        const std::unordered_map<std::string,
            std::variant<std::string, bool, int32_t>> &privateCommand) {     // 18
        return 0;
    }
    virtual int32_t SetPreviewText(const std::u16string &text, const Range &range) { return 0; } // 19
    virtual void FinishTextPreview() {}                                       // 20
    virtual void OnDetach() {}                                                // 21
    virtual bool IsFromTs() { return false; }                                // 22
    virtual std::shared_ptr<OHOS::AppExecFwk::EventHandler> GetEventHandler() { return nullptr; } // 23
};

// InputMethodController — methods we call (mangled symbols resolved from
// libinputmethod_client.z.so, which IS a DT_NEEDED of THIS helper .so).
class InputMethodController : public RefBase {
public:
    static sptr<InputMethodController> GetInstance();
    int32_t Attach(sptr<OnTextChangedListener> listener, bool isShowKeyboard,
                   ClientType type = ClientType::INNER_KIT);
    int32_t ShowSoftKeyboard(ClientType type = ClientType::INNER_KIT);
    int32_t HideSoftKeyboard();
    int32_t Close();
};

} // namespace MiscServices
} // namespace OHOS

// ===========================================================================
// Helper state
// ===========================================================================
namespace {

JavaVM* g_jvm = nullptr;
std::mutex g_mutex;
OHOS::sptr<OHOS::MiscServices::OnTextChangedListener> g_listener;
std::atomic<bool> g_attached{false};

// 2026-06-26 IME de-stick: the bridge (input_method_bridge.cpp) registers a
// callback here so that when the OHOS keyboard reports it has HIDDEN (the user
// dismissed it — back/down-arrow at the OHOS level, or the panel was torn down),
// the bridge clears g_imeActive/g_imeWanted and releases the hideWindow KEEP
// guard.  Without this, a keyboard the bridge "wanted" up could never learn it
// was dismissed (Android's IMM may not route hideSoftInput through the NO_IME
// stub) → stuck keyboard on the grid.  Stored as a plain function pointer (set
// once, before any keyboard event), read lock-free.
typedef void (*oh_ime_hidden_cb_fn)();
std::atomic<oh_ime_hidden_cb_fn> g_hiddenCb{nullptr};

void FireHiddenCb(const char* why) {
    oh_ime_hidden_cb_fn cb = g_hiddenCb.load();
    IMBI("keyboard HIDDEN (%s) -> %s", why, cb ? "notify bridge" : "no cb");
    // Our own bookkeeping: the controller is no longer the bound shown client.
    g_attached.store(false);
    if (cb) cb();
}

std::shared_ptr<OHOS::AppExecFwk::EventRunner> g_runner;
std::shared_ptr<OHOS::AppExecFwk::EventHandler> g_handler;

std::shared_ptr<OHOS::AppExecFwk::EventHandler> EnsureHandler() {
    if (!g_handler) {
        g_runner = OHOS::AppExecFwk::EventRunner::Create("adapter-ime-text");
        if (g_runner) g_handler = std::make_shared<OHOS::AppExecFwk::EventHandler>(g_runner);
    }
    return g_handler;
}

struct ScopedEnv {
    JNIEnv* env = nullptr;
    bool needDetach = false;
    ScopedEnv() {
        if (!g_jvm) return;
        if (g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
            JavaVMAttachArgs args{JNI_VERSION_1_6, "oh-ime-text", nullptr};
            if (g_jvm->AttachCurrentThread(&env, &args) == JNI_OK) needDetach = true;
            else env = nullptr;
        }
    }
    ~ScopedEnv() { if (needDetach && g_jvm) g_jvm->DetachCurrentThread(); }
};

void CallJavaCommitText(const std::u16string& text) {
    ScopedEnv se;
    if (!se.env) { IMBE("commit: no JNIEnv"); return; }
    JNIEnv* env = se.env;
    jclass cls = env->FindClass("adapter/window/OhImeBridge");
    if (!cls || env->ExceptionCheck()) { if (env->ExceptionCheck()) env->ExceptionClear(); IMBE("commit: FindClass"); return; }
    jmethodID m = env->GetStaticMethodID(cls, "nativeOnInsertText", "(Ljava/lang/String;)V");
    if (!m || env->ExceptionCheck()) { if (env->ExceptionCheck()) env->ExceptionClear(); IMBE("commit: methodID"); return; }
    jstring js = env->NewString(reinterpret_cast<const jchar*>(text.data()), static_cast<jsize>(text.size()));
    env->CallStaticVoidMethod(cls, m, js);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
    env->DeleteLocalRef(js);
    env->DeleteLocalRef(cls);
}

void CallJavaStaticInt(const char* name, jint arg) {
    ScopedEnv se;
    if (!se.env) { IMBE("staticInt(%s): no JNIEnv", name); return; }
    JNIEnv* env = se.env;
    jclass cls = env->FindClass("adapter/window/OhImeBridge");
    if (!cls || env->ExceptionCheck()) { if (env->ExceptionCheck()) env->ExceptionClear(); return; }
    jmethodID m = env->GetStaticMethodID(cls, name, "(I)V");
    if (!m || env->ExceptionCheck()) { if (env->ExceptionCheck()) env->ExceptionClear(); IMBE("%s: methodID", name); return; }
    env->CallStaticVoidMethod(cls, m, arg);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
    env->DeleteLocalRef(cls);
}

class AdapterTextListener : public OHOS::MiscServices::OnTextChangedListener {
public:
    void InsertText(const std::u16string &text) override {
        IMBI("InsertText len=%zu", text.size());
        CallJavaCommitText(text);
    }
    void DeleteForward(int32_t length) override {
        IMBI("DeleteForward len=%d", length);
        CallJavaStaticInt("nativeOnDeleteBefore", length > 0 ? length : 1);
    }
    void DeleteBackward(int32_t length) override {
        IMBI("DeleteBackward len=%d", length);
        CallJavaStaticInt("nativeOnDeleteAfter", length > 0 ? length : 1);
    }
    void SendKeyEventFromInputMethod(const OHOS::MiscServices::KeyEvent &) override {}
    // 2026-06-26 IME de-stick: the OHOS IMSA reports keyboard show/hide here.
    // HIDE means the keyboard was dismissed (user back/down-arrow at OHOS level,
    // or IMSA tore it down) — tell the bridge so it clears IME-active and stops
    // holding the activity window shown (otherwise: stuck keyboard).
    void SendKeyboardStatus(const OHOS::MiscServices::KeyboardStatus &status) override {
        if (status == OHOS::MiscServices::KeyboardStatus::HIDE) {
            FireHiddenCb("SendKeyboardStatus=HIDE");
        } else {
            IMBI("SendKeyboardStatus=%d (not HIDE)", static_cast<int>(status));
        }
    }
    // Panel status carries an explicit visible flag; visible==false is a hide.
    void NotifyPanelStatusInfo(const OHOS::MiscServices::PanelStatusInfo &info) override {
        if (!info.visible) {
            FireHiddenCb("PanelStatusInfo visible=false");
        }
    }
    void SetKeyboardStatus(bool status) override {
        if (!status) FireHiddenCb("SetKeyboardStatus=false");
    }
    void SendFunctionKey(const OHOS::MiscServices::FunctionKey &functionKey) override {
        int32_t ekt = static_cast<int32_t>(functionKey.GetEnterKeyType());
        IMBI("SendFunctionKey enterKeyType=%d", ekt);
        CallJavaStaticInt("nativeOnEnterAction", ekt);
    }
    void MoveCursor(const OHOS::MiscServices::Direction) override {}
    void HandleSetSelection(int32_t, int32_t) override {}
    void HandleExtendAction(int32_t) override {}
    void HandleSelect(int32_t, int32_t) override {}
    std::u16string GetLeftTextOfCursor(int32_t) override { return std::u16string(); }
    std::u16string GetRightTextOfCursor(int32_t) override { return std::u16string(); }
    int32_t GetTextIndexAtCursor() override { return 0; }
    std::shared_ptr<OHOS::AppExecFwk::EventHandler> GetEventHandler() override {
        std::lock_guard<std::mutex> lk(g_mutex);
        return EnsureHandler();
    }
};

} // namespace

// ===========================================================================
// C-ABI entry points (dlsym'd by liboh_adapter_bridge.so)
// ===========================================================================
extern "C" void oh_ime_set_vm(JavaVM* vm) { g_jvm = vm; }

// 2026-06-26 IME de-stick: register the bridge's "keyboard hidden" callback.
// Invoked from SendKeyboardStatus(HIDE)/NotifyPanelStatusInfo(visible=false)
// so the bridge clears IME-active + releases the hideWindow KEEP guard when the
// OHOS keyboard is dismissed.
extern "C" void oh_ime_set_hidden_cb(oh_ime_hidden_cb_fn cb) {
    g_hiddenCb.store(cb);
    IMBI("oh_ime_set_hidden_cb: %s", cb ? "registered" : "cleared");
}

extern "C" int oh_ime_show() {
    std::lock_guard<std::mutex> lk(g_mutex);
    auto imc = OHOS::MiscServices::InputMethodController::GetInstance();
    if (imc == nullptr) { IMBE("show: IMC GetInstance null"); return -1; }
    if (!g_attached.load()) {
        if (g_listener == nullptr) {
            g_listener = OHOS::sptr<OHOS::MiscServices::OnTextChangedListener>(new AdapterTextListener());
        }
        int32_t r = imc->Attach(g_listener, /*isShowKeyboard=*/true,
                                OHOS::MiscServices::ClientType::INNER_KIT);
        IMBI("Attach rc=%d", r);
        if (r == 0) g_attached.store(true);
    }
    int32_t s = imc->ShowSoftKeyboard(OHOS::MiscServices::ClientType::INNER_KIT);
    IMBI("ShowSoftKeyboard rc=%d", s);
    // 2026-06-26 IME-focus fix: a failed show means the IMSA rejected us
    // (ERROR_CLIENT_NOT_FOCUSED) and/or tore the client down (OnFocused ->
    // RemoveClient -> OnInputStop unbinds the controller).  Drop g_attached so
    // the next oh_ime_show() (after a fresh WMS focus re-assert) re-Attaches as
    // the bound client before re-showing — ShowSoftKeyboard alone on an unbound
    // controller would keep failing.
    if (s != 0) g_attached.store(false);
    return s;
}

extern "C" int oh_ime_hide() {
    std::lock_guard<std::mutex> lk(g_mutex);
    auto imc = OHOS::MiscServices::InputMethodController::GetInstance();
    if (imc == nullptr) return -1;
    // Try the polite hide first (keeps the binding).
    int32_t r = imc->HideSoftKeyboard();
    IMBI("HideSoftKeyboard rc=%d", r);
    // 2026-06-26 IME de-stick: on this board ShowSoftKeyboard/HideSoftKeyboard
    // return ERROR_CLIENT_NOT_FOCUSED (rc=61) — the IMSA's per-client focus
    // check rejects them even though our app window holds WMS focus, so the
    // keyboard (summoned via Attach(isShowKeyboard=true)) won't go away on a
    // plain HideSoftKeyboard.  Close() is a teardown op (hide + clear listener +
    // unbind IMSA) that does NOT depend on being the focused client, so it
    // reliably dismisses the keyboard.  After Close() we must re-Attach on the
    // next show — drop g_attached so oh_ime_show() re-binds.
    if (r != 0) {
        int32_t c = imc->Close();
        IMBI("HideSoftKeyboard failed (rc=%d) -> Close() rc=%d", r, c);
        g_attached.store(false);
        if (c == 0) r = 0;     // Close succeeded: report hide success
    }
    return r;
}
