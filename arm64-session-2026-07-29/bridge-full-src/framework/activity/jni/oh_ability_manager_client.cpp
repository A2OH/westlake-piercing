/*
 * oh_ability_manager_client.cpp
 *
 * OpenHarmony AbilityManager IPC client implementation.
 *
 * Connects to OH AbilityManagerService via SystemAbilityManager and sends
 * IPC requests (StartAbility, ConnectAbility, etc.) using OH IPC framework.
 *
 * Reference paths:
 *   OH: ability_rt/interfaces/inner_api/ability_manager/include/ability_manager_interface.h
 *   OH: ability_rt/services/abilitymgr/include/ability_manager_proxy.h
 */
#include "oh_ability_manager_client.h"
#include "oh_app_mgr_client.h"  // G2.14i: ApplicationForegrounded reverse IPC
#include "oh_callback_handler.h"
#include <android/log.h>
#include <thread>
#include <atomic>
#include <unistd.h>
#include <cstdint>

#include "ipc_skeleton.h"
#include "iservice_registry.h"
#include "system_ability_manager_proxy.h"
#include "ability_manager_interface.h"
#include "ability_manager_proxy.h"
#include "want.h"

// [FIX-SHARE-PASTEBOARD 2026-06-06] OHOS NDK pasteboard + UDMF C API. noice's
// "与朋友分享 / Share with friends" = ACTION_SEND text/plain -> createChooser ->
// our chooser-unwrap -> ohos.want.action.sendData. This board has NO app
// registered to receive a text share, so OH AMS dead-ends at com.ohos.amsdialog
// ("无法打开此文件"). Instead of that dead-end, copy the share text to the system
// clipboard (real, useful share — user can paste the invite link to a friend).
// C linkage (.so on device at /system/lib/ndk/lib{pasteboard,udmf}.so).
#include "uds.h"
#include "udmf.h"
#include "oh_pasteboard.h"
#include <string>

#define LOG_TAG "OH_AbilityMgrClient"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

// OH system ability ID for AbilityManagerService
static constexpr int32_t ABILITY_MGR_SERVICE_ID = 180;

namespace oh_adapter {

// ---- [FIX-SHARE-PASTEBOARD] helpers ----------------------------------------

// Minimal JSON-string-value unescape (org.json output: \n \t \r \" \\ \/ \b \f).
static std::string shareJsonUnescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '\\' && i + 1 < s.size()) {
            char n = s[++i];
            switch (n) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case '"': out += '"';  break;
                case '\\': out += '\\'; break;
                case '/': out += '/';  break;
                case 'u': if (i + 4 < s.size()) i += 4; break;  // skip \uXXXX
                default: out += n; break;
            }
        } else {
            out += c;
        }
    }
    return out;
}

// Extract the value of a JSON string key from the extras JSON (no full parser).
static std::string shareExtractKey(const std::string& json, const char* keyQuoted) {
    size_t kp = json.find(keyQuoted);
    if (kp == std::string::npos) return "";
    size_t colon = json.find(':', kp + std::string(keyQuoted).size());
    if (colon == std::string::npos) return "";
    size_t q = json.find('"', colon + 1);
    if (q == std::string::npos) return "";
    std::string raw;
    for (size_t i = q + 1; i < json.size(); ++i) {
        char c = json[i];
        if (c == '\\' && i + 1 < json.size()) { raw += c; raw += json[++i]; continue; }
        if (c == '"') break;
        raw += c;
    }
    return shareJsonUnescape(raw);
}

// Copy plain text to the OHOS system clipboard via the NDK pasteboard + UDMF C
// API. Returns true on success (rc == UDMF_E_OK / 0).
static bool shareCopyToPasteboard(const std::string& text) {
    OH_UdsPlainText* pt = OH_UdsPlainText_Create();
    OH_UdmfRecord*   rec = OH_UdmfRecord_Create();
    OH_UdmfData*     data = OH_UdmfData_Create();
    OH_Pasteboard*   pb = OH_Pasteboard_Create();
    int rc = -1;
    if (pt && rec && data && pb) {
        OH_UdsPlainText_SetContent(pt, text.c_str());
        OH_UdmfRecord_AddPlainText(rec, pt);
        OH_UdmfData_AddRecord(data, rec);
        rc = OH_Pasteboard_SetData(pb, data);
    }
    LOGI("shareCopyToPasteboard: %zu chars, OH_Pasteboard_SetData rc=%d", text.size(), rc);
    // NDK Add* deep-copy internally -> destroy each created object independently.
    if (pt)   OH_UdsPlainText_Destroy(pt);
    if (rec)  OH_UdmfRecord_Destroy(rec);
    if (data) OH_UdmfData_Destroy(data);
    if (pb)   OH_Pasteboard_Destroy(pb);
    return rc == 0;
}

