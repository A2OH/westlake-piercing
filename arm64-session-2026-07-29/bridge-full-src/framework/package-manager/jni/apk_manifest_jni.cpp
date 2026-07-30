/*
 * apk_manifest_jni.cpp
 *
 * JNI bridge: on-demand parse of an installed APK's AndroidManifest.xml to
 * deliver Android-specific fields (className/theme/largeHeap/appComponentFactory/
 * classLoaderName/networkSecurityConfigRes/processName + provider list) that
 * OH BMS does not store. Result returned as JSON for Java-side consumption.
 *
 * Authoritative spec: doc/ability_manager_ipc_adapter_design.html §1.1.4.6 / §1.1.5.2
 *
 * Why on-demand (vs. write-time persistence to OH BMS metadata):
 *  - Avoids modifying OH libbms install path
 *  - APK already lives at /system/app/<pkg>/<pkg>.apk after install
 *  - Java side caches result per packageName so the parse cost is one-shot
 *
 * Also exposes a SystemProperties bridge so Java SystemProperties.get fall back
 * to OH OHOS::system::GetParameter for properties not in the AOSP property store.
 */

#include "apk_manifest_parser.h"

#include <cstring>
#include <jni.h>
#include <nlohmann/json.hpp>
#include <string>
#include <sys/stat.h>
#include <cstdlib>

#include "hilog/log.h"
#include "parameters.h"  // OHOS::system::GetParameter

#undef LOG_DOMAIN
#undef LOG_TAG
namespace {
constexpr unsigned int LOG_DOMAIN = 0xD001151;
constexpr const char* LOG_TAG = "OH_ApkManifestJni";

#define LOGI(...) OHOS::HiviewDFX::HiLog::Info({LOG_CORE, LOG_DOMAIN, LOG_TAG}, __VA_ARGS__)
#define LOGW(...) OHOS::HiviewDFX::HiLog::Warn({LOG_CORE, LOG_DOMAIN, LOG_TAG}, __VA_ARGS__)
#define LOGE(...) OHOS::HiviewDFX::HiLog::Error({LOG_CORE, LOG_DOMAIN, LOG_TAG}, __VA_ARGS__)

// Resolve packageName → on-disk APK path.  The adapter installer lays out APKs
// at /system/app/<pkg>/<pkg>.apk (ref: apk_installation_design + memory
// reference_oh_app_sandbox_paths.md — only /system/app/<pkg>/ is visible from
// the app sandbox, so this is the canonical install location).
std::string ResolveApkPath(const std::string& packageName) {
    struct stat st;
    // 0. WESTLAKE (arm64 board, 2026-07-21): the DIRECT-LAUNCH flow never installs the APK
    // into any of the canonical locations below — child_main.cpp stages it at
    // /data/local/tmp/asx/noice.apk and exports ASX_APK_PATH.  Without this, ResolveApkPath
    // returns empty, nativeParseApkManifestJson returns "", the Java logs
    // "[FIX-AII] no manifest JSON for <pkg>", and LoadedApk.makeApplication then dies with
    //   IllegalStateException: Unable to get package info for <pkg>; is package not installed?
    // Honour the env var first, then the staging dir.
    if (const char* envApk = getenv("ASX_APK_PATH")) {
        if (envApk[0] != '\0' && stat(envApk, &st) == 0 && S_ISREG(st.st_mode)) {
            return std::string(envApk);
        }
    }
    std::string p0 = "/data/local/tmp/asx/" + packageName + ".apk";
    if (stat(p0.c_str(), &st) == 0 && S_ISREG(st.st_mode)) return p0;
    // 1. Adapter installer canonical path (visible from app sandbox).
    std::string p1 = "/system/app/" + packageName + "/" + packageName + ".apk";
    if (stat(p1.c_str(), &st) == 0 && S_ISREG(st.st_mode)) return p1;
    // 2. OH BMS canonical path for adapter-installed APKs (per V2 install entry).
    std::string p2 = "/data/app/el1/bundle/public/" + packageName + "/android/base.apk";
    if (stat(p2.c_str(), &st) == 0 && S_ISREG(st.st_mode)) return p2;
    // 3. Legacy path used by very old installer versions.
    std::string p3 = "/data/app/android/" + packageName + "/base.apk";
    if (stat(p3.c_str(), &st) == 0 && S_ISREG(st.st_mode)) return p3;
    LOGW("ResolveApkPath: APK not found for %{public}s (tried %{public}s, %{public}s, %{public}s)",
        packageName.c_str(), p1.c_str(), p2.c_str(), p3.c_str());
    return {};
}

nlohmann::json ProviderToJson(const oh_adapter::ApkManifestParser::ProviderData& p) {
    nlohmann::json j;
    j["name"] = p.name;
    j["authorities"] = p.authorities;
    j["exported"] = p.exported;
    j["readPermission"] = p.readPermission;
    j["writePermission"] = p.writePermission;
    j["grantUriPermissions"] = p.grantUriPermissions;
    j["multiprocess"] = p.multiprocess;
    j["initOrder"] = p.initOrder;
    j["processName"] = p.processName;
    return j;
}

}  // namespace

