/*
 * oh_window_manager_client.cpp
 *
 * OpenHarmony WindowManager / SceneSessionManager IPC client implementation.
 *
 * Connects to OH SceneSessionManager (scene-based WMS) and provides window
 * session lifecycle management. Each Android window maps to an OH session
 * backed by an RSSurfaceNode for rendering.
 *
 * IPC flow for Hello World window display:
 *   1. createSession() -> SSM.CreateAndConnectSpecificSession()
 *      - Creates an OH scene session via ISceneSessionManager (system singleton)
 *      - Registers SessionStageAdapter as ISessionStage callback
 *      - Returns ISession proxy (per-window) + RSSurfaceNode ID
 *   2. updateSessionRect() -> ISession.UpdateSessionRect()
 *      - Sets window position and size via per-window ISession proxy
 *   3. notifyDrawingCompleted() -> ISession.DrawingCompleted()
 *      - Tells OH compositor the window is ready to display
 *   4. destroySession() -> ISession.Disconnect() + SSM.DestroyAndDisconnectSpecificSession()
 *      - Disconnects per-window session, then notifies SSM to clean up
 *
 * Reference:
 *   OH: wms/window_scene/session_manager/include/zidl/scene_session_manager_interface.h
 *   OH: wms/window_scene/session/host/include/zidl/session_interface.h
 */
#include "oh_window_manager_client.h"
#include <window.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "session_stage_adapter.h"
#include <dlfcn.h>
#include "window_session_property.h"   // WESTLAKE §225: V7 property
#include "window.h"                    // WESTLAKE §227: OH client Window::Create
#include "window_option.h"
#include "window_callback_adapter.h"
#include "window_event_channel_adapter.h"
#include "oh_input_bridge.h"  // 2026-05-18 §3.3.5 MMI subscription
#include "oh_ability_manager_client.h"  // 2026-06-04 focus heartbeat (moveMissionToFront)
#include <android/log.h>
#include <atomic>
#include <set>
#include <map>
#include <thread>
#include <string>
#include <vector>
#include <algorithm>
#include <unistd.h>

#include "ipc_skeleton.h"
#include "iservice_registry.h"  // OHOS::SystemAbilityManagerClient
#include "system_ability_manager_proxy.h"
#include "window_manager_hilog.h"
#include "ui/rs_surface_node.h"
#include "transaction/rs_interfaces.h"                       // RSSurfaceNode::Create
#include "transaction/rs_transaction.h"
#include <unistd.h>               // RSTransaction::FlushImplicitTransaction
#include "oh_br_trace.h"                              // G2.14ac IPC trace+log macros

// 2026-06-05 #3: track the FOREGROUND (most-recently-shown) MAIN window session,
// so a translucent dialog parents to the actual foreground activity rather than
// the highest-sessionId main (which is wrong for multi-activity apps: A -> B ->
// dialog must parent to B, not A). Updated when a main window is created/shown.
static std::atomic<int32_t> g_fgMainSession{-1};

// 2026-06-26 IME-focus fix: set true while the adapter is summoning the OHOS
// soft keyboard (input_method_bridge.cpp toggles it via oh_wm_set_ime_active).
// While true, hideWindow() refuses to RemoveWindow a MAIN window: the Material
// SearchView (and IME resize) drives a relayout that momentarily signals the
// DecorView non-VISIBLE, which would otherwise RemoveWindow the still-foreground
// activity -> it loses WMS visibility/focus -> SceneBoard's EntryView grabs
// focus -> the IMSA rejects/tears down the keyboard (rc=61). Keeping the main
// window shown lets it retain focus so the keyboard stays up. (Symmetric to the
// existing translucent-sub-window KEEP guard in hideWindow.)
static std::atomic<bool> g_imeActive{false};
extern "C" __attribute__((visibility("default")))
void oh_wm_set_ime_active(int active) { g_imeActive.store(active != 0); }

// 2026-05-02 G2.14r: forward-declare CreateNativeWindowFromSurface from
// graphic_surface/interfaces/inner_api/surface/window.h.  Direct #include is
// ambiguous because OH has 10+ different files named window.h on its include
// path (window_manager/libwm/include/window.h gets picked first by the
// compiler).  The signature is stable: void* pSurface is `OHOS::sptr<OHOS::Surface>*`.
struct NativeWindow;
typedef struct NativeWindow OHNativeWindow;
extern "C" OHNativeWindow* CreateNativeWindowFromSurface(void* pSurface);
// 2026-05-12 G2.14aw probe A.1: read producer uniqueId from OHNativeWindow.
// Defined in libnative_window.so; signature stable per
// graphic_surface/interfaces/inner_api/surface/external_window.h:674.
extern "C" int32_t OH_NativeWindow_GetSurfaceId(OHNativeWindow* window, uint64_t* surfaceId);
// 2026-06-01 G3.4: configure the OH NativeWindow as a GPU render target (format + usage)
// before hwui binds EGL.  From libnative_window.so; external_window.h NativeWindowOperation.
extern "C" int32_t OH_NativeWindow_NativeWindowHandleOpt(OHNativeWindow* window, int code, ...);

// 2026-05-02 G2.14r: file-scope declaration so namespace-internal createSession
// can call it without `extern "C"` block-level decl (which is not allowed in
// function body).  Same symbol is exported below via "C" linkage.
extern "C" void oh_wm_set_last_session(int32_t);

// 2026-05-09 G2.14ae: oh_anw_wrap from oh_anativewindow_shim.cpp wraps a raw
// OH NativeWindow handle into an AOSP-ABI-compatible ANativeWindow struct so
// hwui can use AOSP NDK offsets without crashing on OH's RefBase / virtual
// class layout. See doc/graphics_rendering_design.html §7.11.
extern "C" struct ANativeWindow* oh_anw_wrap(OHNativeWindow* oh);
// G2.14c (2026-05-01) — pivot from SCB-style 3-hop chain to legacy
// IWindowManager. SA 4606 host on this OH 7.0.0.18 build is libwms.z.so's
// WindowManagerService (legacy), NOT libsms.z.so's MockSessionManagerService.
// Confirmed by: (a) /system/profile/foundation.json says SA 4606 -> libwms.z.so
// (b) deployed libwms.z.so contains only WindowManagerService symbols, no
// MockSessionManagerService (c) OH BUILD.gn shows libwms is the legacy WMS
// library when window_manager_use_sceneboard=false (the project default).
// So we use IWindowManager.CreateWindow + AddWindow instead of SCB's
// CreateAndConnectSpecificSession 3-hop chain.
#include "window_manager_interface.h"                 // IWindowManager (legacy)
#include "window_property.h"                          // legacy WindowProperty

#define LOG_TAG "OH_WindowMgrClient"
// B.37 sediment / memory feedback_prefer_inner_api.md: use OH HiLogPrint
// directly. __android_log_print is no-op on OH (the Android log shim isn't
// wired up for adapter .so bridge code). HiLogPrint goes straight to OH
// hilog so logs from this file actually appear.
#include "hilog/log.h"
#define LOGI(fmt, ...) HiLogPrint(LOG_CORE, LOG_INFO,  0xD000F00u, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) HiLogPrint(LOG_CORE, LOG_ERROR, 0xD000F00u, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) HiLogPrint(LOG_CORE, LOG_WARN,  0xD000F00u, LOG_TAG, fmt, ##__VA_ARGS__)

// OH system ability IDs (per system_ability_definition.h:285)
//   4606 = WINDOW_MANAGER_SERVICE_ID — serves BOTH legacy IWindowManager AND
//          ISceneSessionManager (the latter extends the former); this is what
//          OH's own WindowAdapter / SessionManager call (window_adapter.cpp:508,
//          session_manager.cpp:243).
//   4607 = DISPLAY_MANAGER_SERVICE_SA_ID — serves IDisplayManager (NOT WMS).
//
// G2.14 root cause: pre-fix used 4607 → silent iface_cast<ISceneSessionManager>
// on a DisplayManager proxy → CreateAndConnectSpecificSession got "method not
// found" on the wrong server → SendRequest returned non-ERR_NONE. The
// IPCObjectProxy log line "desc:*.IDisplayManager error:1" came from the same
// proxy and was the smoking gun.
static constexpr int32_t SCENE_SESSION_MANAGER_ID = 4606;

namespace oh_adapter {

// §3.1.4.1 — Android LayoutParams.type → OH WindowType.
//
// Android main DecorView (TYPE_BASE_APPLICATION=1 / TYPE_APPLICATION=2 /
// TYPE_APPLICATION_STARTING=3) is downgraded to OH WINDOW_TYPE_APP_SUB_WINDOW
// because OH manages the actual main window through the Ability lifecycle —
// CreateAndConnectSpecificSession is OH's IPC for sub windows / system windows
// only, and would reject a main-window-typed property.
//
// Casting Android's enum value directly to OH WindowType (the pre-fix behavior)
// dropped values 2/3/etc. into OH enum gaps -> WindowSessionProperty
// Marshalling rejected them -> server returned ERR_INVALID_DATA -> proxy logged
// "SendRequest failed". This map is the actual G2.13 fix.
// WESTLAKE §340: find the ABILITY's own main-window persistentId, to parent our sub-windows to it.
//
// §337 made SceneSessionManager accept CreateAndConnectSpecificSession by asking for
// WINDOW_TYPE_APP_SUB_WINDOW (a plain app may not create a MAIN window), and §338 drove the returned
// ISession to Foreground (WS_OK).  But a SUB window is meaningless on its own: SceneBoard lays it out
// relative to its PARENT, so with no parent id the session is created, immediately has rect [0 0 0 0]
// and ZOrd -1, and is dropped from the window list -- which is why RS still computes
// VisibleRegion [Empty] for our node and never acquires the buffers we queue (see §333/§334).
//
// The parent we want is the ArkTS EntryAbility's own window ("noice0" in the WMS dump, which RS shows
// as WindowScene_noice<persistentId>).  §335 proved we CANNOT reach it through libwm's in-process
// statics -- OHOS musl namespace isolation gives our bridge a different copy of windowSessionMap_,
// so Window::Find/GetTopWindowWithContext always come back empty.  IPC crosses that boundary just
// fine, so ask SceneSessionManager instead and filter its answer by our own pid.
//
// TRANS_ID_GET_ALL_MAIN_WINDOW_INFO = 56 (scene_session_manager_interface.h); request carries only the
// interface token, reply is int32 count + that many MainWindowInfo parcelables + int32 errCode.
static int32_t wl_query_ability_main_window_id(const OHOS::sptr<OHOS::IRemoteObject>& proxy)
{
    static int32_t wl_cached = -1;          // -1 = not asked yet, 0 = asked and not found
    if (wl_cached > 0) { return wl_cached; }
    // §340b: SSM's GetAllMainWindowInfo answers count=0 for us (it reports the recents/topN main
    // windows, which our ability is not in), so allow the launcher to inject the id it can read
    // straight out of the WMS dump.  This proves the mechanism; a programmatic query can replace it.
    const char* wl_envParent = getenv("WL_PARENT_ID");
    if (wl_envParent != nullptr && *wl_envParent != '\0') {
        const int32_t wl_pid = atoi(wl_envParent);
        if (wl_pid > 0) {
            wl_cached = wl_pid;
            fprintf(stderr, "[WESTLAKE-WMC] §340b parentId from WL_PARENT_ID=%d\n", wl_pid);
            fflush(stderr);
            return wl_cached;
        }
    }
    if (proxy == nullptr) { return 0; }

    OHOS::MessageParcel d;
    OHOS::MessageParcel r;
    OHOS::MessageOption o;
    if (!d.WriteInterfaceToken(u"OHOS.ISceneSessionManager")) { return 0; }
    const int32_t err = proxy->SendRequest(56, d, r, o);
    if (err != 0) {
        fprintf(stderr, "[WESTLAKE-WMC] §340 GetAllMainWindowInfo err=%d\n", err);
        fflush(stderr);
        return 0;
    }
    const int32_t n = r.ReadInt32();
    const int32_t me = static_cast<int32_t>(getpid());
    int32_t found = 0;
    if (n > 0 && n < 256) {
        for (int32_t i = 0; i < n; ++i) {
            OHOS::sptr<OHOS::Rosen::MainWindowInfo> info =
                r.ReadParcelable<OHOS::Rosen::MainWindowInfo>();
            if (info == nullptr) { break; }
            fprintf(stderr, "[WESTLAKE-WMC] §340   main[%d] pid=%d id=%d bundle=%s\n",
                    i, info->pid_, info->persistentId_, info->bundleName_.c_str());
            // Our own pid wins outright; otherwise remember any window of our bundle (the ability may
            // be hosted by a differently-named process).
            if (info->persistentId_ > 0 &&
                (info->pid_ == me ||
                 (found == 0 && info->bundleName_.find("ashutoshgngwr") != std::string::npos))) {
                found = info->persistentId_;
                if (info->pid_ == me) { break; }
            }
        }
    }
    fprintf(stderr, "[WESTLAKE-WMC] §340 ability main window: count=%d mypid=%d -> parentId=%d\n",
            n, me, found);
    fflush(stderr);
    if (found > 0) { wl_cached = found; }
    return found;
}

static OHOS::Rosen::WindowType mapAndroidWindowType(int32_t androidType) {
    using OHOS::Rosen::WindowType;
    // G2.14c (legacy mode) — main app DecorView maps to APP_MAIN_WINDOW (1).
    // SUB_WINDOW (1001) requires parent windowId via property->SetParentId,
    // which adapter can't supply for a top-level Activity. Legacy WMS's
    // CheckSystemWindowPermission allows APP_MAIN_WINDOW for any caller (it
    // only blocks SystemWindow types 2000+ for non-SA callers).
    // WESTLAKE §337: the comment above is right for LEGACY WMS but WRONG for this SceneBoard board.
    // Traced end-to-end (§336): our session request reaches SceneSessionManager, parses fine (all
    // fields non-null), and then fails inside `CreateAndConnectSpecificSession` with a NULL
    // sceneSession -> the stub returns ERR_INVALID_STATE (=10, the `V7 err=10` we log). The reason is
    // in the API's name: "SpecificSession" creates SUB/SYSTEM windows only — an APP_MAIN_WINDOW is
    // created by the ability lifecycle (AMS/SceneBoard), never by a client call. So asking for
    // APP_MAIN_WINDOW here can never succeed, we get no session, our RSSurfaceNode is therefore not
    // session-backed, RS leaves its VisibleRegion [Empty] and the window paints black (§332-§334).
    // Sub-windows ARE creatable by a normal app (no system permission, unlike 2000+ system types).
    // Env-gated so it is trivially reversible while we measure.
    static const bool wl_sub = (getenv("WL_SUB_WINDOW") != nullptr);
    switch (androidType) {
        case 1:        // TYPE_BASE_APPLICATION
        case 2:        // TYPE_APPLICATION
        case 3:        // TYPE_APPLICATION_STARTING
            // WESTLAKE §365: let the window type be chosen at run time.
            // MAIN can never come from CreateAndConnectSpecificSession (that is the *specific*
            // session path; a main window is created by SCB when AMS starts an ability — with a
            // privileged child it still returns err=10), and SUB means borrowing an app WindowScene
            // whose owner AMS then kills with LIFECYCLE_TIMEOUT (§363), destroying our nodes with it.
            // A SYSTEM window needs no parent and owns its own session.
            // 2106=FLOAT, 2107=TOAST, 2109=PANEL, 1001=APP_SUB_WINDOW.
            {
                const char* wl_wt = getenv("WL_WINDOW_TYPE");
                if (wl_wt != nullptr && *wl_wt != '\0') {
                    return static_cast<WindowType>(atoi(wl_wt));
                }
            }
            return wl_sub ? WindowType::WINDOW_TYPE_APP_SUB_WINDOW
                          : WindowType::WINDOW_TYPE_APP_MAIN_WINDOW;
        // Real sub windows must have parent set explicitly by caller; for
        // now downgrade to MAIN type as well — adapter doesn't yet plumb
        // parent linkage for Android sub windows.
        case 1000:     // TYPE_APPLICATION_PANEL
        case 1001:     // TYPE_APPLICATION_MEDIA
        case 1002:     // TYPE_APPLICATION_SUB_PANEL
        case 1003:     // TYPE_APPLICATION_ATTACHED_DIALOG
            return WindowType::WINDOW_TYPE_APP_MAIN_WINDOW;
        case 2003:     // TYPE_SYSTEM_ALERT
        case 2008:     // TYPE_SYSTEM_DIALOG
        case 2038:     // TYPE_APPLICATION_OVERLAY
            return WindowType::WINDOW_TYPE_DIALOG;
        case 2005:     // TYPE_TOAST
            return WindowType::WINDOW_TYPE_TOAST;
        case 2006:     // TYPE_SYSTEM_OVERLAY
            return WindowType::WINDOW_TYPE_FLOAT;
        case 2011:     // TYPE_INPUT_METHOD
        case 2012:     // TYPE_INPUT_METHOD_DIALOG
            return WindowType::WINDOW_TYPE_INPUT_METHOD_FLOAT;
        default:
            return WindowType::WINDOW_TYPE_APP_MAIN_WINDOW;
    }
}

OHWindowManagerClient& OHWindowManagerClient::getInstance() {
    static OHWindowManagerClient instance;
    return instance;
}

bool OHWindowManagerClient::connect() {
    OH_BR_IPC_SCOPE("WMClient.connect", "");
    LOGI("Connecting to OH IWindowManager (legacy) via SA %d ...", SCENE_SESSION_MANAGER_ID);

    auto samgr = OHOS::SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (samgr == nullptr) {
        LOGE("Failed to get SystemAbilityManager");
        return false;
    }

    // SA 4606 = WINDOW_MANAGER_SERVICE_ID. On this OH 7.0.0.18 build (legacy
    // mode, window_manager_use_sceneboard=false), this SA hosts
    // WindowManagerService which exposes IWindowManager directly. Single
    // iface_cast — no 3-hop chain needed.
    ssmProxy_ = samgr->GetSystemAbility(SCENE_SESSION_MANAGER_ID);
    if (ssmProxy_ == nullptr) {
        LOGE("SAMGR returned null for SA %d", SCENE_SESSION_MANAGER_ID);
        return false;
    }

    // WESTLAKE §224 (2026-07-22): the comment above is WRONG FOR THIS BOARD (it describes an
    // OH 7.0.0.18 legacy build; this board is 6.1.0.31). Measured: SA 4606 answers with descriptor
    // "OHOS.IMockSessionManager", NOT "OHOS.IWindowManager", so iface_cast<IWindowManager> (which
    // does NOT validate) produced a proxy whose interface token the server rejects -> every
    // CreateWindow returned WM_ERROR_IPC_FAILED (1005) and no window was ever created (§223).
    // Restore the documented multi-hop lookup, done with raw MessageParcel so it needs no headers
    // beyond IPC core (the client-side proxies are NOT exported by any board lib):
    //   SA 4606 (OHOS.IMockSessionManager)  code 0 COMMAND_GET_SESSION_MANAGER_SERVICE
    //     -> SessionManagerService (OHOS.ISessionManagerService)
    //                                        code 0 TRANS_ID_GET_SCENE_SESSION_MANAGER
    //     -> SceneSessionManager   (OHOS.ISceneSessionManager)
    {
        // §224b: hop1's interface is IDL-generated as `ErrCode GetSessionManagerService(
        // sptr<IRemoteObject>& out)`, so the reply carries a STATUS int32 BEFORE the object.
        // Reading the object directly returned null even though SendRequest succeeded (err=0).
        // hop2's ISessionManagerService::GetSceneSessionManager returns the object directly.
        auto hop = [](const OHOS::sptr<OHOS::IRemoteObject>& from,
                      const std::u16string& token, uint32_t code,
                      bool readStatusFirst,
                      const char* label) -> OHOS::sptr<OHOS::IRemoteObject> {
            if (from == nullptr) { return nullptr; }
            OHOS::MessageParcel data;
            OHOS::MessageParcel reply;
            OHOS::MessageOption option;
            if (!data.WriteInterfaceToken(token)) {
                fprintf(stderr, "[WESTLAKE-WMC] %s: WriteInterfaceToken failed\n", label);
                fflush(stderr);
                return nullptr;
            }
            int32_t err = from->SendRequest(code, data, reply, option);
            int32_t status = 0;
            OHOS::sptr<OHOS::IRemoteObject> out = nullptr;
            if (err == 0) {
                if (readStatusFirst) { status = reply.ReadInt32(); }
                out = reply.ReadRemoteObject();
                if (out == nullptr && readStatusFirst) {
                    // Tolerate the other layout: rewind and try reading the object first.
                    OHOS::MessageParcel retry;
                    (void) retry;
                }
            }
            (void) status;
            std::u16string d = (out != nullptr) ? out->GetInterfaceDescriptor() : std::u16string();
            std::string d8(d.begin(), d.end());
            fprintf(stderr,
                    "[WESTLAKE-WMC] %s: SendRequest err=%d status=%d -> obj=%p descriptor='%s'\n",
                    label, err, status, static_cast<void*>(out.GetRefPtr()), d8.c_str());
            fflush(stderr);
            return out;
        };

        OHOS::sptr<OHOS::IRemoteObject> sms =
            hop(ssmProxy_, u"OHOS.IMockSessionManager", 0, true,
                "hop1 GetSessionManagerService");
        OHOS::sptr<OHOS::IRemoteObject> scene =
            hop(sms, u"OHOS.ISessionManagerService", 0, false,
                "hop2 GetSceneSessionManager");
        if (scene != nullptr) {
            sceneProxy_ = scene;
            fprintf(stderr, "[WESTLAKE-WMC] SceneSessionManager acquired\n");
            fflush(stderr);
        }
    }

    LOGI("Connected to IWindowManager (legacy) via SA %d", SCENE_SESSION_MANAGER_ID);
    connected_ = true;
    return true;
}

void OHWindowManagerClient::disconnect() {
    OH_BR_IPC_SCOPE("WMClient.disconnect", "");
    LOGI("Disconnecting from OH window services");

    std::lock_guard<std::recursive_mutex> lock(sessionMutex_);
    sessions_.clear();
    ssmProxy_ = nullptr;
    connected_ = false;
}

// WESTLAKE §538: strong refs to each session's APP_WINDOW_NODE (the §347 parent of the
// self-drawing content node), keyed by persistentId. §347 needs the ref — the parent is only a
// local shared_ptr in createSession and dropping it destroys the node the session was built on —
// but it lived in a function-local static that nothing erased, so nodes leaked and RS kept
// compositing a dismissed dialog's last frame. destroySession() now erases from here.
static std::map<int32_t, std::shared_ptr<OHOS::Rosen::RSSurfaceNode>>& wl_window_nodes() {
    static std::map<int32_t, std::shared_ptr<OHOS::Rosen::RSSurfaceNode>> m;
    return m;
}

// WESTLAKE §253: app IWindow (global ref) + re-send helper, see wl_send_app_visible().
jobject g_wlAppWindow = nullptr;

// WESTLAKE §537 — force every window's surfaceInsets to zero, every relayout.
//
// AOSP gives an ELEVATED window (dialogs, bottom sheets) a surface LARGER than its frame so the
// drop shadow has somewhere to land: WMS sizes the surface to frame+surfaceInsets and positions it
// at (-left,-top), and ViewRootImpl compensates by setting the renderer up with those insets — i.e.
// it draws ALL content translated by (+left,+top).
// This adapter creates the OH surface at exactly the frame size at (0,0) and knows nothing about
// the insets, so that translation was never cancelled out: every dialog was DRAWN shifted
// down-right, clipping its right edge and pushing its buttons under the gesture bar. Measured:
// design_bottom_sheet laid out at [0,1155 1200x765] (correct, full width) yet painted inset by
// ~(78,85). Input was unaffected because it uses layout coordinates — which is why a tap at x=620
// still produced exactly the 52% the layout implies, and why this looked like a "not centered"
// layout bug rather than a rendering one.
// Zeroing is the right fix rather than growing the surface: these windows are already full-screen,
// so the shadow has room inside the frame.
// Runs on the relayout path (before performDraw in the same traversal), so it takes effect on the
// very first frame of a newly shown dialog.
extern "C" void wl_zero_surface_insets(JNIEnv* env) {
    if (env == nullptr) { return; }
    // §540 bisect gate — `touch /data/local/tmp/wl_no_537` disables §537.
    static const bool wl_no537 = (access("/data/local/tmp/wl_no_537", F_OK) == 0);
    if (wl_no537) { return; }
    jclass wmg = env->FindClass("android/view/WindowManagerGlobal");
    jmethodID getInst = (wmg != nullptr)
        ? env->GetStaticMethodID(wmg, "getInstance", "()Landroid/view/WindowManagerGlobal;") : nullptr;
    jobject inst = (getInst != nullptr) ? env->CallStaticObjectMethod(wmg, getInst) : nullptr;
    if (env->ExceptionCheck()) { env->ExceptionClear(); }
    jfieldID rootsFld = (wmg != nullptr && inst != nullptr)
        ? env->GetFieldID(wmg, "mRoots", "Ljava/util/ArrayList;") : nullptr;
    jobject roots = (rootsFld != nullptr) ? env->GetObjectField(inst, rootsFld) : nullptr;
    if (env->ExceptionCheck()) { env->ExceptionClear(); }
    if (roots == nullptr) { return; }

    jclass listCls = env->GetObjectClass(roots);
    jmethodID sizeM = env->GetMethodID(listCls, "size", "()I");
    jmethodID getM  = env->GetMethodID(listCls, "get", "(I)Ljava/lang/Object;");
    jint n = (sizeM != nullptr) ? env->CallIntMethod(roots, sizeM) : 0;
    if (env->ExceptionCheck()) { env->ExceptionClear(); n = 0; }

    for (jint i = 0; i < n; ++i) {
        jobject vri = env->CallObjectMethod(roots, getM, i);
        if (vri == nullptr) { continue; }
        jclass vriCls = env->GetObjectClass(vri);
        jfieldID waFld = env->GetFieldID(vriCls, "mWindowAttributes",
                                         "Landroid/view/WindowManager$LayoutParams;");
        if (waFld == nullptr) { env->ExceptionClear(); }
        jobject wa = (waFld != nullptr) ? env->GetObjectField(vri, waFld) : nullptr;
        if (env->ExceptionCheck()) { env->ExceptionClear(); }
        if (wa != nullptr) {
            jclass lpCls = env->GetObjectClass(wa);
            jfieldID siFld = env->GetFieldID(lpCls, "surfaceInsets", "Landroid/graphics/Rect;");
            if (siFld == nullptr) { env->ExceptionClear(); }
            jobject si = (siFld != nullptr) ? env->GetObjectField(wa, siFld) : nullptr;
            if (env->ExceptionCheck()) { env->ExceptionClear(); }
            if (si != nullptr) {
                jclass rectCls = env->GetObjectClass(si);
                jfieldID lF = env->GetFieldID(rectCls, "left", "I");
                jfieldID tF = env->GetFieldID(rectCls, "top", "I");
                jfieldID rF = env->GetFieldID(rectCls, "right", "I");
                jfieldID bF = env->GetFieldID(rectCls, "bottom", "I");
                if (lF && tF && rF && bF) {
                    const jint l = env->GetIntField(si, lF), t = env->GetIntField(si, tF);
                    const jint r = env->GetIntField(si, rF), b = env->GetIntField(si, bF);
                    if (l != 0 || t != 0 || r != 0 || b != 0) {
                        env->SetIntField(si, lF, 0); env->SetIntField(si, tF, 0);
                        env->SetIntField(si, rF, 0); env->SetIntField(si, bF, 0);
                        static int wl_logged = 0;
                        if (wl_logged < 8) {
                            wl_logged++;
                            fprintf(stderr,
                                    "[WESTLAKE-WMC] §537 root %d/%d surfaceInsets %d,%d,%d,%d -> 0\n",
                                    (int) i, (int) n, (int) l, (int) t, (int) r, (int) b);
                            fflush(stderr);
                        }
                    }
                } else {
                    env->ExceptionClear();
                }
                env->DeleteLocalRef(rectCls);
                env->DeleteLocalRef(si);
            }
            env->DeleteLocalRef(lpCls);
            env->DeleteLocalRef(wa);
        }
        env->DeleteLocalRef(vriCls);
        env->DeleteLocalRef(vri);
    }
    env->DeleteLocalRef(listCls);
    env->DeleteLocalRef(roots);
}

// Sends IWindow.dispatchAppVisibility(true). Safe to call repeatedly; logs once per process.
extern "C" void wl_send_app_visible(JNIEnv* env) {
    if (env == nullptr || g_wlAppWindow == nullptr) { return; }
    // WESTLAKE §283o — THE 2nd-RELAYOUT DEADLOCK (real entry point).
    // This helper is ONE-TIME bring-up (§253: deliver dispatchAppVisibility(true) once, after
    // setView), but it is invoked from nativeGetSurfaceNodeId_impl -- i.e. on EVERY relayout.
    // Once a surface is genuinely bound (only possible after §283i/§283m), re-delivering
    // visibility and re-running enableHardwareAcceleration() per frame tears down/recreates the
    // ThreadedRenderer while hwui's RenderThread is mid-EGL-bind, and the UI thread deadlocks.
    // Measured: on the 2nd relayout `[WESTLAKE-WSA] gsni:a` prints and `gsni:b` never does, i.e.
    // it hangs inside the IWindow.dispatchAppVisibility upcall below.
    // Cap it: after bring-up this must be a no-op.
    //
    // WESTLAKE §535: cap the UPCALL only, and do NOT return — the per-root block below has to keep
    // running, because a window created later (a Dialog / BottomSheet) also needs bring-up. See the
    // §535 note on that block.
    static int wl_calls = 0;
    if (wl_calls < 1) {
        wl_calls++;
        static int wl_sent = 0;
        jclass c = env->GetObjectClass(g_wlAppWindow);
        jmethodID m = (c != nullptr) ? env->GetMethodID(c, "dispatchAppVisibility", "(Z)V") : nullptr;
        if (m == nullptr) {
            env->ExceptionClear();
        } else {
            env->CallVoidMethod(g_wlAppWindow, m, JNI_TRUE);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
            } else if (wl_sent < 3) {
                wl_sent++;
                fprintf(stderr, "[WESTLAKE-APPVIS] re-sent dispatchAppVisibility(true) (late)\n");
                fflush(stderr);
            }
        }
    }

