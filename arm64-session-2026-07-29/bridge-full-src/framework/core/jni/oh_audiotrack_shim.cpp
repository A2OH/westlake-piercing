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
    // §514: frames actually played out of data the APP wrote. framesRendered counts every frame the
    // OH renderer pulled, including the silence we hand it on underrun, so it advances even when the
    // app has written nothing — which is NOT what AudioTrack.getPlaybackHeadPosition() means.
    int64_t framesReal=0;
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
    int bpf = (s->bytesPerFrame>0?s->bytesPerFrame:4);
    s->framesRendered += len / bpf;
    s->framesReal     += give / bpf;   // §514: only real PCM counts toward playback position
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

static long g_writeCalls = 0;
static void logWrite(jint size, jint rc, const char* via) {
    long n = ++g_writeCalls;
    if (n <= 5 || (n % 200) == 0)
        ATLOG("write #%ld via=%s size=%d rc=%d", n, via, (int)size, (int)rc);
}

static jint nWriteByte(JNIEnv* env, jobject thiz, jbyteArray data, jint off, jint size, jint /*fmt*/, jboolean blocking){
    ATShim* s=getShim(env,thiz); if(!s) return -22;
    jbyte* buf=env->GetByteArrayElements(data,nullptr); if(!buf) return -1;
    int w=pushPcm(s,(const uint8_t*)(buf+off),size,blocking);
    env->ReleaseByteArrayElements(data,buf,JNI_ABORT);
    logWrite(size, w, "byte[]");
    return w;
}
static jint nWriteShort(JNIEnv* env, jobject thiz, jshortArray data, jint off, jint size, jint /*fmt*/, jboolean blocking){
    ATShim* s=getShim(env,thiz); if(!s) return -22;
    jshort* buf=env->GetShortArrayElements(data,nullptr); if(!buf) return -1;
    int w=pushPcm(s,(const uint8_t*)(buf+off), size*2, blocking);
    env->ReleaseShortArrayElements(data,buf,JNI_ABORT);
    logWrite(size, w, "short[]");
    return w/2; // return in shorts
}
// §507 instrumentation: the write path was completely silent, so "no write markers in the log" was
// not evidence that ExoPlayer never wrote — it was evidence that we never looked. The observed
// symptom is AudioTrack being created -> started -> released in a loop with
// "volume data counts: 0" from the OH renderer, which is equally consistent with (a) ExoPlayer never
// calling write, and (b) write being called and failing. Those need different fixes, so log it.
// Rate-limited: the first few calls, then every 200th, so a working stream costs almost nothing.

