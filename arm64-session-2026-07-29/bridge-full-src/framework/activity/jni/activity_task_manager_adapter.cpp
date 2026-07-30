/*
 * activity_task_manager_adapter.cpp
 *
 * JNI registration for adapter.activity.ActivityTaskManagerAdapter via
 * RegisterNatives.  Replaces the legacy
 * Java_adapter_bridge_ActivityTaskManagerAdapter_* exports that previously
 * lived in framework/core/jni/adapter_bridge.cpp.
 *
 * Class:  adapter/activity/ActivityTaskManagerAdapter  (BCP - oh-adapter-framework.jar)
 * Registered from adapter_bridge.cpp's JNI_OnLoad via
 *   register_ActivityTaskManagerAdapter(env).
 *
 * 8 natives: 2 generic (GetService + StartAbility) + 6 mission stack ops.
 * The 6 mission natives previously had no adapter_activity_* forwarder and
 * raised UnsatisfiedLinkError on first call from bridgeStartActivityWithStack —
 * see 2026-05-19 helloworld pid 2340 crash (build_patch_log entry).
 */

#include "oh_ability_manager_client.h"

#include <android/log.h>
#include <jni.h>
#include <string>

#define TAG "OH_ATMJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

using namespace oh_adapter;

namespace {

std::string jstr(JNIEnv* env, jstring s) {
    if (!s) return "";
    const char* raw = env->GetStringUTFChars(s, nullptr);
    std::string result(raw);
    env->ReleaseStringUTFChars(s, raw);
    return result;
}

// -------- generic (2) --------

jlong nativeGetOHAbilityManagerService_impl(JNIEnv*, jclass) {
    return (jlong)&OHAbilityManagerClient::getInstance();
}

// WESTLAKE 2026-07-22: drive a DL2-launched Activity to onResume.
// Mirrors AppSchedulerBridge.directResume(int), which is private and hardcoded to recordId 1.
// Without this the Activity is created but never resumed, so handleResumeActivity -> addView
// never runs and the screen stays black. All-JNI so no adapter jar / boot image rebuild.
static void wl_dl2_resume(JNIEnv* env, int rec) {
    jclass atCls = env->FindClass("android/app/ActivityThread");
    jclass regCls = env->FindClass("adapter/core/OhTokenRegistry");
    if (atCls == nullptr || regCls == nullptr) { env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-DL2RESUME] class lookup failed\n"); fflush(stderr); return; }
    jmethodID curAT = env->GetStaticMethodID(atCls, "currentActivityThread",
                                             "()Landroid/app/ActivityThread;");
    jmethodID getAppThread = env->GetMethodID(atCls, "getApplicationThread",
                                             "()Landroid/app/ActivityThread$ApplicationThread;");
    jmethodID findTok = env->GetStaticMethodID(regCls, "findByRecordId",
                                               "(I)Landroid/os/IBinder;");
    if (curAT == nullptr || getAppThread == nullptr || findTok == nullptr) { env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-DL2RESUME] method lookup failed\n"); fflush(stderr); return; }
    jobject at = env->CallStaticObjectMethod(atCls, curAT);
    jobject appThread = (at != nullptr) ? env->CallObjectMethod(at, getAppThread) : nullptr;
    jobject token = env->CallStaticObjectMethod(regCls, findTok, (jint) rec);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
    if (appThread == nullptr || token == nullptr) {
        fprintf(stderr, "[WESTLAKE-DL2RESUME] no appThread/token for recordId=%d\n", rec);
        fflush(stderr); return;
    }
    jclass txCls  = env->FindClass("android/app/servertransaction/ClientTransaction");
    jclass resCls = env->FindClass("android/app/servertransaction/ResumeActivityItem");
    if (txCls == nullptr || resCls == nullptr) { env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-DL2RESUME] transaction classes missing\n"); fflush(stderr); return; }
    jmethodID txObtain = env->GetStaticMethodID(txCls, "obtain",
        "(Landroid/app/IApplicationThread;Landroid/os/IBinder;)"
        "Landroid/app/servertransaction/ClientTransaction;");
    jmethodID resObtain = env->GetStaticMethodID(resCls, "obtain",
        "(ZZ)Landroid/app/servertransaction/ResumeActivityItem;");
    if (txObtain == nullptr || resObtain == nullptr) { env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-DL2RESUME] obtain() lookup failed\n"); fflush(stderr); return; }
    jobject tx  = env->CallStaticObjectMethod(txCls, txObtain, appThread, token);
    jobject res = env->CallStaticObjectMethod(resCls, resObtain, JNI_TRUE, JNI_FALSE);
    jmethodID setLifecycle = env->GetMethodID(txCls, "setLifecycleStateRequest",
        "(Landroid/app/servertransaction/ActivityLifecycleItem;)V");
    if (tx == nullptr || res == nullptr || setLifecycle == nullptr) { env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-DL2RESUME] transaction build failed\n"); fflush(stderr); return; }
    env->CallVoidMethod(tx, setLifecycle, res);
    jclass appThreadCls = env->GetObjectClass(appThread);
    jmethodID sched = env->GetMethodID(appThreadCls, "scheduleTransaction",
        "(Landroid/app/servertransaction/ClientTransaction;)V");
    if (sched == nullptr) { env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-DL2RESUME] scheduleTransaction missing\n"); fflush(stderr); return; }
    env->CallVoidMethod(appThread, sched, tx);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-DL2RESUME] scheduleTransaction threw\n"); fflush(stderr); return; }
    fprintf(stderr, "[WESTLAKE-DL2RESUME] resume transaction scheduled for recordId=%d\n", rec);
    fflush(stderr);

