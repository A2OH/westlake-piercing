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
struct PanelStatusInfo { char _opaque[64]; };
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
    void SendKeyboardStatus(const OHOS::MiscServices::KeyboardStatus &) override {}
    void SendFunctionKey(const OHOS::MiscServices::FunctionKey &functionKey) override {
        int32_t ekt = static_cast<int32_t>(functionKey.GetEnterKeyType());
        IMBI("SendFunctionKey enterKeyType=%d", ekt);
        CallJavaStaticInt("nativeOnEnterAction", ekt);
    }
    void SetKeyboardStatus(bool) override {}
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
    return s;
}

extern "C" int oh_ime_hide() {
    std::lock_guard<std::mutex> lk(g_mutex);
    auto imc = OHOS::MiscServices::InputMethodController::GetInstance();
    if (imc == nullptr) return -1;
    int32_t r = imc->HideSoftKeyboard();
    IMBI("HideSoftKeyboard rc=%d", r);
    return r;
}