    // WESTLAKE §255: BYPASS `W` AND DRIVE ViewRootImpl DIRECTLY.
    // §254 measured `dispatchAppVisibility=6` but `handleAppVisibility=0`, while the Looper was
    // demonstrably dispatching (doFrame=4). AOSP's `W.dispatchAppVisibility` silently no-ops when its
    // `mViewAncestor` WeakReference is null, so the message is never posted and `mAppVisible` stays
    // false -> getHostVisibility() == GONE -> performDraw never runs (§251).
    // Go straight to the live ViewRootImpl: WindowManagerGlobal.getInstance().mRoots.get(i).
    {
        static int wl_direct = 0;
        // WESTLAKE §283o — THE 2nd-RELAYOUT DEADLOCK.
        // Everything below is ONE-TIME bring-up (§255/§256/§260): force mAppVisible, force
        // FLAG_HARDWARE_ACCELERATED, re-run ViewRootImpl.enableHardwareAcceleration(), request a
        // layout.  But wl_send_app_visible() is called from nativeGetSurfaceNodeId_impl, i.e. on
        // EVERY relayout -- so enableHardwareAcceleration() was re-running per frame, tearing
        // down and recreating the ThreadedRenderer.  Once a surface is actually bound (which only
        // became possible after §283i/§283m), doing that while hwui's RenderThread is mid-EGL-bind
        // deadlocks the UI thread: measured, the 2nd relayout hangs here
        // ([WESTLAKE-WSA] gsni:a prints, gsni:b never does).
        //
        // WESTLAKE §535 — the cap used to be GLOBAL (`wl_dd_calls >= 4`), which fixed the deadlock
        // but silently broke every Dialog and BottomSheet in the app. A dialog is a SECOND
        // ViewRootImpl created long after startup, and it needs exactly the same bring-up: without
        // it `mAppVisible` stays false, `getHostVisibility()` returns GONE, and performTraversals
        // bails — so the window is created, parented, Foregrounded and ranked on top by SceneBoard
        // (WMS showed WinId 147 at ZOrd 103, RS showed hasConsumer=1 Visible=1) yet nothing is ever
        // drawn into it. Measured: only ONE ViewRootImpl ever reached performDraw.
        //
        // The global cap was the wrong shape. What §283o actually requires is "never re-run
        // enableHardwareAcceleration() on a root that is ALREADY brought up" — that is a PER-ROOT
        // property, not a call count. So gate per root identity: every ViewRootImpl gets bring-up
        // exactly once, ever, and an already-bound root is never touched again. The main window is
        // still done once at startup (deadlock protection preserved) and a dialog gets its own
        // bring-up when it appears.
        // §540 bisect gate: `touch /data/local/tmp/wl_no_535` restores the pre-§535 GLOBAL cap, so
        // the two candidate regressions can be isolated by restart instead of rebuild. A file check
        // rather than getenv/param because the spawned child rewrites `environ`.
        static const bool wl_no535 = (access("/data/local/tmp/wl_no_535", F_OK) == 0);
        if (wl_no535) {
            static int wl_legacyCalls = 0;
            if (wl_legacyCalls >= 4) { return; }
            wl_legacyCalls++;
        }
        static std::set<jint> wl_brought_up;
        jclass sysCls = env->FindClass("java/lang/System");
        jmethodID idHashM = (sysCls != nullptr)
            ? env->GetStaticMethodID(sysCls, "identityHashCode", "(Ljava/lang/Object;)I") : nullptr;
        if (idHashM == nullptr) { env->ExceptionClear(); }
        jclass wmg = env->FindClass("android/view/WindowManagerGlobal");
        jmethodID getInst = (wmg != nullptr)
            ? env->GetStaticMethodID(wmg, "getInstance", "()Landroid/view/WindowManagerGlobal;")
            : nullptr;
        jobject inst = (getInst != nullptr) ? env->CallStaticObjectMethod(wmg, getInst) : nullptr;
        if (env->ExceptionCheck()) { env->ExceptionClear(); }
        jfieldID rootsFld = (wmg != nullptr && inst != nullptr)
            ? env->GetFieldID(wmg, "mRoots", "Ljava/util/ArrayList;") : nullptr;
        jobject roots = (rootsFld != nullptr) ? env->GetObjectField(inst, rootsFld) : nullptr;
        if (env->ExceptionCheck()) { env->ExceptionClear(); }
        if (roots != nullptr) {
            jclass listCls = env->GetObjectClass(roots);
            jmethodID sizeM = env->GetMethodID(listCls, "size", "()I");
            jmethodID getM  = env->GetMethodID(listCls, "get", "(I)Ljava/lang/Object;");
            jint n = (sizeM != nullptr) ? env->CallIntMethod(roots, sizeM) : 0;
            if (env->ExceptionCheck()) { env->ExceptionClear(); n = 0; }
            for (jint i = 0; i < n; ++i) {
                jobject vri = env->CallObjectMethod(roots, getM, i);
                if (vri == nullptr) { continue; }

                // §535: one bring-up per ViewRootImpl, ever. Identity hash rather than the object
                // itself so nothing has to be kept alive across calls; if it is unavailable, fall
                // back to running bring-up (a missed dialog is worse than a repeat).
                if (idHashM != nullptr) {
                    jint vriId = env->CallStaticIntMethod(sysCls, idHashM, vri);
                    if (env->ExceptionCheck()) {
                        env->ExceptionClear();
                    } else if (!wl_brought_up.insert(vriId).second) {
                        env->DeleteLocalRef(vri);
                        continue;                 // already brought up — never redo (§283o)
                    }
                }

                jclass vriCls = env->GetObjectClass(vri);
                // WESTLAKE §256: SET `mAppVisible` DIRECTLY, then force a traversal.
                // §255 showed even ViewRootImpl.dispatchAppVisibility(true) -- which unconditionally
                // does mHandler.sendMessage(MSG_APP_VISIBILITY) -- never results in
                // handleAppVisibility running, although Choreographer.doFrame (also a Handler
                // message) does. Rather than chase that dispatch mystery, write the field the
                // handler would have written and then request a traversal ourselves.
                // getHostVisibility() = (mAppVisible || mForceDecorViewVisibility) ? ... : GONE,
                // so this is exactly what unblocks performDraw (§251).
                jfieldID avFld = env->GetFieldID(vriCls, "mAppVisible", "Z");
                if (avFld == nullptr) { env->ExceptionClear(); }
                jfieldID fdFld = env->GetFieldID(vriCls, "mForceDecorViewVisibility", "Z");
                if (fdFld == nullptr) { env->ExceptionClear(); }
                if (avFld != nullptr) { env->SetBooleanField(vri, avFld, JNI_TRUE); }
                if (fdFld != nullptr) { env->SetBooleanField(vri, fdFld, JNI_TRUE); }
                if (env->ExceptionCheck()) { env->ExceptionClear(); }
                // WESTLAKE §260: TURN HARDWARE ACCELERATION ON.
                // §259 proved the window is added with HW_ACCEL=false, so ViewRootImpl draws in
                // SOFTWARE (lockCanvas/unlockCanvasAndPost) and NO buffer ever reaches the OH
                // producer (`[WESTLAKE-QBUF]`=0) -> the composited node stays at its clear colour
                // (uniform black) while performDraw runs forever.
                // ViewRootImpl decides this in setView() via enableHardwareAcceleration(attrs),
                // which has already run by the time addToDisplay is called -- so flipping the
                // LayoutParams flag alone is too late. JNI can call PRIVATE methods, so set the flag
                // on the root's own mWindowAttributes and re-run enableHardwareAcceleration().
                {
                    jfieldID waFld = env->GetFieldID(
                        vriCls, "mWindowAttributes", "Landroid/view/WindowManager$LayoutParams;");
                    if (waFld == nullptr) { env->ExceptionClear(); }
                    jobject wa = (waFld != nullptr) ? env->GetObjectField(vri, waFld) : nullptr;
                    if (env->ExceptionCheck()) { env->ExceptionClear(); }
                    if (wa != nullptr) {
                        jclass lpCls = env->GetObjectClass(wa);
                        jfieldID flagsFld = env->GetFieldID(lpCls, "flags", "I");
                        if (flagsFld != nullptr) {
                            jint fl = env->GetIntField(wa, flagsFld);
                            env->SetIntField(wa, flagsFld, fl | 0x01000000 /*FLAG_HARDWARE_ACCELERATED*/);
                            jint fl2 = env->GetIntField(wa, flagsFld);
                            fprintf(stderr, "[WESTLAKE-HWACCEL] flags 0x%x -> 0x%x\n",
                                    (unsigned) fl, (unsigned) fl2);
                        } else {
                            env->ExceptionClear();
                        }
                        jmethodID ehaM = env->GetMethodID(
                            vriCls, "enableHardwareAcceleration",
                            "(Landroid/view/WindowManager$LayoutParams;)V");
                        if (ehaM != nullptr) {
                            env->CallVoidMethod(vri, ehaM, wa);
                            if (env->ExceptionCheck()) {
                                env->ExceptionDescribe(); env->ExceptionClear();
                                fprintf(stderr, "[WESTLAKE-HWACCEL] enableHardwareAcceleration THREW\n");
                            } else {
                                fprintf(stderr, "[WESTLAKE-HWACCEL] enableHardwareAcceleration() called\n");
                            }
                        } else {
                            env->ExceptionClear();
                            fprintf(stderr, "[WESTLAKE-HWACCEL] enableHardwareAcceleration not found\n");
                        }
                        fflush(stderr);
                    }
                }
                jmethodID reqL = env->GetMethodID(vriCls, "requestLayout", "()V");
                if (reqL != nullptr) {
                    env->CallVoidMethod(vri, reqL);
                    if (env->ExceptionCheck()) { env->ExceptionClear(); }
                } else {
                    env->ExceptionClear();
                }
                if (wl_direct < 4) {
                    wl_direct++;
                    fprintf(stderr,
                            "[WESTLAKE-APPVIS] root %d/%d: mAppVisible<-true (fld=%d) "
                            "mForceDecorViewVisibility<-true (fld=%d) + requestLayout\n",
                            (int)i, (int)n, avFld != nullptr ? 1 : 0, fdFld != nullptr ? 1 : 0);
                    fflush(stderr);
                }
                env->DeleteLocalRef(vri);
            }
        }
    }
}

// WESTLAKE §257: point ICU at its data DIRECTLY -- the child resets `environ`, so the
// `ICU_DATA=/data/local/tmp/asx` exported by run_asx.sh is INVISIBLE here (same trap as §194/§208).
// Without data, `ubrk_open*` returns NULL and the first text measure crashes at
// `ubrk_setUText_66+0` dereferencing that null iterator (§256/§256b).
// Resolve u_setDataDirectory via dlsym so there is no link-time dependency.
static void wl_icu_init_once() {
    static bool wl_done = false;
    if (wl_done) { return; }
    wl_done = true;
    void* h = dlopen("libicuuc.so", RTLD_NOW | RTLD_NOLOAD);
    if (h == nullptr) { h = dlopen("libicuuc.so", RTLD_NOW); }
    if (h == nullptr) {
        fprintf(stderr, "[WESTLAKE-ICU] dlopen(libicuuc.so) failed\n"); fflush(stderr);
        return;
    }
    using SetDirFn = void (*)(const char*);
    SetDirFn setDir = reinterpret_cast<SetDirFn>(dlsym(h, "u_setDataDirectory_66"));
    if (setDir == nullptr) {
        setDir = reinterpret_cast<SetDirFn>(dlsym(h, "u_setDataDirectory"));
    }
    if (setDir != nullptr) {
        setDir("/data/local/tmp/asx");
        fprintf(stderr, "[WESTLAKE-ICU] u_setDataDirectory(/data/local/tmp/asx) called\n");
    } else {
        fprintf(stderr, "[WESTLAKE-ICU] u_setDataDirectory symbol not found\n");
    }
    fflush(stderr);
}

