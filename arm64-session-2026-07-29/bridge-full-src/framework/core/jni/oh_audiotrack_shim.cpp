/*
 * oh_audiotrack_shim.cpp
 *
 * [FIX-AUDIO 2026-06-30] ABI-boundary shim: android.media.AudioTrack JNI natives
 * -> OHOS OH_AudioRenderer (libohaudio NDK). This is where Android exits into the
 * platform audio backend, so adapting HERE makes sound work for every app
 * (noice/ExoPlayer, etc.) WITHOUT patching framework internals or the boot image.
 *
 * Android pushes PCM via AudioTrack.write(); OHOS pulls via a write-data callback.
 * A per-track ring buffer bridges the two. MVP: MODE_STREAM, PCM16, mono/stereo,
 * common sample rates, blocking/non-blocking write, silence-on-underrun.
 *
 * libohaudio is dlopen'd (no link/header/build-script changes); OH_Audio types are
 * declared minimally here.
 */
#include <jni.h>
#include <dlfcn.h>
#include <cstring>
#include <cstdint>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <android/log.h>

#define ATLOG(...) __android_log_print(ANDROID_LOG_INFO, "OH_ATShim", __VA_ARGS__)
#define ATERR(...) __android_log_print(ANDROID_LOG_ERROR, "OH_ATShim", __VA_ARGS__)

// ---- minimal OH_Audio NDK decls (from native_audiostreambuilder.h / native_audiorenderer.h) ----
typedef struct OH_AudioStreamBuilderStruct OH_AudioStreamBuilder;
typedef struct OH_AudioRendererStruct OH_AudioRenderer;
typedef struct OH_AudioRenderer_Callbacks_Struct {
    int32_t (*OH_AudioRenderer_OnWriteData)(OH_AudioRenderer*, void*, void*, int32_t);
    int32_t (*OH_AudioRenderer_OnStreamEvent)(OH_AudioRenderer*, void*, int32_t);
    int32_t (*OH_AudioRenderer_OnInterruptEvent)(OH_AudioRenderer*, void*, int32_t, int32_t);
    int32_t (*OH_AudioRenderer_OnError)(OH_AudioRenderer*, void*, int32_t);
} OH_AudioRenderer_Callbacks;
enum { AUDIOSTREAM_TYPE_RENDERER = 1 };
enum { AUDIOSTREAM_SAMPLE_U8 = 0, AUDIOSTREAM_SAMPLE_S16LE = 1, AUDIOSTREAM_SAMPLE_S24LE = 2, AUDIOSTREAM_SAMPLE_S32LE = 3, AUDIOSTREAM_SAMPLE_F32LE = 4 };
enum { AUDIOSTREAM_USAGE_MUSIC = 1 };
enum { AUDIOSTREAM_LATENCY_MODE_NORMAL = 0 };

typedef int32_t (*fn_Create)(OH_AudioStreamBuilder**, int32_t);
typedef int32_t (*fn_Destroy)(OH_AudioStreamBuilder*);
typedef int32_t (*fn_SetRate)(OH_AudioStreamBuilder*, int32_t);
typedef int32_t (*fn_SetChan)(OH_AudioStreamBuilder*, int32_t);
typedef int32_t (*fn_SetFmtT)(OH_AudioStreamBuilder*, int32_t);
typedef int32_t (*fn_SetUsage)(OH_AudioStreamBuilder*, int32_t);
typedef int32_t (*fn_SetLatency)(OH_AudioStreamBuilder*, int32_t);
typedef int32_t (*fn_SetCb)(OH_AudioStreamBuilder*, OH_AudioRenderer_Callbacks, void*);
typedef int32_t (*fn_Gen)(OH_AudioStreamBuilder*, OH_AudioRenderer**);
typedef int32_t (*fn_R)(OH_AudioRenderer*);
typedef int32_t (*fn_SetVol)(OH_AudioRenderer*, float);
typedef int32_t (*fn_Frames)(OH_AudioRenderer*, int64_t*);

