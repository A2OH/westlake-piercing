// Stub libmedia_jni.so. MediaCodec/MediaCodecList.<clinit> call loadLibrary("media_jni")
// then a native (native_init) in the SAME <clinit>; ART re-resolves that class's natives
// against THIS lib at load time, dropping the zygote RegisterNatives bindings. So we
// (re)register the adapter MediaCodec/MediaCodecList shim natives here via the bridge.
#include <jni.h>
#include <dlfcn.h>
jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    JNIEnv* env = 0;
    if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK || !env)
        return JNI_VERSION_1_6;
    void* h = dlopen("liboh_adapter_bridge.so", RTLD_NOW | RTLD_NOLOAD);
    if (!h) h = dlopen("liboh_adapter_bridge.so", RTLD_NOW);
    if (!h) h = dlopen("/system/lib/liboh_adapter_bridge.so", RTLD_NOW);
    if (h) {
        int (*reg)(JNIEnv*) = (int(*)(JNIEnv*))dlsym(h, "register_MediaCodec_shim");
        if (reg) reg(env);
    }
    return JNI_VERSION_1_6;
}