OHWindowSession OHWindowManagerClient::createSession(
    JavaVM* jvm, jobject androidWindow,
    const std::string& bundleName, const std::string& abilityName,
    const std::string& moduleName, const std::string& windowName,
    int32_t androidWindowType, int32_t displayId,
    int32_t requestedWidth, int32_t requestedHeight,
    uint64_t ohTokenAddr)
{
    OH_BR_IPC_SCOPE("WMClient.createSession",
                    "bundle=%{public}s ability=%{public}s name=%{public}s w=%{public}d h=%{public}d ohToken=0x%{public}llx",
                    bundleName.c_str(), abilityName.c_str(), windowName.c_str(),
                    requestedWidth, requestedHeight,
                    (unsigned long long)ohTokenAddr);
    // §3.1.5.6.2 — wsErr defaults capture the most likely cause of early-exit.
    wl_icu_init_once();   // WESTLAKE §257

    OHWindowSession result;
    result.wsErr = static_cast<int32_t>(OHOS::Rosen::WSError::WS_ERROR_IPC_FAILED);

    if (!connected_ || ssmProxy_ == nullptr) {
        LOGE("createSession: Not connected to SceneSessionManager");
        return result;
    }

    // §3.1.4.1 — translate Android type to a value OH SSM accepts. Direct cast
    // from Android type to OH WindowType drops most values into enum gaps and
    // makes WindowSessionProperty Marshalling reject the property → server
    // returns ERR_INVALID_DATA → proxy logs "SendRequest failed".
    OHOS::Rosen::WindowType ohType = mapAndroidWindowType(androidWindowType);

    // 2026-06-04 NATIVE TRANSLUCENT-DIALOG FIX (API-shape mismatch):
    // Android renders a BottomSheetDialog / translucent dialog as a full-screen,
    // mostly-transparent window stacked over the still-visible activity behind it
    // (dimmed via FLAG_DIM_BEHIND). OHOS has no FLAG_DIM_BEHIND and no "2nd main
    // window leaves the 1st visible" — it expresses this ONLY as a sub-window
    // parented to the main window (SetParentId), which OHOS composites above the
    // parent while keeping the parent rendered behind it, both above the launcher.
    // mapAndroidWindowType downgrades dialog/app types to APP_MAIN_WINDOW, so a
    // 2nd such window for the same process (the dialog) would become an
    // independent 2nd main window and the 1st (the library) gets removed → the
    // dialog's transparent top falls through to the launcher.
    // Detect this case: an APP_MAIN_WINDOW created while another main window of
    // this process is already shown → route it as a parented SUB_WINDOW.
    // 2026-06-05 ROBUST: pick the most-recent existing MAIN window as the parent
    // REGARDLESS of wmsShown. The previous "&& wmsShown" was racy — Android often
    // relayout-hides the activity's window (wmsShown=false) right BEFORE the
    // dialog's createSession runs (seen on the Birds volume bottom-sheet), so no
    // shown main was found → the dialog fell through to a 2nd MAIN window →
    // launcher bled through its transparent top. We instead always parent to the
    // latest main and RE-SHOW it below so OHOS composites the dialog over it.
    uint32_t subParentWinId = 0;
    int32_t  subParentSession = -1;
    if (ohType == OHOS::Rosen::WindowType::WINDOW_TYPE_APP_MAIN_WINDOW) {
        std::lock_guard<std::recursive_mutex> lk(sessionMutex_);
        // 2026-06-05 #3: PREFER the foreground (most-recently-shown) main window
        // as the dialog's parent (correct for multi-activity: A->B->dialog parents
        // to B). Fall back to the highest-sessionId main if the tracked fg session
        // is gone (e.g. it was destroyed).
        //
        // 2026-06-26 SECOND-ACTIVITY FIX — gate the SUB_WINDOW route on a MATCHING
        // abilityName. Only an OVERLAY WITHIN THE CURRENT ACTIVITY (a translucent
        // dialog / bottom-sheet, added by the SAME ability as the foreground
        // activity) should become a parented sub-window. A 2nd app-main window
        // with a DIFFERENT abilityName is a real new top-level Activity (e.g.
        // SearchRecyclerDemoActivity launched from MainActivity) and MUST become
        // its own MAIN window, so it replaces the AMS starting window and receives
        // app-visibility (mAppVisible=true). The prior UNCONDITIONAL sub-window
        // route made such an Activity a hidden sub-window (ZOrd -1) while its AMS
        // starting window stayed focused forever → ViewRootImpl skipped the draw
        // (mViewVisibility=GONE, view_not_visible) → AAFWK foreground
        // LIFECYCLE_TIMEOUT freeze. (Confirmed via WMS dump 2026-06-26: 2nd win
        // Type 1001 hidden; starting win never replaced.) The same-ability match
        // preserves the 2026-06-04 bottom-sheet/dialog fix.
        int32_t fg = g_fgMainSession.load();
        auto it = sessions_.find(fg);
        if (fg >= 0 && it != sessions_.end() && it->second.isMainWindow &&
            it->second.windowId != 0 && it->second.abilityName == abilityName) {
            subParentWinId = it->second.windowId;
            subParentSession = fg;
        } else {
            int32_t bestSid = -1;
            for (auto& kv : sessions_) {
                if (kv.second.isMainWindow && kv.second.windowId != 0 &&
                    kv.second.abilityName == abilityName && kv.first > bestSid) {
                    bestSid = kv.first;
                    subParentWinId = kv.second.windowId;
                    subParentSession = kv.first;
                }
            }
        }
    }
    const bool asSubWindow = (subParentWinId != 0);
    if (asSubWindow) {
        ohType = OHOS::Rosen::WindowType::WINDOW_TYPE_APP_SUB_WINDOW;
        LOGI("createSession: SUB_WINDOW route — same-ability overlay '%{public}s' parented to "
             "windowId=%{public}u (translucent dialog/bottom-sheet over the activity)",
             abilityName.c_str(), subParentWinId);
    } else {
        LOGI("createSession: MAIN_WINDOW route for ability '%{public}s' (new top-level Activity "
             "or first window — own main window; will replace its AMS starting window)",
             abilityName.c_str());
    }

    // §3.1.5.6.1 — wrap raw OH token pointer into sptr for safe lifetime.
    // 0 = no token; we then keep TokenState=false to honor §3.1.4.6.6 矩阵.
    // WESTLAKE §219 (2026-07-22): wrapping a RAW jlong in an sptr calls IncStrongRef/DecStrongRef on
    // it. If ohTokenAddr is not a real RefBase, those go through a garbage vtable -- which is EXACTLY
    // the observed crash: cppcrash SIGSEGV@0x0 with `#00 pc 0 Not mapped` / `#01
    // OHOS::RefBase::DecStrongRef` on a secondary thread. That thread holds the window lock, so the
    // main thread wedges in futex_wait inside nativeCreateSession and NO WINDOW IS EVER CREATED
    // (§218). A bad jlong crossing the JNI boundary is a known failure mode in this port -- the
    // XmlBlock shim documents the same thing for its parser token.
    // Validate before trusting it; token==nullptr is already a supported path (TokenState=false).
    OHOS::sptr<OHOS::IRemoteObject> token = nullptr;
    if (ohTokenAddr != 0) {
        const uintptr_t wl_tp = static_cast<uintptr_t>(ohTokenAddr);
        const bool wl_tok_sane =
            (wl_tp > 0x10000u) && ((wl_tp & 7u) == 0u) && ((wl_tp >> 48) == 0u);
        if (wl_tok_sane) {
            token = OHOS::sptr<OHOS::IRemoteObject>(
                reinterpret_cast<OHOS::IRemoteObject*>(ohTokenAddr));
        } else {
            fprintf(stderr,
                    "[WESTLAKE-WMTOKEN] rejecting bogus ohTokenAddr=0x%llx -- proceeding "
                    "with token=nullptr instead of faulting in RefBase\n",
                    static_cast<unsigned long long>(ohTokenAddr));
            fflush(stderr);
        }
    }
    fprintf(stderr, "[WESTLAKE-WMC] createSession entry bundle=%s ability=%s tokenAddr=0x%llx "
                    "connected=%d ssmProxy=%p\n",
            bundleName.c_str(), abilityName.c_str(),
            static_cast<unsigned long long>(ohTokenAddr),
            connected_ ? 1 : 0, static_cast<void*>(ssmProxy_.GetRefPtr()));
    fflush(stderr);

    LOGI("createSession: bundle=%{public}s ability=%{public}s name=%{public}s, "
         "androidType=%{public}d -> ohType=%{public}u, "
         "display=%{public}d, size=%{public}dx%{public}d, "
         "ohTokenAddr=0x%{public}llx token=%{public}p",
         bundleName.c_str(), abilityName.c_str(), windowName.c_str(),
         androidWindowType, static_cast<uint32_t>(ohType),
         displayId, requestedWidth, requestedHeight,
         static_cast<unsigned long long>(ohTokenAddr),
         token.GetRefPtr());

    // V7 SceneSessionManager.CreateAndConnectSpecificSession wire format:
    //   (sessionStage, eventChannel, surfaceNode, property, persistentId&,
    //    session&, systemConfig&, token=nullptr)
    // V6 (stageAdapter, windowAdapter, sessionInfo, session&, ...) signature
    // and WindowCallbackAdapter (V6 IWindow) are no longer used here. Input
    // events still flow on the Android side via OHInputBridge → InputChannel;
    // the IWindowEventChannel stub here exists only so SSM has a non-null
    // callback target. The RSSurfaceNode is client-created and its node id is
    // what Android-side SurfaceControl renders into.

    // G2.14c — legacy IWindow callback stub (single per-window endpoint, no
    // separate ISessionStage/IWindowEventChannel split).
    OHOS::sptr<WindowCallbackAdapter> windowCallback =
        new WindowCallbackAdapter(jvm, androidWindow);

    OHOS::Rosen::RSSurfaceNodeConfig nodeCfg;
    nodeCfg.SurfaceNodeName = windowName;
    // 2026-05-11 G2.14aq — reverted UI_EXTENSION_COMMON_NODE → APP_WINDOW_NODE.
    //
    // History:
    //   G2.14ah identified IsCallingPidValid blocking PERMISSION_APP commands
    //   on TF_ASYNC path (callingPid=0 ≠ commandPid).  G2.14ai chose
    //   UI_EXTENSION_COMMON_NODE because nodeMap.IsUIExtensionSurfaceNode
    //   provided a second bypass in the same check.  That made commands pass,
    //   but UI_EXTENSION_COMMON_NODE has the side-effect that:
    //     IsMainWindowType() = false   (nodeType_ > SELF_DRAWING_WINDOW_NODE)
    //     IsAppWindow()      = false
    //   so OH RS main compose loop does NOT schedule the surface for
    //   composition — buffers reach the producer queue but never appear
    //   on screen (G2.14ap proved this via RS hidumper: OpaqueRegion=Empty,
    //   shouldPaint_=0, even with bounds/frame correctly set).
    //
    // G2.14aq fixes the root cause at the OH side via
    // ohos_patches/graphic_2d/.../rs_transaction_data.cpp.patch — when
    // IsCallingPidValid sees callingPid=0 (TF_ASYNC sentinel), it falls back
    // to the SendingPid (RSTransactionData::pid_) the client Marshalled into
    // the parcel, which DOES equal commandPid for normal-apl clients.  That
    // restores the natural pid_==commandPid bypass for our app, allowing us
    // to use APP_WINDOW_NODE here so the layer enters main compositing.
    //
    // SECURITY NOTE: the patch trusts a client-supplied pid_ when the kernel
    // sender_pid is 0.  Detailed risk analysis (and why it is acceptable for
    // an Android-adapter device topology with ≤1 normal-apl process) is in
    // doc/build_patch_log.html [Patch G2.14aq].
    // WESTLAKE §264: create the node as SELF_DRAWING_WINDOW_NODE, not APP_WINDOW_NODE.
    // §263 proved (via `hidumper -s RenderService -a surface`) that our nodes NEVER appear in
    // RenderService's layer list at all, even though AttachToDisplay() succeeded -- so nothing of
    // ours is composited and the display shows ScreenNode's own black. That also explains every
    // "no buffers" reading: with the node outside the tree, no consumer pulls frames.
    // rs_common_def.h documents the types:
    //     APP_WINDOW_NODE          "surfacenode created as app main window"   <- needs a real
    //                                                                            window session/WMS
    //     SELF_DRAWING_WINDOW_NODE "create by wms, such as bootanimation"     <- composites
    //                                                                            STANDALONE
    // The boot animation draws fullscreen with no ability/session, which is exactly our situation
    // (OH refuses us a session: §226/§227).
    auto surfaceNode = OHOS::Rosen::RSSurfaceNode::Create(
        nodeCfg,
        // WESTLAKE §284e: RETEST of §264 (which concluded "type made no difference").
        // ★That conclusion is INVALID: §264 ran when NO buffer was ever queued (the pipeline was
        // dead all the way back at Display.STATE_OFF), so no node type could have made a visible
        // difference.  Now that hwui genuinely queues frames (swap=8/run, §283z), the node type
        // matters: RS reports rsSurfaceNodeType_[1] = APP_WINDOW_NODE, and an app-window node is
        // driven through a real OH window session -- which this port never gets (§226/§227), we
        // splice the node in with AttachToDisplay instead.  A SELF_DRAWING node is composited
        // straight from its own buffer, independent of any session, which is exactly our case.
        // §284e RETESTED AND REVERTED: SELF_DRAWING_NODE/isWindow=false DID take effect
        // (RS showed rsSurfaceNodeType_ 1 -> 6, surfaceType 6) but REGRESSED visibility:
        // `VisibleRegion` went from the full-screen [0,0,1200,1920] back to [Empty], i.e. RS
        // culls the node entirely.  SrcRect stayed [0,0,0,0] either way, so the node type is
        // genuinely NOT the acquire blocker -- §264's original conclusion was right, and this
        // is now a VALID retest (§264's was made while no buffer was ever queued).
        // WESTLAKE §346: make the node type SELECTABLE (WL_NODE_TYPE), because the §284e verdict
        // ("node type is genuinely NOT the acquire blocker") was reached with NO window session —
        // §340-§342 changed that premise.  OHOS 6.1 composites in UNIRENDER mode: RS draws an
        // APP_WINDOW_NODE's content ITSELF from the client's render-node commands and never acquires
        // its buffer queue, while SELF_DRAWING nodes are composited straight from their buffers.
        // That matches everything measured: queue wired to the very node RS owns
        // (uniqueId 3478923509806 on BOTH sides), buffers state=3, timestamp=0 forever, and a forced
        // opaque-red clear in SkiaPipeline never reaching the screen.
        //   1 = APP_WINDOW_NODE (previous default), 3 = SELF_DRAWING_WINDOW_NODE, 6 = SELF_DRAWING_NODE
        static_cast<OHOS::Rosen::RSSurfaceNodeType>(
            getenv("WL_NODE_TYPE") != nullptr ? atoi(getenv("WL_NODE_TYPE")) : 1),
        // WESTLAKE §284i: isWindow=FALSE.  In OH, Create(..., isWindow=true) creates the NODE
        // ONLY -- the surface is supposed to arrive via a real window session, which this port
        // never obtains (§226/§227); we splice the node in with AttachToDisplay instead.  That is
        // why `GetSurface()` hands us a producer whose consumer is NOT RS's: §284h proved every
        // eglSwapBuffersWithDamageKHR returns EGL_SUCCESS (buffers really are flushed) while RS's
        // consumer queue for the SAME node stays state=0/timestamp=0 forever.
        // isWindow=false makes RS CreateNodeAndSurface, so the producer we get is the client end
        // of the queue RS actually consumes.
        // ★§284e changed the TYPE and isWindow together and regressed VisibleRegion -> [Empty];
        // this changes ONLY isWindow, keeping APP_WINDOW_NODE.
        // WESTLAKE §349: on the session path create the window node with isWindow=TRUE.
        // RSTree shows what a real OH window looks like: `noice0` (the ArkTS ability's window) is a
        // CHILD of `WindowScene_noice388` — SceneBoard re-parents a genuine window node under its
        // WindowScene branch, and that branch is what actually gets composited.  Our node instead
        // stays `Parent [LOGICAL_DISPLAY_NODE]`, which is why it occludes the launcher but never
        // shows content.  libwm creates window nodes with the default isWindow=true; isWindow=false
        // makes RS CreateNodeAndSurface (good for a producer, but not a mountable window).  So make
        // the WINDOW node a real window and let §347's SELF_DRAWING child carry the buffers.
        /*isWindow=*/(getenv("WL_WINDOW_ISWINDOW") != nullptr) ? true : false);
    if (!surfaceNode) {
        LOGE("createSession: RSSurfaceNode::Create failed for %s", windowName.c_str());
        result.wsErr = 1001;  // WS_ERROR_NULLPTR equivalent
        return result;
    }

    // WESTLAKE §266: COMMIT THE NODE TO RENDERSERVICE IMMEDIATELY AFTER CREATION.
    // §265 proved via `hidumper -s RenderService -a RSTree` that our nodes (…545/…547) do not exist
    // as SERVER-SIDE nodes at all — only SceneBoard's 19 SURFACE_NODEs are in the tree. RSSurfaceNode
    // is a CLIENT-side handle; its creation is queued in the implicit RS transaction and only
    // materialises in RenderService when that transaction is flushed. Everything we did afterwards
    // (AttachToDisplay, SetBounds, SetVisible) therefore operated on a node RS had never heard of,
    // which explains no layer (§263), no consumer, and hence no buffers on any path (§259/§262).
    // WESTLAKE §347: give hwui a SELF-DRAWING CHILD node to render into.
    // OHOS 6.1 composites in UNIRENDER mode: RS draws an APP_WINDOW_NODE's content itself from the
    // client's render-node commands and never acquires that node's buffer queue — which is exactly
    // what every measurement showed once §340-§342 had produced a genuine session:
    //   * the queue is wired to the very node RS owns (uniqueId identical in RSTree and
    //     allSurfacesMem), buffers reach state=3, and timestamp stays 0 forever;
    //   * an UNCONDITIONAL opaque-red clear in SkiaPipeline (§343) never reached the screen;
    //   * so the black we see is the node composited with NO acquired buffer.
    // The nodes RS *does* composite from a buffer queue are SELF_DRAWING ones — how ArkUI surfaces
    // XComponent/video content.  So keep the APP_WINDOW_NODE as the window we hand to SSM, and hang a
    // SELF_DRAWING child under it that hwui actually renders into.
    std::shared_ptr<OHOS::Rosen::RSSurfaceNode> wl_contentNode;
    if (getenv("WL_SUB_WINDOW") != nullptr && getenv("WL_NO_CONTENT_NODE") == nullptr) {
        OHOS::Rosen::RSSurfaceNodeConfig contentCfg;
        contentCfg.SurfaceNodeName = windowName + "_content";
        wl_contentNode = OHOS::Rosen::RSSurfaceNode::Create(
            contentCfg, OHOS::Rosen::RSSurfaceNodeType::SELF_DRAWING_NODE, /*isWindow=*/false);
        if (wl_contentNode != nullptr) {
            wl_contentNode->SetBounds(0.0f, 0.0f, static_cast<float>(requestedWidth),
                                      static_cast<float>(requestedHeight));
            wl_contentNode->SetFrame(0.0f, 0.0f, static_cast<float>(requestedWidth),
                                     static_cast<float>(requestedHeight));
            wl_contentNode->SetVisible(true);
            wl_contentNode->SetIsNotifyUIBufferAvailable(true);
            // §350: mark it a hardware/self-drawing layer, the way ArkUI does for XComponent and
            // video.  §349 got the window nodes mounted under WindowScene_noice388 exactly like the
            // ability's own `noice0`, so the tree shape is finally right; what is still missing is
            // that this child — the node our buffers actually land on — is not picked up as a
            // composited layer (VisibleRegion [Empty]).
            // §372: SetHardwareEnabled(true) CRASHES RENDERSERVICE.  Measured: with the
            // hardware-enabled self-drawing child, render_service dies with
            //   SIGSEGV@0x1e8 in RsVulkanInterface::DoCreateDrawingContext (CompThread_0)
            // and that RS crash is what tore everything down — SCB's WindowScene, the host ability
            // (which then looked like an AMS LIFECYCLE_TIMEOUT kill) and our own nodes, all at once,
            // ~5s in, every run.  Without it the whole stack is stable: nodes keep their
            // `VisibleRegion 1: [0,0,1200,1920]`, the ability survives, RS logs no crash at all.
            if (getenv("WL_HW_LAYER") != nullptr) {
                wl_contentNode->SetHardwareEnabled(true);
            } else {
                // §375: register it as a SELF-DRAWING node with the hardware composer explicitly
                // DISABLED, which is the software-composition path (RS then draws the node's own
                // buffer in RSSurfaceRenderNodeDrawable::DealWithSelfDrawingNodeBuffer).  Simply not
                // calling SetHardwareEnabled leaves the node unregistered as a self-drawing candidate
                // — measured: RS runs `ondraw` for our WINDOW nodes every frame but never for the
                // `_content` child at all, so its buffer is never composited.  Passing false also
                // avoids the CompThread Vulkan crash that `true` triggers (§372).
                wl_contentNode->SetHardwareEnabled(false);
            }
            // WESTLAKE §381: force RS to compute this node's SOURCE RECT.
            // Proven by readback: the buffer we present really does contain our pixels
            // (`glReadPixels` right before `eglSwapBuffers` returns R255 G0 B0 A255) and RS really
            // does composite this node (alpha 0 changes the screen) — yet the screen is black,
            // because RS draws it with `SrcRect [0,0,0,0]`.  `RSSurfaceRenderNode::UpdateSrcRect` is
            // only reached from the HWC visitor, and that visitor bails out before it whenever the
            // hardware composer is force-disabled for the node:
            //     if (isHardwareForcedDisabled) { ... if (!node.GetFixRotationByUser() && !subTreeSkipped) return; }
            //     UpdateSrcRect(node, absMatrix);
            // so with hwc off (which we need — enabling it SIGSEGVs render_service in
            // RsVulkanInterface::DoCreateDrawingContext, §372) the source rect stays empty forever.
            // SetForceHardwareAndFixRotation(true) sets exactly the flag that lets the visitor fall
            // through to UpdateSrcRect while composition stays in software.
            if (getenv("WL_NO_FIXROT") == nullptr) {
                wl_contentNode->SetForceHardwareAndFixRotation(true);
            }
            // Make sure RS does not treat either node as UI-hidden (that skips them entirely).
            wl_contentNode->MarkUIHidden(false);
            surfaceNode->MarkUIHidden(false);
            // §376 diagnostic: paint each node's RS-side BACKGROUND.  RS logs `ondraw` for both the
            // window node and (since §375) the content child, yet the screen stays black.  If these
            // colours appear, RS really is rendering our nodes and the gap is only that our BUFFER
            // never reaches the draw params; if the screen stays black, the nodes are being skipped
            // after the ondraw log.  window = RED, content = GREEN.
            if (getenv("WL_BG_COLOR") != nullptr) {
                surfaceNode->SetBackgroundColor(0xFFFF0000u);
                wl_contentNode->SetBackgroundColor(0xFF00FF00u);
            }
            // §377 diagnostic: make our windows fully TRANSPARENT.  The screen goes black the moment
            // our windows exist, and neither the app's frames nor an RS-side background colour ever
            // appears — so before chasing the buffer any further, establish whether that black IS our
            // node.  If alpha 0 reveals the ability's page/launcher, the black is ours (and its
            // content is simply not being drawn); if it stays black, something else is painting it.
            if (getenv("WL_ALPHA0") != nullptr) {
                surfaceNode->SetAlpha(0.0f);
                wl_contentNode->SetAlpha(0.0f);
            }
            surfaceNode->AddChild(wl_contentNode, -1);
        }
        fprintf(stderr, "[WESTLAKE-CONTENTNODE] §347 self-drawing child=%llu under window=%llu (%dx%d)\n",
                (unsigned long long)(wl_contentNode ? wl_contentNode->GetId() : 0),
                (unsigned long long)surfaceNode->GetId(), requestedWidth, requestedHeight);
        fflush(stderr);
    }

    // WESTLAKE §355: PIN every node we create.  Measured timeline (tl2.sh): our nodes are on the RS
    // tree from t≈4s and are GONE at t≈14s — absent from BOTH `RSTree` and `nodeNotOnTree`, i.e.
    // genuinely DESTROYED, not merely detached — while the child is still alive, still logging 1MB/s
    // and still swapping.  An RSSurfaceNode is destroyed server-side when the last CLIENT-side
    // shared_ptr drops, so this decides the question: hold a strong ref forever and see whether the
    // nodes survive.  If they still vanish, the purge is server-side (SCB/SSM) and not our lifetime.
    if (getenv("WL_PIN_NODES") != nullptr) {
        static std::vector<std::shared_ptr<OHOS::Rosen::RSSurfaceNode>> wl_pinned;
        wl_pinned.push_back(surfaceNode);
        if (wl_contentNode != nullptr) { wl_pinned.push_back(wl_contentNode); }
        fprintf(stderr, "[WESTLAKE-PIN] pinned %zu node(s)\n", wl_pinned.size());
        fflush(stderr);
    }

    // WESTLAKE §382: when there is no content child, register the WINDOW node itself as a
    // self-drawing surface.  It is the node that owns the real session and therefore the only one
    // with a genuine non-empty visible region (`VisibleRegion 1: [0,0,1200,1920]`), while the
    // content child always came back `Region [Empty]` / `SrcRect [0,0,0,0]`.  Software composition
    // only (SetHardwareEnabled(false)) — passing true SIGSEGVs render_service (§372).
    if (getenv("WL_SELFDRAW_WINDOW") != nullptr) {
        surfaceNode->SetHardwareEnabled(false);
        surfaceNode->MarkUIHidden(false);
        surfaceNode->SetForceHardwareAndFixRotation(true);
        surfaceNode->SetIsNotifyUIBufferAvailable(true);
        fprintf(stderr, "[WESTLAKE-SELFDRAW] §382 window node %llu registered self-drawing\n",
                (unsigned long long) surfaceNode->GetId());
        fflush(stderr);
    }

    OHOS::Rosen::RSTransaction::FlushImplicitTransaction();
    fprintf(stderr, "[WESTLAKE-RSCOMMIT] flushed RS transaction after node create (id=%llu)\n",
            (unsigned long long) surfaceNode->GetId());
    fflush(stderr);

    // WESTLAKE §269: did RS actually CREATE the node server-side?
    // `RSSurfaceNode::Create` round-trips to RenderService (RSRenderServiceClient::CreateNodeAndSurface)
    // and the producer Surface it returns comes FROM RS. So a non-null GetSurface() proves the node
    // exists server-side; a null one proves RS declined creation -- which would finally name the
    // failure behind §263/§265 (node absent from both the layer list and the node tree) now that the
    // RS connection itself is proven live (§268).
    {
        auto wl_surf = surfaceNode->GetSurface();
        fprintf(stderr, "[WESTLAKE-RSNODE] id=%llu name=%s GetSurface=%p\n",
                (unsigned long long) surfaceNode->GetId(),
                surfaceNode->GetName().c_str(),
                static_cast<void*>(wl_surf.GetRefPtr()));
        fflush(stderr);
    }

    // WESTLAKE §268: is our process even TALKING to RenderService?
    // §267 showed our pid is absent from RS's transactionFlags, which has TWO very different causes:
    //   (a) RS rejects us (no window session)  -> §231 process-identity work is the only fix; or
    //   (b) our process never established an RS connection at all -> fixable here.
    // Distinguish with a ROUND-TRIP call that must reach RenderService and come back.
    {
        static bool wl_probed = false;
        if (!wl_probed) {
            wl_probed = true;
            uint64_t wl_screen = OHOS::Rosen::RSInterfaces::GetInstance().GetDefaultScreenId();
            auto wl_mode = OHOS::Rosen::RSInterfaces::GetInstance().GetScreenActiveMode(wl_screen);
            fprintf(stderr,
                    "[WESTLAKE-RSCONN] GetDefaultScreenId=%llu activeMode=%dx%d refresh=%u\n",
                    (unsigned long long) wl_screen,
                    wl_mode.GetScreenWidth(), wl_mode.GetScreenHeight(),
                    wl_mode.GetScreenRefreshRate());
            fflush(stderr);
        }
    }

    // WESTLAKE §234 (2026-07-22): ATTACH THE SURFACE DIRECTLY TO THE DISPLAY.
    // Both window-creation routes are refused for this process (§226 ERR_INVALID_STATE, §227
    // WM_ERROR_NULLPTR) because it owns no OH ability session -- but a window is only the thing that
    // normally puts a surface into the render tree. The board's librender_service_client.z.so
    // exports `RSSurfaceNode::AttachToDisplay(uint64_t)`, which inserts the node into the display's
    // tree WITHOUT any window/session. hwui already renders into this exact surface, so if the node
    // composites we get a real frame even while the window session keeps failing.
    {
        surfaceNode->SetBounds(0.0f, 0.0f,
                               static_cast<float>(requestedWidth > 0 ? requestedWidth : 1200),
                               static_cast<float>(requestedHeight > 0 ? requestedHeight : 1920));
        surfaceNode->SetFrame(0.0f, 0.0f,
                              static_cast<float>(requestedWidth > 0 ? requestedWidth : 1200),
                              static_cast<float>(requestedHeight > 0 ? requestedHeight : 1920));
        surfaceNode->SetVisible(true);
        // WESTLAKE §271: RAISE Z SO WE ARE NOT OCCLUDED.
        // The RS tree entry for our node shows everything correct EXCEPT the visible region:
        //   Parent[LOGICAL_DISPLAY_NODE], abilityState: foreground, hasConsumer: 1, Alpha: 1.0,
        //   Visible: 1, Bounds[0 0 1200 1920], innerAbsDrawRect [0,0,1200,1920]
        //   ... but  VisibleRegion [Empty]  and  OpaqueRegion [Empty]
        // Correct bounds + empty visible region == FULLY OCCLUDED. We never set a Z, so SceneBoard's
        // windows (SCBDesktop/SCBWallpaper/SCBGestureNavBar, z up to ~4102 in the WMS dump) composite
        // above us and RS culls our node entirely -- which is why it is in the node tree (§269) but
        // never in the LAYER list (§263), and why no buffers are ever consumed (§259/§262).
        // WESTLAKE §284d: give each window a DISTINCT Z.  §271 put every window at exactly
        // 9000, so noice's two full-screen surfaces (session 1 -> MainActivity node,
        // session 2 -> AppIntroActivity node -- both `foreground`, both `shouldPaint_[1]`,
        // both Bounds 1200x1920) are tied, and RS has two identical candidates for the same
        // screen area.  The activity the app actually shows is the LATER one, so bias by
        // sessionId: later session == higher Z == on top.
        // (sessionId is not in scope here, so use a per-call counter: each createSession gets
        // the next Z, i.e. the LATER window ends up on top.)
        static int wl_zseq = 0;
        // §351: with two windows, only ONE presents a frame — the child's second EGLSurface fails
        // (`eglSetDamageRegionKHR` EGL_BAD_ACCESS, then `eglSwapBuffers` err=0x300d EGL_BAD_SURFACE,
        // didSwap=0), so whichever window is on TOP but never swaps paints black over the one that
        // does.  WL_Z_INVERT flips the ordering so the first (successfully swapping) window wins.
        ++wl_zseq;
        surfaceNode->SetPositionZ(getenv("WL_Z_INVERT") != nullptr
                                      ? 9000.0f + static_cast<float>(16 - wl_zseq)
                                      : 9000.0f + static_cast<float>(wl_zseq));
        // WESTLAKE §284j: TELL RS THAT UI FRAMES ARRIVE ON THIS NODE.
        // §284i got our buffers into RS's queue (bufferQueueCache state 0 -> 3) but RS still
        // never ACQUIRES them (SrcRect stays [0,0,0,0], screen black).  An RSSurfaceNode whose
        // content is produced by the client has to advertise that a UI frame is available;
        // without it RS keeps the node on the tree but never pulls a buffer.
        // ★MarkUiFrameAvailable(true) does NOT exist on this board's RSSurfaceNode (it is at
        // rs_surface_node.h:293 but not public here) -- using it fails the TU, which the link
        // then silently drops (§77i).  Only the public notify flag is used.
        surfaceNode->SetIsNotifyUIBufferAvailable(true);
        // §284q TESTED AND REVERTED: `SetBootAnimation(true)` (rs_surface_node.h:223) looked
        // like the session-less compositing path the boot animation uses, but it REGRESSED --
        // noice nodes in the RS tree dropped from 3-5 to 1, `VisibleRegion` went to [Empty], and
        // the screenshot went back to showing the nav bar (i.e. our surface stopped covering the
        // display at all). RS evidently treats a boot-animation node as a special early-boot
        // layer, not as a normal app surface.
        // WESTLAKE §284m: attach to the WINDOW CONTAINER, not straight to the display.
        // §284l found the structural difference: every node RS composites hangs under a
        // CANVAS_NODE (LOGICAL_DISPLAY_NODE -> CANVAS_NODE -> SURFACE_NODE, e.g. SCBWallpaper5),
        // while AttachToDisplay() splices ours directly onto LOGICAL_DISPLAY_NODE -- RS keeps it
        // on the tree and takes its buffers (state=3) but never composites it (SrcRect [0,0,0,0]).
        // rs_surface_node.h also exposes AttachToWindowContainer(ScreenId), which is the path that
        // puts a surface into the screen's window container -- i.e. the hierarchy SCB windows use.
        // §284m TESTED AND REVERTED: `AttachToWindowContainer(screenId)` (rs_surface_node.h:354)
        // looked like the API that would give us SCB's parentage, but on this board it produces
        // the IDENTICAL result -- tried both after AttachToDisplay and ALONE, and in each case
        // the node's Parent stays `LOGICAL_DISPLAY_NODE[4243427688449]` (never a CANVAS_NODE),
        // with the same trav=40/swap=8/SrcRect[0,0,0,0].  So re-parenting needs a real
        // CANVAS_NODE built under the display (or a genuine window session), not this API.
        // WESTLAKE §344: with a genuine session, STOP splicing the node onto the display ourselves.
        // The §284m note above already concluded that re-parenting "needs a real CANVAS_NODE built
        // under the display (or a genuine window session)" — §340-§342 now produce exactly that
        // genuine session (SceneBoard adopts the node: persistId set, its own pid's modifiers
        // applied, VisibleRegion 1: [0,0,1200,1920]).  Self-attaching on top of that leaves the node
        // parented to LOGICAL_DISPLAY_NODE instead of the WindowScene branch SceneBoard manages, and
        // measurably RS still never acquires our buffers: with an UNCONDITIONAL opaque-red clear in
        // SkiaPipeline (§343 WL_TEST_COLOR) the screen stayed pure black, i.e. none of our pixels are
        // presented, while `nodes=1 nothingToDraw=0` proves Android is drawing real content.
        // So on the session path let SceneBoard own placement entirely.
        const bool wl_selfAttach =
            (getenv("WL_SUB_WINDOW") == nullptr) || (getenv("WL_SELF_ATTACH") != nullptr);
        if (wl_selfAttach) {
            surfaceNode->AttachToDisplay(static_cast<uint64_t>(displayId));
            OHOS::Rosen::RSTransaction::FlushImplicitTransaction();
            fprintf(stderr,
                    "[WESTLAKE-RSATTACH] surfaceNode id=%llu attached to display %d (%dx%d)\n",
                    (unsigned long long)surfaceNode->GetId(), displayId,
                    requestedWidth, requestedHeight);
        } else {
            OHOS::Rosen::RSTransaction::FlushImplicitTransaction();
            fprintf(stderr,
                    "[WESTLAKE-RSATTACH] §344 self-attach SKIPPED (session path owns placement) "
                    "id=%llu (%dx%d)\n",
                    (unsigned long long)surfaceNode->GetId(), requestedWidth, requestedHeight);
        }
        fflush(stderr);

        // WESTLAKE §253: stash the IWindow so the visibility callback can be RE-SENT later, from a
        // point where ViewRootImpl.setView() has already completed (§252 sent it too early).
        {
            JNIEnv* wl_genv = nullptr;
            if (jvm != nullptr) {
                jvm->GetEnv(reinterpret_cast<void**>(&wl_genv), JNI_VERSION_1_6);
            }
            // §254: ALWAYS track the CURRENT window, never keep the first one.
            // Measured: dispatchAppVisibility ran 6x but handleAppVisibility NEVER did. In AOSP
            // `W.dispatchAppVisibility` silently no-ops when `mViewAncestor.get()` is null, and the
            // Looper is demonstrably alive (doFrame=4 via posted callbacks) -- so the message was
            // never posted, i.e. the W we held belonged to a DEAD ViewRootImpl. This app has TWO
            // activities/DecorViews (§251), and the old code captured only the FIRST one.
            if (wl_genv != nullptr && androidWindow != nullptr) {
                if (g_wlAppWindow != nullptr) {
                    wl_genv->DeleteGlobalRef(g_wlAppWindow);
                }
                g_wlAppWindow = wl_genv->NewGlobalRef(androidWindow);
            }
        }
        // WESTLAKE §252: TELL THE APP ITS WINDOW IS VISIBLE.
        // §251 measured that the DecorView's LAST setVisibility is 0 (VISIBLE) -- so the decor is
        // visible -- yet performTraversals still takes the !isViewVisible branch (§248). The reason
        // is that ViewRootImpl does NOT read the decor directly:
        //     getHostVisibility() { return (mAppVisible || mForceDecorViewVisibility)
        //                                   ? mView.getVisibility() : View.GONE; }
        // `mAppVisible` is set ONLY by ViewRootImpl.handleAppVisibility(), which is driven by
        // IWindow.dispatchAppVisibility(true) sent by the window manager. We have no real OH window,
        // so that call never arrives, mAppVisible stays false, getHostVisibility() returns GONE and
        // the draw is skipped forever. Send it ourselves -- we already hold the app's IWindow.
        if (androidWindow != nullptr) {
            JNIEnv* wl_venv = nullptr;
            if (jvm != nullptr) {
                jvm->GetEnv(reinterpret_cast<void**>(&wl_venv), JNI_VERSION_1_6);
            }
            if (wl_venv != nullptr) {
                jclass wl_wcls = wl_venv->GetObjectClass(androidWindow);
                jmethodID wl_dav = (wl_wcls != nullptr)
                    ? wl_venv->GetMethodID(wl_wcls, "dispatchAppVisibility", "(Z)V") : nullptr;
                if (wl_dav != nullptr) {
                    wl_venv->CallVoidMethod(androidWindow, wl_dav, JNI_TRUE);
                    if (wl_venv->ExceptionCheck()) {
                        wl_venv->ExceptionDescribe();
                        wl_venv->ExceptionClear();
                        fprintf(stderr, "[WESTLAKE-APPVIS] dispatchAppVisibility(true) THREW\n");
                    } else {
                        fprintf(stderr, "[WESTLAKE-APPVIS] dispatchAppVisibility(true) sent\n");
                    }
                } else {
                    wl_venv->ExceptionClear();
                    fprintf(stderr, "[WESTLAKE-APPVIS] dispatchAppVisibility not found on IWindow\n");
                }
                fflush(stderr);
            }
        }
    }

    // 2026-05-11 G2.14am-PROBE result captured (memory project_g214am_probe_red.md):
    // RED bg + isOpaque=0 → screen all red → hwui buffer is fully transparent
    // (no GL clear, no Skia draws).  Conclusion: hwui produces empty buffer.
    // Probe code removed; next diagnostic phase (G2.14an) intercepts Canvas
    // DrawOps at adapter/shim layer to confirm whether helloworld View.draw
    // submits any draws to BaseCanvas natives.  See compat_shim.cpp.

    // Legacy WindowProperty (NOT V7 WindowSessionProperty). Fewer fields,
    // no SessionInfo / TokenState dance.
    // WESTLAKE §243: CONTAIN the legacy WindowProperty too (§226 recipe).
    // §220 only PINNED this object to dodge its destructor, but it was still heap-allocated with
    // OUR header's sizeof while the BOARD's ctor (libwmutil.z.so) writes into it -- if the board's
    // layout is larger, that overflows the malloc block and corrupts the heap. That is exactly what
    // the current tombstone shows: `#00 get_meta+12 / #01 __libc_free+24` in ld-musl (mallocng's
    // heap-corruption signature) on a secondary thread, which then wedges the main thread in
    // futex_wait so its Looper never pumps and Choreographer.doFrame never runs (§242).
    // Placement-new into an oversized static buffer, pinned, never destroyed -- the same fix that
    // made WindowSessionProperty safe in §226.
    static char wl_legacyPropBuf[8192] __attribute__((aligned(16)));
    static OHOS::Rosen::WindowProperty* wl_legacyProp = nullptr;
    if (wl_legacyProp == nullptr) {
        memset(wl_legacyPropBuf, 0, sizeof(wl_legacyPropBuf));
        wl_legacyProp = new (wl_legacyPropBuf) OHOS::Rosen::WindowProperty();
        for (int i = 0; i < 8; ++i) { wl_legacyProp->IncStrongRef(nullptr); }
    }
    OHOS::sptr<OHOS::Rosen::WindowProperty> property(wl_legacyProp);
    // WESTLAKE §220 (2026-07-22): DELIBERATELY PIN THIS OBJECT.
    // Every call on `property` below succeeds; it is only its RELEASE that faults. The tombstone
    // (cppcrash-2993) is
    //     #00 pc 0 Not mapped
    //     #01 OHOS::RefBase::DecStrongRef+92        (libutils.z.so)
    //     #02 liboh_adapter_bridge.so +0x18a6f8  -> disassembles to
    //         `bl OHOS::sptr<OHOS::Rosen::WindowProperty>::~sptr()`
    //     #03 nativeCreateSession_impl
    // i.e. the last strong ref drops, RefBase calls the virtual OnLastStrongRef, and the vtable slot
    // is 0. WindowProperty's methods are UNDEFINED in this .so and bind at load from the board's
    // libwmutil.z.so, so the class we `new` (our header's size/layout) and the code that runs on it
    // (the board's) can disagree -- classic header-vs-board ABI drift for this port.
    // That crash happens on the thread inside nativeCreateSession while it holds the window lock,
    // which wedges the main thread in futex_wait and is why NO WINDOW IS EVER CREATED (§218).
    // Taking one extra strong ref means the count never reaches 0, so OnLastStrongRef is never
    // called. Costs one small leak per window creation; unblocks window creation entirely.
    property->SetWindowName(windowName);
    property->SetWindowType(ohType);
    // 2026-06-04 NATIVE TRANSLUCENT-DIALOG FIX: parent the sub-window to the main
    // window so OHOS composites it above the parent and keeps the parent visible.
    if (asSubWindow) {
        property->SetParentId(subParentWinId);
        LOGI("createSession: SetParentId(%{public}u) on SUB_WINDOW", subParentWinId);
    }
    property->SetWindowMode(OHOS::Rosen::WindowMode::WINDOW_MODE_FULLSCREEN);
    OHOS::Rosen::Rect rect{0, 0,
        static_cast<uint32_t>(requestedWidth), static_cast<uint32_t>(requestedHeight)};
    property->SetWindowRect(rect);
    property->SetRequestRect(rect);  // legacy WMS uses requestRect for AddWindow
    property->SetOriginRect(rect);
    property->SetDisplayId(displayId);

    // Bug B fix (2026-05-18): SetTokenState(true) when valid abilityToken exists.
    //
    // Root cause: WMS server stub (foundation/window/window_manager/wmserver/src/
    // zidl/window_manager_stub.cpp:49-51) gates Parcel token deserialization on
    // property.GetTokenState():
    //   sptr<IRemoteObject> token = nullptr;
    //   if (windowProperty && windowProperty->GetTokenState()) {
    //       token = data.ReadRemoteObject();
    //   }
    //   ... CreateWindow(..., token);
    // adapter has been passing token through wmsProxy->CreateWindow but keeping
    // property.tokenState_=false (default) → WMS server side reads token=nullptr
    // → node->abilityToken_=nullptr → FindWindowNodeWithToken(token) returns null
    // both for (a) WindowController::CreateWindow's "replace starting window
    // with main window" replacement path (line 268), and (b) CancelStartingWindow
    // on ability death (window_controller.cpp:117). Net effect: every cold start
    // creates an orphan leashWindow+startingWindow pair that force-stop never
    // cleans up — the "white block covers desktop" leak.
    //
    // OH native WindowImpl::Create (~wm/src/window_impl.cpp:1554-1557) sets
    // tokenState_=true whenever context_->GetToken() returns non-null. We mirror
    // that. The earlier "§3.1.4.6.6 keep TokenState=false matrix" comment was
    // based on an assumption empirically invalidated by hilog evidence.
    if (token != nullptr) {
        property->SetTokenState(true);
    }

    // 2026-05-19: Pre-set NEED_AVOID flag so adapter's property matches the OH
    // starting window state for API<10 apps (foundation/window/window_manager/
    // wmserver/src/starting_window.cpp:262 ChangePropertyByApiVersion adds
    // WINDOW_FLAG_NEED_AVOID by default).  When AddWindow's CopyFrom copies
    // adapter property into node, the layout policy first computes safe-area
    // winRect [0,72,720,1136] (matching the inherited leash bounds), so
    // node.windowRect aligns with leash from the start — no divergence.  Then
    // a single post-AddWindow UpdateProperty(FLAGS=0) cleanly transitions
    // safe-area → fullscreen with a real diff that drives leash SetBounds.
    //
    // Without this pre-set: CopyFrom would write node.windowRect=[0,0,720,1280]
    // (adapter's fullscreen rect) BUT leash stays at safe-area; the divergence
    // makes single UpdateProperty(FLAGS=0) ineffective (winRect == preRect → no
    // diff → leash SetBounds skipped → black band remains).
    //
    // Mimics how OH's normal-app boot flow behaves: starting window is safe-area
    // with NEED_AVOID; ArkUI's WindowImpl then optionally calls
    // SetLayoutFullScreen(true) to escape into fullscreen.  Adapter does the
    // equivalent unconditionally because AOSP DecorView always assumes
    // fullscreen window (no app-level opt-in API for that — it's the baseline).
    property->SetWindowFlags(static_cast<uint32_t>(
        OHOS::Rosen::WindowFlag::WINDOW_FLAG_NEED_AVOID));

    // 2026-06-04 SYSTEM-BAR FIX: disable the OHOS status bar + navigation bar
    // for this window (immersive). AOSP DecorView assumes a fullscreen window
    // and the adapter doesn't dispatch WindowInsets, so with the OH system bars
    // SHOWN, the status bar overlays noice's toolbar and the nav bar (◁○□)
    // overlays noice's BottomNavigationView at y~1218 (occluding + making the
    // tabs untappable). noice has its own bottom-nav menu, so the redundant OH
    // bars are turned off → noice uses the full [0,0,720,1280] cleanly, its 5-
    // tab menu is visible at the bottom and reachable. (Addresses "window too
    // big", "◁○□ overlaid on the menu", and "two navs".)
    {
        OHOS::Rosen::SystemBarProperty barOff(false, 0x00000000, 0x00000000);
        property->SetSystemBarProperty(OHOS::Rosen::WindowType::WINDOW_TYPE_STATUS_BAR, barOff);
        property->SetSystemBarProperty(OHOS::Rosen::WindowType::WINDOW_TYPE_NAVIGATION_BAR, barOff);
        LOGI("createSession: disabled OH status+navigation system bars (immersive)");
    }

    auto wmsInterface = OHOS::iface_cast<OHOS::Rosen::IWindowManager>(ssmProxy_);
    if (wmsInterface == nullptr) {
        LOGE("createSession: Failed to cast to IWindowManager");
        result.wsErr = 1001;
        return result;
    }

    // WESTLAKE §225 (2026-07-22) — V7 PATH ATTEMPTED, REVERTED, kept as notes.
    // Everything needed is now in place: sceneProxy_ is a verified ISceneSessionManager (§224), the
    // ISessionStage / IWindowEventChannel stubs already exist and compile (session_stage_adapter.cpp,
    // window_event_channel_adapter.cpp), and the wire format is known:
    //   token u"OHOS.ISceneSessionManager"; WriteRemoteObject(sessionStage->AsObject());
    //   WriteRemoteObject(eventChannel->AsObject()); surfaceNode->Marshalling(data);
    //   WriteStrongParcelable(property /* WindowSessionProperty */); [token if non-null];
    //   SendRequest(TRANS_ID_CREATE_AND_CONNECT_SPECIFIC_SESSION = 0);
    //   reply: persistentId = ReadInt32(); session = ReadRemoteObject();
    // BLOCKER: `new OHOS::Rosen::WindowSessionProperty()` CRASHES on destruction --
    //   cppcrash-4637: SIGSEGV@0x800000034
    //   #00 OHOS::Rosen::WindowSessionProperty::~WindowSessionProperty  (libwindow_scene_common.z.so)
    //   #01 virtual thunk to ~WindowSessionProperty
    //   #02/#03 liboh_adapter_bridge.so -> nativeCreateSession_impl
    // i.e. the SAME header-vs-board ABI drift as §220's WindowProperty: we allocate with OUR
    // header's layout/size and the board's constructor+destructor run on it. An IncStrongRef pin does
    // NOT help here (the destructor is reached anyway, via a virtual thunk).
    // NEXT: do not `new` this type from our side. Options: (a) obtain a property object from the
    // board (a factory/exported helper in libwindow_scene_common.z.so), (b) marshal the property
    // bytes by hand into the parcel to match the server's Unmarshalling, or (c) verify our header
    // really matches this build (sizeof/vtable) before constructing it.
    // WESTLAKE §226 (2026-07-22) — V7 RETRY: ABI drift SOLVED, but the SERVER REJECTS the request.
    // Containing the allocation DOES work: placement-new WindowSessionProperty into an 8 KB static
    // buffer (+ a hard IncStrongRef pin, never destroyed) removed §225's ~WindowSessionProperty
    // crash entirely -- "[WESTLAKE-WMC] V7 property ready at 0x7f215a0cc0" and no tombstone.
    // The FULL parcel then marshalled successfully (`wrote=1`, i.e. interface token + sessionStage +
    // eventChannel + surfaceNode->Marshalling + WriteStrongParcelable(property) all OK) and the IPC
    // was delivered. The SceneSessionManager answered:
    //     SendRequest(TRANS_ID_CREATE_AND_CONNECT_SPECIFIC_SESSION) -> err=10 = ERR_INVALID_STATE
    //     (ipc_types.h: NULL_OBJECT=7, UNKNOWN_REASON=8, INVALID_REPLY=9, INVALID_STATE=10)
    // i.e. this is no longer a marshalling/ABI problem -- the server refuses to create a session for
    // this process. Consistent with the long-standing note that OH StartAbility fails for our
    // UNREGISTERED bundle: SceneSessionManager has no SessionInfo/ability record for us, so there is
    // no valid state in which to attach a window.
    // Reverted to notes because keeping the call live also stopped nativeCreateSession from
    // returning (POST-native disappeared) with no crash -- a hang later in the path.
    // NEXT: make the app known to OH before asking for a session (register a real ability/SessionInfo,
    // or attach to an existing SCB session), rather than more marshalling work.
    // WESTLAKE §227 (2026-07-22) — TRIED OH's OWN CLIENT API `Window::Create`; ALSO REFUSED.
    // `OHOS::Rosen::Window::Create(name, option, context=nullptr, err)` is exported by the board's
    // libwm.z.so and would normally do the session registration itself, with a FLOAT/system window
    // type needing no AbilityRuntime::Context. Applied the §226 containment recipe to WindowOption
    // (placement-new into an oversized buffer, pinned, never destroyed) so the board type is safe.
    // RESULT: `[WESTLAKE-WMC] Window::Create err=1001 win=0` -> WM_ERROR_NULLPTR (base 1000),
    // i.e. it fails EARLY and returns null; window count in WMS stayed 21.
    // ⇒ Together with §226's ERR_INVALID_STATE this is consistent and conclusive: BOTH the raw SSM
    // IPC and OH's own client entry point refuse to create a window for this process. The blocker is
    // that the process is not a registered OH app/ability, NOT the window plumbing.
    // NEXT: register with OH first (real ability/SessionInfo, or attach to an existing SCB session);
    // only then will either path succeed.
    // WESTLAKE §230 (2026-07-22) — V7 re-enabled after registration, then REVERTED. Two findings:
    //  1. Even with the bundle installed (§230) SceneSessionManager still answers
    //     `err=10 ERR_INVALID_STATE` -- registration alone is not enough, the session must belong to
    //     an AMS-started ability process (see §231 notes).
    //  2. ★It also BROKE STARTUP: `SessionStageAdapter`'s constructor logs
    //     "[6/OH_SessionStageAdapter] Failed to find SessionStageBridge class" and throws
    //     java.lang.NoClassDefFoundError, which propagates out of nativeCreateSession ->
    //     addToDisplayAsUser -> ActivityThread.main -> [INITCHILD-FAIL]. The Java class
    //     `SessionStageBridge` (bridge-build/src/framework/window/java/SessionStageBridge.java) is
    //     NOT on the boot classpath, so the V7 stubs cannot be constructed at all yet.
    // ⇒ Before re-enabling V7, PUT `SessionStageBridge` (and the event-channel bridge) ON THE BCP --
    //    e.g. inject as classes2/3.dex into an early BCP jar (the §210 trick works for NEW classes;
    //    it only fails for classes the boot image already owns).
    // WESTLAKE §233: V7 retry — SessionStageBridge is now ON THE BCP (injected as classes4.dex into
    // apache-xml.jar), which was the §230 blocker ("Failed to find SessionStageBridge class" ->
    // NoClassDefFoundError). Containment recipe from §226 retained for the board types.
    if (sceneProxy_ != nullptr) {
        static char wl_propbuf[8192] __attribute__((aligned(16)));
        static OHOS::Rosen::WindowSessionProperty* wl_prop = nullptr;
        if (wl_prop == nullptr) {
            memset(wl_propbuf, 0, sizeof(wl_propbuf));
            wl_prop = new (wl_propbuf) OHOS::Rosen::WindowSessionProperty();
            for (int i = 0; i < 8; ++i) { wl_prop->IncStrongRef(nullptr); }
        }
        wl_prop->SetWindowName(windowName);
        wl_prop->SetWindowType(ohType);
        wl_prop->SetWindowRect(rect);
        // WESTLAKE §339: also set the REQUEST rect. §338 got real sessions (persistentId 491/492)
        // and ISession::Foreground returned WS_OK, and our two windows finally appear in the WMS
        // list as Type 1001 (APP_SUB_WINDOW) — but with rect [0 0 0 0] and ZOrd -1, so RS still has
        // nothing to make visible. SceneBoard lays a window out from its REQUEST rect; the legacy
        // AddWindow path here always set both (SetWindowRect + SetRequestRect) while this V7 path
        // set only SetWindowRect, so the session was created with a zero-size request.
        wl_prop->SetRequestRect(rect);
        wl_prop->SetDisplayId(displayId);
        // §340: anchor the sub-window to the ability's real main window, else SceneBoard has nothing
        // to lay it out against (rect [0 0 0 0] / ZOrd -1 / dropped from the window list).
        // WESTLAKE §363: do not take FOCUS.  Measured: creating the session (even with §338
        // Foreground suppressed) makes AMS kill the parent ability with
        // `sysfreeze … Reason: LIFECYCLE_TIMEOUT / ability:EntryAbility foreground timeout`, and the
        // same run WITHOUT any session produces no sysfreeze at all — so it is the session, not the
        // child's CPU load.  A sub-window that grabs focus stalls the parent's foreground handshake,
        // and when the parent dies its WindowScene (and our nodes under it) go with it.
        // Our windows are driven by the tap channel, not WMS focus, so focus is not needed.
        // WESTLAKE §366: fill in the session's IDENTITY.  We never called SetSessionInfo, so the
        // property carried an all-empty SessionInfo — and SSM copies exactly those fields into the
        // SessionInfo it hands to RequestSceneSession/NotifyCreateSubSession, which is what
        // SceneBoard's JS uses to attach the sub-session to its parent WindowScene.  With no bundle,
        // module or ability name there is nothing for SCB to attach it to, which fits the measured
        // symptom: the session is created and the node is mounted, SCB never calls our session stage
        // at all (STAGE=0 across every instrumented callback), and ~5s later the PARENT ability is
        // killed with LIFECYCLE_TIMEOUT because its foreground handshake never completes.
        {
            OHOS::Rosen::SessionInfo wl_si;
            // §369: the session's identity decides whether AMS tries to drive an ABILITY lifecycle
            // for it.  Creating the session alone (Foreground suppressed) still kills the parent with
            // `LIFECYCLE_TIMEOUT / ability:EntryAbility foreground timeout`, so SCB/AMS is resolving
            // our session to noice's real EntryAbility and then waiting for a foreground ack that
            // never comes.  WL_SESSION_BUNDLE lets us hand it a name with no ability behind it.
            const char* wl_sb = getenv("WL_SESSION_BUNDLE");
            wl_si.bundleName_  = (wl_sb != nullptr && *wl_sb != '\0') ? std::string(wl_sb)
                               : (bundleName.empty() ? std::string("com.github.ashutoshgngwr.noice")
                                                     : bundleName);
            wl_si.moduleName_  = "entry";
            wl_si.abilityName_ = (wl_sb != nullptr && *wl_sb != '\0') ? std::string("WlSurface")
                               : (abilityName.empty() ? std::string("EntryAbility") : abilityName);
            wl_si.windowType_  = static_cast<uint32_t>(ohType);
            wl_si.screenId_    = static_cast<uint64_t>(displayId);
            wl_prop->SetSessionInfo(wl_si);
            fprintf(stderr, "[WESTLAKE-WMC] §366 SessionInfo bundle=%s module=%s ability=%s type=%u\n",
                    wl_si.bundleName_.c_str(), wl_si.moduleName_.c_str(),
                    wl_si.abilityName_.c_str(), wl_si.windowType_);
            fflush(stderr);
        }
        if (getenv("WL_FOCUSABLE") == nullptr) {
            wl_prop->SetFocusable(false);
            wl_prop->SetFocusableOnShow(false);
        }
        const int32_t wl_parentId = wl_query_ability_main_window_id(sceneProxy_);
        if (wl_parentId > 0) {
            wl_prop->SetParentId(wl_parentId);
            wl_prop->SetParentPersistentId(wl_parentId);
        }

        OHOS::sptr<oh_adapter::SessionStageAdapter> sessionStage =
            new oh_adapter::SessionStageAdapter(jvm, androidWindow);
        OHOS::sptr<oh_adapter::WindowEventChannelAdapter> eventChannel =
            new oh_adapter::WindowEventChannelAdapter(jvm);
        sessionStage->IncStrongRef(nullptr);
        eventChannel->IncStrongRef(nullptr);
        // §233: a pending Java exception from the stub ctors (e.g. NoClassDefFoundError) must NOT be
        // allowed to propagate out of this JNI call -- that is what killed startup in §230.
        JNIEnv* wl_env = nullptr;
        if (jvm != nullptr) {
            jvm->GetEnv(reinterpret_cast<void**>(&wl_env), JNI_VERSION_1_6);
        }
        if (wl_env != nullptr && wl_env->ExceptionCheck()) {
            wl_env->ExceptionDescribe();
            wl_env->ExceptionClear();
            fprintf(stderr, "[WESTLAKE-WMC] V7 stub ctor threw; cleared, falling back\n");
            fflush(stderr);
        } else {
            OHOS::MessageParcel data;
            OHOS::MessageParcel reply;
            OHOS::MessageOption option;
            OHOS::sptr<OHOS::Rosen::WindowSessionProperty> wl_propSptr(wl_prop);
            bool wrote = data.WriteInterfaceToken(u"OHOS.ISceneSessionManager") &&
                         data.WriteRemoteObject(sessionStage->AsObject()) &&
                         data.WriteRemoteObject(eventChannel->AsObject()) &&
                         surfaceNode->Marshalling(data) &&
                         data.WriteStrongParcelable(wl_propSptr);
            int32_t v7err = wrote ? sceneProxy_->SendRequest(0, data, reply, option) : -1;
            int32_t persistentId = 0;
            OHOS::sptr<OHOS::IRemoteObject> sessionObject = nullptr;
            if (v7err == 0) {
                persistentId = reply.ReadInt32();
                sessionObject = reply.ReadRemoteObject();
            }
            fprintf(stderr, "[WESTLAKE-WMC] V7 wrote=%d err=%d persistentId=%d session=%p\n",
                    wrote ? 1 : 0, v7err, persistentId,
                    static_cast<void*>(sessionObject.GetRefPtr()));
            fflush(stderr);
            // WESTLAKE §566: start the input side-channels REGARDLESS of whether SceneBoard
            // adopted us.  They used to live only in the `v7err == 0` branch below, which made
            // input hostage to session adoption — and adoption needs a parent main window that
            // the entry.hap ability never provides (it is a launcher-icon stub: `aa dump -l`
            // reports `window attached #0` even when FOREGROUND).  After a reboot that reliably
            // yields `V7 err=10` / `parentId=0`, and then the app RENDERS PERFECTLY but is
            // completely undrivable — 7 launches in a row looked like an input regression and
            // were really this gate.  The channels only poll two files and inject through
            // ViewRootImpl, so they have no dependency on the session (the §391 comment below
            // already said as much); both starters are idempotent (atomic once-flag), so the
            // call in the adopted path is now simply a no-op.
            if (getenv("WL_MMI") == nullptr) {
                OHInputBridge::getInstance().startTapControlChannel();
                OHInputBridge::getInstance().startTextControlChannel();
                fprintf(stderr, "[WESTLAKE-WMC] §566 tap/text side-channels started (ungated)\n");
                fflush(stderr);
            }
            if (v7err == 0 && sessionObject != nullptr) {
                // WESTLAKE §338: SHOW the session we just created.
                // §337 got CreateAndConnectSpecificSession to SUCCEED (persistentId 488/489, real
                // ISession objects) — but a freshly created session is NOT visible: SceneBoard only
                // gives it a layout/visible region once the client drives it to the foreground.
                // Nothing here ever did that, so RS kept our node at VisibleRegion [Empty] and the
                // window stayed black even with a valid session. Send ISession::Foreground.
                // Parcel per SessionStub::HandleForeground (session_stub.cpp:375):
                //   bool hasProperty -> [WindowSessionProperty] -> bool isFromClient -> string token
                // Code = SessionInterfaceCode::TRANS_ID_FOREGROUND = 1 (TRANS_ID_CONNECT = 0).
                // §361: bisect what kills the PARENT ability.  Without any SSM session everything is
                // stable (our nodes, the ability's WindowScene and its process all survive, 20 swaps);
                // the moment we create sessions the ability dies ~5s later and takes our windows with
                // it.  Gate the two session-state calls so each can be tested on its own.
                if (getenv("WL_NO_FOREGROUND") == nullptr) {
                    OHOS::MessageParcel fdata;
                    OHOS::MessageParcel freply;
                    OHOS::MessageOption foption;
                    bool fwrote = fdata.WriteInterfaceToken(u"OHOS.ISession") &&
                                  fdata.WriteBool(true) &&
                                  fdata.WriteStrongParcelable(wl_propSptr) &&
                                  fdata.WriteBool(true) &&              // isFromClient
                                  fdata.WriteString("");                // identityToken
                    int32_t ferr = fwrote ? sessionObject->SendRequest(1, fdata, freply, foption) : -1;
                    int32_t fret = (ferr == 0) ? freply.ReadInt32() : -1;
                    fprintf(stderr,
                            "[WESTLAKE-WMC] §338 Foreground(persistentId=%d) wrote=%d err=%d ret=%d\n",
                            persistentId, fwrote ? 1 : 0, ferr, fret);
                    fflush(stderr);
                }
                // WESTLAKE §345: tell the session the app has DRAWN, and drop the starting window.
                // §343/§344 proved RS never presents our pixels: with an UNCONDITIONAL opaque-red
                // clear in SkiaPipeline the screen stayed pure black, even though the session is real
                // (persistId, VisibleRegion 1: [0,0,1200,1920]) and Android is drawing real content
                // (nodes=1 nothingToDraw=0, didSwap=1).  Something opaque is composited OVER us — and
                // SceneBoard's STARTING WINDOW is exactly that: it covers a launching app until the
                // client reports its first frame.  A real client drives ISession for this; we never
                // did, so the placeholder is never removed.
                //   TRANS_ID_DRAWING_COMPLETED = 10, TRANS_ID_APP_REMOVE_STARTING_WINDOW = 11
                //   (session_ipc_interface_code.h) — both take only the interface token.
                {
                    // §345b MEASURED: sending BOTH removed our windows from the RS tree entirely
                    // (0 nodes, launcher visible again) — RemoveStartingWindow tears the session
                    // down rather than revealing content beneath it.  Keep DrawingCompleted only;
                    // set WL_REMOVE_STARTING to re-enable the second call for experiments.
                    const int wl_calls = (getenv("WL_DRAWCOMPLETE") == nullptr) ? 0
                                       : ((getenv("WL_REMOVE_STARTING") != nullptr) ? 2 : 1);
                    for (int wl_i = 0; wl_i < wl_calls; ++wl_i) {
                        const uint32_t wl_code = (wl_i == 0) ? 10u : 11u;
                        OHOS::MessageParcel ddata;
                        OHOS::MessageParcel dreply;
                        OHOS::MessageOption doption;
                        bool dwrote = ddata.WriteInterfaceToken(u"OHOS.ISession");
                        int32_t derr = dwrote ? sessionObject->SendRequest(wl_code, ddata, dreply,
                                                                          doption) : -1;
                        int32_t dret = (derr == 0) ? dreply.ReadInt32() : -1;
                        fprintf(stderr,
                                "[WESTLAKE-WMC] §345 %s(persistentId=%d) err=%d ret=%d\n",
                                (wl_i == 0) ? "DrawingCompleted" : "RemoveStartingWindow",
                                persistentId, derr, dret);
                    }
                    fflush(stderr);
                }
                // WESTLAKE §385: RAISE THE SUB-WINDOW TO THE TOP OF THE APP.
                // The hitrace finally showed RS genuinely drawing our buffer —
                //   OnDraw[..._content] -> RSUniRenderEngine::DrawSurfaceNodeWithParams
                //                       -> RSBaseRenderEngine::DrawImage(GPU) -> BuildFromTexture
                // so the pixels ARE composited; they are simply BEHIND the parent. The RSTree shows
                // why: we set `pid:<ours>->PositionZ[9001]` but SceneBoard overrides it with
                // `pid:<scb>->PositionZ[0.0]`, so SCB stacks our sub-window under the ability's own
                // window (`noice0`) and all we ever see is that window's page.
                // `ISession::TRANS_ID_RAISE_TO_APP_TOP` (=102) is the call a real sub-window client
                // uses to be stacked above its parent; the stub takes only the interface token.
                // WESTLAKE §400: optionally re-raise ONE named activity's window a few seconds after
                // creation.  noice creates BOTH AppIntroActivity and MainActivity windows at startup
                // (both get sessions and both are on the RS tree), and §385 raises them in creation
                // order, so AppIntro ends up on top.  Tapping SKIP to get past it is impossible on
                // this runtime — libart stubs MethodHandles (`[PFCUT] MethodHandles.lookup
                // intrinsic`), so noice's Kotlin lambda click-listeners NPE in
                // MethodType$ConcurrentWeakInternSet and abort the child.  Raising MainActivity
                // directly sidesteps the intro entirely.
                if (const char* wl_top = getenv("WL_TOP_ACTIVITY")) {
                    if (!abilityName.empty() && abilityName.find(wl_top) != std::string::npos) {
                        OHOS::sptr<OHOS::IRemoteObject> wl_sess = sessionObject;
                        std::thread([wl_sess, persistentId]() {
                            sleep(8);
                            OHOS::MessageParcel d, r; OHOS::MessageOption o;
                            if (d.WriteInterfaceToken(u"OHOS.ISession")) {
                                int32_t e = wl_sess->SendRequest(102, d, r, o);
                                fprintf(stderr, "[WESTLAKE-WMC] §400 re-raise id=%d err=%d\n",
                                        persistentId, e);
                                fflush(stderr);
                            }
                        }).detach();
                    }
                }
                // §401: when WL_TOP_ACTIVITY names an activity, ONLY that activity's window is
                // raised — raising AppIntro after MainActivity is what buries MainActivity, and
                // re-raising later (§400) does not reorder sibling sub-windows.
                const char* wl_only = getenv("WL_TOP_ACTIVITY");
                const bool wl_skipRaise =
                    (wl_only != nullptr && *wl_only != '\0' &&
                     (abilityName.empty() || abilityName.find(wl_only) == std::string::npos));
                if (wl_skipRaise) {
                    fprintf(stderr, "[WESTLAKE-WMC] §401 NOT raising %s (top=%s)\n",
                            abilityName.c_str(), wl_only);
                    fflush(stderr);
                }
                if (!wl_skipRaise && getenv("WL_NO_RAISE") == nullptr) {
                    OHOS::MessageParcel rdata;
                    OHOS::MessageParcel rreply;
                    OHOS::MessageOption roption;
                    bool rwrote = rdata.WriteInterfaceToken(u"OHOS.ISession");
                    int32_t rerr = rwrote ? sessionObject->SendRequest(102, rdata, rreply, roption) : -1;
                    int32_t rret = (rerr == 0) ? rreply.ReadInt32() : -1;
                    fprintf(stderr, "[WESTLAKE-WMC] §385 RaiseToAppTop(persistentId=%d) err=%d ret=%d\n",
                            persistentId, rerr, rret);
                    fflush(stderr);
                }
                // WESTLAKE §342: REGISTER the session before returning.
                // §341 finally got a genuine, SceneBoard-adopted window out of this path
                // (persistId 497/498, VisibleRegion 1: [0,0,1200,1920], PositionZ 9001/9002) — but the
                // screen still showed the ability's page through it, because this early return skipped
                // the sessions_ bookkeeping that the legacy path below does.  With no SessionEntry,
                // `oh_wm_get_native_window(498) returned 0`, so hwui was never handed an ANativeWindow
                // and drew nothing at all (NWGEOM=0, HWSWAP=0): a perfectly composited EMPTY window.
                // Mirror the legacy registration so the render path can find this session.
                {
                    std::lock_guard<std::recursive_mutex> lock(sessionMutex_);
                    SessionEntry entry{};
                    entry.sessionId      = persistentId;
                    entry.surfaceNodeId  = static_cast<int64_t>(surfaceNode->GetId());
                    entry.sessionProxy   = nullptr;
                    entry.sessionRemote  = sessionObject;   // §536: keep it, teardown needs it
                    entry.stageAdapter   = sessionStage;
                    entry.eventChannel   = eventChannel;
                    // §347: hwui renders into the SELF_DRAWING child (RS composites that from its
                    // buffer); the APP_WINDOW_NODE parent stays the session's window node.
                    // The parent is only a local shared_ptr here, so keep a strong ref of its own —
                    // dropping it would destroy the very node the session was built on.
                    // §538: this used to be a FUNCTION-LOCAL static map that nothing ever erased,
                    // so the window node outlived its session forever — a dismissed dialog kept
                    // compositing its last frame even after §536 tore the OH session down. Keep
                    // the strong ref (still required), but somewhere destroySession() can reach.
                    if (wl_contentNode != nullptr) {
                        wl_window_nodes()[persistentId] = surfaceNode;
                    }
                    entry.surfaceNode    = (wl_contentNode != nullptr) ? wl_contentNode : surfaceNode;
                    entry.windowId       = static_cast<uint32_t>(persistentId);
                    entry.windowProperty = property;
                    entry.wmsShown       = true;   // §338 Foreground succeeded
                    entry.isMainWindow   = !asSubWindow;
                    entry.parentWindowId = subParentWinId;
                    entry.abilityName    = abilityName;
                    sessions_[persistentId] = entry;
                    // §392: record this as the foreground session even though it is registered as a
                    // SUB window (§337 has to ask for APP_SUB_WINDOW for SSM to accept us at all).
                    // Without it `dispatchTouchViaViewRoot` logs `resolveTouchInjector failed` and
                    // the tap side-channel cannot drive the UI — every window here is ours anyway.
                    g_fgMainSession.store(persistentId);
                }
                // WESTLAKE §387: CPU-FILL PROBE — bypass hwui/EGL/GPU entirely.
                // State of play: RS mounts our node correctly, acquires our buffers and calls
                // RSUniRenderEngine::DrawSurfaceNodeWithParams on them (verified by hitrace in BOTH
                // the self-drawing-child and the window-node configurations), yet the screen stays
                // black — even for a raw glClear(red) whose pixels we read back as R255 G0 B0 A255
                // immediately before eglSwapBuffers.  This probe removes the GPU from the picture:
                // request a buffer straight from the producer, memset it opaque RED on the CPU, and
                // flush it.  If THAT appears, the fault is in the GPU-produced buffer (acquire fence
                // / usage flags / texture import); if it is still black, the fault is in how RS
                // samples any buffer we give it.
                if (getenv("WL_CPU_FILL") != nullptr) {
                    const int32_t wl_sid = persistentId;
                    std::thread([wl_sid]() {
                        void* wl_nwv = nullptr;
                        for (int i = 0; i < 40 && wl_nwv == nullptr; ++i) {
                            wl_nwv = OHWindowManagerClient::getInstance().getOhNativeWindow(wl_sid);
                            if (wl_nwv == nullptr) { usleep(200 * 1000); }
                        }
                        if (wl_nwv == nullptr) {
                            fprintf(stderr, "[WESTLAKE-CPUFILL] no native window for session %d\n", wl_sid);
                            fflush(stderr);
                            return;
                        }
                        OHNativeWindow* wl_nw = reinterpret_cast<OHNativeWindow*>(wl_nwv);
                        // §390: log exactly what this thread manages to do — §389 showed the same
                        // SET_USAGE returning 40001000 (GSERROR_INVALID_ARGUMENTS) from libhwui, so
                        // establish whether it is the usage call, the RequestBuffer calls, or merely
                        // this thread's repeated getOhNativeWindow() that makes the app visible.
                        const int wl_mode = atoi(getenv("WL_CPU_FILL"));
                        uint64_t wl_ub = 0;
                        OH_NativeWindow_NativeWindowHandleOpt(wl_nw, /*GET_USAGE*/ 4, &wl_ub);
                        const int32_t wl_urc =
                                OH_NativeWindow_NativeWindowHandleOpt(wl_nw, /*SET_USAGE*/ 5, (uint64_t) 0x30BULL);
                        uint64_t wl_ua2 = 0;
                        OH_NativeWindow_NativeWindowHandleOpt(wl_nw, /*GET_USAGE*/ 4, &wl_ua2);
                        fprintf(stderr, "[WESTLAKE-CPUFILL] mode=%d nw=%p usage 0x%llx -> rc=%d now=0x%llx\n",
                                wl_mode, (void*) wl_nw, (unsigned long long) wl_ub, wl_urc,
                                (unsigned long long) wl_ua2);
                        fflush(stderr);
                        if (wl_mode == 2) {   // mode 2: ONLY re-resolve the window, touch nothing else
                            for (int f = 0; f < 120; ++f) {
                                OHWindowManagerClient::getInstance().getOhNativeWindow(wl_sid);
                                usleep(60 * 1000);
                            }
                            fprintf(stderr, "[WESTLAKE-CPUFILL] mode2 done\n"); fflush(stderr);
                            return;
                        }
                        for (int f = 0; f < 120; ++f) {
                            OHNativeWindowBuffer* wl_buf = nullptr;
                            int wl_fence = -1;
                            int wl_rc = OH_NativeWindow_NativeWindowRequestBuffer(wl_nw, &wl_buf, &wl_fence);
                            if (wl_rc != 0 || wl_buf == nullptr) {
                                if (f < 3) {
                                    fprintf(stderr, "[WESTLAKE-CPUFILL] RequestBuffer rc=%d\n", wl_rc);
                                    fflush(stderr);
                                }
                                usleep(50 * 1000);
                                continue;
                            }
                            BufferHandle* wl_h = OH_NativeWindow_GetBufferHandleFromNative(wl_buf);
                            if (wl_h != nullptr && wl_h->virAddr != nullptr && wl_h->size > 0) {
                                // opaque RED in RGBA_8888 little-endian byte order R,G,B,A
                                uint8_t* wl_p = static_cast<uint8_t*>(wl_h->virAddr);
                                for (int wl_i = 0; wl_i + 3 < wl_h->size; wl_i += 4) {
                                    wl_p[wl_i + 0] = 0xFF; wl_p[wl_i + 1] = 0x00;
                                    wl_p[wl_i + 2] = 0x00; wl_p[wl_i + 3] = 0xFF;
                                }
                            }
                            Region wl_r{};
                            Region::Rect wl_rect{0, 0, 1200, 1920};
                            wl_r.rectNumber = 1; wl_r.rects = &wl_rect;
                            int wl_frc = OH_NativeWindow_NativeWindowFlushBuffer(wl_nw, wl_buf, -1, wl_r);
                            if (f < 3) {
                                fprintf(stderr,
                                        "[WESTLAKE-CPUFILL] #%d filled+flushed rc=%d virAddr=%p size=%d\n",
                                        f, wl_frc, wl_h ? wl_h->virAddr : nullptr, wl_h ? wl_h->size : 0);
                                fflush(stderr);
                            }
                            usleep(60 * 1000);
                        }
                        fprintf(stderr, "[WESTLAKE-CPUFILL] done session %d\n", wl_sid);
                        fflush(stderr);
                    }).detach();
                }
                oh_wm_set_last_session(persistentId);
                // §342b: do NOT subscribe MMI here by default.  Measured twice: this call SIGBUSes
                // the child with BUS_ADRALN inside libeventhandler's EventHandler(shared_ptr<EventRunner>)
                // ctor — the same ABI-boundary misalignment family as §296/§319, reached because the
                // legacy path never actually got this far in the child.  Touch already arrives through
                // the tap-control channel the child starts (/data/local/tmp/noice_tap), so keep the
                // session registration (the part that unblocks rendering) and leave MMI opt-in.
                //   [WESTLAKE-CHILDSEGV] #0 sig=7 code=1 ... libeventhandler.z.so ... EventHandler
                //     fr01 OHInputBridge::subscribeMmi(int)  fr02 OHWindowManagerClient::createSession
                if (getenv("WL_MMI") != nullptr) {
                    OHInputBridge::getInstance().subscribeMmi(persistentId);
                } else {
                    // WESTLAKE §391: start ONLY the file-backed input side-channels.
                    // `subscribeMmi` SIGBUSes this child inside libeventhandler's EventHandler ctor
                    // (§342b), but the tap/text channels it would set up are started BEFORE that
                    // point and are independent of MMI — they poll /data/local/tmp/noice_tap and
                    // noice_text and inject in-process.  Now that noice actually renders, this is
                    // how we drive its UI.
                    OHInputBridge::getInstance().startTapControlChannel();
                    OHInputBridge::getInstance().startTextControlChannel();
                    fprintf(stderr, "[WESTLAKE-WMC] §391 tap/text side-channels started\n");
                    fflush(stderr);
                }
                fprintf(stderr, "[WESTLAKE-WMC] §342 registered session %d (surfaceNodeId=%lld)\n",
                        persistentId, static_cast<long long>(surfaceNode->GetId()));
                fflush(stderr);

                result.sessionId     = persistentId;
                result.surfaceNodeId = static_cast<int32_t>(surfaceNode->GetId());
                result.displayId     = displayId;
                result.width         = requestedWidth;
                result.height        = requestedHeight;
                result.valid         = true;   // §342: the V7 path never set this
                result.wsErr         = 0;
                return result;
            }
        }
    }

    // Step 1: CreateWindow — pass our IWindow stub IN; server allocates windowId.
    uint32_t windowId = 0;
    OHOS::sptr<OHOS::Rosen::IWindow> windowProxy(windowCallback.GetRefPtr());
    auto retCreate = wmsInterface->CreateWindow(windowProxy, property, surfaceNode,
                                                 windowId, token);
    // WESTLAKE §222: CreateWindow returns WM_ERROR_IPC_FAILED (1005). In OH's WindowManagerProxy
    // that means one of: WriteInterfaceToken / WriteRemoteObject(window->AsObject()) /
    // property->Marshalling / SendRequest failed. hilog is empty on this build, so check OUR inputs:
    // a null AsObject() on the IWindow stub would fail WriteRemoteObject and produce exactly this.
    {
        OHOS::sptr<OHOS::IRemoteObject> wl_ao =
            (windowProxy != nullptr) ? windowProxy->AsObject() : nullptr;
        // §223: all inputs are non-null, so IPC_FAILED must come from Marshalling or SendRequest.
        // Prime suspect: SA 4606 is a SceneSessionManager but we iface_cast it to IWindowManager,
        // so the interface token we write does not match the server's descriptor and SendRequest is
        // rejected. Compare the REMOTE's descriptor against the one our proxy will write.
        {
            std::u16string wl_remote = (ssmProxy_ != nullptr)
                ? ssmProxy_->GetInterfaceDescriptor() : std::u16string();
            std::string wl_r8(wl_remote.begin(), wl_remote.end());
            std::u16string wl_want = OHOS::Rosen::IWindowManager::GetDescriptor();
            std::string wl_w8(wl_want.begin(), wl_want.end());
            fprintf(stderr, "[WESTLAKE-WMC] descriptor remote='%s' expected='%s' match=%d\n",
                    wl_r8.c_str(), wl_w8.c_str(), (wl_remote == wl_want) ? 1 : 0);
            fflush(stderr);
        }
        fprintf(stderr,
                "[WESTLAKE-WMC] inputs: windowProxy=%p AsObject=%p surfaceNode=%p property=%p "
                "wmsInterface=%p\n",
                static_cast<void*>(windowProxy.GetRefPtr()),
                static_cast<void*>(wl_ao.GetRefPtr()),
                static_cast<void*>(surfaceNode.get()),
                static_cast<void*>(property.GetRefPtr()),
                static_cast<void*>(wmsInterface.GetRefPtr()));
        fflush(stderr);
    }
    // WESTLAKE §221: native logging here goes to HILOG, so the child's stderr shows only
    // "delegate returned -9" with no cause. Mirror the outcome to stderr.
    fprintf(stderr, "[WESTLAKE-WMC] CreateWindow ret=%d windowId=%u\n",
            static_cast<int>(retCreate), windowId);
    fflush(stderr);
    if (retCreate != OHOS::Rosen::WMError::WM_OK) {
        LOGE("createSession: IWindowManager::CreateWindow failed ret=%{public}d",
             static_cast<int>(retCreate));
        // WESTLAKE §235 (2026-07-22): SURFACE-ONLY SUCCESS.
        // OH refuses a window session for this process (no ability record: §226 ERR_INVALID_STATE,
        // §227 WM_ERROR_NULLPTR, §223 wrong-interface). But §234 has ALREADY attached our
        // RSSurfaceNode straight into the display's render tree, which is the only thing a window
        // would ultimately have done for us. Reporting failure here makes ViewRootImpl throw
        // WindowManager$InvalidDisplayException BEFORE it ever draws, so the attached surface stays
        // empty and nothing can appear. Report SUCCESS instead so the Android side proceeds to
        // measure/layout/draw into that surface; the compositor then has real content to show.
        // §235b: the session MUST be registered in sessions_, otherwise
        // nativeGetSurfaceNodeId(sessionId) -> getSurfaceNodeId() returns -1 and
        // WindowSessionAdapter.relayout builds a SurfaceControl with no real node, so ViewRootImpl
        // still has nothing to draw into. Register the surface-only session the same way the
        // successful path does.
        const int32_t wl_sid = nextSessionId_++;
        {
            std::lock_guard<std::recursive_mutex> wl_lk(sessionMutex_);
            SessionEntry wl_e{};
            wl_e.sessionId      = wl_sid;
            wl_e.surfaceNodeId  = static_cast<uint64_t>(surfaceNode->GetId());
            wl_e.sessionProxy   = nullptr;
            wl_e.stageAdapter   = nullptr;
            wl_e.eventChannel   = nullptr;
            wl_e.surfaceNode    = surfaceNode;
            wl_e.windowId       = 0;
            wl_e.windowProperty = property;
            wl_e.wmsShown       = true;      // display-attached via §234
            wl_e.isMainWindow   = !asSubWindow;
            wl_e.parentWindowId = 0;
            wl_e.abilityName    = abilityName;
            sessions_[wl_sid]   = wl_e;
            if (!asSubWindow) g_fgMainSession.store(wl_sid);
        }
        result.sessionId     = wl_sid;
        result.surfaceNodeId = static_cast<int32_t>(surfaceNode->GetId());
        result.displayId     = displayId;
        result.width         = requestedWidth;
        result.height        = requestedHeight;
        result.wsErr         = 0;                 // WS_OK -> addToDisplay returns ADD_OKAY
        fprintf(stderr,
                "[WESTLAKE-SURFONLY] no OH window session (ret=%d); proceeding surface-only so the "
                "app draws into the display-attached RSSurfaceNode\n",
                static_cast<int>(retCreate));
        fflush(stderr);
        return result;
    }
    LOGI("createSession: CreateWindow OK, windowId=%{public}u", windowId);

    // Step 2: AddWindow — actually display the window. WMS reads
    // property->GetWindowId() server-side; out-param windowId from CreateWindow
    // doesn't auto-propagate, so we must SetWindowId on the property before
    // sending it through the AddWindow IPC.
    property->SetWindowId(windowId);
    auto retAdd = wmsInterface->AddWindow(property);
    fprintf(stderr, "[WESTLAKE-WMC] AddWindow ret=%d windowId=%u\n",
            static_cast<int>(retAdd), windowId);
    fflush(stderr);
    if (retAdd != OHOS::Rosen::WMError::WM_OK) {
        LOGE("createSession: IWindowManager::AddWindow failed ret=%{public}d (windowId=%{public}u)",
             static_cast<int>(retAdd), windowId);
        result.wsErr = static_cast<int32_t>(retAdd);
        wmsInterface->RemoveWindow(windowId, true);  // cleanup partial
        return result;
    }

    // 2026-05-19: Single-step NEED_AVOID clear — switches window from the
    // safe-area state (established by pre-CreateWindow SetWindowFlags(NEED_AVOID)
    // above + AddWindow's layout pass) to fullscreen.
    //
    // State at this point (after AddWindow, before this call):
    //   property.windowFlags = NEED_AVOID, property.windowMode = FULLSCREEN
    //   node.windowRect = [0,72,720,1136] (safe-area, computed by AddWindow's
    //                     layout pass for FULLSCREEN+NEED_AVOID)
    //   leash bounds  = [0,72,720,1136] (matches node, set when AddWindow's
    //                   layout pass saw preRect=[0,0,720,1280] != safe-area)
    //
    // After this UpdateProperty(FLAGS=0):
    //   Server: SetWindowFlags(windowId, 0) updates node.property.flags
    //           → UpdateWindowNode(UPDATE_FLAGS) → WindowLayoutPolicyCascade
    //           computes winRect = full-display [0,0,720,1280]
    //           preRect = node.windowRect = safe-area → winRect != preRect → diff
    //           → leashWinSurfaceNode_->SetBounds(0,0,720,1280) ✓
    //           → node.SetWindowRect(fullscreen)
    //   Result: helloworld layer rendered at display [0,0,720,1280], black band
    //   at Y=72..155 gone, AOSP DecorView's status-bar-inset slot now aligns
    //   with display top as it expects.
    //
    // See also: pre-CreateWindow SetWindowFlags(NEED_AVOID) block above for
    // the matching half of this two-call sequence.
    {
        property->SetWindowFlags(0);
        auto retFs = wmsInterface->UpdateProperty(
            property, OHOS::Rosen::PropertyChangeAction::ACTION_UPDATE_FLAGS);
        if (retFs == OHOS::Rosen::WMError::WM_OK) {
            LOGI("createSession: cleared NEED_AVOID -> fullscreen layout (UpdateProperty FLAGS OK)");
        } else {
            LOGW("createSession: clear NEED_AVOID failed rc=%{public}d (window may stay in safe-area)",
                 static_cast<int>(retFs));
        }
    }

    int64_t surfaceNodeId = static_cast<int64_t>(surfaceNode->GetId());
    int32_t persistentId = static_cast<int32_t>(windowId);

    int32_t sessionId;
    {
        std::lock_guard<std::recursive_mutex> lock(sessionMutex_);
        sessionId = persistentId > 0 ? persistentId : nextSessionId_++;
        // Legacy path: sessionProxy/stageAdapter/eventChannel are unused.
        // Store windowCallback in stageAdapter slot (sptr type compatible
        // through reinterpret) — TODO P2: refactor SessionEntry to be
        // path-aware (legacy vs V7). For now, keep stage/channel null and
        // hold windowCallback separately via static map keyed by sessionId.
        SessionEntry entry{};
        entry.sessionId = sessionId;
        entry.surfaceNodeId = surfaceNodeId;
        entry.sessionProxy = nullptr;
        entry.stageAdapter = nullptr;
        entry.eventChannel = nullptr;
        entry.surfaceNode = surfaceNode;
        // 2026-05-19: store WMS-side identity + property + visibility state
        // for hideWindow / showWindow helpers (counterpart to AddWindow above).
        entry.windowId = windowId;
        entry.windowProperty = property;
        entry.wmsShown = true;  // AddWindow above succeeded
        // 2026-06-04 NATIVE TRANSLUCENT-DIALOG FIX: record parent linkage so
        // hideWindow() won't remove the parent (library) while this child
        // (dialog/bottom-sheet) is shown over it.
        entry.isMainWindow = !asSubWindow;
        entry.parentWindowId = subParentWinId;  // 0 for a main window
        entry.abilityName = abilityName;        // 2026-06-26 2nd-Activity routing discriminator
        sessions_[sessionId] = entry;
        // 2026-06-05 #3: a newly-created MAIN window is the new foreground activity.
        if (!asSubWindow) g_fgMainSession.store(sessionId);
        // Keep windowCallback alive for the session's lifetime.
        static std::map<int32_t, OHOS::sptr<WindowCallbackAdapter>> windowCallbacks;
        windowCallbacks[sessionId] = windowCallback;
    }

    // 2026-06-05: re-show the parent main window so OHOS composites this dialog
    // OVER it (the parent was often relayout-hidden right before this dialog was
    // created → its transparent top bled through to the launcher). The
    // hideWindow() guard then keeps the parent shown while this child is up.
    if (asSubWindow && subParentSession >= 0) {
        showWindow(subParentSession);
        LOGI("createSession: re-showed parent session=%{public}d behind SUB_WINDOW", subParentSession);
    }

    result.sessionId = sessionId;
    result.surfaceNodeId = static_cast<int32_t>(surfaceNodeId);
    result.displayId = displayId;
    result.width = requestedWidth;
    result.height = requestedHeight;
    result.valid = true;
    result.wsErr = 0;  // §3.1.5.6.2 — explicit zero on success path

    LOGI("createSession: success, sessionId=%d, surfaceNodeId=%lld",
         sessionId, static_cast<long long>(surfaceNodeId));
    // 2026-05-02 G2.14r: stamp last-attached-session for BBQ_nativeUpdate
    // fallback (avoids BCP-jar boot-image rebuild for one new native method).
    oh_wm_set_last_session(sessionId);

    // 2026-05-18: request input focus.  Without this, OH MMI server keeps the
    // launcher window as the focus target and routes all PointerEvents to it,
    // so HelloWorld's window receives 0 taps despite being on top.  Mirrors
    // OH native WindowImpl::RequestFocus() (foundation/window/window_manager/
    // wm/src/window_impl.cpp:2621).  Call it BEFORE subscribeMmi so that by
    // the time MMI begins routing, this window is already the focus target.
    {
        OHOS::Rosen::WMError focusRet = wmsInterface->RequestFocus(windowId);
        if (focusRet == OHOS::Rosen::WMError::WM_OK) {
            LOGI("createSession: RequestFocus(windowId=%{public}u) OK", windowId);
        } else {
            LOGW("createSession: RequestFocus(windowId=%{public}u) failed rc=%{public}d",
                 windowId, static_cast<int>(focusRet));
        }
    }

    // 2026-05-18: subscribe to OH MMI input events. Without this, MMI service
    // never dispatches PointerEvent / KeyEvent to this process and HelloWorld's
    // CHANGE COLOR button (and any other touch UI) never receives events.
    // Mirrors OH native InputTransferStation::AddInputWindow() called from
    // WindowImpl::Create on the OH side. See Input_Adapter_design §3.3.5.
    OHInputBridge::getInstance().subscribeMmi(sessionId);

    // 2026-06-05 #4 REMOVED the focus heartbeat. It was dead code: a 4s polling
    // thread calling moveMissionToFront(getMissionIdForBundle(bundle)), but
    // getMissionIdForBundle returns -1 on this AMS so it never fired; it logged
    // via __android_log_print (a no-op on OH); and polling cannot fix the real
    // displayId-compositing / WMS focus-arbitration race anyway. The real fix
    // belongs at the RS/WMS layer (RequestFocus rc=1 + launcher compositing),
    // not a per-process busy thread here. See code-review 2026-06-05.

    return result;
}