    // WESTLAKE §245: MAKE THE DECOR VISIBLE.
    // §244 measured `[OH_WSA-relayout] ... visibility=8` == View.GONE, so ViewRootImpl skips surface
    // acquisition and performDraw() entirely -- hwui is never asked for a buffer and the (correctly
    // composited, §243) surface stays BLACK. AOSP flips this in
    // ActivityThread.handleResumeActivity via `r.activity.makeVisible()`; this in-process DL2 resume
    // path schedules ResumeActivityItem but never performs that step. Do it here.
    // `Activity.makeVisible()` is package-private, so call it reflectively; fall back to
    // getWindow().getDecorView().setVisibility(View.VISIBLE) if it is unavailable.
    {
        jclass atCls2 = env->FindClass("android/app/ActivityThread");
        jmethodID curAT2 = (atCls2 != nullptr)
            ? env->GetStaticMethodID(atCls2, "currentActivityThread",
                                     "()Landroid/app/ActivityThread;") : nullptr;
        jmethodID getAct = (atCls2 != nullptr)
            ? env->GetMethodID(atCls2, "getActivity",
                               "(Landroid/os/IBinder;)Landroid/app/Activity;") : nullptr;
        jclass regCls2 = env->FindClass("adapter/core/OhTokenRegistry");
        jmethodID findTok2 = (regCls2 != nullptr)
            ? env->GetStaticMethodID(regCls2, "findByRecordId", "(I)Landroid/os/IBinder;") : nullptr;
        if (env->ExceptionCheck()) { env->ExceptionClear(); }
        jobject act = nullptr;
        if (curAT2 != nullptr && getAct != nullptr && findTok2 != nullptr) {
            jobject at2  = env->CallStaticObjectMethod(atCls2, curAT2);
            jobject tok2 = env->CallStaticObjectMethod(regCls2, findTok2, (jint) rec);
            if (at2 != nullptr && tok2 != nullptr) {
                act = env->CallObjectMethod(at2, getAct, tok2);
            }
            if (env->ExceptionCheck()) { env->ExceptionClear(); }
        }
        if (act != nullptr) {
            jclass actCls = env->GetObjectClass(act);
            jmethodID mkVis = env->GetMethodID(actCls, "makeVisible", "()V");
            if (mkVis != nullptr) {
                env->CallVoidMethod(act, mkVis);
                if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
                fprintf(stderr, "[WESTLAKE-DL2RESUME] makeVisible() called for recordId=%d\n", rec);
            } else {
                env->ExceptionClear();
                fprintf(stderr, "[WESTLAKE-DL2RESUME] makeVisible not found for recordId=%d\n", rec);
            }
            fflush(stderr);
        } else {
            fprintf(stderr, "[WESTLAKE-DL2RESUME] no Activity for recordId=%d (cannot makeVisible)\n",
                    rec);
            fflush(stderr);
        }
    }
}

