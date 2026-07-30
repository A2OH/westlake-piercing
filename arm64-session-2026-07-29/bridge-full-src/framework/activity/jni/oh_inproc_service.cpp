#include <cstring>
/*
 * oh_inproc_service.cpp
 *
 * [FIX-AUDIO 2026-06-30 / SYSTEMATIC BIND 2026-07-03] In-process execution of
 * in-app Android Services in the NATIVE bridge (no boot-image regen; the ohaf
 * BCP jar is not rebuildable without regressing fragile apps, so all of this
 * stays at the JNI ABI boundary). In-app Android Services are not OHOS
 * abilities: OHOS StartAbility/ConnectAbility cannot run them, so the bridge
 * runs them in-process.
 *
 * startService: DELEGATE to the framework's own ActivityThread service
 * lifecycle — IApplicationThread.scheduleCreateService + scheduleServiceArgs
 * (main-looper handleCreateService/handleServiceArgs, onCreate/onStartCommand
 * on the main thread, instance tracked in ActivityThread.mServices).
 *
 * bindService (generic policy, NO per-app class names):
 *  - instance already running (found in ActivityThread.mServices) -> call
 *    onBind (once per service; the binder is cached and re-delivered on later
 *    binds, matching AOSP rebind semantics) and deliver it through
 *    adapter.activity.ServiceConnectionRegistry.onServiceConnected;
 *  - not created yet -> schedule the framework create (the same delegated
 *    scheduleCreateService path startService uses) and return success; a small
 *    delivery worker waits for the instance to appear in mServices (== its
 *    onCreate finished on the main looper) and then delivers
 *    onServiceConnected. Android's bind contract is asynchronous, so the late
 *    delivery is spec-conformant — this replaces both the per-app
 *    "SoundPlaybackService reuse-only" heuristic and the synchronous
 *    hand-rolled attach/onCreate that used to run heavy service init inside a
 *    launch-time bindService call.
 *
 * Why a bridge-side worker delivers the binder instead of the framework's own
 * scheduleBindService: handleBindService would run onBind on the main thread
 * and hand the result to IActivityManager.publishService — which in the
 * deployed oh-adapter-framework.jar is a logging NO-OP, so the binder is
 * dropped. Rebuilding that BCP jar is the known app-regressing trap. When ohaf
 * becomes rebuildable, the principled completion is scheduleBindService +
 * publishService -> ServiceConnectionRegistry routing, and the worker (and the
 * off-main onBind it implies) goes away.
 *
 * The IServiceConnection reached through ServiceConnectionRegistry posts the
 * app's onServiceConnected to its main looper (LoadedApk.ServiceDispatcher),
 * so delivery is thread-safe from any thread.
 *
 * Intent is rebuilt from the Want via the existing BCP helper
 * adapter.activity.IntentWantConverter.wantToIntent (reachable via JNI).
 */
#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <mutex>

#define IPLOG(...) __android_log_print(ANDROID_LOG_INFO, "OH_InProcSvc", __VA_ARGS__)
#define IPERR(...) __android_log_print(ANDROID_LOG_ERROR, "OH_InProcSvc", __VA_ARGS__)

// Per-service-class record (one Service instance per class per process, as in
// AOSP). All jobjects are JNI global refs, process-lifetime.
//  token    — IBinder key used with scheduleCreateService/scheduleServiceArgs;
//             ours when we scheduled the create, or recovered from the
//             ActivityThread.mServices KEY for instances created elsewhere
//             (never confuse the two — a token is a plain Binder, calling
//             Service methods on it is JNI UB).
//  instance — the live android.app.Service object out of mServices.
//  binder   — cached onBind() result; bindCalled distinguishes "onBind returned
//             null" (legit; delivered as null -> onNullBinding) from "not yet
//             bound". Granularity is PER CLASS, not per bind-Intent: the native
//             bind entry receives no Intent (bundle/class/connId only — the ohaf
//             caller can't be changed), so every in-app bind uses the same empty
//             intent and one cached binder. Services that expose different
//             binders per bind action are unsupported at this boundary (codex
//             review 2026-07-03) — revisit if the jar ever becomes rebuildable.
struct SvcRec {
    jobject token = nullptr;
    jobject instance = nullptr;
    jobject binder = nullptr;
    bool bindCalled = false;
};

