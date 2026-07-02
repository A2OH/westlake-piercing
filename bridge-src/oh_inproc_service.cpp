#include <cstring>
/*
 * oh_inproc_service.cpp
 *
 * [FIX-AUDIO 2026-06-30] In-process EXECUTION of in-app Android Services in the
 * NATIVE bridge (no boot-image regen, avoids the ohaf BCP-rebuild that regressed
 * noice). noice routes playback through SoundPlaybackService: tapping play does
 * startService(action=playSound); ExoPlayer lives inside the service. The adapter
 * can't run an in-app service via OHOS StartAbility/ConnectAbility, so we run it
 * in-process here: instantiate the Service, attach + onCreate (once), then
 * onStartCommand / onBind. The service then drives ExoPlayer -> android.media.
 * AudioTrack -> the AudioTrack->OH_AudioRenderer shim (oh_audiotrack_shim.cpp).
 *
 * nativeStartAbility/nativeConnectAbility are invoked in-process on the app's
 * calling thread (typically the main/Looper thread), so the service lifecycle is
 * run synchronously there — correct thread for Service callbacks + ExoPlayer.
 *
 * Intent is rebuilt from the Want via the existing BCP helper
 * adapter.activity.IntentWantConverter.wantToIntent (reachable via JNI).
 */
#include <jni.h>
#include <android/log.h>
#include <map>
#include <string>
#include <mutex>

#define IPLOG(...) __android_log_print(ANDROID_LOG_INFO, "OH_InProcSvc", __VA_ARGS__)
#define IPERR(...) __android_log_print(ANDROID_LOG_ERROR, "OH_InProcSvc", __VA_ARGS__)

static std::map<std::string, jobject> g_services;   // className -> global Service ref
static std::mutex g_svcMu;
static int g_startId = 0;

static jobject callStaticObj(JNIEnv* env, const char* cls, const char* m, const char* sig) {
    jclass c = env->FindClass(cls);
    if (!c) { if(env->ExceptionCheck())env->ExceptionClear(); return nullptr; }
    jmethodID id = env->GetStaticMethodID(c, m, sig);
    jobject r = id ? env->CallStaticObjectMethod(c, id) : nullptr;
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(c);
    return r;
}

extern "C" int inproc_isInApp(JNIEnv* env, const char* bundle) {
    if (!bundle || !*bundle) return 0;
    jobject app = callStaticObj(env, "android/app/ActivityThread", "currentApplication", "()Landroid/app/Application;");
    if (!app) return 0;
    jclass ctxC = env->FindClass("android/content/Context");
    jmethodID gp = env->GetMethodID(ctxC, "getPackageName", "()Ljava/lang/String;");
    jstring jpkg = (jstring)env->CallObjectMethod(app, gp);
    bool match = false;
    if (jpkg) { const char* p = env->GetStringUTFChars(jpkg, nullptr); match = (p && std::string(p) == bundle); if(p) env->ReleaseStringUTFChars(jpkg, p); }
    return match ? 1 : 0;
}

// True iff `ability` (app class) is an android.app.Service subclass. Cached.
extern "C" int inproc_isService(JNIEnv* env, const char* ability) {
    static std::map<std::string,bool> cache; static std::mutex cmu;
    if (!ability || !*ability) return 0;
    { std::lock_guard<std::mutex> lk(cmu); auto it=cache.find(ability); if(it!=cache.end()) return it->second?1:0; }
    jobject app = callStaticObj(env, "android/app/ActivityThread", "currentApplication", "()Landroid/app/Application;");
    if (!app) return 0;
    jclass ctxC = env->FindClass("android/content/Context");
    jobject cl = env->CallObjectMethod(app, env->GetMethodID(ctxC,"getClassLoader","()Ljava/lang/ClassLoader;"));
    jclass clC = env->FindClass("java/lang/ClassLoader");
    jmethodID loadClass = env->GetMethodID(clC,"loadClass","(Ljava/lang/String;)Ljava/lang/Class;");
    jstring jcls = env->NewStringUTF(ability);
    jclass tgt = (jclass)env->CallObjectMethod(cl, loadClass, jcls);
    env->DeleteLocalRef(jcls);
    bool isSvc=false;
    if (!env->ExceptionCheck() && tgt) {
        jclass svcBase = env->FindClass("android/app/Service");
        isSvc = env->IsAssignableFrom(tgt, svcBase);
    } else if (env->ExceptionCheck()) env->ExceptionClear();
    { std::lock_guard<std::mutex> lk(cmu); cache[ability]=isSvc; }
    return isSvc?1:0;
}