jint nativeStartAbility_impl(JNIEnv* env, jclass,
                             jstring bundleName, jstring abilityName,
                             jstring action, jstring uri, jstring extraJson,
                             jlong callerOhTokenAddr) {
    WantParams want;
    want.bundleName = jstr(env, bundleName);
    want.abilityName = jstr(env, abilityName);
    want.action = jstr(env, action);
    want.uri = jstr(env, uri);
    want.parametersJson = jstr(env, extraJson);

    LOGI("nativeStartAbility: bundle=%s, ability=%s, action=%s, callerOhToken=0x%llx",
         want.bundleName.c_str(), want.abilityName.c_str(), want.action.c_str(),
         static_cast<unsigned long long>(callerOhTokenAddr));

    // 2026-06-04 LINK-OPEN GUARD: an implicit ACTION_VIEW (ohos.want.action.viewData
    // with no explicit bundle) is an Android startActivity(ACTION_VIEW, url) — e.g.
    // tapping a license hyperlink on noice's "About this sound" page. This device
    // has no browser/handler, so OH StartAbility returns ERR_IMPLICIT_START_ABILITY_FAIL
    // (2097199) AND the failed implicit start pushes the caller (noice) to the
    // BACKGROUND → looks like a crash (launcher comes forward). Since the link can
    // never open anyway, no-op it and report success so noice keeps foreground.
    if (want.bundleName.empty() && want.action == "ohos.want.action.viewData") {
        LOGI("nativeStartAbility: suppressing implicit viewData link-open (no handler; "
             "keeps noice foreground) uri=%s", want.uri.c_str());
        return 0;  // pretend success to the AOSP caller; do not yield foreground
    }

    // WESTLAKE (2026-07-22) DIRECT-LAUNCH SECOND ACTIVITY:
    // In DIRECT-LAUNCH mode the bundle is never registered with OH BMS, so routing our OWN
    // Activities through OH StartAbility fails (observed: returns 2097152) and nothing is drawn.
    // noice's MainActivity immediately starts AppIntroActivity on first run, so this blocked the
    // first frame. Launch our own package's activities IN-PROCESS via the same
    // AppSchedulerBridge.nativeOnScheduleLaunchAbility(...) call that directLaunchNoBms() uses
    // for MainActivity.
    // WESTLAKE §231 (2026-07-22) — REAL OH StartAbility WORKS NOW, but is REVERTED (see below).
    // With the signed HAP installed (§230) the old justification for this bypass is obsolete, and
    // the real path succeeds IF the ability name is mapped to one OH actually knows:
    //     ...noice.activity.AppIntroActivity -> 2097152   (OH cannot resolve ANDROID class names)
    //     EntryAbility                        -> 0        (the only ability our HAP declares)
    // i.e. `OHAbilityManagerClient::startAbilityWithCaller(want /*abilityName="EntryAbility"*/, tok)`
    // returns 0 and AMS creates a real ability record.
    // WHY REVERTED: AMS then spawns ITS OWN process for that ability, so the session/token belong to
    // that pid, not to this appspawn-x child -- the V7 window call still got ERR_INVALID_STATE -- and
    // the AMS callback into this process made startup fail with
    //     [INITCHILD-FAIL] NoClassDefFoundError: Class not found using the boot class loader
    // (regressing the clean §216/§230 state of missingview=0 / initfail=0). No window was gained.
    // Re-enable only together with the process-identity fix (§231 notes: run the Android app INSIDE
    // the process AMS starts for EntryAbility, or adopt that ability token as ohTokenAddr).
    {
        const char* dl  = getenv("ASX_DIRECT_LAUNCH");
        const char* pkg = getenv("ASX_LAUNCH_PKG");
        if (dl != nullptr && pkg != nullptr && !want.abilityName.empty() &&
            (want.bundleName == pkg || want.bundleName.empty())) {
            jclass cls = env->FindClass("adapter/activity/AppSchedulerBridge");
            jmethodID m = (cls != nullptr)
                ? env->GetStaticMethodID(cls, "nativeOnScheduleLaunchAbility",
                      "(Ljava/lang/Object;Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;"
                      "Ljava/lang/String;J)V")
                : nullptr;
            if (m != nullptr) {
                static int wl_record = 1;
                const int rec = ++wl_record;
                jstring jp = env->NewStringUTF(pkg);
                jstring ja = env->NewStringUTF(want.abilityName.c_str());
                env->CallStaticVoidMethod(cls, m, nullptr, jp, ja, (jint) rec,
                                          nullptr, nullptr, (jlong) 0);
                if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
                fprintf(stderr, "[WESTLAKE-DL2] in-process launch of %s (recordId=%d)\n",
                        want.abilityName.c_str(), rec);
                fflush(stderr);
                // 2026-07-22: the launch alone leaves the Activity CREATED BUT NOT VISIBLE —
                // AOSP adds the window (wm.addView(decor, l)) in handleResumeActivity, so
                // without a resume there is no ViewRootImpl, no traversal and no frame.
                // AppSchedulerBridge.directResume() exists for exactly this but is private and
                // hardcoded to recordId 1 (correct only for the first, direct-launched Activity).
                // Replicate it here for THIS launch's recordId — no adapter-jar/boot-image work.
                wl_dl2_resume(env, rec);
                return 0;
            }
            env->ExceptionClear();
            fprintf(stderr, "[WESTLAKE-DL2] could not resolve AppSchedulerBridge; "
                            "falling back to OH StartAbility\n");
            fflush(stderr);
        }
    }

    return OHAbilityManagerClient::getInstance().startAbilityWithCaller(
            want, callerOhTokenAddr);
}