OHAbilityManagerClient& OHAbilityManagerClient::getInstance() {
    static OHAbilityManagerClient instance;
    return instance;
}

bool OHAbilityManagerClient::connect() {
    LOGI("Connecting to OH AbilityManagerService...");

    auto samgr = OHOS::SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (samgr == nullptr) {
        LOGE("Failed to get SystemAbilityManager");
        return false;
    }

    auto remoteObject = samgr->GetSystemAbility(ABILITY_MGR_SERVICE_ID);
    if (remoteObject == nullptr) {
        LOGE("Failed to get AbilityManagerService remote object (SA ID=%d)", ABILITY_MGR_SERVICE_ID);
        return false;
    }

    proxy_ = OHOS::iface_cast<OHOS::AAFwk::IAbilityManager>(remoteObject);
    if (proxy_ == nullptr) {
        LOGE("Failed to cast remote object to IAbilityManager");
        return false;
    }

    connected_ = true;
    LOGI("Connected to OH AbilityManagerService successfully");
    return true;
}

void OHAbilityManagerClient::disconnect() {
    LOGI("Disconnecting from OH AbilityManagerService");
    proxy_ = nullptr;
    connected_ = false;
}

int OHAbilityManagerClient::startAbility(const WantParams& want) {
    // Legacy entry: no caller token.  Kept for callers that genuinely lack
    // one (e.g., system-initiated cold-start paths).  For App-initiated
    // startActivity() — which is the helloworld user button case — use
    // startAbilityWithCaller() so OH AMS [ABMS1361]caller-invalid check
    // passes.  See 2026-05-19 doc/build_patch_log.html "Start SecondActivity"
    // entry for the full diagnosis.
    return startAbilityWithCaller(want, /*callerOhTokenAddr=*/0);
}

int OHAbilityManagerClient::startAbilityWithCaller(const WantParams& want,
                                                    jlong callerOhTokenAddr) {
    if (!connected_ || proxy_ == nullptr) {
        LOGE("Not connected to AbilityManagerService");
        return -1;
    }

    LOGI("StartAbility: bundle=%s, ability=%s, action=%s, callerOhTokenAddr=0x%llx",
         want.bundleName.c_str(), want.abilityName.c_str(), want.action.c_str(),
         static_cast<unsigned long long>(callerOhTokenAddr));

    // [FIX-SHARE-PASTEBOARD 2026-06-06] Implicit text share (ACTION_SEND ->
    // sendData) with no target ability on this board dead-ends at OH AMS's
    // com.ohos.amsdialog ("无法打开此文件"). Intercept it: copy the share text
    // (android.intent.extra.TEXT) to the system clipboard and report success,
    // so the user can paste the invite link to a friend. Only for implicit
    // sends (no explicit target component) — explicit sendData targets still
    // route to OH AMS normally.
    if (want.action == "ohos.want.action.sendData" && want.bundleName.empty()) {
        std::string text = shareExtractKey(want.parametersJson,
                                           "\"android.intent.extra.TEXT\"");
        if (!text.empty() && shareCopyToPasteboard(text)) {
            LOGI("sendData (share) -> copied to clipboard, skipping no-target StartAbility");
            return 0;  // START_SUCCESS; avoids com.ohos.amsdialog dead-end
        }
        LOGW("sendData share: clipboard copy unavailable (text empty or rc!=0); "
             "falling through to normal StartAbility");
    }

    // Construct OH Want object
    OHOS::AAFwk::Want ohWant;
    OHOS::AppExecFwk::ElementName element("", want.bundleName, want.abilityName);
    ohWant.SetElement(element);

    if (!want.action.empty()) {
        ohWant.SetAction(want.action);
    }
    if (!want.uri.empty()) {
        ohWant.SetUri(want.uri);
    }
    if (!want.parametersJson.empty()) {
        // Pass extras as a string parameter for Java-side parsing
        ohWant.SetParam("android_extras_json", want.parametersJson);
    }

    // Call AbilityManager.StartAbility via IPC.  Two overloads:
    //   - StartAbility(want, userId, requestCode, ...) — system caller path,
    //     rejected by OH AMS for normal-apl App callers with [ABMS1361]
    //     caller invalid.
    //   - StartAbility(want, callerToken, userId, requestCode, ...) —
    //     App-caller path, validates callerToken against existing ability
    //     nodes server-side.
    // We pick based on callerOhTokenAddr non-zero (App caller path).
    int result;
    if (callerOhTokenAddr != 0) {
        OHOS::IRemoteObject* raw =
            reinterpret_cast<OHOS::IRemoteObject*>(callerOhTokenAddr);
        OHOS::sptr<OHOS::IRemoteObject> callerToken(raw);
        result = proxy_->StartAbility(ohWant, callerToken);
    } else {
        result = proxy_->StartAbility(ohWant);
    }

    LOGI("StartAbility returned %d (0=success)", result);
    return result;
}

