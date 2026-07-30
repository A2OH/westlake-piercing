// AOSP-compat header at "android_runtime/AndroidRuntime.h" path.
// Provides minimum surface AOSP register_* sources need:
//   class android::AndroidRuntime { static JNIEnv* getJNIEnv(); ... }
// Real impl is in our liboh_android_runtime/src/AndroidRuntime.cpp.
#pragma once
#include <jni.h>

namespace android {

class AndroidRuntime {
public:
    static int startReg(JNIEnv* env);
    static JNIEnv* getJNIEnv();
    static void setJavaVM(JavaVM* vm);
    // 2026-05-18 (L2): AOSP core_jni_helpers.h's RegisterMethodsOrDie inline
    // calls this; declaration only, real impl is in adapter's
    // src/AndroidRuntime.cpp (jniRegisterNativeMethods passthrough).
    static int registerNativeMethods(JNIEnv* env, const char* className,
                                      const JNINativeMethod* gMethods,
                                      int numMethods);
};

}  // namespace android