int OHWindowManagerClient::updateSessionRect(int32_t sessionId,
                                              int32_t x, int32_t y,
                                              int32_t width, int32_t height)
{
    OH_BR_IPC_SCOPE("WMClient.updateSessionRect",
                    "session=%{public}d rect=[%{public}d,%{public}d,%{public}d,%{public}d]",
                    sessionId, x, y, width, height);

    // G2.14c — legacy path: IWindowManager has no per-window UpdateRect IPC;
    // geometry updates go through IWindowManager::UpdateProperty(property, action).
    // For now (P1), no-op — Android Activity main DecorView doesn't strictly
    // need relayout to put pixels on screen for HelloWorld. P2: add legacy
    // UpdateProperty plumbing.
    LOGI("updateSessionRect: legacy path, no-op (window=%d %dx%d)",
         sessionId, width, height);
    return 0;
}

int OHWindowManagerClient::notifyDrawingCompleted(int32_t sessionId) {
    OH_BR_IPC_SCOPE("WMClient.notifyDrawingCompleted", "session=%{public}d", sessionId);
    // G2.14c — legacy IWindowManager has no DrawingCompleted IPC; OH legacy
    // WMS triggers display once AddWindow runs.
    return 0;
}

void OHWindowManagerClient::destroySession(int32_t sessionId) {
    OH_BR_IPC_SCOPE("WMClient.destroySession", "session=%{public}d", sessionId);

    OHOS::sptr<OHOS::Rosen::ISession> sessionProxy;
    OHOS::sptr<OHOS::IRemoteObject> sessionRemote;
    std::shared_ptr<OHOS::Rosen::RSSurfaceNode> contentNode;
    {
        std::lock_guard<std::recursive_mutex> lock(sessionMutex_);
        auto it = sessions_.find(sessionId);
        if (it != sessions_.end()) {
            sessionProxy  = it->second.sessionProxy;
            sessionRemote = it->second.sessionRemote;
            contentNode   = it->second.surfaceNode;
            sessions_.erase(it);
        }
    }

    // WESTLAKE §538 — take the nodes OUT OF THE RS TREE, do not merely drop the refs.
    // Releasing the last shared_ptr is not enough on its own and is not immediate; RS keeps
    // compositing a node that is still parented, which is exactly what left a dismissed dialog's
    // last frame painted on screen after §536 had already destroyed its OH session (WMS listed one
    // window, yet RSTree still showed two MainActivity SURFACE_NODEs, both consumer=1 visible=1).
    // Detach child first, then the window node, then flush so RS acts on it now.
    {
        auto& nodes = wl_window_nodes();
        auto nit = nodes.find(sessionId);
        std::shared_ptr<OHOS::Rosen::RSSurfaceNode> windowNode =
            (nit != nodes.end()) ? nit->second : nullptr;
        // WESTLAKE §567 — RemoveFromTree() ALONE IS NOT ENOUGH FOR THESE NODES.
        // It only unlinks a node from a PARENT, but our window surfaces are top-level nodes
        // registered with RS directly (RSSurfaceNode::Create round-trips to RenderService and
        // hands back a producer Surface), so they have no parent to be unlinked from and RS keeps
        // compositing them. Measured: dismissing the Save-Preset bottom sheet logged
        // "§538 detached RS nodes for session=2 (content=1 window=0)" and RSTree STILL showed the
        // dialog's SURFACE_NODE with `Visible: 1` on top of the main window — the whole screen
        // stayed black, through further taps and a tab switch, while the app kept running and
        // playing audio. Hiding the node is what actually stops composition, so do that too.
        auto wl_hide = [](const std::shared_ptr<OHOS::Rosen::RSSurfaceNode>& n) {
            if (n == nullptr) return;
            n->SetVisible(false);
            n->MarkUIHidden(true);
            n->RemoveFromTree();
        };
        wl_hide(contentNode);
        if (windowNode != contentNode) { wl_hide(windowNode); }
        if (nit != nodes.end()) { nodes.erase(nit); }
        OHOS::Rosen::RSTransaction::FlushImplicitTransaction();
        fprintf(stderr, "[WESTLAKE-WMC] §538/§567 hid+detached RS nodes for session=%d (content=%d window=%d)\n",
                sessionId, contentNode != nullptr ? 1 : 0, windowNode != nullptr ? 1 : 0);
        fflush(stderr);
    }

    // WESTLAKE §536 — V7 teardown. A session created by CreateAndConnectSpecificSession is a
    // SPECIFIC session: the legacy IWindowManager::RemoveWindow/DestroyWindow below does not own
    // it, so before §536 a dismissed dialog kept its OH window forever — WMS still listed 148/149/
    // 150 stacked after every dialog had already left WindowManagerGlobal.mRoots, and the screen
    // kept showing the last frame each one drew. The client-side teardown is ISession
    // Background -> Disconnect, sent exactly the way §338 sends Foreground.
    //   SessionInterfaceCode: TRANS_ID_CONNECT=0, FOREGROUND=1, BACKGROUND=2, DISCONNECT=3.
    // Both handlers read (bool isFromClient, string identityToken). Failures are logged and fall
    // through to the legacy path rather than being fatal.
    if (sessionRemote != nullptr) {
        auto sendSessionCall = [&](uint32_t code, const char* what) {
            OHOS::MessageParcel data;
            OHOS::MessageParcel reply;
            OHOS::MessageOption option;
            const bool wrote = data.WriteInterfaceToken(u"OHOS.ISession") &&
                               data.WriteBool(true) &&   // isFromClient
                               data.WriteString("");     // identityToken
            const int32_t err = wrote ? sessionRemote->SendRequest(code, data, reply, option) : -1;
            const int32_t ret = (err == 0) ? reply.ReadInt32() : -1;
            fprintf(stderr, "[WESTLAKE-WMC] §536 %s(persistentId=%d) wrote=%d err=%d ret=%d\n",
                    what, sessionId, wrote ? 1 : 0, err, ret);
            fflush(stderr);
        };
        sendSessionCall(2, "Background");
        sendSessionCall(3, "Disconnect");
    }

    // G2.14c — legacy path uses IWindowManager.RemoveWindow + DestroyWindow.
    // sessionProxy is null in legacy path (V7-only field); skip Disconnect().
    if (connected_ && ssmProxy_ != nullptr) {
        auto wmsInterface = OHOS::iface_cast<OHOS::Rosen::IWindowManager>(ssmProxy_);
        if (wmsInterface) {
            wmsInterface->RemoveWindow(static_cast<uint32_t>(sessionId), true);
            wmsInterface->DestroyWindow(static_cast<uint32_t>(sessionId), false);
        }
    }
}