int OHAbilityManagerClient::connectAbility(const WantParams& want, int connectionId) {
    if (!connected_ || proxy_ == nullptr) {
        LOGE("Not connected to AbilityManagerService");
        return -1;
    }

    LOGI("ConnectAbility: bundle=%s, ability=%s, connId=%d",
         want.bundleName.c_str(), want.abilityName.c_str(), connectionId);

    // Construct OH Want
    OHOS::AAFwk::Want ohWant;
    OHOS::AppExecFwk::ElementName element("", want.bundleName, want.abilityName);
    ohWant.SetElement(element);

    // Get per-connection AbilityConnection stub from OHCallbackHandler
    auto connection = OHCallbackHandler::getInstance().getAbilityConnection(connectionId);
    if (connection == nullptr) {
        LOGE("Failed to create AbilityConnection for connId=%d", connectionId);
        return -1;
    }

    // V7 ConnectAbility(want, connect, callerToken, userId). We have no
    // meaningful caller token from the Android side; pass nullptr.
    int result = proxy_->ConnectAbility(ohWant, connection, nullptr, -1);
    LOGI("ConnectAbility returned %d", result);
    return result;
}

int OHAbilityManagerClient::disconnectAbility(int connectionId) {
    if (!connected_ || proxy_ == nullptr) {
        LOGE("Not connected to AbilityManagerService");
        return -1;
    }

    LOGI("DisconnectAbility: connectionId=%d", connectionId);

    auto connection = OHCallbackHandler::getInstance().getAbilityConnection(connectionId);
    if (connection == nullptr) {
        LOGE("AbilityConnection not found for connId=%d", connectionId);
        return -1;
    }

    int result = proxy_->DisconnectAbility(connection);
    // Clean up the connection adapter
    OHCallbackHandler::getInstance().removeAbilityConnection(connectionId);
    LOGI("DisconnectAbility returned %d", result);
    return result;
}

int OHAbilityManagerClient::stopServiceAbility(const WantParams& want) {
    if (!connected_ || proxy_ == nullptr) {
        LOGE("Not connected to AbilityManagerService");
        return -1;
    }

    LOGI("StopServiceAbility: bundle=%s, ability=%s",
         want.bundleName.c_str(), want.abilityName.c_str());

    OHOS::AAFwk::Want ohWant;
    OHOS::AppExecFwk::ElementName element("", want.bundleName, want.abilityName);
    ohWant.SetElement(element);

    int result = proxy_->StopServiceAbility(ohWant);

    LOGI("StopServiceAbility returned %d (0=success)", result);
    return result;
}

int OHAbilityManagerClient::startAbilityInMission(const WantParams& want, int32_t missionId) {
    if (!connected_ || proxy_ == nullptr) {
        LOGE("Not connected to AbilityManagerService");
        return -1;
    }

    LOGI("StartAbilityInMission: bundle=%s, ability=%s, missionId=%d",
         want.bundleName.c_str(), want.abilityName.c_str(), missionId);

    OHOS::AAFwk::Want ohWant;
    OHOS::AppExecFwk::ElementName element("", want.bundleName, want.abilityName);
    ohWant.SetElement(element);

    if (!want.action.empty()) {
        ohWant.SetAction(want.action);
    }
    if (!want.uri.empty()) {
        ohWant.SetUri(want.uri);
    }
    if (!want.parametersJson.empty()) {
        ohWant.SetParam("android_extras_json", want.parametersJson);
    }

    // ABI-safe routing: use standard IAbilityManager::StartAbility IPC with a
    // marker parameter. AMS-side patch (mission_list_manager) reads the marker
    // and routes to MissionListManager::StartAbilityInMission internally.
    // Never modifies IAbilityManager vtable.
    ohWant.SetParam("__android_in_mission_id", missionId);
    int result = proxy_->StartAbility(ohWant);

    LOGI("StartAbilityInMission returned %d (0=success)", result);
    return result;
}