struct PendingBind {
    std::string cls;
    std::string bundle;
    int connId;
    int64_t deadlineMs;   // CLOCK_MONOTONIC
};

static std::map<std::string, SvcRec> g_svc;   // className -> record
static std::vector<PendingBind> g_pending;    // binds awaiting service creation
static std::set<int> g_inappConnIds;          // connIds owned by the in-proc path
static bool g_workerRunning = false;
// Recursive: a Service's onCreate/onBind may call back into
// bindService/startService on the same thread (re-enters these natives).
static std::recursive_mutex g_mu;
// Serializes onBind invocations ONLY (once-per-service semantics). Never held
// together with app-visible work other than onBind itself, and g_mu is NOT held
// while app code runs — bindService/startService/disconnect on other threads
// stay lock-free of app code (codex review 2026-07-03: don't hold the state
// lock across arbitrary app callbacks).
static std::recursive_mutex g_bindOpMu;
static JavaVM* g_vm = nullptr;
static int g_startId = 0;

static int64_t nowMs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

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
    // standard ones). Service onStartCommand implementations check the ORIGINAL
    // action, so strip the prefix here.
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

static jobject getAppThread(JNIEnv* env) {
    jobject at = callStaticObj(env, "android/app/ActivityThread", "currentActivityThread", "()Landroid/app/ActivityThread;");
    if (!at) return nullptr;
    jclass atC = env->GetObjectClass(at);
    jmethodID m = env->GetMethodID(atC, "getApplicationThread", "()Landroid/app/ActivityThread$ApplicationThread;");
    jobject t = m ? env->CallObjectMethod(at, m) : nullptr;
    if (env->ExceptionCheck()) env->ExceptionClear();
    return t;
}