// 2026-05-19: visibility helpers — call OH WMS RemoveWindow / AddWindow to
// hide / show, gated by per-session wmsShown bool to make the IPC idempotent.
// Mirrors ArkUI WindowImpl::Hide (line 2034) / Show (line 1972) — keeps the
// adapter aligned with how OH native window clients toggle visibility.
//
// Design alternatives considered (see doc for full rationale):
//   A. Java-side Map<sessionId, Boolean> — adapter Java state, fast, but extra
//      state to keep in sync.
//   B. Reflect AOSP ViewRootImpl.mAppVisible — no adapter cache but fragile
//      (AOSP internal field name may change between API levels).
//   C. Call WMS RemoveWindow / AddWindow every relayout, let OH server's own
//      idempotency handle dups (WM_DO_NOTHING / WM_ERROR_INVALID_OPERATION).
//      No adapter state but adds binder IPC every relayout.
//   D. (chosen) In-App-process C++ cache on SessionEntry — state lives in
//      the data structure the adapter already maintains; Java caller is
//      stateless; per-relayout IPC only on actual transitions.
//
// Future drift mitigation: if adapter's wmsShown diverges from real OH state
// (e.g., WMS unilaterally hides due to AMS / focus policy), switch to A or B.

int32_t OHWindowManagerClient::hideWindow(int32_t sessionId) {
    OH_BR_IPC_SCOPE("WMClient.hideWindow", "session=%{public}d", sessionId);

    uint32_t windowId = 0;
    {
        std::lock_guard<std::recursive_mutex> lock(sessionMutex_);
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            LOGW("hideWindow: session %{public}d not found", sessionId);
            return -1;
        }
        if (!it->second.wmsShown) {
            // Already hidden — no-op, save the IPC.
            return 0;
        }
        windowId = it->second.windowId;
        // 2026-06-04 NATIVE TRANSLUCENT-DIALOG FIX: never remove a main window
        // while a shown sub-window (dialog/bottom-sheet) is parented to it.
        // Android keeps the activity behind a translucent dialog visible; if we
        // RemoveWindow the parent here (triggered by a relayout that signals the
        // covered main window non-VISIBLE), the dialog's transparent top falls
        // through to the launcher. Keeping the parent shown lets OHOS composite
        // the sheet over the live library. (Guard is symmetric to the SetParentId
        // routing in createSession.)
        if (it->second.isMainWindow) {
            for (auto& kv : sessions_) {
                if (kv.second.parentWindowId == windowId && kv.second.wmsShown) {
                    LOGI("hideWindow: KEEP main window %{public}u shown — sub-window child "
                         "windowId=%{public}u (session %{public}d) is up over it",
                         windowId, kv.second.windowId, kv.first);
                    return 0;
                }
            }
            // 2026-06-26 IME-focus fix: keep the foreground activity shown while
            // the soft keyboard is being summoned. The SearchView expand / IME
            // resize relayout signals the DecorView non-VISIBLE; hiding the
            // window here makes it lose WMS focus and the keyboard is torn down.
            if (g_imeActive.load()) {
                LOGI("hideWindow: KEEP main window %{public}u shown — IME active "
                     "(SearchView/keyboard up over it)", windowId);
                return 0;
            }
        }
    }

    if (!connected_ || ssmProxy_ == nullptr) {
        LOGW("hideWindow: ssmProxy not ready, session=%{public}d", sessionId);
        return -2;
    }
    auto wmsInterface = OHOS::iface_cast<OHOS::Rosen::IWindowManager>(ssmProxy_);
    if (wmsInterface == nullptr) {
        LOGE("hideWindow: cast IWindowManager failed");
        return -3;
    }

    auto rc = wmsInterface->RemoveWindow(windowId, /*isFromInnerkits=*/true);
    LOGI("hideWindow: RemoveWindow(windowId=%{public}u) rc=%{public}d",
         windowId, static_cast<int>(rc));

    if (rc == OHOS::Rosen::WMError::WM_OK) {
        std::lock_guard<std::recursive_mutex> lock(sessionMutex_);
        auto it = sessions_.find(sessionId);
        if (it != sessions_.end()) {
            it->second.wmsShown = false;
        }
    }
    return static_cast<int32_t>(rc);
}