int OHAbilityManagerClient::cleanMission(int32_t missionId) {
    if (!connected_ || proxy_ == nullptr) {
        LOGE("Not connected to AbilityManagerService");
        return -1;
    }

    LOGI("CleanMission: missionId=%d", missionId);
    int result = proxy_->CleanMission(missionId);
    LOGI("CleanMission returned %d", result);
    return result;
}

int OHAbilityManagerClient::moveMissionToFront(int32_t missionId) {
    if (!connected_ || proxy_ == nullptr) {
        LOGE("Not connected to AbilityManagerService");
        return -1;
    }

    LOGI("MoveMissionToFront: missionId=%d", missionId);
    int result = proxy_->MoveMissionToFront(missionId);
    LOGI("MoveMissionToFront returned %d", result);
    return result;
}

int OHAbilityManagerClient::setMultiAbilityMode(int32_t missionId, bool enabled) {
    // This is set on the Mission object via the StartAbility return path.
    // The adapter stores the mapping locally; the actual flag is set
    // via a custom parameter in the StartAbility Want.
    LOGI("setMultiAbilityMode: missionId=%d, enabled=%d", missionId, enabled);
    // The flag is set on the OH side during Mission creation when
    // the Want contains the android adapter marker.
    return 0;
}

bool OHAbilityManagerClient::isTopAbility(int32_t missionId, const std::string& abilityName) {
    if (!connected_ || proxy_ == nullptr) {
        return false;
    }

    // Query the Mission's top Ability via GetMissionInfo
    OHOS::AAFwk::MissionInfo missionInfo;
    int result = proxy_->GetMissionInfo("", missionId, missionInfo);
    if (result != 0) {
        return false;
    }

    auto element = missionInfo.want.GetElement();
    return element.GetAbilityName() == abilityName;
}

int OHAbilityManagerClient::clearAbilitiesAbove(int32_t missionId, const std::string& abilityName) {
    if (!connected_ || proxy_ == nullptr) {
        LOGE("Not connected to AbilityManagerService");
        return -1;
    }

    LOGI("ClearAbilitiesAbove: missionId=%d, abilityName=%s", missionId, abilityName.c_str());

    // This operation is handled within OH MissionListManager via a custom IPC call.
    // The adapter sets a Want parameter that triggers stack clearing in the patched
    // TerminateAbilityLocked / StartAbilityInMission path.
    //
    // Approach: start the target ability with a CLEAR_TOP marker in the Want.
    // The patched MissionListManager.StartAbilityInMission checks this marker
    // and calls Mission::PopAbilitiesAbove before pushing.
    OHOS::AAFwk::Want ohWant;
    // Use existing mission's bundle from MissionInfo
    OHOS::AAFwk::MissionInfo missionInfo;
    int result = proxy_->GetMissionInfo("", missionId, missionInfo);
    if (result != 0) {
        LOGE("ClearAbilitiesAbove: failed to get MissionInfo for %d", missionId);
        return -1;
    }

    auto element = missionInfo.want.GetElement();
    OHOS::AppExecFwk::ElementName newElement("", element.GetBundleName(), abilityName);
    ohWant.SetElement(newElement);
    ohWant.SetParam("android_clear_top", true);
    // ABI-safe routing: marker param read by patched MissionListManager.
    ohWant.SetParam("__android_in_mission_id", missionId);

    result = proxy_->StartAbility(ohWant);
    LOGI("ClearAbilitiesAbove returned %d", result);
    return result;
}

