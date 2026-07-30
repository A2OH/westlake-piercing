/*
 * oh_environment.cpp
 *
 * JNI registration for adapter.core.OHEnvironment via RegisterNatives.
 * Replaces the legacy Java_adapter_OHEnvironment_* / Java_adapter_core_OHEnvironment_*
 * symbol exports that previously lived in framework/core/jni/adapter_bridge.cpp.
 *
 * Class:  adapter/core/OHEnvironment  (BCP - oh-adapter-framework.jar)
 * Registered from adapter_bridge.cpp's JNI_OnLoad via register_OHEnvironment(env).
 */

#include "adapter_bridge.h"
#include "oh_ability_manager_client.h"
#include "oh_app_mgr_client.h"
#include "oh_callback_handler.h"
#include "oh_window_manager_client.h"
#include "oh_input_bridge.h"
#include "common_event_subscriber_adapter.h"   // initCommonEventJNI

#include <android/log.h>
#include <jni.h>
#include <mutex>
#include <string>

#include "ipc_skeleton.h"

#define TAG "OH_EnvJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

using namespace oh_adapter;

namespace {

std::once_flag g_initFlag;
bool g_initialized = false;

jboolean nativeInitialize_impl(JNIEnv* env, jclass) {
    LOGI("nativeInitialize()");

    jboolean result = JNI_FALSE;
    std::call_once(g_initFlag, [&]() {
        AdapterBridge& bridge = AdapterBridge::getInstance();
        if (!bridge.initialize(env)) {
            LOGE("AdapterBridge initialization failed");
            return;
        }

        // Initialize OH IPC framework — SetCallingIdentity takes std::string&
        // (non-const ref) in V7, so we can't pass a string literal directly.
        std::string emptyId;
        OHOS::IPCSkeleton::SetCallingIdentity(emptyId);
        LOGI("OH IPC framework initialized");

        // Initialize input event bridge — fetch JavaVM from JNIEnv since `vm`
        // is only in scope inside JNI_OnLoad.
        JavaVM* jvm = nullptr;
        env->GetJavaVM(&jvm);
        OHInputBridge::getInstance().setJavaVM(jvm);
        LOGI("Input event bridge initialized");

        // Initialize CommonEvent JNI callbacks
        if (!initCommonEventJNI(env)) {
            LOGE("CommonEvent JNI initialization failed (non-fatal)");
        }

        g_initialized = true;
        result = JNI_TRUE;
    });

    if (g_initialized) result = JNI_TRUE;
    return result;
}

jboolean nativeConnectToOHServices_impl(JNIEnv*, jclass) {
    LOGI("nativeConnectToOHServices()");

    bool abilityMgrOk = OHAbilityManagerClient::getInstance().connect();
    bool appMgrOk = OHAppMgrClient::getInstance().connect();
    bool windowMgrOk = OHWindowManagerClient::getInstance().connect();
    bool callbackOk = OHCallbackHandler::getInstance().registerCallbacks();

    LOGI("Service connections: AbilityMgr=%d, AppMgr=%d, WindowMgr=%d, Callbacks=%d",
         abilityMgrOk, appMgrOk, windowMgrOk, callbackOk);

    return (jboolean)(abilityMgrOk && appMgrOk);
}

jboolean nativeAttachApplication_impl(JNIEnv* env, jclass, jobject thread,
                                      jint pid, jint uid, jstring packageName) {
    const char* pkgName = env->GetStringUTFChars(packageName, nullptr);
    LOGI("nativeAttachApplication: pid=%d, uid=%d, pkg=%s, thread=%p",
         pid, uid, pkgName, thread);

    JavaVM* jvm = nullptr;
    env->GetJavaVM(&jvm);
    bool result = OHAppMgrClient::getInstance().attachApplication(
            jvm, thread, pid, uid, pkgName);

    env->ReleaseStringUTFChars(packageName, pkgName);
    return (jboolean)result;
}

void nativeNotifyAppState_impl(JNIEnv*, jclass, jint state) {
    LOGI("nativeNotifyAppState: state=%d", state);
    OHAppMgrClient::getInstance().notifyAppState(state);
}

void nativeShutdown_impl(JNIEnv*, jclass) {
    LOGI("nativeShutdown()");
    OHCallbackHandler::getInstance().unregisterCallbacks();
    OHWindowManagerClient::getInstance().disconnect();
    OHAbilityManagerClient::getInstance().disconnect();
    OHAppMgrClient::getInstance().disconnect();
    AdapterBridge::getInstance().shutdown();
}

jboolean nativeIsOHEnvironment_impl(JNIEnv*, jclass) {
    return (jboolean)g_initialized;
}

const JNINativeMethod kMethods[] = {
    {"nativeInitialize",          "()Z",  (void*)nativeInitialize_impl},
    {"nativeConnectToOHServices", "()Z",  (void*)nativeConnectToOHServices_impl},
    {"nativeAttachApplication",
        "(Ljava/lang/Object;IILjava/lang/String;)Z",
        (void*)nativeAttachApplication_impl},
    {"nativeNotifyAppState",      "(I)V", (void*)nativeNotifyAppState_impl},
    {"nativeShutdown",            "()V",  (void*)nativeShutdown_impl},
    {"nativeIsOHEnvironment",     "()Z",  (void*)nativeIsOHEnvironment_impl},
};

}  // namespace

int register_OHEnvironment(JNIEnv* env) {
    jclass clazz = env->FindClass("adapter/core/OHEnvironment");
    if (!clazz) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        LOGE("register_OHEnvironment: FindClass(adapter/core/OHEnvironment) null");
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
        LOGE("register_OHEnvironment: RegisterNatives failed rc=%d", (int)rc);
    } else {
        LOGI("register_OHEnvironment: OK 6 methods");
    }
    return rc;
}