int32_t OHWindowManagerClient::showWindow(int32_t sessionId) {
    OH_BR_IPC_SCOPE("WMClient.showWindow", "session=%{public}d", sessionId);

    OHOS::sptr<OHOS::Rosen::WindowProperty> property;
    {
        std::lock_guard<std::recursive_mutex> lock(sessionMutex_);
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            LOGW("showWindow: session %{public}d not found", sessionId);
            return -1;
        }
        if (it->second.wmsShown) {
            // Already shown — no-op, save the IPC.
            return 0;
        }
        property = it->second.windowProperty;
    }
    if (property == nullptr) {
        LOGW("showWindow: session %{public}d has null windowProperty (createSession "
             "predates 2026-05-19 storage)", sessionId);
        return -2;
    }

    if (!connected_ || ssmProxy_ == nullptr) {
        LOGW("showWindow: ssmProxy not ready, session=%{public}d", sessionId);
        return -3;
    }
    auto wmsInterface = OHOS::iface_cast<OHOS::Rosen::IWindowManager>(ssmProxy_);
    if (wmsInterface == nullptr) {
        LOGE("showWindow: cast IWindowManager failed");
        return -4;
    }

    auto rc = wmsInterface->AddWindow(property);
    LOGI("showWindow: AddWindow(windowId=%{public}u) rc=%{public}d",
         property->GetWindowId(), static_cast<int>(rc));

    if (rc == OHOS::Rosen::WMError::WM_OK) {
        std::lock_guard<std::recursive_mutex> lock(sessionMutex_);
        auto it = sessions_.find(sessionId);
        if (it != sessions_.end()) {
            it->second.wmsShown = true;
            // 2026-06-05 #3: a shown MAIN window becomes the foreground activity
            // (tracks A->B resume order so dialogs parent to the right activity).
            if (it->second.isMainWindow) g_fgMainSession.store(sessionId);
        }
    }
    return static_cast<int32_t>(rc);
}


int64_t OHWindowManagerClient::getSurfaceNodeId(int32_t sessionId) const {
    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        return it->second.surfaceNodeId;
    }
    return -1;
}

// 2026-05-06 — Per design §5.1 / §9.1 三件套 condition #2:
//   Single-source rule for surfaceNode.  Used by oh_surface_bridge.cpp to fetch
//   the same RSSurfaceNode that was registered with WMS, so the WMS-side
//   layer node and the hwui-producer-side buffer feed share one node.
std::shared_ptr<OHOS::Rosen::RSSurfaceNode>
OHWindowManagerClient::getRSSurfaceNode(int32_t sessionId) {
    OH_BR_IPC_SCOPE("WMClient.getRSSurfaceNode", "session=%{public}d", sessionId);
    std::lock_guard<std::recursive_mutex> lock(sessionMutex_);
    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        LOGE("getRSSurfaceNode: unknown sessionId=%d", sessionId);
        return nullptr;
    }
    if (!it->second.surfaceNode) {
        LOGE("getRSSurfaceNode: sessionId=%d has no RSSurfaceNode", sessionId);
        return nullptr;
    }
    return it->second.surfaceNode;
}

// 2026-05-02 G2.14r: stable C wrapper for cross-.so callers (e.g.,
// liboh_android_runtime.so::compat_shim BBQ_nativeUpdate).  Avoids C++
// namespace + name mangling issues at the dlsym boundary.
extern "C" __attribute__((visibility("default")))
void* oh_wm_get_native_window(int32_t sessionId) {
    return oh_adapter::OHWindowManagerClient::getInstance().getOhNativeWindow(sessionId);
}