int32_t OHAbilityManagerClient::getMissionIdForBundle(const std::string& bundleName) {
    if (!connected_ || proxy_ == nullptr) {
        LOGE("Not connected to AbilityManagerService");
        return -1;
    }

    LOGI("GetMissionIdForBundle: bundle=%s", bundleName.c_str());

    // Query recent missions and find one matching the bundle name
    std::vector<OHOS::AAFwk::MissionInfo> missionInfos;
    int result = proxy_->GetMissionInfos("", 100, missionInfos);
    if (result != 0) {
        LOGE("GetMissionIdForBundle: GetMissionInfos failed with %d", result);
        return -1;
    }

    // Search for a mission whose Want element matches the bundle name
    for (const auto& info : missionInfos) {
        auto element = info.want.GetElement();
        if (element.GetBundleName() == bundleName) {
            LOGI("GetMissionIdForBundle: found missionId=%d for bundle=%s",
                 info.id, bundleName.c_str());
            return info.id;
        }
    }

    LOGW("GetMissionIdForBundle: no mission found for bundle=%s", bundleName.c_str());
    return -1;
}

// 2026-04-30 (B.48 §1.2.4.3 P2): TerminateAbility for App finish() reverse callback.
// AppSchedulerBridge OhTokenRegistry stores OH IRemoteObject addr at launch;
// when ActivityClientControllerAdapter.finishActivity gets Android IBinder, it
// looks up the OH token addr and calls this entry to drive OH AbilityMS::TerminateAbility.
int32_t OHAbilityManagerClient::terminateAbilityByTokenAddr(jlong ohTokenAddr, int32_t resultCode) {
    if (!connected_ || proxy_ == nullptr) {
        LOGE("terminateAbility: Not connected to AbilityManagerService");
        return -1;
    }
    if (ohTokenAddr == 0) {
        LOGE("terminateAbility: null token addr");
        return -2;
    }
    // Reconstruct sptr<IRemoteObject> from raw ptr that we stored at launch.
    // Safety: Java side MUST hold the original OH ScheduleLaunchAbility token via
    // OhTokenRegistry; the IRemoteObject stays alive at least until OH itself
    // releases it via OnProcessDied/OnAbilityDied. After TerminateAbility OH side
    // will release; subsequent finish() calls with same addr are no-ops.
    OHOS::IRemoteObject* raw = reinterpret_cast<OHOS::IRemoteObject*>(ohTokenAddr);
    OHOS::sptr<OHOS::IRemoteObject> token(raw);
    int result = proxy_->TerminateAbility(token, resultCode, nullptr);
    LOGI("TerminateAbility(token=0x%llx, code=%d) returned %d",
         static_cast<unsigned long long>(ohTokenAddr), resultCode, result);
    return result;
}

// 2026-04-30 (G2.1 LIFECYCLE_HALF_TIMEOUT fix):
// After Android lifecycle transitions complete (onCreate/onResume/onPause/...)
// AOSP framework calls IActivityClientController.activityResumed(token,...).
// We need to forward this to OH IAbilityManager::AbilityTransitionDone(token,
// state, saveData) so OH AMS knows the app finished the transition; otherwise
// OH AMS triggers LIFECYCLE_HALF_TIMEOUT (~1s) and kills the app.
// Spec: doc/ability_manager_ipc_adapter_design.html (planned §3.x reverse).
// 2026-06-04 LIFECYCLE-FREEZE FIX: foreground heartbeat. See header.
static std::atomic<uint64_t> g_hbTokenAddr{0};
static std::atomic<bool> g_hbStarted{false};
static std::atomic<int> g_hbIntervalMs{60000};