static fn_Create  p_Create=nullptr;  static fn_Destroy p_Destroy=nullptr;
static fn_SetRate p_SetRate=nullptr;  static fn_SetChan p_SetChan=nullptr;
static fn_SetFmtT p_SetFmt=nullptr;   static fn_SetUsage p_SetUsage=nullptr;
static fn_SetLatency p_SetLatency=nullptr; static fn_SetCb p_SetCb=nullptr;
static fn_Gen p_Gen=nullptr;
static fn_R p_Start=nullptr, p_Stop=nullptr, p_Pause=nullptr, p_Flush=nullptr, p_Release=nullptr;
static fn_SetVol p_SetVol=nullptr;  static fn_Frames p_Frames=nullptr;
static bool g_ohLoaded=false;

static bool loadOhAudio() {
    if (g_ohLoaded) return p_Gen != nullptr;
    g_ohLoaded = true;
    void* h = dlopen("libohaudio.so", RTLD_NOW|RTLD_GLOBAL);
    if (!h) h = dlopen("/system/lib/ndk/libohaudio.so", RTLD_NOW|RTLD_GLOBAL);
    if (!h) { ATERR("dlopen libohaudio failed: %s", dlerror()); return false; }
    p_Create=(fn_Create)dlsym(h,"OH_AudioStreamBuilder_Create");
    p_Destroy=(fn_Destroy)dlsym(h,"OH_AudioStreamBuilder_Destroy");
    p_SetRate=(fn_SetRate)dlsym(h,"OH_AudioStreamBuilder_SetSamplingRate");
    p_SetChan=(fn_SetChan)dlsym(h,"OH_AudioStreamBuilder_SetChannelCount");
    p_SetFmt=(fn_SetFmtT)dlsym(h,"OH_AudioStreamBuilder_SetSampleFormat");
    p_SetUsage=(fn_SetUsage)dlsym(h,"OH_AudioStreamBuilder_SetRendererInfo"); // (builder,usage) — see note
    p_SetLatency=(fn_SetLatency)dlsym(h,"OH_AudioStreamBuilder_SetLatencyMode");
    p_SetCb=(fn_SetCb)dlsym(h,"OH_AudioStreamBuilder_SetRendererCallback");
    p_Gen=(fn_Gen)dlsym(h,"OH_AudioStreamBuilder_GenerateRenderer");
    p_Start=(fn_R)dlsym(h,"OH_AudioRenderer_Start");
    p_Stop=(fn_R)dlsym(h,"OH_AudioRenderer_Stop");
    p_Pause=(fn_R)dlsym(h,"OH_AudioRenderer_Pause");
    p_Flush=(fn_R)dlsym(h,"OH_AudioRenderer_Flush");
    p_Release=(fn_R)dlsym(h,"OH_AudioRenderer_Release");
    p_SetVol=(fn_SetVol)dlsym(h,"OH_AudioRenderer_SetVolume");
    p_Frames=(fn_Frames)dlsym(h,"OH_AudioRenderer_GetFramesWritten");
    ATLOG("libohaudio loaded: Create=%p Gen=%p Start=%p", (void*)p_Create,(void*)p_Gen,(void*)p_Start);
    return p_Create && p_Gen && p_Start;
}

// ---- per-AudioTrack shim ----
struct ATShim {
    OH_AudioStreamBuilder* builder=nullptr;
    OH_AudioRenderer* renderer=nullptr;
    int sampleRate=44100, channels=2, bytesPerFrame=4;
    std::vector<uint8_t> ring;
    size_t cap=0, head=0, tail=0, count=0;   // byte ring
    std::mutex mu; std::condition_variable spaceCv;
    bool started=false, stopping=false;
    int64_t framesRendered=0;
};

static jfieldID g_fNative = nullptr;
static ATShim* getShim(JNIEnv* env, jobject thiz) {
    if (!g_fNative) return nullptr;
    return reinterpret_cast<ATShim*>(env->GetLongField(thiz, g_fNative));
}

