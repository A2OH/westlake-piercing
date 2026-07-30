/*
 * window_manager_adapter.cpp
 *
 * JNI registration for adapter.window.WindowManagerAdapter via RegisterNatives.
 * Replaces the legacy Java_adapter_bridge_WindowManagerAdapter_* export
 * (and the adapter_window_ forwarder) previously in
 * framework/core/jni/adapter_bridge.cpp.
 *
 * Class:  adapter/window/WindowManagerAdapter  (BCP - oh-adapter-framework.jar)
 * Registered from adapter_bridge.cpp's JNI_OnLoad via
 *   register_WindowManagerAdapter(env).
 */

#include "oh_window_manager_client.h"

#include <android/log.h>
#include <jni.h>

#define TAG "OH_WMJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

using namespace oh_adapter;

namespace {

jlong nativeGetOHWindowManagerService_impl(JNIEnv*, jclass) {
    return (jlong)&OHWindowManagerClient::getInstance();
}

const JNINativeMethod kMethods[] = {
    {"nativeGetOHWindowManagerService", "()J",
        (void*)nativeGetOHWindowManagerService_impl},
};

}  // namespace

int register_WindowManagerAdapter(JNIEnv* env) {
    jclass clazz = env->FindClass("adapter/window/WindowManagerAdapter");
    if (!clazz) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        LOGE("register_WindowManagerAdapter: FindClass returned null");
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
        LOGE("register_WindowManagerAdapter: RegisterNatives failed rc=%d", (int)rc);
    } else {
        LOGI("register_WindowManagerAdapter: OK 1 method");
    }
    return rc;
}