void OHAbilityManagerClient::startForegroundHeartbeat(jlong ohTokenAddr, int intervalMs) {
    if (ohTokenAddr != 0) {
        g_hbTokenAddr.store(static_cast<uint64_t>(ohTokenAddr));
    }
    if (intervalMs > 0) {
        g_hbIntervalMs.store(intervalMs);
    }
    bool expected = false;
    if (!g_hbStarted.compare_exchange_strong(expected, true)) {
        return;  // poller already running; token/interval updated above
    }
    std::thread([]() {
        // [COLD-FG-FIX 2026-06-29] On a cold-booted system noice's initial
        // foreground ACK returns rc=22 (AMS hasn't reached FOREGROUNDING when the
        // one-shot ACK lands) and a slow heartbeat is far too late to disarm the
        // ForegroundLifecycle watchdog (fires in a few sec) -> APP_FREEZE kill in
        // 5-15s. The interval is now 1000ms (see startForegroundHeartbeat caller),
        // so this sleep-first loop re-ACKs at 1,2,3,4,5s and reliably disarms the
        // watchdog once AMS reaches FOREGROUNDING. Verified noice 6/6 cold-launch
        // (a fast-until-converged variant that backed off to 45s was 0/6 — a
        // premature converge left a later watchdog uncovered, so do NOT back off).
        for (;;) {
            int iv = g_hbIntervalMs.load();
            if (iv < 1000) iv = 1000;
            usleep(static_cast<useconds_t>(iv) * 1000);
            uint64_t tok = g_hbTokenAddr.load();
            if (tok == 0) continue;
            // FOREGROUND_NEW = 5 (AbilityLifeCycleState wire enum). When the
            // ability is mid foreground-transition (FOREGROUNDING) this disarms
            // AMS's FOREGROUND_TIMEOUT_MSG; otherwise AMS returns ERR_INVALID_VALUE
            // (22) — a harmless no-op — so the heartbeat is safe to run always.
            int rc = OHAbilityManagerClient::getInstance()
                         .abilityTransitionDoneByTokenAddr(static_cast<jlong>(tok), 5);
            __android_log_print(ANDROID_LOG_INFO, "OH_AbilityMgrClient",
                "[LC-HEARTBEAT] re-ACK FOREGROUND token=0x%llx rc=%d",
                static_cast<unsigned long long>(tok), rc);
        }
    }).detach();
    __android_log_print(ANDROID_LOG_INFO, "OH_AbilityMgrClient",
        "[LC-HEARTBEAT] started: token=0x%llx intervalMs=%d",
        static_cast<unsigned long long>(g_hbTokenAddr.load()), g_hbIntervalMs.load());
}

int32_t OHAbilityManagerClient::abilityTransitionDoneByTokenAddr(jlong ohTokenAddr, int32_t ohState) {
    if (!connected_ || proxy_ == nullptr) {
        LOGE("AbilityTransitionDone: Not connected to AbilityManagerService");
        return -1;
    }
    if (ohTokenAddr == 0) {
        LOGE("AbilityTransitionDone: null token addr");
        return -2;
    }
    OHOS::IRemoteObject* raw = reinterpret_cast<OHOS::IRemoteObject*>(ohTokenAddr);
    OHOS::sptr<OHOS::IRemoteObject> token(raw);
    OHOS::AAFwk::PacMap saveData;  // empty — Android lifecycle doesn't carry OH PacMap
    int result = proxy_->AbilityTransitionDone(token, ohState, saveData);
    LOGI("AbilityTransitionDone(token=0x%llx, ohState=%d) returned %d",
         static_cast<unsigned long long>(ohTokenAddr), ohState, result);
    return result;
}

}  // namespace oh_adapter

