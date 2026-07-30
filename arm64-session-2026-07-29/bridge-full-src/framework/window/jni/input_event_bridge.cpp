/*
 * input_event_bridge.cpp
 *
 * JNI registration for adapter.window.InputEventBridge via RegisterNatives.
 * Replaces the legacy Java_adapter_window_InputEventBridge_* exports
 * previously in framework/core/jni/adapter_bridge.cpp.
 *
 * Class:  adapter/window/InputEventBridge  (BCP - oh-adapter-framework.jar)
 * Registered from adapter_bridge.cpp's JNI_OnLoad via
 *   register_InputEventBridge(env).
 *
 * 2 natives: RegisterInputChannel / UnregisterInputChannel.
 *
 * Phase 2 (2026-05-18 Input_Adapter_design §3.3): extracts the server-side
 * socketpair fd from AOSP InputChannel.mPtr and hands it to OHInputBridge so
 * MMI-sourced PointerEvents can be encoded as Android InputMessage and
 * written into ViewRootImpl's event pipeline.
 *
 * AOSP 14 InputChannel exposes no public getFd(); adapter's stub
 * (framework/android-runtime/src/android_view_InputChannel.cpp) stores the
 * fd inside OhInputChannel{} which Java sees as `mPtr` (long).  We:
 *   (a) reflect mPtr to obtain the native pointer
 *   (b) resolve oh_input_channel_get_fd() from liboh_android_runtime.so via
 *       dlsym to safely extract the fd without duplicating the struct
 *       definition (which would risk ABI drift).
 */

#include "oh_input_bridge.h"

#include <android/log.h>
#include <jni.h>
#include <dlfcn.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

#define TAG "OH_InputJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

using namespace oh_adapter;

namespace {

void nativeRegisterInputChannel_impl(JNIEnv* env, jclass,
                                     jint sessionId, jobject inputChannel) {
    if (inputChannel == nullptr) {
        LOGE("nativeRegisterInputChannel: null inputChannel");
        return;
    }
    jclass channelClass = env->GetObjectClass(inputChannel);
    jfieldID mPtrField = env->GetFieldID(channelClass, "mPtr", "J");
    if (mPtrField == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        LOGE("nativeRegisterInputChannel: InputChannel.mPtr field not found");
        env->DeleteLocalRef(channelClass);
        return;
    }
    jlong nativePtr = env->GetLongField(inputChannel, mPtrField);
    env->DeleteLocalRef(channelClass);
    if (nativePtr == 0) {
        LOGE("nativeRegisterInputChannel: InputChannel.mPtr is 0 (session=%d)",
             sessionId);
        return;
    }

    // Cache the dlsym result; called once per createInputChannelPair.
    static int (*get_fd)(jlong) = nullptr;
    if (get_fd == nullptr) {
        // RTLD_NOLOAD: don't load if not already loaded — we only succeed if
        // liboh_android_runtime.so was already brought up by appspawn-x init.
        void* h = dlopen("liboh_android_runtime.so", RTLD_NOW | RTLD_NOLOAD);
        if (h == nullptr) {
            h = dlopen("liboh_android_runtime.so", RTLD_NOW);
        }
        if (h != nullptr) {
            get_fd = reinterpret_cast<int(*)(jlong)>(
                    dlsym(h, "oh_input_channel_get_fd"));
        }
        if (get_fd == nullptr) {
            LOGE("nativeRegisterInputChannel: cannot resolve "
                 "oh_input_channel_get_fd (dl=%s)",
                 dlerror() ? dlerror() : "?");
            return;
        }
    }

    int fd = get_fd(nativePtr);
    if (fd < 0) {
        LOGE("nativeRegisterInputChannel: oh_input_channel_get_fd returned %d "
             "for mPtr=0x%llx session=%d", fd,
             static_cast<unsigned long long>(nativePtr), sessionId);
        return;
    }

    int dupFd = dup(fd);
    if (dupFd < 0) {
        LOGE("Failed to dup InputChannel fd: %s", strerror(errno));
        return;
    }

    LOGI("nativeRegisterInputChannel: session=%d serverFd=%d (dup of channel fd=%d)",
         sessionId, dupFd, fd);
    OHInputBridge::getInstance().registerInputChannel(sessionId, dupFd);
}

void nativeUnregisterInputChannel_impl(JNIEnv*, jclass, jint sessionId) {
    LOGI("nativeUnregisterInputChannel: session=%d", sessionId);
    OHInputBridge::getInstance().unregisterInputChannel(sessionId);
}

const JNINativeMethod kMethods[] = {
    {"nativeRegisterInputChannel",
        "(ILandroid/view/InputChannel;)V",
        (void*)nativeRegisterInputChannel_impl},
    {"nativeUnregisterInputChannel",
        "(I)V",
        (void*)nativeUnregisterInputChannel_impl},
};

}  // namespace

int register_InputEventBridge(JNIEnv* env) {
    jclass clazz = env->FindClass("adapter/window/InputEventBridge");
    if (!clazz) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        LOGE("register_InputEventBridge: FindClass returned null");
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
        LOGE("register_InputEventBridge: RegisterNatives failed rc=%d", (int)rc);
    } else {
        LOGI("register_InputEventBridge: OK 2 methods");
    }
    return rc;
}