// OHOS audio thread pulls here
static int32_t onWriteData(OH_AudioRenderer*, void* user, void* buffer, int32_t len) {
    ATShim* s = reinterpret_cast<ATShim*>(user);
    uint8_t* out = reinterpret_cast<uint8_t*>(buffer);
    std::unique_lock<std::mutex> lk(s->mu);
    int32_t give = (int32_t)((s->count < (size_t)len) ? s->count : (size_t)len);
    for (int32_t i=0;i<give;i++){ out[i]=s->ring[s->head]; s->head=(s->head+1)%s->cap; }
    s->count -= give;
    if (give < len) memset(out+give, 0, len-give);     // silence on underrun
    s->framesRendered += len / (s->bytesPerFrame>0?s->bytesPerFrame:4);
    lk.unlock();
    s->spaceCv.notify_all();
    return 0; // AUDIOSTREAM_SUCCESS
}

static int pushPcm(ATShim* s, const uint8_t* data, int sizeBytes, bool blocking) {
    int written=0;
    std::unique_lock<std::mutex> lk(s->mu);
    while (written < sizeBytes) {
        while (s->count >= s->cap && !s->stopping) {
            if (!blocking) { lk.unlock(); return written; }
            s->spaceCv.wait_for(lk, std::chrono::milliseconds(200));
        }
        if (s->stopping) break;
        while (written < sizeBytes && s->count < s->cap) {
            s->ring[s->tail]=data[written++]; s->tail=(s->tail+1)%s->cap; s->count++;
        }
    }
    return written;
}