static jobject buildIntent(JNIEnv* env, const char* bundle, const char* ability,
                           const char* action, const char* uri, const char* params) {
    jclass conv = env->FindClass("adapter/activity/IntentWantConverter");
    if (!conv) { if(env->ExceptionCheck())env->ExceptionClear(); return nullptr; }
    jmethodID m = env->GetStaticMethodID(conv, "wantToIntent",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;");
    if (!m) { if(env->ExceptionCheck())env->ExceptionClear(); return nullptr; }
    jstring jb=env->NewStringUTF(bundle?bundle:""), ja=env->NewStringUTF(ability?ability:"");
    jstring jac=env->NewStringUTF(action?action:""), ju=env->NewStringUTF(uri?uri:""), jp=env->NewStringUTF(params?params:"");
    jobject intent = env->CallStaticObjectMethod(conv, m, jb, ja, jac, ju, jp);
    if (env->ExceptionCheck()) { env->ExceptionClear(); intent = nullptr; }
    // [FIX-AUDIO 2026-07-01] reverseMapAction() leaves custom actions with the
    // "ohos.want.action." prefix (mapAction adds it; reverse only strips the 4
    // standard ones). noice's SoundPlaybackService.onStartCommand checks the
    // ORIGINAL action ("playSound"/"playPreset"/...), so strip the prefix here.
    if (intent && action) {
        static const char* PFX = "ohos.want.action.";
        size_t pl = 17; // strlen("ohos.want.action.")
        if (strncmp(action, PFX, pl) == 0) {
            const char* orig = action + pl;
            if (strcmp(orig,"home") && strcmp(orig,"viewData") &&
                strcmp(orig,"sendData") && strcmp(orig,"select") && *orig) {
                jclass ic = env->GetObjectClass(intent);
                jmethodID sa = env->GetMethodID(ic, "setAction",
                    "(Ljava/lang/String;)Landroid/content/Intent;");
                if (sa) {
                    jstring jo = env->NewStringUTF(orig);
                    jobject r = env->CallObjectMethod(intent, sa, jo);
                    if (r) env->DeleteLocalRef(r);
                    env->DeleteLocalRef(jo);
                    IPLOG("buildIntent: action %s -> %s", action, orig);
                }
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
        }
    }
    return intent;
}

// DELEGATE to the framework's ActivityThread service lifecycle via
// IApplicationThread.scheduleCreateService (posts to the main Looper; runs onCreate
// on the main thread; tracks in mServices; completes the AMS handshake) — NOT a
// hand-rolled attach/onCreate. g_services maps className -> service token (global
// ref); presence == created.
static jobject getAppThread(JNIEnv* env) {
    jobject at = callStaticObj(env, "android/app/ActivityThread", "currentActivityThread", "()Landroid/app/ActivityThread;");
    if (!at) return nullptr;
    jclass atC = env->GetObjectClass(at);
    jmethodID m = env->GetMethodID(atC, "getApplicationThread", "()Landroid/app/ActivityThread$ApplicationThread;");
    jobject t = m ? env->CallObjectMethod(at, m) : nullptr;
    if (env->ExceptionCheck()) env->ExceptionClear();
    return t;
}

// scheduleCreateService once; returns the service token (global ref) or null
static jobject ensureCreated(JNIEnv* env, const std::string& cls, const char* bundle) {
    std::lock_guard<std::mutex> lk(g_svcMu);
    auto it = g_services.find(cls);
    if (it != g_services.end()) return it->second;

    jobject appThread = getAppThread(env);
    jobject app = callStaticObj(env, "android/app/ActivityThread", "currentApplication", "()Landroid/app/Application;");
    if (!appThread || !app) { IPERR("no appThread/app"); return nullptr; }

    // The adapter's PackageManager.getServiceInfo is unimplemented (returns null), so
    // construct a minimal ServiceInfo from the app's ApplicationInfo. handleCreateService
    // needs name (class), packageName, applicationInfo (for the LoadedApk/classloader),
    // and processName.
    jclass ctxC = env->FindClass("android/content/Context");
    jobject appInfo = env->CallObjectMethod(app, env->GetMethodID(ctxC, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;"));
    if (env->ExceptionCheck() || !appInfo) { if(env->ExceptionCheck())env->ExceptionClear(); IPERR("no ApplicationInfo"); return nullptr; }
    jclass siC = env->FindClass("android/content/pm/ServiceInfo");
    jobject svcInfo = env->NewObject(siC, env->GetMethodID(siC, "<init>", "()V"));
    env->SetObjectField(svcInfo, env->GetFieldID(siC, "name", "Ljava/lang/String;"), env->NewStringUTF(cls.c_str()));
    env->SetObjectField(svcInfo, env->GetFieldID(siC, "packageName", "Ljava/lang/String;"), env->NewStringUTF(bundle));
    env->SetObjectField(svcInfo, env->GetFieldID(siC, "applicationInfo", "Landroid/content/pm/ApplicationInfo;"), appInfo);
    jobject procName = env->GetObjectField(appInfo, env->GetFieldID(env->GetObjectClass(appInfo), "processName", "Ljava/lang/String;"));
    if (!procName) procName = env->NewStringUTF(bundle);
    env->SetObjectField(svcInfo, env->GetFieldID(siC, "processName", "Ljava/lang/String;"), procName);
    if (env->ExceptionCheck()) { env->ExceptionClear(); IPERR("ServiceInfo build threw"); return nullptr; }

    jclass ciC = env->FindClass("android/content/res/CompatibilityInfo");
    jobject compat = env->GetStaticObjectField(ciC, env->GetStaticFieldID(ciC, "DEFAULT_COMPATIBILITY_INFO", "Landroid/content/res/CompatibilityInfo;"));

    jclass binderC = env->FindClass("android/os/Binder");
    jobject token = env->NewGlobalRef(env->NewObject(binderC, env->GetMethodID(binderC, "<init>", "()V")));

    jclass appThrC = env->GetObjectClass(appThread);
    jmethodID scs = env->GetMethodID(appThrC, "scheduleCreateService",
        "(Landroid/os/IBinder;Landroid/content/pm/ServiceInfo;Landroid/content/res/CompatibilityInfo;I)V");
    env->CallVoidMethod(appThread, scs, token, svcInfo, compat, 0);
    if (env->ExceptionCheck()) { env->ExceptionClear(); IPERR("scheduleCreateService threw %s", cls.c_str()); return nullptr; }

    g_services[cls] = token;
    IPLOG("scheduleCreateService %s (delegated to ActivityThread)", cls.c_str());
    return token;
}

extern "C" int inproc_startService(JNIEnv* env, const char* bundle, const char* ability,
                                   const char* action, const char* uri, const char* params) {
    jobject token = ensureCreated(env, ability, bundle);
    if (!token) return -1;
    jobject appThread = getAppThread(env);
    if (!appThread) return -1;
    jobject intent = buildIntent(env, bundle, ability, action, uri, params);

    jclass ssaC = env->FindClass("android/app/ServiceStartArgs");
    int sid = ++g_startId;
    jobject ssa = env->NewObject(ssaC, env->GetMethodID(ssaC, "<init>", "(ZIILandroid/content/Intent;)V"),
                                 JNI_FALSE, sid, 0, intent);
    jclass alC = env->FindClass("java/util/ArrayList");
    jobject list = env->NewObject(alC, env->GetMethodID(alC, "<init>", "()V"));
    env->CallBooleanMethod(list, env->GetMethodID(alC, "add", "(Ljava/lang/Object;)Z"), ssa);
    jclass plsC = env->FindClass("android/content/pm/ParceledListSlice");
    jobject pls = env->NewObject(plsC, env->GetMethodID(plsC, "<init>", "(Ljava/util/List;)V"), list);

    jclass appThrC = env->GetObjectClass(appThread);
    jmethodID ssargs = env->GetMethodID(appThrC, "scheduleServiceArgs",
        "(Landroid/os/IBinder;Landroid/content/pm/ParceledListSlice;)V");
    env->CallVoidMethod(appThread, ssargs, token, pls);
    if (env->ExceptionCheck()) { env->ExceptionClear(); IPERR("scheduleServiceArgs threw %s", ability); return -2; }
    IPLOG("scheduleServiceArgs %s startId=%d action=%s (delegated)", ability, sid, action?action:"");
    return 0;
}

extern "C" int inproc_bindService(JNIEnv* env, const char* bundle, const char* ability, int connId) {
    jobject token = ensureCreated(env, ability, bundle);
    if (!token) return -1;
    jobject appThread = getAppThread(env);
    if (!appThread) return -1;
    jobject intent = buildIntent(env, bundle, ability, "", "", "");
    jclass appThrC = env->GetObjectClass(appThread);
    jmethodID sbs = env->GetMethodID(appThrC, "scheduleBindService",
        "(Landroid/os/IBinder;Landroid/content/Intent;ZIJ)V");
    env->CallVoidMethod(appThread, sbs, token, intent, JNI_FALSE, 0, (jlong)0);
    if (env->ExceptionCheck()) { env->ExceptionClear(); IPERR("scheduleBindService threw %s", ability); return -2; }
    IPLOG("scheduleBindService %s connId=%d (delegated)", ability, connId);
    return 0;
}

// [FIX-AUDIO 2026-07-03] SYNCHRONOUS in-process bind for LIGHT in-app services.
// noice's SoundPlaybackService.onCreate binds SubscriptionStatusPollService to
// resolve subscription state, and playback GATES on that state; the adapter
// failing in-app binds left the play blocked before it ever streams. Here we
// create the service (attach + onCreate, once, cached in g_services), call
// onBind, and deliver the IBinder to the app's ServiceConnection via
// ServiceConnectionRegistry.onServiceConnected (which posts the real callback to
// the app main Looper, so no re-entrancy). Runs on the calling thread (the app's
// main/Looper thread within bindService). Deliberately NOT used for the heavy
// SoundPlaybackService (its ExoPlayer onCreate at launch-bind regressed noice) —
// gated by the caller in activity_manager_adapter.cpp.
// Find an already-running Service instance of `className` in ActivityThread.mServices
// (e.g. one created via startService / scheduleCreateService). Returns a global ref
// or null. Lets us bind (onBind) an existing heavy service WITHOUT re-creating it.
static jobject findExistingService(JNIEnv* env, const char* className) {
    jobject thread = callStaticObj(env, "android/app/ActivityThread", "currentActivityThread", "()Landroid/app/ActivityThread;");
    if (!thread) return nullptr;
    jclass atC = env->GetObjectClass(thread);
    jfieldID msF = env->GetFieldID(atC, "mServices", "Landroid/util/ArrayMap;");
    if (!msF) { if (env->ExceptionCheck()) env->ExceptionClear(); return nullptr; }
    jobject ms = env->GetObjectField(thread, msF);
    if (!ms) return nullptr;
    jclass amC = env->GetObjectClass(ms);
    jmethodID sizeM = env->GetMethodID(amC, "size", "()I");
    jmethodID valAtM = env->GetMethodID(amC, "valueAt", "(I)Ljava/lang/Object;");
    if (!sizeM || !valAtM) { if (env->ExceptionCheck()) env->ExceptionClear(); return nullptr; }
    jclass classC = env->FindClass("java/lang/Class");
    jmethodID getNameM = env->GetMethodID(classC, "getName", "()Ljava/lang/String;");
    int n = env->CallIntMethod(ms, sizeM);
    jobject found = nullptr;
    for (int i = 0; i < n && !found; i++) {
        jobject svc = env->CallObjectMethod(ms, valAtM, i);
        if (!svc) continue;
        jclass svcC = env->GetObjectClass(svc);
        jstring nm = (jstring)env->CallObjectMethod(svcC, getNameM);
        if (nm) {
            const char* nmc = env->GetStringUTFChars(nm, nullptr);
            if (nmc && std::string(nmc) == className) found = env->NewGlobalRef(svc);
            if (nmc) env->ReleaseStringUTFChars(nm, nmc);
        }
        env->DeleteLocalRef(svc);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    return found;
}

// createIfMissing: for LIGHT bind-only services (SubscriptionStatusPollService) we
// create the service synchronously when absent. For HEAVY services already started
// (SoundPlaybackService) we pass false: only bind an EXISTING instance (from
// mServices) so the UI's controller connects, and NEVER create it at launch (which
// regressed noice). Returns 0 on delivery, -1 if unavailable/failed.
extern "C" int inproc_bindServiceSync2(JNIEnv* env, const char* bundle, const char* ability, int connId, int createIfMissing) {
    if (!ability || !*ability) return -1;
    std::string cls(ability);
    jobject app = callStaticObj(env, "android/app/ActivityThread", "currentApplication", "()Landroid/app/Application;");
    jobject thread = callStaticObj(env, "android/app/ActivityThread", "currentActivityThread", "()Landroid/app/ActivityThread;");
    if (!app || !thread) { IPERR("bindSync: no app/thread"); return -1; }

    jobject svc = nullptr;
    { std::lock_guard<std::mutex> lk(g_svcMu); auto it = g_services.find(cls); if (it != g_services.end()) svc = it->second; }
    if (!svc) {
        // reuse an already-running instance (e.g. startService-created SoundPlaybackService)
        svc = findExistingService(env, ability);
        if (svc) { std::lock_guard<std::mutex> lk(g_svcMu); g_services[cls] = svc; IPLOG("bindSync: reusing running service %s", ability); }
    }
    if (!svc && !createIfMissing) { IPLOG("bindSync: %s not yet created, defer (no create)", ability); return -1; }
    if (!svc) {
        jclass appC = env->GetObjectClass(app);
        jobject clo = env->CallObjectMethod(app, env->GetMethodID(appC, "getClassLoader", "()Ljava/lang/ClassLoader;"));
        jclass clC = env->FindClass("java/lang/ClassLoader");
        jstring jcls = env->NewStringUTF(ability);
        jclass svcClass = (jclass)env->CallObjectMethod(clo, env->GetMethodID(clC, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;"), jcls);
        if (env->ExceptionCheck() || !svcClass) { env->ExceptionClear(); IPERR("bindSync: loadClass %s failed", ability); return -1; }
        jmethodID ctor = env->GetMethodID(svcClass, "<init>", "()V");
        jobject inst = ctor ? env->NewObject(svcClass, ctor) : nullptr;
        if (env->ExceptionCheck() || !inst) { env->ExceptionClear(); IPERR("bindSync: newInstance %s failed", ability); return -1; }
        jclass binderC = env->FindClass("android/os/Binder");
        jobject token = env->NewObject(binderC, env->GetMethodID(binderC, "<init>", "()V"));
        jmethodID attach = env->GetMethodID(svcClass, "attach",
            "(Landroid/content/Context;Landroid/app/ActivityThread;Ljava/lang/String;Landroid/os/IBinder;Landroid/app/Application;Ljava/lang/Object;)V");
        if (!attach) { if(env->ExceptionCheck())env->ExceptionClear(); IPERR("bindSync: no attach method"); return -1; }
        env->CallVoidMethod(inst, attach, app, thread, jcls, token, app, (jobject)nullptr);
        if (env->ExceptionCheck()) { env->ExceptionClear(); IPERR("bindSync: attach %s failed", ability); return -1; }
        env->CallVoidMethod(inst, env->GetMethodID(svcClass, "onCreate", "()V"));
        if (env->ExceptionCheck()) { env->ExceptionClear(); IPERR("bindSync: onCreate %s threw (continuing)", ability); }
        svc = env->NewGlobalRef(inst);
        { std::lock_guard<std::mutex> lk(g_svcMu); g_services[cls] = svc; }
        IPLOG("bindSync: created in-process service %s", ability);
    }

    jobject intent = buildIntent(env, bundle, ability, "", "", "");
    jclass svcC = env->GetObjectClass(svc);
    jobject binder = env->CallObjectMethod(svc, env->GetMethodID(svcC, "onBind", "(Landroid/content/Intent;)Landroid/os/IBinder;"), intent);
    if (env->ExceptionCheck()) { env->ExceptionClear(); IPERR("bindSync: onBind %s threw", ability); binder = nullptr; }

    jclass regC = env->FindClass("adapter/activity/ServiceConnectionRegistry");
    if (!regC) { if(env->ExceptionCheck())env->ExceptionClear(); IPERR("bindSync: no ServiceConnectionRegistry"); return -1; }
    jobject reg = env->CallStaticObjectMethod(regC, env->GetStaticMethodID(regC, "getInstance", "()Ladapter/activity/ServiceConnectionRegistry;"));
    jstring jb = env->NewStringUTF(bundle ? bundle : "");
    jstring ja = env->NewStringUTF(ability);
    env->CallVoidMethod(reg, env->GetMethodID(regC, "onServiceConnected", "(ILjava/lang/String;Ljava/lang/String;Landroid/os/IBinder;)V"),
                        (jint)connId, jb, ja, binder);
    if (env->ExceptionCheck()) { env->ExceptionClear(); IPERR("bindSync: onServiceConnected %s threw", ability); return -1; }
    IPLOG("bindSync: delivered onServiceConnected %s connId=%d binder=%p", ability, connId, (void*)binder);
    return 0;
}