// 2026-05-02 G2.14r: cross-process "last touched session" hint.  Used by
// BBQ_nativeUpdate when its SurfaceControl carries no sessionId (avoids the
// need for a BCP-class native method to attach session, which would require
// boot image rebuild on every change).  Each child appspawn-x process spawns
// one app with one session, so a process-global last-session is unambiguous.
namespace {
std::atomic<int32_t> g_lastAttachedSession{0};
}  // namespace
// WESTLAKE §284k: per-frame RS commit hook, called by libhwui right after it swaps.
// §284i got our buffers into RS's consumer queue (state=3) but RS never ACQUIRES them
// (SrcRect stays [0,0,0,0]).  We only FlushImplicitTransaction() at SETUP, so RS is never told
// there is anything new to composite.  libhwui dlsyms this after eglSwapBuffersWithDamageKHR.
extern "C" __attribute__((visibility("default")))
void wl_rs_commit_frame() {
    // WESTLAKE §353: this runs on hwui's RenderThread, and `FlushImplicitTransaction` operates on
    // RSTransactionProxy's THREAD-LOCAL implicit transaction — so it races the UI thread that is
    // building the real one.  Timeline evidence: our RS nodes are alive on the tree from t≈4s, and
    // are DESTROYED (gone from both RSTree and nodeNotOnTree ⇒ purged, not merely detached) at
    // t≈14s, exactly when `[WESTLAKE-RSCOMMIT] flush #1` fires just before the first swap; swaps
    // then freeze at 6 while the child keeps running.  Gate it off by default and keep it available
    // via WL_RSCOMMIT for comparison.
    static const bool wl_enabled = (getenv("WL_RSCOMMIT") != nullptr);
    static int wl_n = 0;
    if (wl_n < 6) {
        wl_n++;
        fprintf(stderr, "[WESTLAKE-RSCOMMIT] flush #%d enabled=%d\n", wl_n, wl_enabled ? 1 : 0);
        fflush(stderr);
    }
    if (wl_enabled) {
        OHOS::Rosen::RSTransaction::FlushImplicitTransaction();
    }
}

extern "C" __attribute__((visibility("default")))
int32_t oh_wm_get_last_session() {
    return g_lastAttachedSession.load(std::memory_order_acquire);
}
extern "C" __attribute__((visibility("default")))
void oh_wm_set_last_session(int32_t sessionId) {
    OH_BR_IPC_SCOPE("oh_wm_set_last_session", "session=%{public}d", sessionId);
    g_lastAttachedSession.store(sessionId, std::memory_order_release);
}

// 2026-06-26 IME-focus fix — re-assert WMS input focus on the foreground main
// window.  Mirrors the createSession RequestFocus call (line ~586) but resolves
// the windowId from the tracked foreground main session (g_fgMainSession, with
// the highest-sessionId fallback used elsewhere in this file).  Called from
// input_method_bridge.cpp right before oh_ime_show() so the IMSA's IsFocused
// gate passes and OnFocused(appPid) is a no-op.
int32_t oh_adapter::OHWindowManagerClient::requestFocusForMainWindow() {
    // 2026-06-26: lazily (re)connect — the singleton's connection may not be
    // established/live in the path that reaches the IME show.
    if (!connected_ || ssmProxy_ == nullptr) {
        LOGW("requestFocusForMainWindow: not connected (connected_=%{public}d, ssmProxy_=%{public}p) -> connect()",
             (int)connected_, (void*)ssmProxy_.GetRefPtr());
        connect();
    }
    if (!connected_ || ssmProxy_ == nullptr) {
        LOGW("requestFocusForMainWindow: still not connected after connect()");
        return -1;
    }
    auto wmsInterface = OHOS::iface_cast<OHOS::Rosen::IWindowManager>(ssmProxy_);
    if (wmsInterface == nullptr) {
        LOGW("requestFocusForMainWindow: iface_cast failed");
        return -3;
    }
    // Collect ALL candidate main-window ids (the tracked-fg first, then every
    // main window by descending sessionId) and try RequestFocus on each until
    // one returns WM_OK.  Stale sessions_ entries (from prior activities) point
    // at destroyed/invisible windowIds (WMS RequestFocus -> INVALID_OPERATION),
    // so a single best-guess id is unreliable; try them all.
    std::vector<uint32_t> ids;
    {
        std::lock_guard<std::recursive_mutex> lk(sessionMutex_);
        int32_t fg = g_fgMainSession.load();
        auto it = sessions_.find(fg);
        if (fg >= 0 && it != sessions_.end() && it->second.isMainWindow && it->second.windowId != 0) {
            ids.push_back(it->second.windowId);
        }
        // descending sessionId
        for (auto rit = sessions_.rbegin(); rit != sessions_.rend(); ++rit) {
            if (rit->second.isMainWindow && rit->second.windowId != 0) {
                uint32_t w = rit->second.windowId;
                if (std::find(ids.begin(), ids.end(), w) == ids.end()) ids.push_back(w);
            }
        }
        LOGI("requestFocusForMainWindow: fg=%{public}d, %{public}zu session(s), %{public}zu candidate winId(s)",
             fg, sessions_.size(), ids.size());
    }
    if (ids.empty()) {
        LOGW("requestFocusForMainWindow: no main window in sessions_");
        return -2;
    }
    OHOS::Rosen::WMError last = OHOS::Rosen::WMError::WM_ERROR_INVALID_WINDOW;
    for (uint32_t w : ids) {
        last = wmsInterface->RequestFocus(w);
        LOGI("requestFocusForMainWindow: RequestFocus(windowId=%{public}u) rc=%{public}d",
             w, static_cast<int>(last));
        if (last == OHOS::Rosen::WMError::WM_OK) return 0;
    }
    return -static_cast<int32_t>(last);
}

// C-ABI wrapper dlsym'd / linked by input_method_bridge.cpp (same .so, so a
// direct call also works; exported for robustness).
extern "C" __attribute__((visibility("default")))
int32_t oh_wm_request_focus_current() {
    return oh_adapter::OHWindowManagerClient::getInstance().requestFocusForMainWindow();
}

// 2026-05-06 — Per design §5.6 / §9.1 三件套 condition #3:
//   Cross-.so C wrappers so liboh_android_runtime.so::android_view_SurfaceControl.cpp
//   (in a different .so) can route SurfaceControl property setters and apply()
//   into RSSurfaceNode + RSTransactionProxy without link-time dependency.
//   All callers use dlsym RTLD_DEFAULT; if liboh_adapter_bridge.so isn't yet
//   loaded into the process, the no-op fallback keeps the SC stub intact.
extern "C" __attribute__((visibility("default")))
void oh_rs_set_layer_bounds(int32_t sessionId, float x, float y, float w, float h) {
    OH_BR_IPC_SCOPE("oh_rs_set_layer_bounds",
                    "session=%{public}d xywh=[%{public}.1f,%{public}.1f,%{public}.1f,%{public}.1f]",
                    sessionId, x, y, w, h);
    auto node = oh_adapter::OHWindowManagerClient::getInstance().getRSSurfaceNode(sessionId);
    if (!node) return;
    // 2026-05-11 G2.14ap: OH RSNode requires SetBounds AND SetFrame as a pair
    // (per foundation/graphic/graphic_2d/.../rs_screen_render_node.h standard
    //  usage).  Without SetFrame, Frame stays at sentinel [-inf, -inf, -inf, -inf]
    //  and ClipToFrame=true (which adapter sets via SurfaceControl) clips the
    //  entire surface to an empty region — VisibleRegion / OpaqueRegion=Empty,
    //  shouldPaint_=0, localDrawRect_=[0,0,0,0] — hwui's frame submission then
    //  no-ops at the RS level even though buffers reach the producer queue.
    node->SetBounds(x, y, w, h);
    node->SetFrame(x, y, w, h);
}

extern "C" __attribute__((visibility("default")))
void oh_rs_set_layer_alpha(int32_t sessionId, float alpha) {
    OH_BR_IPC_SCOPE("oh_rs_set_layer_alpha", "session=%{public}d alpha=%{public}.2f", sessionId, alpha);
    auto node = oh_adapter::OHWindowManagerClient::getInstance().getRSSurfaceNode(sessionId);
    if (!node) return;
    node->SetAlpha(alpha);
}

extern "C" __attribute__((visibility("default")))
void oh_rs_set_layer_visible(int32_t sessionId, int32_t visible) {
    OH_BR_IPC_SCOPE("oh_rs_set_layer_visible", "session=%{public}d visible=%{public}d", sessionId, visible);
    auto node = oh_adapter::OHWindowManagerClient::getInstance().getRSSurfaceNode(sessionId);
    if (!node) return;
    node->SetVisible(visible != 0);
}

extern "C" __attribute__((visibility("default")))
void oh_rs_flush_transaction() {
    OH_BR_IPC_SCOPE("oh_rs_flush_transaction", "");
    // Triggers RSTransactionProxy::FlushImplicitTransaction which commits
    // pending RSCommand batch (createNode / setBounds / setAlpha / ...)
    // to RenderService via RSIClientToRenderConnection::CommitTransaction.
    OHOS::Rosen::RSTransaction::FlushImplicitTransaction();
}

// 2026-05-11 G2.14al — bridge AOSP SurfaceControl.Transaction.setOpaque to OH.
// Java side: SurfaceControl.Transaction.setOpaque(sc, isOpaque) compiles to
//   nativeSetFlags(tx, sc, isOpaque ? SURFACE_OPAQUE : 0, SURFACE_OPAQUE=0x02)
// android_view_SurfaceControl.cpp SC_nativeSetFlags extracts the opaque bit
// and forwards here via dlsym RTLD_DEFAULT (same indirection pattern as
// oh_rs_set_layer_alpha / oh_rs_set_layer_visible — keeps liboh_android_
// runtime.so independent of OH C++ headers).
//
// OH equivalent: RSSurfaceNode::SetSurfaceBufferOpaque(bool isOpaque).
// Without this hint OH RS composes the layers underneath (the OH SCB
// starting/leash window stack, 720×1136 white), letting their white show
// through transparent helloworld surface even when hwui has drawn opaque
// TextView content into the buffer.
extern "C" __attribute__((visibility("default")))
void oh_rs_set_layer_opaque(int32_t sessionId, int32_t isOpaque) {
    OH_BR_IPC_SCOPE("oh_rs_set_layer_opaque",
                    "session=%{public}d isOpaque=%{public}d",
                    sessionId, isOpaque);
    auto node = oh_adapter::OHWindowManagerClient::getInstance()
                    .getRSSurfaceNode(sessionId);
    if (!node) return;
    node->SetSurfaceBufferOpaque(isOpaque != 0);
}

// ============================================================
// 2026-05-08 G2.14aa: ASurfaceControl/ASurfaceTransaction NDK 真桥 helpers
//
// AOSP hwui RenderThread (ASurfaceControlFunctions ctor in
// frameworks/base/libs/hwui/renderthread/RenderThread.cpp) dlopen("libandroid.so")
// + dlsym 9 个 NDK 符号。device 上 libandroid.so 是 liboh_android_runtime.so
// 的 symlink，需要在那边 export 9 个 wrapper；wrapper 通过 dlsym(RTLD_DEFAULT,
// "oh_rs_*") 找下面的 helper 真桥到 OH RS。
//
// OH RS = SurfaceFlinger 等价：
//   ASurfaceControl     ↔ RSSurfaceNode (sptr<>)
//   ASurfaceTransaction ↔ RSTransaction (隐式 transaction via RSTransactionProxy)
// ============================================================

/**
 * Create a sub-RSSurfaceNode (non-window). hwui WebViewFunctorManager 用此
 * 创建 child surface; helloworld 主路径不真触发。
 *
 * @return opaque RSSurfaceNode* (caller stores as void*; release via
 *         oh_rs_destroy_subsurface)
 */
extern "C" __attribute__((visibility("default")))
void* oh_rs_create_subsurface(const char* name) {
    OH_BR_IPC_SCOPE("oh_rs_create_subsurface", "name=%{public}s", name ? name : "(null)");
    OHOS::Rosen::RSSurfaceNodeConfig cfg;
    cfg.SurfaceNodeName = (name && *name) ? name : "adapter_subsurface";
    auto node = OHOS::Rosen::RSSurfaceNode::Create(cfg, /*isWindow=*/false);
    if (!node) {
        LOGE("oh_rs_create_subsurface: RSSurfaceNode::Create failed for %s", cfg.SurfaceNodeName.c_str());
        return nullptr;
    }
    // 转 sptr → raw 指针给 C ABI；wrapper 用 holder map 维持 sptr 引用
    auto* holder = new std::shared_ptr<OHOS::Rosen::RSSurfaceNode>(node);
    return holder;
}

/**
 * Release sub-RSSurfaceNode. Decrements sptr refcount (likely destroy).
 */
extern "C" __attribute__((visibility("default")))
void oh_rs_destroy_subsurface(void* opaque) {
    OH_BR_IPC_SCOPE("oh_rs_destroy_subsurface", "holder=%p", opaque);
    if (!opaque) return;
    delete reinterpret_cast<std::shared_ptr<OHOS::Rosen::RSSurfaceNode>*>(opaque);
}

/**
 * Register buffer-available listener on RSSurfaceNode. AOSP hwui
 * CanvasContext.cpp 通过 ASurfaceControl_registerSurfaceStatsListener 让
 * RenderThread 知道 buffer ready；OH 等价是 RSSurfaceNode::RegisterBufferAvailableListener。
 *
 * Callback 签名（hwui ASC_StatsListener）：
 *   void cb(void* context, int32_t controlFd, ASurfaceTransactionStats* stats)
 * 我们桥时 controlFd=0、stats=nullptr（OH 暂无 stats 等价；hwui 处理 null 安全）。
 */
extern "C" __attribute__((visibility("default")))
void oh_rs_register_buffer_listener(void* opaque,
                                     void (*cb)(void* /*context*/, int32_t /*ctlFd*/, void* /*stats*/),
                                     void* context) {
    OH_BR_IPC_SCOPE("oh_rs_register_buffer_listener",
                    "holder=%p cb=%p ctx=%p", opaque, (void*)cb, context);
    if (!opaque || !cb) return;
    auto* holder = reinterpret_cast<std::shared_ptr<OHOS::Rosen::RSSurfaceNode>*>(opaque);
    if (!*holder) return;
    // OH 7.0.0.18 public API 是 SetBufferAvailableCallback (signature: std::function<void()>)，
    // 内部走 RSRenderPipelineClient::RegisterBufferAvailableListener。一次只能 set 一个 cb。
    (*holder)->SetBufferAvailableCallback([cb, context]() {
        cb(context, 0, nullptr);
    });
}

// 2026-05-02 G2.14r: bridge from sessionId to OHNativeWindow*.  Cached
// per-session so repeated calls return the same pointer (Java Surface lifecycle
// expects a stable native object).
// WESTLAKE §335: obtain the ABILITY'S OWN window surface node (the one RS actually composites).
// §334 measured, in one RSTree dump, that RS grants a real `VisibleRegion` ONLY to windows it knows
// through a genuine session — the ability's `noice0` and SCB's windows — while every node we create
// ourselves and splice on with AttachToDisplay stays `VisibleRegion [Empty]`: on the tree, handed
// buffers (state=3), but never composited, hence the black screen. Every previous workaround
// (§271/§284d raise-Z, §284j notify flag, §284m AttachToWindowContainer, §284q boot-animation)
// still targeted a session-less node, which is why none of them could work.
// So: render into the ability's window instead of our own node. Safe to call the C++ Window API
// here — we compile against the BOARD'S OWN OHOS 6.1 headers, so there is no ABI drift (unlike the
// AOSP-14-vs-board-skia boundary). In-process only: the appspawn-x child owns no OH window.
static std::shared_ptr<OHOS::Rosen::RSSurfaceNode> wl_ability_window_node() {
    // §335b: the ability's window does NOT exist yet when Android first sets its surface up — our
    // first two lookups ran before `onWindowStageCreate`, so both returned null while WMS later
    // showed `noice0` (pid ours, 1200x1920, ZOrd 102, VisibleRegion 1). Cache the node once found
    // and, on the early calls, WAIT briefly for the window to appear instead of giving up.
    static std::shared_ptr<OHOS::Rosen::RSSurfaceNode> wl_cached;
    if (wl_cached != nullptr) return wl_cached;
    OHOS::sptr<OHOS::Rosen::Window> w = nullptr;
    const char* how = "?";
    static const char* kNames[] = { "noice0", "noice", "EntryAbility" };
    for (int attempt = 0; attempt < 40 && w == nullptr; ++attempt) {   // ~8s max
        w = OHOS::Rosen::Window::GetTopWindowWithContext();
        if (w != nullptr) { how = "GetTopWindowWithContext"; break; }
        for (unsigned i = 0; i < sizeof(kNames)/sizeof(kNames[0]) && w == nullptr; ++i) {
            w = OHOS::Rosen::Window::Find(kNames[i]);
            if (w != nullptr) { how = kNames[i]; break; }
        }
        if (w == nullptr) { usleep(200 * 1000); }
    }
    if (w == nullptr) {
        fprintf(stderr, "[WESTLAKE-ABWIN] no ability window found\n"); fflush(stderr);
        return nullptr;
    }
    std::shared_ptr<OHOS::Rosen::RSSurfaceNode> n = w->GetSurfaceNode();
    wl_cached = n;
    fprintf(stderr, "[WESTLAKE-ABWIN] ability window via %s -> surfaceNode=%p id=%llu\n",
            how, (void*) n.get(),
            (unsigned long long) (n ? n->GetId() : 0ULL));
    fflush(stderr);
    return n;
}