extern "C" {

// JNI entry for ActivityClientControllerAdapter.finishActivity reverse callback.
// adapter.activity.ActivityClientControllerAdapter -> nativeTerminateAbilityByTokenAddr
JNIEXPORT jint JNICALL
Java_adapter_activity_ActivityClientControllerAdapter_nativeTerminateAbilityByTokenAddr(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong ohTokenAddr, jint resultCode) {
    return oh_adapter::OHAbilityManagerClient::getInstance()
        .terminateAbilityByTokenAddr(ohTokenAddr, resultCode);
}

// 2026-04-30 (G2.1): JNI for ActivityClientControllerAdapter.activityResumed
// reverse callback.  Forwards to OH AbilityMS::AbilityTransitionDone.
//
// G2.14j (2026-05-01): translate AbilityState (internal AMS enum, what the Java
// caller passes) → AbilityLifeCycleState (IPC wire enum the AMS server expects).
// AMS calls StateUtils::ConvertStateMap(AbilityLifeCycleState) → AbilityState
// internally before dispatch. Sending FOREGROUND(9) directly is interpreted as
// AbilityLifeCycleState::ABILITY_STATE_BACKGROUND_FAILED(=9) → DispatchBackground
// → expects BACKGROUNDING → fails with rc=22. Correct value is
// ABILITY_STATE_FOREGROUND_NEW(=5).
//
// AbilityState (input from Java)         | AbilityLifeCycleState (IPC wire)
//   INACTIVE=1                           | ABILITY_STATE_INACTIVE=1     (same)
//   FOREGROUND=9                         | ABILITY_STATE_FOREGROUND_NEW=5
//   BACKGROUND=10                        | ABILITY_STATE_BACKGROUND_NEW=6
JNIEXPORT jint JNICALL
Java_adapter_activity_ActivityClientControllerAdapter_nativeAbilityTransitionDone(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong ohTokenAddr, jint ohState) {
    int wireState = ohState;
    switch (ohState) {
        case 9:  wireState = 5; break;   // FOREGROUND -> ABILITY_STATE_FOREGROUND_NEW
        case 10: wireState = 6; break;   // BACKGROUND -> ABILITY_STATE_BACKGROUND_NEW
        case 1:  wireState = 1; break;   // INACTIVE   -> ABILITY_STATE_INACTIVE (identity)
        default:
            // pass through unknown values; OH ConvertStateMap returns DEFAULT_INVAL_VALUE
            // for unrecognized values, AMS will reject explicitly.
            break;
    }
    __android_log_print(ANDROID_LOG_INFO, "OH_AbilityMgrClient",
        "[G2.14j] nativeAbilityTransitionDone: ohState=%d -> wireState=%d", ohState, wireState);
    // 2026-06-04 LIFECYCLE-FREEZE FIX: on a foreground ACK, (re)start the
    // foreground heartbeat for this ability token. AMS arms a 2nd
    // ForegroundLifecycle watchdog (post AppForegrounded) that the one-shot ACK
    // never disarms → APP_FREEZE kill. The heartbeat re-ACKs foreground every
    // 45s so any pending FOREGROUND_TIMEOUT_MSG is removed before it fires.
    if (ohState == 9 /*FOREGROUND*/ && ohTokenAddr != 0) {
        // [COLD-FG-FIX 2026-06-29] 45000 -> 1000: re-ACK every 1s so the cold-boot
        // foreground transition converges before the ForegroundLifecycle watchdog
        // kills the app. Verified noice 6/6 cold-launch + reboot-persistent.
        oh_adapter::OHAbilityManagerClient::getInstance()
            .startForegroundHeartbeat(ohTokenAddr, 1000);
    }
    return oh_adapter::OHAbilityManagerClient::getInstance()
        .abilityTransitionDoneByTokenAddr(ohTokenAddr, wireState);
}

// Alias for AppSchedulerBridge usage (PathClassLoader scope, RegisterNatives).
JNIEXPORT jint JNICALL
Java_adapter_activity_AppSchedulerBridge_nativeTerminateAbility(
        JNIEnv* env, jclass clazz, jlong ohTokenAddr, jint resultCode) {
    return Java_adapter_activity_ActivityClientControllerAdapter_nativeTerminateAbilityByTokenAddr(
        env, clazz, ohTokenAddr, resultCode);
}

// G2.14i (2026-05-01): JNI for AppSchedulerBridge.notifyForegroundDeferred main-looper
// callback. Routes ApplicationForegrounded reverse IPC to OH AppMS. Called from
// Java main-looper Runnable that was posted by AppSchedulerAdapter::ScheduleForegroundApplication.
// By the time this fires, AppMS::AbilityForeground has already finished
// foregroundingAbilityTokens_.insert(token), so PopForegroundingAbilityTokens →
// OnAbilityRequestDone → AMS::ForegroundLifecycle works.
JNIEXPORT void JNICALL
Java_adapter_activity_AppSchedulerBridge_nativeNotifyApplicationForegrounded(
        JNIEnv* /*env*/, jclass /*clazz*/, jint recordId) {
    // Update the cached recordId so subsequent calls also use the latest.
    if (recordId >= 0) {
        oh_adapter::OHAppMgrClient::getInstance().setRecordId(recordId);
    }
    oh_adapter::OHAppMgrClient::getInstance().notifyAppState(
        static_cast<int>(oh_adapter::AppState::STATE_FOREGROUND));
}

// 2026-04-30 (B.48): stub for AOSP ActivityThread.nInitZygoteChildHeapProfiling.
// handleBindApplication line ~6856 calls this when isAppDebuggable || Build.IS_DEBUGGABLE.
// AOSP impl reads SystemProperties to enable malloc heap profiling — OH has no
// equivalent infrastructure. Stub return is safe (no caller relies on side effect).
// Registered via RegisterNatives from appspawnx_runtime.cpp (BCP class).
JNIEXPORT void JNICALL
Java_android_app_ActivityThread_nInitZygoteChildHeapProfiling(
        JNIEnv* /*env*/, jclass /*clazz*/) {
    // no-op
}

}  // extern "C"