// ================= JNI natives =================
extern "C" {

static jint nSetup(JNIEnv* env, jobject thiz, jobject /*weak*/, jobject /*attrs*/,
        jintArray sampleRateArr, jint channelMask, jint /*chIdxMask*/, jint audioFormat,
        jint buffSize, jint /*mode*/, jintArray sessionId, jobject /*parcel*/,
        jlong /*nativeAT*/, jboolean /*offload*/, jint /*encaps*/, jobject /*tuner*/, jstring /*opPkg*/) {
    if (!loadOhAudio()) return -20; // ERROR
    int rate=44100;
    if (sampleRateArr) { jint v=0; env->GetIntArrayRegion(sampleRateArr,0,1,&v); if(v>0) rate=v; }
    // channelMask -> count (popcount of the position bits; fall back to 2)
    int ch = __builtin_popcount((unsigned)channelMask & 0x000FFFFF);
    if (ch<1||ch>2) ch = (channelMask!=0 && ch>2)?2:((ch==1)?1:2);
    if (ch<1) ch=2;
    // audioFormat: ENCODING_PCM_16BIT=2, 8BIT=3, FLOAT=4. MVP -> S16LE.
    int ohFmt=AUDIOSTREAM_SAMPLE_S16LE, bpf=2*ch;
    ATShim* s=new ATShim();
    s->sampleRate=rate; s->channels=ch; s->bytesPerFrame=bpf;
    s->cap = (size_t)((buffSize>0?buffSize:rate*bpf/5) * 4); if(s->cap<rate*bpf/5) s->cap=rate*bpf/5; // ~hint*4, >=200ms
    if (s->cap < 16384) s->cap=16384;
    s->ring.assign(s->cap, 0);
    if (p_Create(&s->builder, AUDIOSTREAM_TYPE_RENDERER)!=0){ ATERR("Create fail"); delete s; return -21; }
    p_SetRate(s->builder, rate); p_SetChan(s->builder, ch); p_SetFmt(s->builder, ohFmt);
    if (p_SetUsage) p_SetUsage(s->builder, AUDIOSTREAM_USAGE_MUSIC);  // RendererInfo: required for GenerateRenderer
    if (p_SetLatency) p_SetLatency(s->builder, AUDIOSTREAM_LATENCY_MODE_NORMAL);
    OH_AudioRenderer_Callbacks cbs; memset(&cbs,0,sizeof(cbs));
    cbs.OH_AudioRenderer_OnWriteData=onWriteData;
    p_SetCb(s->builder, cbs, s);
    if (p_Gen(s->builder, &s->renderer)!=0){ ATERR("GenerateRenderer fail"); p_Destroy(s->builder); delete s; return -22; }
    if (sessionId){ jint z=0; env->SetIntArrayRegion(sessionId,0,1,&z); }
    if (!g_fNative){ jclass c=env->GetObjectClass(thiz); g_fNative=env->GetFieldID(c,"mNativeTrackInJavaObj","J"); }
    env->SetLongField(thiz, g_fNative, reinterpret_cast<jlong>(s));
    ATLOG("native_setup ok rate=%d ch=%d cap=%zu", rate, ch, s->cap);
    return 0; // SUCCESS
}

static void nStart(JNIEnv* env, jobject thiz){ ATShim* s=getShim(env,thiz); if(s&&s->renderer&&!s->started){ p_Start(s->renderer); s->started=true; ATLOG("start"); } }
static void nStop(JNIEnv* env, jobject thiz){ ATShim* s=getShim(env,thiz); if(s&&s->renderer){ { std::lock_guard<std::mutex> lk(s->mu); s->stopping=true; } s->spaceCv.notify_all(); if(p_Stop)p_Stop(s->renderer); s->started=false; } }
static void nPause(JNIEnv* env, jobject thiz){ ATShim* s=getShim(env,thiz); if(s&&s->renderer&&p_Pause) p_Pause(s->renderer); }
static void nFlush(JNIEnv* env, jobject thiz){ ATShim* s=getShim(env,thiz); if(s){ std::lock_guard<std::mutex> lk(s->mu); s->head=s->tail=s->count=0; if(p_Flush&&s->renderer)p_Flush(s->renderer);} }
static void nRelease(JNIEnv* env, jobject thiz){ ATShim* s=getShim(env,thiz); if(s){ { std::lock_guard<std::mutex> lk(s->mu); s->stopping=true; } s->spaceCv.notify_all(); if(p_Release&&s->renderer)p_Release(s->renderer); if(p_Destroy&&s->builder)p_Destroy(s->builder); env->SetLongField(thiz,g_fNative,0); delete s; ATLOG("release"); } }
static void nFinalize(JNIEnv* env, jobject thiz){ nRelease(env,thiz); }

static jint nWriteByte(JNIEnv* env, jobject thiz, jbyteArray data, jint off, jint size, jint /*fmt*/, jboolean blocking){
    ATShim* s=getShim(env,thiz); if(!s) return -22;
    jbyte* buf=env->GetByteArrayElements(data,nullptr); if(!buf) return -1;
    int w=pushPcm(s,(const uint8_t*)(buf+off),size,blocking);
    env->ReleaseByteArrayElements(data,buf,JNI_ABORT);
    return w;
}
static jint nWriteShort(JNIEnv* env, jobject thiz, jshortArray data, jint off, jint size, jint /*fmt*/, jboolean blocking){
    ATShim* s=getShim(env,thiz); if(!s) return -22;
    jshort* buf=env->GetShortArrayElements(data,nullptr); if(!buf) return -1;
    int w=pushPcm(s,(const uint8_t*)(buf+off), size*2, blocking);
    env->ReleaseShortArrayElements(data,buf,JNI_ABORT);
    return w/2; // return in shorts
}
static jint nWriteNative(JNIEnv* env, jobject thiz, jobject byteBuf, jint pos, jint size, jint /*fmt*/, jboolean blocking){
    ATShim* s=getShim(env,thiz); if(!s) return -22;
    uint8_t* base=(uint8_t*)env->GetDirectBufferAddress(byteBuf); if(!base) return -1;
    return pushPcm(s, base+pos, size, blocking);
}
static jint nWriteFloat(JNIEnv* env, jobject thiz, jfloatArray data, jint off, jint size, jint /*fmt*/, jboolean blocking){
    // MVP: convert F32 -> S16LE
    ATShim* s=getShim(env,thiz); if(!s) return -22;
    jfloat* buf=env->GetFloatArrayElements(data,nullptr); if(!buf) return -1;
    std::vector<int16_t> tmp(size);
    for(int i=0;i<size;i++){ float v=buf[off+i]; if(v>1)v=1; if(v<-1)v=-1; tmp[i]=(int16_t)(v*32767.f); }
    env->ReleaseFloatArrayElements(data,buf,JNI_ABORT);
    int w=pushPcm(s,(const uint8_t*)tmp.data(), size*2, blocking);
    return w/2;
}
static jint nGetPos(JNIEnv* env, jobject thiz){ ATShim* s=getShim(env,thiz); if(!s) return 0; int64_t f=s->framesRendered; if(p_Frames&&s->renderer)p_Frames(s->renderer,&f); return (jint)f; }
static void nSetVolume(JNIEnv* env, jobject thiz, jfloat l, jfloat r){ ATShim* s=getShim(env,thiz); if(s&&p_SetVol&&s->renderer)p_SetVol(s->renderer,(l+r)/2.f); }
static jint nMinBuf(JNIEnv*, jclass, jint rate, jint chCfg, jint fmt){ int ch=__builtin_popcount((unsigned)chCfg&0xFFFFF); if(ch<1)ch=2; int bpf=2*ch; int b=rate*bpf/10; if(b<4096)b=4096; return b; }

// §505 (2026-08-04): AudioTrack.<init> calls native_setPlayerIId to hand AudioService the player id
// allocated by PlayerBase, purely for that service's own bookkeeping. We have no AudioService, so a
// no-op is the correct behaviour — but leaving it UNBOUND is not. Unbound, it raised
//   UnsatisfiedLinkError: No implementation found for
//     void android.media.AudioTrack.native_setPlayerIId(int)
//       at android.media.AudioTrack.<init>(AudioTrack.java:908)
//       at android.media.AudioTrack$Builder.build(AudioTrack.java:1450)
// on ExoPlayer's audio HandlerThread. That is an Error, so it is not caught by ExoPlayer and, with
// ThreadGroup.uncaughtException a no-op in this runtime, it killed the thread with no report — the
// exact same failure shape as §504's libcore.io.Memory.pokeByteArray. The visible symptom was the
// codec feeding a few buffers (queueInput=17, cbOutput=4) and then freezing forever, because the
// sink that was supposed to drain it no longer had a thread.
static void nSetPlayerIId(JNIEnv*, jobject, jint) { }

int register_AudioTrack_shim(JNIEnv* env) {
    if (!loadOhAudio()) { ATERR("OH audio unavailable; AudioTrack shim NOT registered"); return -1; }
    jclass c = env->FindClass("android/media/AudioTrack");
    if (!c) { if(env->ExceptionCheck())env->ExceptionClear(); ATERR("AudioTrack class not found"); return -1; }
    g_fNative = env->GetFieldID(c, "mNativeTrackInJavaObj", "J");
    if (!g_fNative){ if(env->ExceptionCheck())env->ExceptionClear(); ATERR("mNativeTrackInJavaObj field not found"); return -1; }
    JNINativeMethod m[] = {
        {"native_setup", "(Ljava/lang/Object;Ljava/lang/Object;[IIIIII[ILandroid/os/Parcel;JZILjava/lang/Object;Ljava/lang/String;)I", (void*)nSetup},
        {"native_start", "()V", (void*)nStart},
        {"native_stop", "()V", (void*)nStop},
        {"native_pause", "()V", (void*)nPause},
        {"native_flush", "()V", (void*)nFlush},
        {"native_release", "()V", (void*)nRelease},
        {"native_finalize", "()V", (void*)nFinalize},
        {"native_write_byte", "([BIIIZ)I", (void*)nWriteByte},
        {"native_write_short", "([SIIIZ)I", (void*)nWriteShort},
        {"native_write_float", "([FIIIZ)I", (void*)nWriteFloat},
        {"native_write_native_bytes", "(Ljava/nio/ByteBuffer;IIIZ)I", (void*)nWriteNative},
        {"native_get_position", "()I", (void*)nGetPos},
        {"native_setVolume", "(FF)V", (void*)nSetVolume},
        {"native_get_min_buff_size", "(III)I", (void*)nMinBuf},
        // §505: no-op, but it MUST be bound — see nSetPlayerIId above.
        {"native_setPlayerIId", "(I)V", (void*)nSetPlayerIId},
    };
    int n = sizeof(m)/sizeof(m[0]); int ok=0;
    for (int i=0;i<n;i++){ if(env->RegisterNatives(c,&m[i],1)==0) ok++; else { if(env->ExceptionCheck())env->ExceptionClear(); ATERR("reg fail: %s", m[i].name);} }
    ATLOG("AudioTrack shim registered %d/%d", ok, n);
    return ok==n?0:-1;
}

} // extern "C"