// Scan ActivityThread.mServices (ArrayMap<IBinder token, Service instance>) for
// a running instance of `className`. On match, adopt BOTH the instance and its
// real framework token into g_svc[className]. Covers services created by any
// path (our scheduleCreateService, OHOS-driven service abilities, ...) and
// keeps scheduleServiceArgs/rebinds working against the framework's own token.
static bool adoptExistingService(JNIEnv* env, const char* className) {
    jobject thread = callStaticObj(env, "android/app/ActivityThread", "currentActivityThread", "()Landroid/app/ActivityThread;");
    if (!thread) return false;
    jclass atC = env->GetObjectClass(thread);
    jfieldID msF = env->GetFieldID(atC, "mServices", "Landroid/util/ArrayMap;");
    if (!msF) { if (env->ExceptionCheck()) env->ExceptionClear(); return false; }
    jobject ms = env->GetObjectField(thread, msF);
    if (!ms) return false;
    jclass amC = env->GetObjectClass(ms);
    jmethodID sizeM = env->GetMethodID(amC, "size", "()I");
    jmethodID keyAtM = env->GetMethodID(amC, "keyAt", "(I)Ljava/lang/Object;");
    jmethodID valAtM = env->GetMethodID(amC, "valueAt", "(I)Ljava/lang/Object;");
    if (!sizeM || !keyAtM || !valAtM) { if (env->ExceptionCheck()) env->ExceptionClear(); return false; }
    jclass classC = env->FindClass("java/lang/Class");
    jmethodID getNameM = env->GetMethodID(classC, "getName", "()Ljava/lang/String;");
    int n = env->CallIntMethod(ms, sizeM);
    bool adopted = false;
    for (int i = 0; i < n && !adopted; i++) {
        jobject svc = env->CallObjectMethod(ms, valAtM, i);
        if (!svc) continue;
        jclass svcC = env->GetObjectClass(svc);
        jstring nm = (jstring)env->CallObjectMethod(svcC, getNameM);
        if (nm) {
            const char* nmc = env->GetStringUTFChars(nm, nullptr);
            if (nmc && std::string(nmc) == className) {
                jobject tok = env->CallObjectMethod(ms, keyAtM, i);
                std::lock_guard<std::recursive_mutex> lk(g_mu);
                SvcRec& rec = g_svc[className];
                if (!rec.instance) rec.instance = env->NewGlobalRef(svc);
                if (!rec.token && tok) rec.token = env->NewGlobalRef(tok);
                adopted = true;
                IPLOG("adopted running service %s (token=%p)", className, (void*)rec.token);
                if (tok) env->DeleteLocalRef(tok);
            }
            if (nmc) env->ReleaseStringUTFChars(nm, nmc);
            env->DeleteLocalRef(nm);
        }
        env->DeleteLocalRef(svcC);
        env->DeleteLocalRef(svc);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    return adopted;
}

// Ensure a framework create has been scheduled for `cls`; returns the service
// token (borrowed global ref) or null. DELEGATES to ActivityThread —
// scheduleCreateService posts CREATE_SERVICE to the main looper, which runs
// handleCreateService (instantiate + attach + onCreate + mServices bookkeeping).
// Never a hand-rolled attach/onCreate.
static jobject ensureCreated(JNIEnv* env, const std::string& cls, const char* bundle) {
    std::lock_guard<std::recursive_mutex> lk(g_mu);
    {
        auto it = g_svc.find(cls);
        if (it != g_svc.end() && it->second.token) return it->second.token;
    }
    // Running already (created by another path)? Adopt its framework token.
    if (adoptExistingService(env, cls.c_str())) {
        SvcRec& rec = g_svc[cls];
        if (rec.token) return rec.token;
        IPERR("ensureCreated: %s running but token unrecoverable", cls.c_str());
        return nullptr;   // never double-create over a live instance
    }

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
    if (env->ExceptionCheck()) { env->ExceptionClear(); IPERR("scheduleCreateService threw %s", cls.c_str()); env->DeleteGlobalRef(token); return nullptr; }

    g_svc[cls].token = token;
    IPLOG("scheduleCreateService %s (delegated to ActivityThread)", cls.c_str());
    return token;
}

extern "C" int inproc_startService(JNIEnv* env, const char* bundle, const char* ability,
                                   const char* action, const char* uri, const char* params) {
    if (!g_vm) env->GetJavaVM(&g_vm);
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

// Deliver onServiceConnected(connId) for a RUNNING service instance. onBind is
// called exactly once per service and its binder cached; later binds re-deliver
// the cached binder (AOSP parity: repeat binds do not re-invoke onBind). The
// IServiceConnection behind ServiceConnectionRegistry posts the app callback to
// its main looper, so calling this from any thread is safe; onBind itself runs
// on the current thread (see the file header for why the framework's
// main-thread bind path can't be used yet).
static int deliverConnected(JNIEnv* env, const char* bundle, const char* cls, int connId) {
    // Snapshot the record under g_mu; never hold g_mu while app code runs.
    jobject instance = nullptr;
    jobject binder = nullptr;
    bool needBind = false;
    {
        std::lock_guard<std::recursive_mutex> lk(g_mu);
        auto it = g_svc.find(cls);
        if (it == g_svc.end() || !it->second.instance) return -1;
        instance = it->second.instance;               // global ref, stable
        needBind = !it->second.bindCalled;
        binder = it->second.binder;
    }
    if (needBind) {
        // g_bindOpMu serializes concurrent first-binds; re-check under g_mu so
        // exactly one thread runs onBind (the loser reuses the cached result).
        std::lock_guard<std::recursive_mutex> blk(g_bindOpMu);
        bool doBind = false;
        {
            std::lock_guard<std::recursive_mutex> lk(g_mu);
            auto it = g_svc.find(cls);
            if (it == g_svc.end() || !it->second.instance) return -1;
            doBind = !it->second.bindCalled;
        }
        if (doBind) {
            jobject intent = buildIntent(env, bundle, cls, "", "", "");
            jclass svcC = env->GetObjectClass(instance);
            jmethodID ob = env->GetMethodID(svcC, "onBind", "(Landroid/content/Intent;)Landroid/os/IBinder;");
            jobject b = ob ? env->CallObjectMethod(instance, ob, intent) : nullptr;
            if (env->ExceptionCheck()) { env->ExceptionClear(); IPERR("onBind %s threw", cls); b = nullptr; }
            jobject g = b ? env->NewGlobalRef(b) : nullptr;
            {
                std::lock_guard<std::recursive_mutex> lk(g_mu);
                SvcRec& rec = g_svc[cls];
                rec.bindCalled = true;
                rec.binder = g;
            }
            if (b) env->DeleteLocalRef(b);
            if (intent) env->DeleteLocalRef(intent);
            env->DeleteLocalRef(svcC);
        }
        std::lock_guard<std::recursive_mutex> lk(g_mu);
        binder = g_svc[cls].binder;
    }
    jclass regC = env->FindClass("adapter/activity/ServiceConnectionRegistry");
    if (!regC) { if(env->ExceptionCheck())env->ExceptionClear(); IPERR("no ServiceConnectionRegistry"); return -1; }
    jobject reg = env->CallStaticObjectMethod(regC, env->GetStaticMethodID(regC, "getInstance", "()Ladapter/activity/ServiceConnectionRegistry;"));
    jstring jb = env->NewStringUTF(bundle ? bundle : "");
    jstring ja = env->NewStringUTF(cls);
    env->CallVoidMethod(reg, env->GetMethodID(regC, "onServiceConnected", "(ILjava/lang/String;Ljava/lang/String;Landroid/os/IBinder;)V"),
                        (jint)connId, jb, ja, binder);
    if (env->ExceptionCheck()) { env->ExceptionClear(); IPERR("onServiceConnected %s threw", cls); return -1; }
    IPLOG("delivered onServiceConnected %s connId=%d binder=%p", cls, connId, (void*)binder);
    return 0;
}

// Delivery worker: waits for scheduled creates to land in mServices (onCreate
// finished on the main looper), then delivers the queued onServiceConnected
// callbacks. Exits when the queue drains; restarted on demand.
static void* pendingWorker(void*) {
    JNIEnv* env = nullptr;
    if (!g_vm || g_vm->AttachCurrentThread(&env, nullptr) != 0 || !env) {
        std::lock_guard<std::recursive_mutex> lk(g_mu);
        g_workerRunning = false;
        return nullptr;
    }
    IPLOG("bind-delivery worker up");
    for (;;) {
        std::vector<PendingBind> snapshot;
        {
            std::lock_guard<std::recursive_mutex> lk(g_mu);
            if (g_pending.empty()) { g_workerRunning = false; break; }
            snapshot = g_pending;
        }
        for (auto& pb : snapshot) {
            env->PushLocalFrame(64);
            bool have;
            {
                std::lock_guard<std::recursive_mutex> lk(g_mu);
                auto it = g_svc.find(pb.cls);
                have = it != g_svc.end() && it->second.instance;
            }
            if (!have) have = adoptExistingService(env, pb.cls.c_str());
            if (have || nowMs() > pb.deadlineMs) {
                // CLAIM the entry (remove under lock) before delivering: if an
                // unbind raced us and already removed it, skip delivery — never
                // deliver onServiceConnected after unbindService (codex 2026-07-03).
                bool claimed = false;
                {
                    std::lock_guard<std::recursive_mutex> lk(g_mu);
                    for (auto it = g_pending.begin(); it != g_pending.end(); ++it) {
                        if (it->connId == pb.connId && it->cls == pb.cls) {
                            g_pending.erase(it);
                            claimed = true;
                            break;
                        }
                    }
                }
                if (claimed && have) {
                    deliverConnected(env, pb.bundle.c_str(), pb.cls.c_str(), pb.connId);
                } else if (claimed) {
                    IPERR("bind wait timed out for %s connId=%d (service never appeared)", pb.cls.c_str(), pb.connId);
                }
            }
            env->PopLocalFrame(nullptr);
        }
        usleep(100 * 1000);
    }
    g_vm->DetachCurrentThread();   // mandatory: ART aborts on undetached exit
    IPLOG("bind-delivery worker idle, exiting");
    return nullptr;
}

// Generic in-app bind entry — replaces both the per-app createIfMissing
// heuristic and the synchronous hand-rolled create (inproc_bindServiceSync2).
// Returns 0 when the bind is accepted (delivery immediate or deferred),
// -1 on hard failure (caller falls back / fails the bind).
extern "C" int inproc_bindService2(JNIEnv* env, const char* bundle, const char* ability, int connId) {
    if (!ability || !*ability) return -1;
    if (!g_vm) env->GetJavaVM(&g_vm);
    std::string cls(ability);

    { std::lock_guard<std::recursive_mutex> lk(g_mu); g_inappConnIds.insert(connId); }

    bool have;
    {
        std::lock_guard<std::recursive_mutex> lk(g_mu);
        auto it = g_svc.find(cls);
        have = it != g_svc.end() && it->second.instance;
    }
    if (!have) have = adoptExistingService(env, ability);
    if (have) return deliverConnected(env, bundle, ability, connId);   // running: deliver now

    // Not created yet: schedule the framework create, deliver when it lands.
    if (!ensureCreated(env, cls, bundle)) { IPERR("bind: create-schedule failed for %s", ability); return -1; }
    {
        std::lock_guard<std::recursive_mutex> lk(g_mu);
        bool dup = false;
        for (auto& pb : g_pending) if (pb.connId == connId && pb.cls == cls) dup = true;
        if (!dup) g_pending.push_back({cls, bundle ? bundle : "", connId, nowMs() + 20000});
        if (!g_workerRunning) {
            pthread_t t;
            pthread_attr_t a;
            pthread_attr_init(&a);
            pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
            if (pthread_create(&t, &a, pendingWorker, nullptr) == 0) {
                g_workerRunning = true;
            } else {
                // No worker => this bind would strand forever while reporting
                // success. Undo the queue entry and fail the bind honestly.
                for (auto it = g_pending.begin(); it != g_pending.end(); ++it) {
                    if (it->connId == connId && it->cls == cls) { g_pending.erase(it); break; }
                }
                pthread_attr_destroy(&a);
                IPERR("bind: worker spawn failed for %s connId=%d", ability, connId);
                return -1;
            }
            pthread_attr_destroy(&a);
        }
    }
    IPLOG("bind: %s connId=%d queued, async create scheduled", ability, connId);
    return 0;
}

// In-app unbind routing: connIds owned by the in-proc path must not be sent to
// OHOS DisconnectAbility (it never saw them). Returns 0 when handled here, -1
// to let the caller route to OHOS. onUnbind/onDestroy delivery is a known
// remaining gap (in-proc services are process-lifetime for now).
extern "C" int inproc_disconnect(JNIEnv*, int connId) {
    std::lock_guard<std::recursive_mutex> lk(g_mu);
    for (auto it = g_pending.begin(); it != g_pending.end(); ++it) {
        if (it->connId == connId) { g_pending.erase(it); break; }
    }
    if (g_inappConnIds.erase(connId)) {
        IPLOG("disconnect connId=%d (in-proc; not routed to OHOS)", connId);
        return 0;
    }
    return -1;
}