// -------- mission stack (6) --------

jint nativeStartAbilityInMission_impl(JNIEnv* env, jclass,
                                      jstring bundleName, jstring abilityName,
                                      jstring action, jstring uri, jstring extraJson,
                                      jint missionId) {
    WantParams want;
    want.bundleName = jstr(env, bundleName);
    want.abilityName = jstr(env, abilityName);
    want.action = jstr(env, action);
    want.uri = jstr(env, uri);
    want.parametersJson = jstr(env, extraJson);

    LOGI("nativeStartAbilityInMission: bundle=%s, ability=%s, missionId=%d",
         want.bundleName.c_str(), want.abilityName.c_str(), missionId);

    return OHAbilityManagerClient::getInstance().startAbilityInMission(want, missionId);
}

jint nativeCleanMission_impl(JNIEnv*, jclass, jint missionId) {
    return OHAbilityManagerClient::getInstance().cleanMission(missionId);
}

jint nativeMoveMissionToFront_impl(JNIEnv*, jclass, jint missionId) {
    return OHAbilityManagerClient::getInstance().moveMissionToFront(missionId);
}

jboolean nativeIsTopAbility_impl(JNIEnv* env, jclass,
                                 jint missionId, jstring abilityName) {
    std::string name = jstr(env, abilityName);
    return (jboolean)OHAbilityManagerClient::getInstance().isTopAbility(missionId, name);
}

jint nativeClearAbilitiesAbove_impl(JNIEnv* env, jclass,
                                    jint missionId, jstring abilityName) {
    std::string name = jstr(env, abilityName);
    return OHAbilityManagerClient::getInstance().clearAbilitiesAbove(missionId, name);
}

jint nativeGetMissionIdForBundle_impl(JNIEnv* env, jclass, jstring bundleName) {
    std::string bundle = jstr(env, bundleName);
    return OHAbilityManagerClient::getInstance().getMissionIdForBundle(bundle);
}

const JNINativeMethod kMethods[] = {
    {"nativeGetOHAbilityManagerService", "()J",
        (void*)nativeGetOHAbilityManagerService_impl},
    {"nativeStartAbility",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"
        "Ljava/lang/String;Ljava/lang/String;J)I",
        (void*)nativeStartAbility_impl},
    {"nativeStartAbilityInMission",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"
        "Ljava/lang/String;Ljava/lang/String;I)I",
        (void*)nativeStartAbilityInMission_impl},
    {"nativeCleanMission",         "(I)I",
        (void*)nativeCleanMission_impl},
    {"nativeMoveMissionToFront",   "(I)I",
        (void*)nativeMoveMissionToFront_impl},
    {"nativeIsTopAbility",
        "(ILjava/lang/String;)Z",
        (void*)nativeIsTopAbility_impl},
    {"nativeClearAbilitiesAbove",
        "(ILjava/lang/String;)I",
        (void*)nativeClearAbilitiesAbove_impl},
    {"nativeGetMissionIdForBundle",
        "(Ljava/lang/String;)I",
        (void*)nativeGetMissionIdForBundle_impl},
};

}  // namespace

int register_ActivityTaskManagerAdapter(JNIEnv* env) {
    jclass clazz = env->FindClass("adapter/activity/ActivityTaskManagerAdapter");
    if (!clazz) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        LOGE("register_ActivityTaskManagerAdapter: FindClass returned null");
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
        LOGE("register_ActivityTaskManagerAdapter: RegisterNatives failed rc=%d", (int)rc);
    } else {
        LOGI("register_ActivityTaskManagerAdapter: OK 8 methods");
    }
    return rc;
}