void* OHWindowManagerClient::getOhNativeWindow(int32_t sessionId) {
    fprintf(stderr, "[WESTLAKE-GONW] 1 ENTER tid=%d session=%d\n",
            (int) syscall(178 /*SYS_gettid on aarch64*/), (int) sessionId); fflush(stderr);

    // WESTLAKE §368: render straight into the ABILITY'S OWN window surface.
    // Everything measured says our own node can never be composited on this board without a session,
    // and with a session the parent ability is killed by AMS (LIFECYCLE_TIMEOUT) which destroys the
    // whole WindowScene branch.  But the ability's window `noice0` IS composited (`VisibleRegion 1:
    // [0,0,1200,1920]`) and RS publishes its surface id in `hidumper -s RenderService -a RSTree`
    // (`uniqueId[...]`).  `OH_NativeWindow_CreateNativeWindowFromSurfaceId` takes exactly that id and
    // is pure C — no libwm, no C++ statics, so it crosses the process and namespace boundary that
    // defeated §335's `Window::Find`.  Hand hwui THAT window and Android content lands in a surface
    // OpenHarmony already puts on screen.
    {
        static const char* wl_sidEnv = getenv("WL_SURFACE_ID");
        static void* wl_sidWindow = nullptr;
        static bool wl_sidTried = false;
        if (wl_sidEnv != nullptr && *wl_sidEnv != '\0') {
            if (!wl_sidTried) {
                wl_sidTried = true;
                const uint64_t wl_sid = strtoull(wl_sidEnv, nullptr, 10);
                OHNativeWindow* wl_nw = nullptr;
                const int32_t wl_rc = OH_NativeWindow_CreateNativeWindowFromSurfaceId(wl_sid, &wl_nw);
                fprintf(stderr, "[WESTLAKE-SURFID] §368 surfaceId=%llu rc=%d nw=%p\n",
                        (unsigned long long) wl_sid, wl_rc, (void*) wl_nw);
                if (wl_rc == 0 && wl_nw != nullptr) {
                    int32_t wl_w = 0, wl_h = 0;
                    OH_NativeWindow_NativeWindowHandleOpt(wl_nw, /*GET_BUFFER_GEOMETRY*/ 1, &wl_h, &wl_w);
                    if (wl_w <= 0 || wl_h <= 0) {
                        OH_NativeWindow_NativeWindowHandleOpt(wl_nw, /*SET_BUFFER_GEOMETRY*/ 0, 1200, 1920);
                        OH_NativeWindow_NativeWindowHandleOpt(wl_nw, /*SET_FORMAT*/ 3, 12);
                        uint64_t wl_u = 0;
                        OH_NativeWindow_NativeWindowHandleOpt(wl_nw, /*GET_USAGE*/ 4, &wl_u);
                        OH_NativeWindow_NativeWindowHandleOpt(wl_nw, /*SET_USAGE*/ 5,
                                                              wl_u | 0x100ULL | 0x200ULL);
                    }
                    wl_sidWindow = wl_nw;
                }
                fflush(stderr);
            }
            if (wl_sidWindow != nullptr) { return wl_sidWindow; }
        }
    }

    OH_BR_IPC_SCOPE("WMClient.getOhNativeWindow", "session=%{public}d", sessionId);
    // WESTLAKE §283n — LOCK INVERSION FIX.
    // This used to hold `sessionMutex_` across its ENTIRE body: RS flushes, an 80ms usleep and
    // CreateNativeWindowFromSurface.  hwui's RenderThread calls in here during the EGL bind, so
    // while it was inside, the UI thread's next relayout blocked forever in getSurfaceNodeId(),
    // which takes the SAME mutex.  Measured: the 2nd relayout completes updateSessionRect /
    // createOHSurface / getSurfaceHandle / updateSurfaceSize then hangs in nativeGetSurfaceNodeId
    // #2 (main futex_wait_queue_me) while RenderThread is RUNNING.
    // Fix: hold the lock ONLY for map access.  References into std::map/std::unordered_map stay
    // valid across insert/rehash (node-based), so capturing &entry.producerSurface and using it
    // unlocked is safe unless the entry is erased (destroySession only).
    std::unique_lock<std::recursive_mutex> lock(sessionMutex_);
    auto it = sessions_.find(sessionId);
    fprintf(stderr, "[WESTLAKE-GONW] 2 lock acquired + find done\n"); fflush(stderr);

    if (it == sessions_.end()) {
        LOGE("getOhNativeWindow: unknown sessionId=%d", sessionId);
        return nullptr;
    }
    // §335: prefer the ability's real, session-backed window node (see helper above).
    {
        static const bool wl_use_abw = (getenv("WL_USE_ABILITY_WINDOW") != nullptr);
        if (wl_use_abw) {
            std::shared_ptr<OHOS::Rosen::RSSurfaceNode> wl_abw = wl_ability_window_node();
            if (wl_abw != nullptr &&
                (!it->second.surfaceNode || it->second.surfaceNode->GetId() != wl_abw->GetId())) {
                fprintf(stderr,
                        "[WESTLAKE-ABWIN] session=%d swapping our node %llu -> ability node %llu\n",
                        (int) sessionId,
                        (unsigned long long) (it->second.surfaceNode ? it->second.surfaceNode->GetId() : 0ULL),
                        (unsigned long long) wl_abw->GetId());
                fflush(stderr);
                it->second.surfaceNode = wl_abw;
                it->second.producerSurface = nullptr;   // force a fresh producer for the new node
                it->second.ohNativeWindow = nullptr;    // and a fresh OHNativeWindow (see §328)
                it->second.ohNativeWindowNodeId = 0;
            }
        }
    }

    // WESTLAKE §328 — STALE-NATIVE-WINDOW INVALIDATION (ported from the arm32 chain that shipped;
    // see westlake-piercing docs/noice-egl-rootcause-g34.md "2nd layer"). The cache below is keyed
    // ONLY by sessionId, but a session's RSSurfaceNode is REPLACED when the window is re-created
    // (noice has TWO windows — AppIntro + MainActivity — and a relayout can mint a new SurfaceControl
    // /surfaceNode for the same session). Returning the old OHNativeWindow then hands hwui a window
    // that is already EGL-bound to a DEAD surface: the 2nd eglCreateWindowSurface fails and the
    // RenderThread ends up drawing/flushing against a stale context. On arm32 this was the wall
    // after the format/usage fix; the recorded fix is exactly this — invalidate when
    // surfaceNode->GetId() differs from the node the cached window was built from.
    if (it->second.ohNativeWindow != nullptr) {
        const uint64_t wl_now = it->second.surfaceNode ? it->second.surfaceNode->GetId() : 0ULL;
        if (wl_now != 0ULL && it->second.ohNativeWindowNodeId != 0ULL &&
            wl_now != it->second.ohNativeWindowNodeId) {
            fprintf(stderr,
                    "[WESTLAKE-NWSTALE] session=%d surfaceNode changed %llu -> %llu; dropping cached nw=%p\n",
                    (int) sessionId,
                    (unsigned long long) it->second.ohNativeWindowNodeId,
                    (unsigned long long) wl_now, it->second.ohNativeWindow);
            fflush(stderr);
            // Drop the stale window + its producer so a FRESH one is built (and re-configured with
            // geometry/format/usage below) for the new surface. Deliberately do NOT destroy the old
            // OHNativeWindow: hwui's RenderThread may still hold it asynchronously, and the arm32
            // chain proved that freeing surfaces under the RenderThread is what caused the
            // ASurfaceControl_release use-after-free (G3.8). Leaking it is the safe trade.
            it->second.ohNativeWindow = nullptr;
            it->second.producerSurface = nullptr;
        } else {
            return it->second.ohNativeWindow;
        }
    }
    if (!it->second.surfaceNode) {
        LOGE("getOhNativeWindow: sessionId=%d has no RSSurfaceNode", sessionId);
        return nullptr;
    }
    // 2026-05-08 G2.14ab: store sptr<Surface> in SessionEntry (not local var)
    // so its lifetime equals session lifetime. hwui RenderThread holds
    // OHNativeWindow* and async IncStrongRef on the backing ProducerSurface;
    // a local sptr would release on return, leading to use-after-free
    // (libsurface RefBase::IncStrongRef on 0xcafe5c02 poisoned memory).
    if (!it->second.producerSurface) {
    fprintf(stderr, "[WESTLAKE-GONW] 3 calling surfaceNode->GetSurface()\n"); fflush(stderr);
        it->second.producerSurface = it->second.surfaceNode->GetSurface();
    }
    // WESTLAKE §284g: RE-FETCH the producer rather than trusting the cached one.
    // §284f proved the node id we hold MATCHES RS's node, yet the producer we hand hwui belongs
    // to a DIFFERENT BufferQueue than RS's consumer, and RS's queue is never filled.  Same node +
    // different queue == the surface was (re)created on the RS side after our first GetSurface(),
    // orphaning our cached producer.  Re-fetch and report whether the id changes.  Still stored
    // in the SessionEntry so the G2.14ab lifetime guarantee holds.
    {
        auto wl_fresh = it->second.surfaceNode->GetSurface();
        if (wl_fresh) {
            const unsigned long long wl_oldId = it->second.producerSurface
                ? (unsigned long long) it->second.producerSurface->GetUniqueId() : 0ULL;
            const unsigned long long wl_newId = (unsigned long long) wl_fresh->GetUniqueId();
            fprintf(stderr, "[WESTLAKE-QID] session=%d refetch %llu -> %llu (%s)\n",
                    (int) sessionId, wl_oldId, wl_newId,
                    (wl_oldId == wl_newId) ? "SAME" : "CHANGED");
            fflush(stderr);
            it->second.producerSurface = wl_fresh;
        }
    }
    if (false) {
    fprintf(stderr, "[WESTLAKE-GONW] 4 GetSurface returned\n"); fflush(stderr);

    }
    if (!it->second.producerSurface) {
        LOGE("getOhNativeWindow: sessionId=%d RSSurfaceNode has no producer surface", sessionId);
        return nullptr;
    }
    // 2026-05-12 G2.14aw probe A.1 baseline: producer 出身证据.
    // surfaceNode->GetSurface() 拿到的 ProducerSurface 真身 + RS 服务端注册的 uniqueId,
    // 后面 wrap/swap 路径上的所有 uniqueId 都应等于此值才算同源.
    LOGI("getOhNativeWindow[probe-baseline]: sessionId=%d surfaceNode=%p surfaceNodeId=%lld "
         "producerSurface_sptrAddr=%p producerSurface_refPtr=%p uniqueId=0x%llx",
         sessionId, it->second.surfaceNode.get(),
         (long long)it->second.surfaceNodeId,
         (void*)&it->second.producerSurface,
         it->second.producerSurface.GetRefPtr(),
         (unsigned long long)it->second.producerSurface->GetUniqueId());

    // [G3.0-SURFACE-READY] commit RSSurfaceNode + let RS settle before hwui EGL bind
    OHOS::Rosen::RSTransaction::FlushImplicitTransaction();
    // §283n: everything below is heavy/blocking OH+RS work -- run it WITHOUT the lock.
    OHOS::sptr<OHOS::Surface>* wl_psPtr = &it->second.producerSurface;
    lock.unlock();
    fprintf(stderr, "[WESTLAKE-GONW] 4b lock RELEASED for heavy work\n"); fflush(stderr);
    fprintf(stderr, "[WESTLAKE-GONW] 5 pre-flush#1/usleep\n"); fflush(stderr);
    usleep(80 * 1000);
    fprintf(stderr, "[WESTLAKE-GONW] 6 flush#1 + usleep done\n"); fflush(stderr);

    OHOS::Rosen::RSTransaction::FlushImplicitTransaction();
    fprintf(stderr, "[WESTLAKE-GONW] 6a flush#2 returned\n"); fflush(stderr);
    LOGI("getOhNativeWindow[G3.0]: flushed+settled RSSurfaceNode before hwui EGL bind (session %d)", sessionId);
    fprintf(stderr, "[WESTLAKE-GONW] 6b G3.0 LOGI done\n"); fflush(stderr);


    // [G3.3-EGL-DIAG 2026-06-01] Diagnose the FUNDAMENTAL eglCreateWindowSurface failure.
    // hwui's eglCreateWindowSurface(producer) internally Connect()s + RequestBuffer()s this
    // producer surface.  We perform the same RequestBuffer here and dump producer state +
    // the precise GSError — so we learn the real cause (no-consumer vs format/geometry vs
    // alloc) from DATA, not guesses (prior G2.9/G3.0 flush attempts were guesses).
    // The RequestBuffer'd buffer is immediately CancelBuffer'd to leave the queue pristine,
    // and this runs once per session (result is cached) so it cannot race hwui's RenderThread.
    {
        OHOS::sptr<OHOS::Surface>& ps = *wl_psPtr;  // §283n: iterator may be stale once unlocked
    fprintf(stderr, "[WESTLAKE-GONW] 6c calling GetDefaultWidth\n"); fflush(stderr);
        const int dw = ps->GetDefaultWidth();
    fprintf(stderr, "[WESTLAKE-GONW] 6d GetDefaultWidth ok\n"); fflush(stderr);

        const int dh = ps->GetDefaultHeight();
    fprintf(stderr, "[WESTLAKE-GONW] 6e GetDefaultHeight ok\n"); fflush(stderr);

        const uint64_t du = ps->GetDefaultUsage();
    fprintf(stderr, "[WESTLAKE-GONW] 6f GetDefaultUsage ok\n"); fflush(stderr);

        const int df = ps->GetDefaultFormat();
    fprintf(stderr, "[WESTLAKE-GONW] 6g GetDefaultFormat ok\n"); fflush(stderr);

        // WESTLAKE §283i — THE UI-THREAD DEADLOCK.  ps->GetQueueSize() is a synchronous IPC
        // to the BufferQueue consumer and NEVER RETURNS on this port (bisected with the
        // [WESTLAKE-GONW] markers: '6g GetDefaultFormat ok' prints, '6h GetQueueSize ok'
        // never does).  It parked the UI thread inside Surface.copyFrom -> ... ->
        // getOhNativeWindow, which is why giving a SurfaceControl a real session made
        // relayout stop after ONE pass (§283c/e).  It is used ONLY for the diagnostic
        // LOGI below, so it is pure cost.  GetName() is the same class of call and is
        // dropped for the same reason.
        const uint32_t qs = 0;  // was ps->GetQueueSize() -- BLOCKS FOREVER
    fprintf(stderr, "[WESTLAKE-GONW] 6h GetQueueSize ok\n"); fflush(stderr);

        fprintf(stderr, "[WESTLAKE-GONW] 6i calling IsConsumer\n"); fflush(stderr);
        // WESTLAKE §283i (2nd): ps->IsConsumer() ALSO never returns (marker 6i prints, 6j
        // does not).  Like GetQueueSize() it is a synchronous consumer-side IPC used only
        // for the diagnostic LOGI below.  Dropped.
        const int wl_isc = -1;  // was ps->IsConsumer() -- BLOCKS FOREVER
        fprintf(stderr, "[WESTLAKE-GONW] 6j IsConsumer ok -> about to LOGI G3.3-state\n"); fflush(stderr);
        LOGI("getOhNativeWindow[G3.3-state]: session=%{public}d isConsumer=%{public}d defW=%{public}d defH=%{public}d "
             "defUsage=0x%{public}llx defFormat=%{public}d queueSize=%{public}u name=%{public}s",
             sessionId, wl_isc, dw, dh,
             (unsigned long long)du, df, qs, "<skipped:§283i>");
        fprintf(stderr, "[WESTLAKE-GONW] 6k G3.3-state LOGI done\n"); fflush(stderr);


        OHOS::BufferRequestConfig cfg;
        cfg.width  = dw > 0 ? dw : 720;
        cfg.height = dh > 0 ? dh : 1280;
        cfg.strideAlignment = 8;
        cfg.format = df > 0 ? df : 12;               // 12 = GRAPHIC_PIXEL_FMT_RGBA_8888
        cfg.usage  = du != 0 ? du : 0x108ULL;        // 0x108 = HW_RENDER(1<<8)|MEM_DMA(1<<3)
        cfg.timeout = 0;                             // non-blocking: won't hang the bind path

        // WESTLAKE §283i (3rd): the G3.3 RequestBuffer/CancelBuffer DIAGNOSTIC probe also
        // never returns (marker 7 prints, 8 does not) -- cfg.timeout=0 does NOT mean
        // "non-blocking" here, it blocks forever when no consumer is attached.  It exists
        // only to log a GSError, and it was running on the UI thread inside
        // Surface.copyFrom, so it is a pure deadlock source.  Disabled.
#if 0
        OHOS::sptr<OHOS::SurfaceBuffer> probeBuf;
        int32_t probeFence = -1;
    fprintf(stderr, "[WESTLAKE-GONW] 7 calling RequestBuffer\n"); fflush(stderr);
        auto rqErr = ps->RequestBuffer(probeBuf, probeFence, cfg);
    fprintf(stderr, "[WESTLAKE-GONW] 8 RequestBuffer returned\n"); fflush(stderr);

        LOGI("getOhNativeWindow[G3.3-request]: session=%{public}d RequestBuffer rc=%{public}d buf=%{public}p fence=%{public}d  "
             "[decode 0=OK 41202000=NO_CONSUMER 41211000=CONSUMER_DISCONNECTED 40601000=NO_BUFFER "
             "40001000=INVALID_ARGS 41206000=CONSUMER_IS_CONNECTED 41203000=NOT_INIT]",
             sessionId, (int)rqErr, (void*)probeBuf.GetRefPtr(), probeFence);
        if (probeBuf != nullptr) {
            auto cErr = ps->CancelBuffer(probeBuf);
            LOGI("getOhNativeWindow[G3.3-cancel]: session=%{public}d CancelBuffer rc=%{public}d", sessionId, (int)cErr);
        }
#endif

        // [G3.4-RENDER-CONFIG 2026-06-01] THE FIX. G3.3 proved the surface is functional but
        // reports defFormat=0 (UNSET) + defUsage=0x408 (NO HW_RENDER GPU-write bit 0x100).  A
        // GPU render target needs a valid pixel format + HW_RENDER usage, else hwui's
        // eglCreateWindowSurface returns EGL_NO_SURFACE -> libhwui abort (deterministic; s014
        // was a rare alloc fluke).  Set the producer's DEFAULTS via the typed Surface API
        // (reliable; the variadic NativeWindowHandleOpt corrupted the 64-bit usage arg) so the
        // OH NativeWindow + eglCreateWindowSurface inherit a valid GPU render-target config.
        // WESTLAKE §284b TESTED AND REVERTED: setting the producer's default width/height is
        // NOT the fix. (a) `ps->GetDefaultWidth()` does not return a width at all here -- it
        // returns the GSError **50102000 (NOT_SUPPORT)**, the same code the G3.4b note records
        // for SetDefaultFormat, so these producer default-size APIs are unsupported on this
        // surface; passing that value made SetDefaultWidthAndHeight return 40001000
        // (INVALID_ARGS). (b) More importantly `SrcRect [0,0,0,0]` in RS is a SYMPTOM of "no
        // buffer has been ACQUIRED", not of a wrong size -- the queued buffers already carry
        // 1200x1920. Fix the acquire/release path, not the default size.
        auto sfErr = ps->SetDefaultFormat(12);                       // GRAPHIC_PIXEL_FMT_RGBA_8888
        auto suErr = ps->SetDefaultUsage(du | 0x100ULL | 0x200ULL);  // +HW_RENDER +HW_TEXTURE
        const int      vF = ps->GetDefaultFormat();
        const uint64_t vU = ps->GetDefaultUsage();
        LOGI("getOhNativeWindow[G3.4-config]: session=%{public}d SetDefaultFormat rc=%{public}d SetDefaultUsage rc=%{public}d",
             sessionId, (int)sfErr, (int)suErr);
        LOGI("getOhNativeWindow[G3.4-verify]: session=%{public}d after-set defFormat=%{public}d defUsage_lo32=0x%{public}x "
             "(want fmt=12, want bits 0x100|0x200 set)", sessionId, vF, (unsigned)(vU & 0xFFFFFFFFu));
    }

    // CreateNativeWindowFromSurface signature: OHNativeWindow* fn(void* pSurface)
    // expects address of sptr<Surface> (not the raw surface ptr).  See
    // foundation/graphic/graphic_surface/surface/src/native_window.cpp.
    // Pass address of SessionEntry-held sptr so the backing producer survives
    // hwui's async refcount increments.
    fprintf(stderr, "[WESTLAKE-GONW] 9 calling CreateNativeWindowFromSurface\n"); fflush(stderr);
    OHNativeWindow* nw = ::CreateNativeWindowFromSurface(wl_psPtr);
    fprintf(stderr, "[WESTLAKE-GONW] 10 CreateNativeWindowFromSurface returned tid=%d\n",
            (int) syscall(178)); fflush(stderr);
    // WESTLAKE §321: give the fresh OHNativeWindow a real BUFFER GEOMETRY.
    // Measured (§320): hwui drew real content (`drawContent boundsW=1200 boundsH=1920`,
    // renderNodes=1) but `SkiaOpenGLPipeline::swapBuffers drew=1 width=0 height=0` and then
    // eglSwapBuffers returned EGL_BAD_SURFACE (0x300d). hwui's Frame w/h come from
    // eglQuerySurface(EGL_WIDTH/EGL_HEIGHT), so 0x0 means the EGLSurface is DEGENERATE: the
    // native window it was created from carries no geometry. Nothing ever set it — the shim's
    // SET_BUFFER_GEOMETRY path only runs if Android calls native_window_set_buffers_geometry,
    // which hwui does not do here, and the producer-side default-size API is NOT_SUPPORT on this
    // board (§284b). Set it here, at the ABI boundary, using the documented NativeWindow op.
    // Arg order matters and differs per op (see oh_anativewindow_shim.cpp): SET takes
    // (width, height); GET takes (&height, &width).
    if (nw != nullptr) {
        // WESTLAKE §388b ★★★ ALWAYS assert the buffer usage — this is what makes our frames appear.
        // The block below only runs when the geometry is still unset, so on any window that already
        // had geometry the usage was never touched and stayed 0x309 (HW_RENDER|HW_TEXTURE|MEM_DMA|
        // CPU_READ). With 0x309 the whole pipeline works and still presents BLACK; adding CPU_WRITE
        // (0x2 -> 0x30B) is what actually puts noice's UI on the panel. Proven by §387's CPU-fill
        // probe: the screen changed the moment SET_USAGE(0x30B) was issued on the producer, before
        // any CPU fill had succeeded, and it changed back when only the conditional path ran.
        {
            uint64_t wl_ua = 0;
            OH_NativeWindow_NativeWindowHandleOpt(nw, /*GET_USAGE*/ 4, &wl_ua);
            const uint64_t wl_ub = wl_ua | 0x100ULL | 0x200ULL | 0x2ULL;
            if (wl_ub != wl_ua) {
                const int32_t wl_rcua =
                        OH_NativeWindow_NativeWindowHandleOpt(nw, /*SET_USAGE*/ 5, wl_ub);
                uint64_t wl_uc = 0;
                OH_NativeWindow_NativeWindowHandleOpt(nw, /*GET_USAGE*/ 4, &wl_uc);
                fprintf(stderr,
                        "[WESTLAKE-NWUSAGE] §388b session=%d usage 0x%llx -> 0x%llx (rc=%d) now=0x%llx\n",
                        (int) sessionId, (unsigned long long) wl_ua, (unsigned long long) wl_ub,
                        wl_rcua, (unsigned long long) wl_uc);
                fflush(stderr);
            }
        }
        int32_t wl_gh = 0, wl_gw = 0;
        OH_NativeWindow_NativeWindowHandleOpt(nw, /*GET_BUFFER_GEOMETRY*/ 1, &wl_gh, &wl_gw);
        if (wl_gw <= 0 || wl_gh <= 0) {
            // Same fullscreen convention this file already uses for SetBounds/SetFrame.
            const int32_t wl_w = 1200, wl_h = 1920;
            const int32_t wl_rc =
                    OH_NativeWindow_NativeWindowHandleOpt(nw, /*SET_BUFFER_GEOMETRY*/ 0, wl_w, wl_h);
            // RGBA_8888 — matches the SetDefaultFormat(12) the producer path already requests.
            const int32_t wl_rcf = OH_NativeWindow_NativeWindowHandleOpt(nw, /*SET_FORMAT*/ 3, 12);
            // §321b: SET_BUFFER_GEOMETRY can reset the producer's usage, and the G3.4 note in this
            // file records that WITHOUT the HW_RENDER (0x100) bit eglCreateWindowSurface / GPU
            // rendering fails. Re-assert usage AFTER the geometry change, and log before/after so a
            // clobber is visible rather than inferred.
            uint64_t wl_u0 = 0;
            OH_NativeWindow_NativeWindowHandleOpt(nw, /*GET_USAGE*/ 4, &wl_u0);
            // WESTLAKE §388 ★★★ THE FIX THAT PUT noice ON SCREEN.
            // We set HW_RENDER|HW_TEXTURE (0x300) but NOT CPU_WRITE (0x2).  With usage 0x309 the
            // buffers were produced, queued, acquired by RS and even drawn
            // (RSUniRenderEngine::DrawSurfaceNodeWithParams ran every frame) — and came out BLACK.
            // Adding CPU_WRITE, i.e. usage 0x30B, makes the very same pipeline present our pixels:
            // noice's AppIntro ("Welcome" / the noice logo / SKIP) renders on the panel.
            // Found by a CPU-fill probe (§387) that set 0x30B on the producer before memset-ing a
            // buffer: the screen changed on the SET_USAGE call, before any CPU fill succeeded.
            const uint64_t wl_u1 = wl_u0 | 0x100ULL | 0x200ULL | 0x2ULL;   // +HW_RENDER +HW_TEXTURE
            const int32_t wl_rcu = OH_NativeWindow_NativeWindowHandleOpt(nw, /*SET_USAGE*/ 5, wl_u1);
            uint64_t wl_u2 = 0;
            OH_NativeWindow_NativeWindowHandleOpt(nw, /*GET_USAGE*/ 4, &wl_u2);
            int32_t wl_vh = 0, wl_vw = 0;
            OH_NativeWindow_NativeWindowHandleOpt(nw, /*GET_BUFFER_GEOMETRY*/ 1, &wl_vh, &wl_vw);
            fprintf(stderr,
                    "[WESTLAKE-NWGEOM] session=%d was %dx%d -> set %dx%d (rc=%d fmtrc=%d) verify=%dx%d "
                    "usage 0x%llx -> 0x%llx (rc=%d) now=0x%llx\n",
                    (int) sessionId, (int) wl_gw, (int) wl_gh, (int) wl_w, (int) wl_h,
                    (int) wl_rc, (int) wl_rcf, (int) wl_vw, (int) wl_vh,
                    (unsigned long long) wl_u0, (unsigned long long) wl_u1,
                    (int) wl_rcu, (unsigned long long) wl_u2);
            fflush(stderr);
        } else {
            fprintf(stderr, "[WESTLAKE-NWGEOM] session=%d already %dx%d (no change)\n",
                    (int) sessionId, (int) wl_gw, (int) wl_gh);
            fflush(stderr);
        }
    }
    // WESTLAKE §284f: RS shows a correctly-sized BufferQueue for this node whose buffers are
    // ALL state=0/timestamp=0 (never filled) even though hwui swaps 8 frames.  Print the
    // producer uniqueId hwui will render into so it can be compared against the consumer
    // uniqueId RS reports (hidumper -s RenderService -a allSurfacesMem).  A mismatch means
    // we hand hwui a producer for a DIFFERENT queue than the one RS consumes.
    {
        std::shared_ptr<OHOS::Rosen::RSSurfaceNode> wl_node;
        {
            std::lock_guard<std::recursive_mutex> wl_lk(sessionMutex_);
            auto wl_it = sessions_.find(sessionId);
            if (wl_it != sessions_.end()) wl_node = wl_it->second.surfaceNode;
        }
        fprintf(stderr,
                "[WESTLAKE-QID] session=%d producer uniqueId=%llu rsNodeId=%llu nodeName=%s nw=%p\n",
                (int) sessionId,
                (unsigned long long) ((*wl_psPtr) ? (*wl_psPtr)->GetUniqueId() : 0ULL),
                (unsigned long long) (wl_node ? wl_node->GetId() : 0ULL),
                wl_node ? wl_node->GetName().c_str() : "?",
                (void*) nw);
        fflush(stderr);
    }

    if (!nw) {
        LOGE("getOhNativeWindow: CreateNativeWindowFromSurface failed for sessionId=%d", sessionId);
        return nullptr;
    }
    // 2026-05-12 G2.14aw probe A.1: post-CreateNativeWindowFromSurface uniqueId.
    // OH NativeWindow internally copies sptr<Surface>, so uniqueId here MUST match the baseline above.
    // Mismatch ⇒ CreateNativeWindowFromSurface silently swapped the backing surface (very unlikely
    // but worth checking once — if equal we can drop this log later).
    {
        uint64_t uidPost = 0;
        int32_t qrc = ::OH_NativeWindow_GetSurfaceId(nw, &uidPost);
        LOGI("getOhNativeWindow[probe-postCreate]: sessionId=%d nw=%p uniqueId=0x%llx (rc=%d)",
             sessionId, (void*)nw, (unsigned long long)uidPost, qrc);
    }

    // [G3.4b-NW-FORMAT 2026-06-01] producer->SetDefaultFormat returns NOT_SUPPORT (50102000),
    // so the surface's defFormat stays 0 — but eglCreateWindowSurface needs a valid pixel
    // format.  Set the format (and re-assert GPU usage) on the OH NativeWindow itself via
    // NativeWindowHandleOpt, which DOES work for format (SET_FORMAT rc=0).  GET-readback
    // confirms what eglCreateWindowSurface will actually see.
    {
        const int32_t  kRGBA8888 = 12;
        const uint64_t kGpuUsage = (*wl_psPtr)->GetDefaultUsage() | 0x100ULL | 0x200ULL;  // §283n
        int32_t rcF = OH_NativeWindow_NativeWindowHandleOpt(nw, 3 /*SET_FORMAT*/, kRGBA8888);
        int32_t rcU = OH_NativeWindow_NativeWindowHandleOpt(nw, 5 /*SET_USAGE*/, kGpuUsage);
        int32_t goF = -1; uint64_t goU = 0;
        OH_NativeWindow_NativeWindowHandleOpt(nw, 2 /*GET_FORMAT*/, &goF);
        OH_NativeWindow_NativeWindowHandleOpt(nw, 4 /*GET_USAGE*/, &goU);
        LOGI("getOhNativeWindow[G3.4b-nwcfg]: session=%{public}d SET_FORMAT rc=%{public}d SET_USAGE rc=%{public}d",
             sessionId, rcF, rcU);
        LOGI("getOhNativeWindow[G3.4b-verify]: session=%{public}d nw GET_FORMAT=%{public}d GET_USAGE_lo32=0x%{public}x "
             "(want fmt=12, want 0x100|0x200 set)", sessionId, goF, (unsigned)(goU & 0xFFFFFFFFu));
    }

    // 2026-05-09 G2.14ae: hwui treats ANativeWindow as an AOSP POD struct
    // with a function-pointer table at fixed offsets (system/window.h).
    // OH NativeWindow is a C++ virtual class deriving from RefBase with
    // sptr<Surface> / unordered_map / atomic members — completely
    // incompatible layout. Returning the raw OH handle to hwui caused
    // SIGSEGV in CanvasContext::setupPipelineSurface where hwui dereferenced
    // an offset that, on AOSP, would be common.reserved[2] but on OH falls
    // inside RefBase internals.
    //
    // Wrap the OH handle in an adapter-allocated AOSP-ABI-compatible
    // ANativeWindow struct whose 10 function pointers route to wrappers
    // calling OH_NativeWindow_* NDK equivalents. hwui sees the AOSP ABI
    // it expects; OH-side details stay hidden behind the wrapper.
    //
    // Reference: doc/graphics_rendering_design.html §7.11.
    void* aospAnw = ::oh_anw_wrap(reinterpret_cast<OHNativeWindow*>(nw));
    if (!aospAnw) {
        LOGE("getOhNativeWindow: oh_anw_wrap failed for sessionId=%d", sessionId);
        return nullptr;
    }
    // §283n: re-acquire briefly to publish the result (the iterator may be stale by now).
    {
        std::lock_guard<std::recursive_mutex> relock(sessionMutex_);
        auto it2 = sessions_.find(sessionId);
        if (it2 != sessions_.end()) {
            it2->second.ohNativeWindow = aospAnw;  // hwui-facing handle (AOSP ABI)
            // §328: stamp which RSSurfaceNode this window was built from, so a later call can tell
            // the cache is stale after a window re-creation replaces the session's surfaceNode.
            it2->second.ohNativeWindowNodeId =
                    it2->second.surfaceNode ? it2->second.surfaceNode->GetId() : 0ULL;
        }
    }
    LOGI("getOhNativeWindow: sessionId=%d -> AOSP-compat ANativeWindow=%p "
         "(oh=%p, surfaceNodeId=%lld)",
         sessionId, aospAnw, (void*)nw, (long long)it->second.surfaceNodeId);
    return aospAnw;
}

}  // namespace oh_adapter