extern "C" {

// Returns the parsed AndroidManifest as JSON, fields documented in
// ability_manager_ipc_adapter_design §1.1.4.  Empty string on failure.
JNIEXPORT jstring JNICALL
Java_adapter_packagemanager_PackageManagerAdapter_nativeParseApkManifestJson(
        JNIEnv* env, jclass /*clazz*/, jstring jPackageName) {
    if (jPackageName == nullptr) {
        return env->NewStringUTF("");
    }
    const char* pkgChars = env->GetStringUTFChars(jPackageName, nullptr);
    std::string pkg(pkgChars ? pkgChars : "");
    env->ReleaseStringUTFChars(jPackageName, pkgChars);

    if (pkg.empty()) {
        return env->NewStringUTF("");
    }

    std::string apkPath = ResolveApkPath(pkg);
    if (apkPath.empty()) {
        return env->NewStringUTF("");
    }

    oh_adapter::ApkManifestParser::ManifestData m;
    if (!oh_adapter::ApkManifestParser::Parse(apkPath, m)) {
        LOGE("nativeParseApkManifestJson: ApkManifestParser::Parse failed for %{public}s",
            apkPath.c_str());
        return env->NewStringUTF("");
    }

    nlohmann::json j;
    // WESTLAKE (2026-07-22) ROOT CAUSE FIX: PackageInfoBuilder.fromBundleInfoJson starts with
    //   pi.packageName = json.getString("name");
    // `getString` (NOT optString) THROWS JSONException when the key is absent, and
    // fromBundleInfo() catches it and returns null. We only emitted "packageName", so EVERY
    // fromBundleInfo() call returned null -> PackageInfo.applicationInfo null ->
    // getApplicationInfo() null -> handleBindApplication NPE on ApplicationInfo.metaData ->
    // NetworkSecurityConfigProvider.install() never ran -> no JSSE provider -> SunX509 ->
    // MainActivity never started -> NO FRAME.  The builder uses the OHOS BundleInfo key "name".
    j["name"] = m.packageName;
    j["packageName"] = m.packageName;
    j["versionCode"] = m.versionCode;
    j["versionName"] = m.versionName;
    j["minSdkVersion"] = m.minSdkVersion;
    j["targetSdkVersion"] = m.targetSdkVersion;
    j["sharedUserId"] = m.sharedUserId;

    j["appClassName"] = m.appClassName;
    j["appLabel"] = m.appLabel;
    j["appTheme"] = m.appTheme;
    j["appProcessName"] = m.appProcessName;
    j["appComponentFactory"] = m.appComponentFactory;
    j["classLoaderName"] = m.classLoaderName;
    j["networkSecurityConfigRes"] = m.networkSecurityConfigRes;
    j["debuggable"] = m.debuggable;
    j["largeHeap"] = m.largeHeap;
    j["allowBackup"] = m.allowBackup;
    j["persistent"] = m.persistent;
    j["hardwareAccelerated"] = m.hardwareAccelerated;
    j["extractNativeLibs"] = m.extractNativeLibs;

    nlohmann::json libs = nlohmann::json::array();
    for (const auto& lib : m.sharedLibraryFiles) libs.push_back(lib);
    j["sharedLibraryFiles"] = libs;

    // [META-DATA 2026-06-22] application-level <meta-data> name->value, so the
    // Java side can populate ApplicationInfo.metaData (non-null Bundle).
    nlohmann::json meta = nlohmann::json::object();
    for (const auto& kv : m.appMetaData) meta[kv.first] = kv.second;
    j["appMetaData"] = meta;

    nlohmann::json provs = nlohmann::json::array();
    for (const auto& p : m.providers) provs.push_back(ProviderToJson(p));
    j["providers"] = provs;

    nlohmann::json perms = nlohmann::json::array();
    for (const auto& p : m.usesPermissions) perms.push_back(p);
    j["usesPermissions"] = perms;

    // ===================================================================================
    // WESTLAKE (2026-07-22) -- NESTED "applicationInfo" NODE.  ROOT CAUSE FIX.
    //
    // `adapter.packagemanager.PackageInfoBuilder.buildApplicationInfo(JSONObject, String)`
    // fetches a NESTED "applicationInfo" object and reads OHOS-BundleInfo-style keys from it
    // (process, dataDir, codePath, cpuAbi, nativeLibraryPath, uid, versionCode, maxSdkVersion,
    // apiTargetVersion, apiCompatibleVersion).  We only ever emitted FLAT AOSP-manifest keys, so
    // that lookup found nothing and buildApplicationInfo returned null.  Consequence chain:
    //   PackageInfo.applicationInfo == null -> PackageManagerAdapter.getApplicationInfo() returns
    //   null -> ActivityThread.handleBindApplication NPEs reading ApplicationInfo.metaData ->
    //   NetworkSecurityConfigProvider.install() never runs -> no JSSE provider ->
    //   NoSuchAlgorithmException: SunX509 -> MainActivity never starts -> NO FRAME.
    // The flat keys are kept (other call sites read them); extra members are harmless.
    // ===================================================================================
    {
        nlohmann::json ai = nlohmann::json::object();
        const std::string dataDir = "/data/data/" + m.packageName;
        ai["name"] = m.appClassName;
        ai["packageName"] = m.packageName;
        ai["process"] = m.appProcessName.empty() ? m.packageName : m.appProcessName;
        ai["dataDir"] = dataDir;
        ai["codePath"] = apkPath;
        ai["sourceDir"] = apkPath;
        ai["publicSourceDir"] = apkPath;
        ai["cpuAbi"] = "arm64-v8a";
        ai["nativeLibraryPath"] = dataDir + "/lib";
        ai["nativeLibraryDir"] = dataDir + "/lib";
        ai["uid"] = 10042;
        ai["versionCode"] = m.versionCode;
        ai["maxSdkVersion"] = m.targetSdkVersion;
        ai["apiTargetVersion"] = m.targetSdkVersion;
        ai["apiCompatibleVersion"] = m.minSdkVersion;
        ai["minSdkVersion"] = m.minSdkVersion;
        ai["targetSdkVersion"] = m.targetSdkVersion;
        ai["className"] = m.appClassName;
        ai["appClassName"] = m.appClassName;
        ai["label"] = m.appLabel;
        ai["theme"] = m.appTheme;
        ai["debuggable"] = m.debuggable;
        ai["largeHeap"] = m.largeHeap;
        ai["persistent"] = m.persistent;
        ai["hardwareAccelerated"] = m.hardwareAccelerated;
        ai["extractNativeLibs"] = m.extractNativeLibs;
        ai["allowBackup"] = m.allowBackup;
        ai["classLoaderName"] = m.classLoaderName;
        ai["appComponentFactory"] = m.appComponentFactory;
        ai["networkSecurityConfigRes"] = m.networkSecurityConfigRes;
        ai["metaData"] = meta;      // so ApplicationInfo.metaData is a non-null Bundle
        ai["appMetaData"] = meta;
        j["applicationInfo"] = ai;
    }

    LOGI("nativeParseApkManifestJson: pkg=%{public}s providers=%{public}zu className=%{public}s "
         "theme=0x%{public}x largeHeap=%{public}d",
         m.packageName.c_str(), m.providers.size(),
         m.appClassName.c_str(), m.appTheme, m.largeHeap);
    return env->NewStringUTF(j.dump().c_str());
}

// 2026-04-30 (P2-B v2): JNI alias for AppSchedulerBridge.  Same impl as
// PackageManagerAdapter.nativeParseApkManifestJson but registered under
// adapter/activity/AppSchedulerBridge.  This lets the PathClassLoader-loaded
// AppSchedulerBridge call manifest parse without depending on the BCP class
// PackageManagerAdapter (which would require a boot image rebuild to add new
// methods — see memory feedback_boot_image_full_rebuild_risk.md).
JNIEXPORT jstring JNICALL
Java_adapter_activity_AppSchedulerBridge_nativeParseManifestJson(
        JNIEnv* env, jclass clazz, jstring jPackageName) {
    return Java_adapter_packagemanager_PackageManagerAdapter_nativeParseApkManifestJson(
            env, clazz, jPackageName);
}

// OH SystemProperties bridge.  Returns the value of the given key from OH
// system parameter store (OHOS::system::GetParameter), or defValue if the key
// is not set.  Used by adapter Java for ro.serialno and similar OH-stored
// system properties that AOSP SystemProperties cannot see.
JNIEXPORT jstring JNICALL
Java_adapter_packagemanager_PackageManagerAdapter_nativeGetOhSystemProperty(
        JNIEnv* env, jclass /*clazz*/, jstring jKey, jstring jDefValue) {
    if (jKey == nullptr) {
        return env->NewStringUTF(jDefValue ? "" : "");
    }
    const char* keyChars = env->GetStringUTFChars(jKey, nullptr);
    const char* defChars = jDefValue ? env->GetStringUTFChars(jDefValue, nullptr) : "";
    std::string key(keyChars ? keyChars : "");
    std::string def(defChars ? defChars : "");
    env->ReleaseStringUTFChars(jKey, keyChars);
    if (jDefValue) env->ReleaseStringUTFChars(jDefValue, defChars);

    std::string val = OHOS::system::GetParameter(key, def);
    return env->NewStringUTF(val.c_str());
}

// 2026-04-30 (P2-B v2): JNI alias for AppSchedulerBridge.  Same impl as
// PackageManagerAdapter.nativeGetOhSystemProperty but registered for the
// PathClassLoader-loaded class.
JNIEXPORT jstring JNICALL
Java_adapter_activity_AppSchedulerBridge_nativeGetSysProp(
        JNIEnv* env, jclass clazz, jstring jKey, jstring jDefValue) {
    return Java_adapter_packagemanager_PackageManagerAdapter_nativeGetOhSystemProperty(
            env, clazz, jKey, jDefValue);
}


// WESTLAKE (arm64 board, 2026-07-21): PackageManagerAdapter.nativeGetApplicationInfo /
// nativeGetBundleInfo had NO implementation at all, so ActivityThread's bind path threw
// UnsatisfiedLinkError; returning null instead made LoadedApk.makeApplication fail with
//   IllegalStateException: Unable to get package info for <pkg>; is package not installed?
// Serve both from the real manifest parser above — for the DIRECT-LAUNCH flow the APK is
// the staged one (ResolveApkPath honours ASX_APK_PATH), which is the only source of truth
// we have since the bundle is never registered with OH BMS.
JNIEXPORT jstring JNICALL
Java_adapter_packagemanager_PackageManagerAdapter_nativeGetApplicationInfo(
        JNIEnv* env, jclass clazz, jstring jPackageName, jint flags) {
    // WESTLAKE (2026-07-22) PROBE: ActivityThread.handleBindApplication calls
    // getApplicationInfo(pkg, GET_META_DATA=128, uid) and immediately dereferences the result,
    // NPE'ing on a null ApplicationInfo. flags are ignored here, so log the package name and the
    // JSON length to see whether the parser is being asked for something it cannot resolve.
    jstring res = Java_adapter_packagemanager_PackageManagerAdapter_nativeParseApkManifestJson(
            env, clazz, jPackageName);
    {
        const char* pn = (jPackageName != nullptr) ? env->GetStringUTFChars(jPackageName, nullptr)
                                                   : nullptr;
        jsize len = (res != nullptr) ? env->GetStringLength(res) : -1;
        const char* js = (res != nullptr) ? env->GetStringUTFChars(res, nullptr) : nullptr;
        fprintf(stderr, "[WESTLAKE-AIPROBE] nativeGetApplicationInfo pkg='%s' flags=%d jsonLen=%d\n",
                pn ? pn : "<null>", (int) flags, (int) len);
        if (js != nullptr) {
            fprintf(stderr, "[WESTLAKE-AIJSON] %.700s\n", js);
            env->ReleaseStringUTFChars(res, js);
        }
        fflush(stderr);
        if (pn != nullptr) env->ReleaseStringUTFChars(jPackageName, pn);
    }
    return res;
}

JNIEXPORT jstring JNICALL
Java_adapter_packagemanager_PackageManagerAdapter_nativeGetBundleInfo(
        JNIEnv* env, jclass clazz, jstring jPackageName, jint /*flags*/) {
    return Java_adapter_packagemanager_PackageManagerAdapter_nativeParseApkManifestJson(
            env, clazz, jPackageName);
}

// Activity/ability queries are served from the same manifest JSON (it carries the
// activities/services/receivers/providers lists).
JNIEXPORT jstring JNICALL
Java_adapter_packagemanager_PackageManagerAdapter_nativeQueryAbilityInfos(
        JNIEnv* env, jclass clazz, jstring jPackageName, jint /*flags*/) {
    return Java_adapter_packagemanager_PackageManagerAdapter_nativeParseApkManifestJson(
            env, clazz, jPackageName);
}

}  // extern "C"
