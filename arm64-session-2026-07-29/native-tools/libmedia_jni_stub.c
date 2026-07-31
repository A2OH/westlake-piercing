// Stub libmedia_jni.so for arm64/OHOS.
//
// §496. Two separate reasons this file has to exist:
//
// 1. MediaCodec/MediaCodecList.<clinit> calls System.loadLibrary("media_jni"). The library does not
//    exist on this board, and THIS RUNTIME ABORTS THE PROCESS on a failed nativeLoad rather than
//    throwing UnsatisfiedLinkError:
//        [PF202N] Runtime_nativeLoad path=libmedia_jni.so
//        Runtime aborting...
//    So merely binding the natives from the bridge (§494) can never be enough — the load itself must
//    succeed.
//
// 2. That same <clinit> then calls a native (native_init) declared by the class. ART re-resolves the
//    class's natives against THIS library at load time, which DROPS the RegisterNatives bindings the
//    bridge installed earlier. So the stub must re-register them, which is what JNI_OnLoad does here.
//
// arm64 note: the bridge lives in the appspawn-x staging dir, not /system/lib as on arm32.
#include <jni.h>
#include <dlfcn.h>
#include <stdio.h>

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)reserved;
    JNIEnv* env = 0;
    if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK || !env)
        return JNI_VERSION_1_6;

    static const char* kPaths[] = {
        "liboh_adapter_bridge.so",                          /* already loaded, by soname */
        "/data/local/tmp/asx/liboh_adapter_bridge.so",      /* arm64 staging dir */
        "/system/lib64/liboh_adapter_bridge.so",
        "/system/lib/liboh_adapter_bridge.so",              /* arm32 layout, harmless here */
        0
    };
    void* h = dlopen(kPaths[0], RTLD_NOW | RTLD_NOLOAD);
    for (int i = 0; !h && kPaths[i]; i++) h = dlopen(kPaths[i], RTLD_NOW);
    if (!h) {
        fprintf(stderr, "[WESTLAKE-496] libmedia_jni stub: bridge not found (%s)\n", dlerror());
        fflush(stderr);
        return JNI_VERSION_1_6;
    }
    int (*reg)(JNIEnv*) = (int (*)(JNIEnv*))dlsym(h, "register_MediaCodec_shim");
    if (!reg) {
        fprintf(stderr, "[WESTLAKE-496] libmedia_jni stub: register_MediaCodec_shim missing\n");
        fflush(stderr);
        return JNI_VERSION_1_6;
    }
    int rc = reg(env);
    fprintf(stderr, "[WESTLAKE-496] libmedia_jni stub: re-registered MediaCodec shim rc=%d\n", rc);
    fflush(stderr);
    return JNI_VERSION_1_6;
}