static jint nWriteNative(JNIEnv* env, jobject thiz, jobject byteBuf, jint pos, jint size, jint /*fmt*/, jboolean blocking){
    ATShim* s=getShim(env,thiz);
    if(!s) { logWrite(size, -22, "bytebuf"); return -22; }
    uint8_t* base=(uint8_t*)env->GetDirectBufferAddress(byteBuf);
    if(!base) { logWrite(size, -1, "bytebuf"); return -1; }
    jint rc = pushPcm(s, base+pos, size, blocking);
    logWrite(size, rc, "bytebuf");
    return rc;
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
static long g_posCalls = 0;
static jint nGetPos(JNIEnv* env, jobject thiz){
    ATShim* s=getShim(env,thiz); if(!s) return 0;
    // §514: AudioTrack.getPlaybackHeadPosition() is "frames played from what the app wrote". Using
    // OH_AudioRenderer_GetFramesWritten reported 8916 frames while the app had written ZERO bytes,
    // because RENDER_MODE_CALLBACK starts pulling the moment the stream starts and we answer with
    // silence. ExoPlayer's AudioTrackPositionTracker compares that head position against its own
    // writtenFrames; position >> written is an impossible state and it aborts the sink with a bare
    // IllegalArgumentException, before the first write ever happens. Report frames drained from the
    // ring instead, which is the quantity AudioTrack actually documents.
    int64_t f; { std::lock_guard<std::mutex> l(s->mu); f = s->framesReal; }
    // §512: distinguishes "sink is alive and polling position" from "sink bailed and calls nothing".
    // AudioTrack.write() returns ERROR_INVALID_OPERATION *before reaching native* when
    // mState != STATE_INITIALIZED, so a silent write path and a dead sink look identical from the
    // write side alone. Also report the ring occupancy, because a sink that is polling but never
    // writing leaves count=0 forever.
    long n = ++g_posCalls;
    if (n <= 3 || (n % 100) == 0) {
        size_t cnt; { std::lock_guard<std::mutex> l(s->mu); cnt = s->count; }
        ATLOG("getPos #%ld frames=%lld ringBytes=%zu started=%d", n, (long long)f, cnt, (int)s->started);
    }
    return (jint)f;
}
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

// §506: the same trap, once per round, all over DefaultAudioSink's setup path. ExoPlayer registers a
// routing listener right after building the track (addOnRoutingChangedListener -> AudioRouting ->
// native_enableDeviceCallback), and that one was unbound too, killing the audio thread exactly like
// §505 did — the feed froze at the identical queueInput=16/cbOutput=4.
//
// So rather than discover these one 15-minute round at a time, bind the whole cluster whose
// semantics are unambiguous on this platform. Every method below reports or subscribes to state
// owned by AudioService / AudioPolicy — device routing, port ids, output flags, underruns, log
// session ids. We have none of those, so "nothing to report" IS the correct answer, and returning it
// is strictly better than leaving an Error-throwing stub.
//
// ⚠️Deliberately NOT bound here: native_get_timestamp, native_get_buffer_size_frames,
// native_set_buffer_size_frames, native_applyVolumeShaper and friends. Those have real semantics
// that ExoPlayer's position tracking depends on, and inventing values would trade a loud crash for
// silent drift. Leave them until evidence names them.
static void nEnableDeviceCb(JNIEnv*, jobject) { }
static void nDisableDeviceCb(JNIEnv*, jobject) { }
static jint nRoutedDeviceId(JNIEnv*, jobject) { return 0; }   // 0 = unknown/default device
static jint nPortId(JNIEnv*, jobject) { return 0; }
static jint nGetFlags(JNIEnv*, jobject) { return 0; }         // no AUDIO_OUTPUT_FLAG_* apply
static jint nUnderrunCount(JNIEnv*, jobject) { return 0; }
static jint nGetLatency(JNIEnv*, jobject) { return 0; }
static void nSetLogSessionId(JNIEnv*, jobject, jstring) { }

// §506b: the rest of what ExoPlayer can actually reach, taken from a static scan of the app dex
// (tools/FindClassRefs.java on Landroid/media/AudioTrack;) rather than from another round of
// discover-one-crash-at-a-time. That scan is also why getBufferSizeInFrames is absent here: the app
// never references it, so binding it would be speculation.
//
// Each of these returns the honest "not available / not supported" answer rather than a plausible
// lie, because ExoPlayer degrades gracefully on all of them:
//   getTimestamp   -> non-zero status makes AudioTrack.getTimestamp() return false, and
//                     AudioTimestampPoller falls back to getPlaybackHeadPosition(), which IS real
//                     here (backed by OH_AudioRenderer frames written).
//   direct output  -> false keeps ExoPlayer on the decode path that already works, instead of
//                     attempting MP3 passthrough this renderer cannot do.
//   preferred device -> false; we have no routing to honour and saying otherwise would be a lie.
// ⚠️AudioTimestampPoller calls getTimestamp with NO catch, so leaving it unbound is fatal, while
// answering "unavailable" is a state real devices report routinely.
static jint nGetTimestamp(JNIEnv*, jobject, jlongArray) { return -1; }         // != SUCCESS
static jint nAttachAuxEffect(JNIEnv*, jobject, jint) { return 0; }             // SUCCESS, no-op
static jint nSetAuxSendLevel(JNIEnv*, jobject, jfloat) { return 0; }
static jboolean nSetOutputDevice(JNIEnv*, jobject, jint) { return JNI_FALSE; }
static void nSetDelayPadding(JNIEnv*, jobject, jint, jint) { }
static jboolean nIsDirectSupported(JNIEnv*, jclass, jint, jint, jint, jint, jint, jint, jint) {
    return JNI_FALSE;
}

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
        // §505/§506: no-ops, but they MUST be bound — see nSetPlayerIId above. Per-method
        // registration below means any that this framework.jar does not declare are skipped
        // harmlessly rather than taking the whole table down with them.
        {"native_setPlayerIId", "(I)V", (void*)nSetPlayerIId},
        {"native_enableDeviceCallback", "()V", (void*)nEnableDeviceCb},
        {"native_disableDeviceCallback", "()V", (void*)nDisableDeviceCb},
        {"native_getRoutedDeviceId", "()I", (void*)nRoutedDeviceId},
        {"native_getPortId", "()I", (void*)nPortId},
        {"native_get_flags", "()I", (void*)nGetFlags},
        {"native_get_underrun_count", "()I", (void*)nUnderrunCount},
        {"native_get_latency", "()I", (void*)nGetLatency},
        {"native_setLogSessionId", "(Ljava/lang/String;)V", (void*)nSetLogSessionId},
        {"native_get_timestamp", "([J)I", (void*)nGetTimestamp},
        // ⛔The five other natives the dex scan turned up — attachAuxEffect,
        // setAuxEffectSendLevel, setOutputDevice, set_delay_padding and
        // is_direct_output_supported — are NOT bound, and binding them is not a free win.
        // Registering all six at once hung the child at startup: it stayed alive but never rendered
        // a frame (swaps=0 for 5 minutes, versus a 44 s boot on the build without them). Only
        // get_timestamp was ever named by JNIMISS; the rest were me predicting ahead of the
        // evidence. Bisecting cost a full board cycle, so: bind what the runtime has actually
        // asked for, one at a time, and let JNIMISS name the next one.
    };
    int n = sizeof(m)/sizeof(m[0]);
    // Entries from kFirstOptional on are the §505/§506 no-ops. Whether a given framework.jar
    // declares each of them varies by API level, and a method this AudioTrack.java does not declare
    // is not an error — so a miss there must not fail the shim and hide a real core-method failure.
    const int kFirstOptional = 14;
    int ok=0, reqFail=0;
    for (int i=0;i<n;i++){
        if (env->RegisterNatives(c,&m[i],1)==0) { ok++; continue; }
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (i < kFirstOptional) { reqFail++; ATERR("reg fail (required): %s", m[i].name); }
        else                    { ATLOG("not declared, skipped: %s", m[i].name); }
    }
    ATLOG("AudioTrack shim registered %d/%d (required failures=%d)", ok, n, reqFail);
    return reqFail==0?0:-1;
}

} // extern "C"
