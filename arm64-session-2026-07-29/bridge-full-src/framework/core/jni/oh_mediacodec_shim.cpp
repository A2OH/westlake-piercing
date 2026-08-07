// oh_mediacodec_shim.cpp  [FIX-AUDIO 2026-07-02]
// Bridge android.media.MediaCodec (audio decode) -> OHOS OH_AudioCodec
// (libnative_media_acodec.so), so ExoPlayer can decode noice's MP3 stream to
// PCM -> AudioTrack -> the OH_AudioRenderer shim -> speaker.
//
// OH_AudioCodec is async (onNeedInputBuffer / onNewOutputBuffer callbacks).
// ExoPlayer's default MediaCodecAdapter is SYNCHRONOUS (dequeueInputBuffer /
// dequeueOutputBuffer with timeout), so we buffer the async callbacks into
// queues and serve the sync dequeue calls from them.
//
// Registered onto android/media/MediaCodec + android/media/MediaCodecList via
// register_MediaCodec_shim(env), called from adapter_bridge.cpp JNI_OnLoad.

#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <cstring>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <pthread.h>
#include <condition_variable>
#include <chrono>
#include <string>
#include <atomic>

#define MCLOG(...) __android_log_print(ANDROID_LOG_INFO, "OH_MCShim", __VA_ARGS__)
#define MCERR(...) __android_log_print(ANDROID_LOG_ERROR, "OH_MCShim", __VA_ARGS__)

// ---- OH_AVCodec types (declared inline; resolved via dlopen) ----
typedef struct OH_AVCodec OH_AVCodec;
typedef struct OH_AVFormat OH_AVFormat;
typedef struct OH_AVBuffer OH_AVBuffer;
typedef struct OH_AVCodecBufferAttr { int64_t pts; int32_t size; int32_t offset; uint32_t flags; } OH_AVCodecBufferAttr;
enum { OH_FLAG_NONE = 0, OH_FLAG_EOS = 1, OH_FLAG_CODEC_DATA = 8 };
typedef void (*OH_OnError)(OH_AVCodec*, int32_t, void*);
typedef void (*OH_OnStreamChanged)(OH_AVCodec*, OH_AVFormat*, void*);
typedef void (*OH_OnNeedInputBuffer)(OH_AVCodec*, uint32_t, OH_AVBuffer*, void*);
typedef void (*OH_OnNewOutputBuffer)(OH_AVCodec*, uint32_t, OH_AVBuffer*, void*);
struct OH_AVCodecCallback { OH_OnError onError; OH_OnStreamChanged onStreamChanged; OH_OnNeedInputBuffer onNeedInputBuffer; OH_OnNewOutputBuffer onNewOutputBuffer; };

// ---- resolved OH functions ----
static OH_AVCodec* (*p_CreateByMime)(const char*, bool);
static int (*p_Destroy)(OH_AVCodec*);
static int (*p_RegisterCallback)(OH_AVCodec*, OH_AVCodecCallback, void*);
static int (*p_Configure)(OH_AVCodec*, const OH_AVFormat*);
static int (*p_Prepare)(OH_AVCodec*);
static int (*p_Start)(OH_AVCodec*);
static int (*p_Stop)(OH_AVCodec*);
static int (*p_Flush)(OH_AVCodec*);
static int (*p_PushInput)(OH_AVCodec*, uint32_t);
static int (*p_FreeOutput)(OH_AVCodec*, uint32_t);
static OH_AVBuffer* (*p_GetInputBuffer)(OH_AVCodec*, uint32_t);
static OH_AVBuffer* (*p_GetOutputBuffer)(OH_AVCodec*, uint32_t);
static OH_AVFormat* (*p_GetOutputDesc)(OH_AVCodec*);
static uint8_t* (*p_BufAddr)(OH_AVBuffer*);
static int32_t (*p_BufCap)(OH_AVBuffer*);
static int (*p_BufGetAttr)(OH_AVBuffer*, OH_AVCodecBufferAttr*);
static int (*p_BufSetAttr)(OH_AVBuffer*, const OH_AVCodecBufferAttr*);
static OH_AVFormat* (*p_FmtCreate)();
static void (*p_FmtDestroy)(OH_AVFormat*);
static bool (*p_FmtSetInt)(OH_AVFormat*, const char*, int32_t);
static bool (*p_FmtSetLong)(OH_AVFormat*, const char*, int64_t);
static bool (*p_FmtSetString)(OH_AVFormat*, const char*, const char*);
static bool (*p_FmtGetInt)(OH_AVFormat*, const char*, int32_t*);

static bool g_ok = false;
static void resolveOH() {
    static bool tried = false; if (tried) return; tried = true;
    void* a = dlopen("libnative_media_acodec.so", RTLD_NOW);
    if (!a) a = dlopen("/system/lib/ndk/libnative_media_acodec.so", RTLD_NOW);
    void* c = dlopen("libnative_media_core.so", RTLD_NOW);
    if (!c) c = dlopen("/system/lib/ndk/libnative_media_core.so", RTLD_NOW);
    if (!a || !c) { MCERR("dlopen acodec=%p core=%p: %s", a, c, dlerror()); return; }
    #define R(dst,lib,sym) dst = (decltype(dst))dlsym(lib, sym); if(!dst){ MCERR("missing %s", sym); }
    R(p_CreateByMime, a, "OH_AudioCodec_CreateByMime");
    R(p_Destroy, a, "OH_AudioCodec_Destroy");
    R(p_RegisterCallback, a, "OH_AudioCodec_RegisterCallback");
    R(p_Configure, a, "OH_AudioCodec_Configure");
    R(p_Prepare, a, "OH_AudioCodec_Prepare");
    R(p_Start, a, "OH_AudioCodec_Start");
    R(p_Stop, a, "OH_AudioCodec_Stop");
    R(p_Flush, a, "OH_AudioCodec_Flush");
    R(p_PushInput, a, "OH_AudioCodec_PushInputBuffer");
    R(p_FreeOutput, a, "OH_AudioCodec_FreeOutputBuffer");
    R(p_GetInputBuffer, a, "OH_AudioCodec_GetInputBuffer");
    R(p_GetOutputBuffer, a, "OH_AudioCodec_GetOutputBuffer");
    R(p_GetOutputDesc, a, "OH_AudioCodec_GetOutputDescription");
    R(p_BufAddr, c, "OH_AVBuffer_GetAddr");
    R(p_BufCap, c, "OH_AVBuffer_GetCapacity");
    R(p_BufGetAttr, c, "OH_AVBuffer_GetBufferAttr");
    R(p_BufSetAttr, c, "OH_AVBuffer_SetBufferAttr");
    R(p_FmtCreate, c, "OH_AVFormat_Create");
    R(p_FmtDestroy, c, "OH_AVFormat_Destroy");
    R(p_FmtSetInt, c, "OH_AVFormat_SetIntValue");
    R(p_FmtSetLong, c, "OH_AVFormat_SetLongValue");
    R(p_FmtSetString, c, "OH_AVFormat_SetStringValue");
    R(p_FmtGetInt, c, "OH_AVFormat_GetIntValue");
    #undef R
    g_ok = p_CreateByMime && p_Configure && p_Start && p_GetOutputBuffer && p_BufAddr;
    MCLOG("resolveOH ok=%d", g_ok);
}

// ---- per-codec state, stored in MediaCodec.mNativeContext (jlong) ----
struct Codec {
    OH_AVCodec* codec = nullptr;
    std::mutex mu; std::condition_variable cv;
    std::deque<uint32_t> inIdx;
    struct Out { uint32_t idx; OH_AVCodecBufferAttr attr; };
    std::deque<Out> outQ;
    // OH async delivers the OH_AVBuffer in the callback; retain it per index so
    // getBuffer/queueInput can use it (OH_AudioCodec_GetInputBuffer(idx) returns null).
    std::map<uint32_t, OH_AVBuffer*> inBufs, outBufs;
    // §519: the §510 per-index ByteBuffer cache was removed — see nGetBuffer.
    // (kept only as a marker of what NOT to reintroduce without re-reading AOSP MediaCodec.)
    // §510 note (obsolete):
    // android.media.MediaCodec keeps a per-index cache (mCachedInputBuffers/mCachedOutputBuffers)
    // and returns the SAME object for repeat getInputBuffer/getOutputBuffer(index) calls. Callers
    // rely on that: ExoPlayer's DefaultAudioSink.handleBuffer opens with
    //     Assertions.checkArgument(outputBuffer == null || buffer == outputBuffer);
    // which is an IDENTITY comparison. Minting a fresh NewDirectByteBuffer per call fails it and
    // throws a bare IllegalArgumentException (Assertions.checkArgument(boolean) passes no message —
    // which is exactly the message-less IAE seen here), before a single PCM byte is written.
    // releaseOutputBuffer then never runs, so the codec's output pool drains and cbOutput freezes.
    int32_t sampleRate = 44100, channels = 2;
    bool formatSent = false;   // emit INFO_OUTPUT_FORMAT_CHANGED once (sync path)
    bool started = false;
    volatile bool err = false;
    // async (setCallback) path — modern ExoPlayer uses AsynchronousMediaCodecAdapter
    jobject javaCodec = nullptr;   // global ref to the android.media.MediaCodec
    jobject cb = nullptr;          // global ref to MediaCodec$Callback
    bool asyncFmtSent = false;     // emit onOutputFormatChanged once (async path)
};

// MediaCodec dequeue return codes (android.media.MediaCodec constants)
enum { MC_TRY_AGAIN = -1, MC_OUTPUT_FORMAT_CHANGED = -2, MC_OUTPUT_BUFFERS_CHANGED = -3 };
enum { MC_FLAG_EOS = 4 };  // BUFFER_FLAG_END_OF_STREAM

// ---- async (setCallback) callback plumbing ----
static JavaVM* g_vm = nullptr;
static jmethodID cb_onInput = nullptr, cb_onOutput = nullptr, cb_onFormat = nullptr;
static jclass g_biClass = nullptr; static jmethodID g_biCtor = nullptr, g_biSet = nullptr;
static jclass g_mfClass = nullptr; static jmethodID g_mfCreateAudio = nullptr, g_mfSetInt = nullptr;
// The OH_AudioCodec callbacks run on native threads we must attach to the JVM.
// ART aborts if an attached thread exits without DetachCurrentThread, so we register
// a pthread TLS destructor that detaches when the OH callback thread exits.
static pthread_key_t g_detachKey;
static pthread_once_t g_keyOnce = PTHREAD_ONCE_INIT;
static void detachThread(void*) { if (g_vm) g_vm->DetachCurrentThread(); }
static void makeDetachKey() { pthread_key_create(&g_detachKey, detachThread); }
static JNIEnv* attachEnv() {
    if (!g_vm) return nullptr;
    JNIEnv* env = nullptr;
    if (g_vm->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_EDETACHED) {
        if (g_vm->AttachCurrentThread(&env, nullptr) != 0) return nullptr;
        pthread_once(&g_keyOnce, makeDetachKey);
        pthread_setspecific(g_detachKey, (void*)1);  // -> detachThread() on thread exit
    }
    return env;
}
// fire MediaCodec$Callback.onOutputFormatChanged once, then onOutputBufferAvailable
static void fireAsyncOutput(Codec* c, uint32_t idx, const OH_AVCodecBufferAttr& a) {
    if (!c->cb || !c->javaCodec) return;
    JNIEnv* env = attachEnv(); if (!env) return;
    if (!c->asyncFmtSent && cb_onFormat && g_mfCreateAudio) {
        c->asyncFmtSent = true;
        jstring raw = env->NewStringUTF("audio/raw");
        jobject mf = env->CallStaticObjectMethod(g_mfClass, g_mfCreateAudio, raw, c->sampleRate, c->channels);
        if (mf && g_mfSetInt) { jstring k = env->NewStringUTF("pcm-encoding"); env->CallVoidMethod(mf, g_mfSetInt, k, 2); env->DeleteLocalRef(k); }
        env->CallVoidMethod(c->cb, cb_onFormat, c->javaCodec, mf);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (mf) env->DeleteLocalRef(mf); env->DeleteLocalRef(raw);
    }
    if (cb_onOutput && g_biCtor) {
        jobject bi = env->NewObject(g_biClass, g_biCtor);
        env->CallVoidMethod(bi, g_biSet, a.offset, a.size, a.pts, (jint)((a.flags & OH_FLAG_EOS)?MC_FLAG_EOS:0));
        env->CallVoidMethod(c->cb, cb_onOutput, c->javaCodec, (jint)idx, bi);
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(bi);
    }
}
static void fireAsyncInput(Codec* c, uint32_t idx) {
    if (!c->cb || !c->javaCodec || !cb_onInput) return;
    JNIEnv* env = attachEnv(); if (!env) return;
    env->CallVoidMethod(c->cb, cb_onInput, c->javaCodec, (jint)idx);
    if (env->ExceptionCheck()) env->ExceptionClear();
}

static void cbError(OH_AVCodec*, int32_t e, void* u) { Codec* c=(Codec*)u; c->err=true; { std::lock_guard<std::mutex> l(c->mu);} c->cv.notify_all(); MCERR("OH codec error %d", e); }
static void cbStream(OH_AVCodec*, OH_AVFormat* f, void* u) {
    Codec* c=(Codec*)u; if (f && p_FmtGetInt) { int v; if(p_FmtGetInt(f,"sample_rate",&v)) c->sampleRate=v; if(p_FmtGetInt(f,"channel_count",&v)) c->channels=v; }
    MCLOG("OH streamChanged rate=%d ch=%d", c->sampleRate, c->channels);
}
static void cbInput(OH_AVCodec*, uint32_t idx, OH_AVBuffer* b, void* u) {
    Codec* c=(Codec*)u; static int n=0; if(n++<6) MCLOG("cbInput idx=%u buf=%p", idx, (void*)b);
    { std::lock_guard<std::mutex> l(c->mu); c->inIdx.push_back(idx); c->inBufs[idx]=b; } c->cv.notify_all();
    if (c->cb) fireAsyncInput(c, idx);   // async ExoPlayer path
}
static void cbOutput(OH_AVCodec*, uint32_t idx, OH_AVBuffer* b, void* u) {
    Codec* c=(Codec*)u; OH_AVCodecBufferAttr a{}; if(b&&p_BufGetAttr) p_BufGetAttr(b,&a);
    static long oN = 0; static long oBytes = 0;
    oN++; oBytes += (a.size > 0 ? a.size : 0);
    if (oN <= 3 || (oN % 250) == 0)
        MCLOG("cbOutput #%ld idx=%u size=%d totalBytes=%ld (OH produced PCM)", oN, idx, a.size, oBytes);
    { std::lock_guard<std::mutex> l(c->mu); c->outQ.push_back({idx,a}); c->outBufs[idx]=b; } c->cv.notify_all();
    if (c->cb) fireAsyncOutput(c, idx, a);   // async ExoPlayer path
}

// ---- MediaCodec.mNativeContext accessor ----
static jfieldID f_ctx = nullptr;
static Codec* getCodec(JNIEnv* env, jobject thiz) {
    if (!f_ctx) return nullptr;
    return (Codec*)(intptr_t)env->GetLongField(thiz, f_ctx);
}

// native_setup(String name, boolean nameIsType, boolean encoder, int pid, int uid)
static void nSetup(JNIEnv* env, jobject thiz, jstring jname, jboolean nameIsType, jboolean encoder, jint, jint) {
    resolveOH();
    const char* name = jname ? env->GetStringUTFChars(jname, nullptr) : "";
    MCLOG("native_setup name=%s isType=%d enc=%d", name, nameIsType, encoder);
    Codec* c = new Codec();
    // audio decoder only. Map android mime -> OH. noice = audio/mpeg (MP3).
    const char* mime = (name && strstr(name, "mp")) ? "audio/mpeg" : (name?name:"audio/mpeg");
    if (nameIsType) mime = name;                // createDecoderByType passes the mime
    if (g_ok) {
        c->codec = p_CreateByMime(mime, encoder ? true : false);
        if (c->codec) {
            OH_AVCodecCallback cb{ cbError, cbStream, cbInput, cbOutput };
            p_RegisterCallback(c->codec, cb, c);
        } else MCERR("CreateByMime(%s) failed", mime);
    }
    if (jname) env->ReleaseStringUTFChars(jname, name);
    if (f_ctx) env->SetLongField(thiz, f_ctx, (jlong)(intptr_t)c);
}

// native_configure(String[] keys, Object[] values, Surface, MediaCrypto, IHwBinder, int flags)
static void nConfigure(JNIEnv* env, jobject thiz, jobjectArray keys, jobjectArray vals, jobject, jobject, jobject, jint) {
    Codec* c = getCodec(env, thiz); if (!c || !c->codec) return;
    // parse sample-rate / channel-count from the MediaFormat keys/values
    int n = keys ? env->GetArrayLength(keys) : 0;
    jclass integerC = env->FindClass("java/lang/Integer");
    jmethodID intVal = env->GetMethodID(integerC, "intValue", "()I");
    for (int i = 0; i < n; i++) {
        jstring k = (jstring)env->GetObjectArrayElement(keys, i);
        const char* ks = env->GetStringUTFChars(k, nullptr);
        jobject v = env->GetObjectArrayElement(vals, i);
        if (v && env->IsInstanceOf(v, integerC)) {
            int iv = env->CallIntMethod(v, intVal);
            if (!strcmp(ks, "sample-rate")) c->sampleRate = iv;
            else if (!strcmp(ks, "channel-count")) c->channels = iv;
        }
        env->ReleaseStringUTFChars(k, ks);
        if (v) env->DeleteLocalRef(v); env->DeleteLocalRef(k);
    }
    MCLOG("configure rate=%d ch=%d", c->sampleRate, c->channels);
    if (g_ok && p_FmtCreate) {
        OH_AVFormat* f = p_FmtCreate();
        p_FmtSetInt(f, "sample_rate", c->sampleRate);
        p_FmtSetInt(f, "channel_count", c->channels);
        p_FmtSetInt(f, "audio_sample_format", 1 /*SAMPLE_S16LE*/);
        int rc = p_Configure(c->codec, f);
        p_FmtDestroy(f);
        MCLOG("OH Configure rc=%d", rc);
        if (p_Prepare) { int pr = p_Prepare(c->codec); MCLOG("OH Prepare rc=%d", pr); }
    }
}

static void nStart(JNIEnv* env, jobject thiz) {
    Codec* c = getCodec(env, thiz); if (!c || !c->codec) return;
    int sr = g_ok ? p_Start(c->codec) : -1; c->started = (sr == 0);
    MCLOG("OH Start rc=%d", sr);
}

static jint nDequeueInput(JNIEnv* env, jobject thiz, jlong timeoutUs) {
    Codec* c = getCodec(env, thiz); if (!c) return MC_TRY_AGAIN;
    static int dbg = 0;
    std::unique_lock<std::mutex> l(c->mu);
    if (c->inIdx.empty()) {
        if (timeoutUs == 0) { if (dbg++ < 8) MCLOG("dequeueIn: empty (TRY_AGAIN) inQ=%zu", c->inIdx.size()); return MC_TRY_AGAIN; }
        auto pred = [&]{ return !c->inIdx.empty() || c->err; };
        if (timeoutUs < 0) c->cv.wait(l, pred);
        else c->cv.wait_for(l, std::chrono::microseconds(timeoutUs), pred);
    }
    if (c->inIdx.empty()) { if (dbg++ < 8) MCLOG("dequeueIn: still empty after wait"); return MC_TRY_AGAIN; }
    uint32_t idx = c->inIdx.front(); c->inIdx.pop_front();
    if (dbg++ < 12) MCLOG("dequeueIn -> idx=%u", idx);
    return (jint)idx;
}

// getBuffer(boolean input, int index) -> ByteBuffer (direct, over OH_AVBuffer)
// §515: ByteBuffer.order(ByteOrder.nativeOrder()), resolved ONCE at registration rather than lazily
// in the hot path. AOSP's MediaCodec does the same thing in createByteBufferFromABuffer; java.nio
// specifies that every freshly created ByteBuffer — NewDirectByteBuffer included — is BIG_ENDIAN,
// while every caller assumes little-endian PCM. ExoPlayer asserts it outright:
//     if (outputBuffer == null) Assertions.checkArgument(buffer.order() == ByteOrder.LITTLE_ENDIAN);
// (DefaultAudioSink.handleBuffer, instruction [153] in the minified dex). checkArgument(boolean)
// throws IllegalArgumentException with NO message, which is why this was invisible for so long.
static jmethodID g_bbOrder = nullptr;
static jobject   g_nativeOrder = nullptr;   // global ref

static void resolveByteOrder(JNIEnv* env) {
    if (g_bbOrder != nullptr && g_nativeOrder != nullptr) return;
    jclass bbC = env->FindClass("java/nio/ByteBuffer");
    jclass boC = env->FindClass("java/nio/ByteOrder");
    if (bbC != nullptr && boC != nullptr) {
        g_bbOrder = env->GetMethodID(bbC, "order", "(Ljava/nio/ByteOrder;)Ljava/nio/ByteBuffer;");
        jmethodID nativeOrderM = env->GetStaticMethodID(boC, "nativeOrder", "()Ljava/nio/ByteOrder;");
        if (nativeOrderM != nullptr) {
            jobject no = env->CallStaticObjectMethod(boC, nativeOrderM);
            if (no != nullptr) { g_nativeOrder = env->NewGlobalRef(no); env->DeleteLocalRef(no); }
        }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (bbC != nullptr) env->DeleteLocalRef(bbC);
    if (boC != nullptr) env->DeleteLocalRef(boC);
    if (g_bbOrder == nullptr || g_nativeOrder == nullptr)
        MCERR("byte-order helpers unresolved; codec buffers would stay BIG_ENDIAN");
}

// Build the Java wrapper for a codec buffer. Calls INTO Java, so it must run with no shim lock held.
static jobject makeOrderedBuffer(JNIEnv* env, uint8_t* addr, int32_t cap) {
    jobject local = env->NewDirectByteBuffer(addr, cap);
    if (local == nullptr) return nullptr;
    // §519: native order is not decoration, it is the contract. Returning a BIG_ENDIAN buffer is the
    // exact defect §515 fixed, so a failure here must NOT be swallowed — returning the buffer anyway
    // would silently reinstate it. Leave any pending exception in flight for the caller.
    if (g_bbOrder == nullptr || g_nativeOrder == nullptr) {
        MCERR("byte-order helpers unresolved; refusing to hand out a BIG_ENDIAN codec buffer");
        env->DeleteLocalRef(local);
        return nullptr;
    }
    jobject reordered = env->CallObjectMethod(local, g_bbOrder, g_nativeOrder);
    if (env->ExceptionCheck()) {            // OOME / linkage / invocation failure — propagate it
        env->DeleteLocalRef(local);
        return nullptr;
    }
    if (reordered != nullptr) env->DeleteLocalRef(reordered);   // order() returns `this`
    return local;                            // local ref: MediaCodec owns the lifetime, not us
}

// getBuffer(boolean input, int index) -> ByteBuffer (direct, over OH_AVBuffer)
//
// §519: the §510 per-index ByteBuffer cache is REMOVED. It was built on a false premise — that
// android.media.MediaCodec returns the same object for repeat getInputBuffer/getOutputBuffer(index)
// calls. AOSP does the opposite:
//     ByteBuffer newBuffer = getBuffer(false, index);
//     invalidateByteBuffer(mCachedOutputBuffers, index);
//     mDequeuedOutputBuffers.put(index, newBuffer);
//     return newBuffer;
// i.e. it INVALIDATES the previous wrapper and hands back a new one. ExoPlayer's identity assertion
// (`outputBuffer == null || buffer == outputBuffer`) holds because the SINK retains the object it was
// given, not because the codec re-serves it.
//
// Caching therefore bought nothing and cost a great deal: a wrapper published after queue/release/
// flush could cover recycled memory, nStop invalidated nothing, flush cleared state before p_Flush so
// a callback could repopulate stale indices, and returning the stored global ref let another thread
// DeleteGlobalRef it out from under the caller. The real fix here was always §515 (byte order).
static jobject nGetBuffer(JNIEnv* env, jobject thiz, jboolean input, jint index) {
    Codec* c = getCodec(env, thiz); if (!c || !c->codec) return nullptr;
    OH_AVBuffer* b = nullptr;
    { std::lock_guard<std::mutex> l(c->mu);
      auto& m = input ? c->inBufs : c->outBufs; auto it = m.find((uint32_t)index); if (it != m.end()) b = it->second; }
    if (!b) b = input ? p_GetInputBuffer(c->codec, index) : p_GetOutputBuffer(c->codec, index);
    uint8_t* addr = b ? p_BufAddr(b) : nullptr; int32_t cap = b ? p_BufCap(b) : 0;
    // §511: running total, never a hard cap — a counter that cannot exceed its ceiling reads exactly
    // like a stall, which cost this bring-up days.
    static std::atomic<long> gbN{0};
    const long nth = ++gbN;
    if (nth <= 3 || (nth % 500) == 0)
        MCLOG("getBuffer #%ld in=%d idx=%d buf=%p addr=%p cap=%d", nth, input, index, (void*)b, (void*)addr, cap);
    if (!b || !addr || cap <= 0) return nullptr;
    return makeOrderedBuffer(env, addr, cap);
}

// §510: drop a cached ByteBuffer once its OH_AVBuffer goes back to the codec. Must be called with
// c->mu held, or from a path that owns the codec exclusively.

// native_queueInputBuffer(int index, int offset, int size, long pts, int flags)
static void nQueueInput(JNIEnv* env, jobject thiz, jint index, jint offset, jint size, jlong pts, jint flags) {
    Codec* c = getCodec(env, thiz); if (!c || !c->codec) return;
    static long qN = 0; static long qBytes = 0;
    qN++; qBytes += (size > 0 ? size : 0);
    if (qN <= 3 || (qN % 250) == 0)
        MCLOG("queueInput #%ld idx=%d size=%d flags=%d totalBytes=%ld (feeding MP3)", qN, index, size, flags, qBytes);
    OH_AVBuffer* b = nullptr;
    { std::lock_guard<std::mutex> l(c->mu);
      auto it = c->inBufs.find((uint32_t)index); if (it != c->inBufs.end()) b = it->second;
      c->inBufs.erase((uint32_t)index); }
    if (!b) b = p_GetInputBuffer(c->codec, index);
    if (b) { OH_AVCodecBufferAttr a{ pts, size, offset, (uint32_t)((flags & MC_FLAG_EOS)?OH_FLAG_EOS:OH_FLAG_NONE) }; p_BufSetAttr(b, &a); }
    p_PushInput(c->codec, (uint32_t)index);
}

static jfieldID bi_off=nullptr, bi_size=nullptr, bi_pts=nullptr, bi_flags=nullptr;
// native_dequeueOutputBuffer(BufferInfo info, long timeoutUs) -> int
static jint nDequeueOutput(JNIEnv* env, jobject thiz, jobject info, jlong timeoutUs) {
    Codec* c = getCodec(env, thiz); if (!c) return MC_TRY_AGAIN;
    static int dbg = 0;
    if (!c->formatSent) { c->formatSent = true; MCLOG("dequeueOut -> FORMAT_CHANGED"); return MC_OUTPUT_FORMAT_CHANGED; }
    std::unique_lock<std::mutex> l(c->mu);
    if (c->outQ.empty()) {
        if (dbg++ < 8) MCLOG("dequeueOut: outQ empty (TRY_AGAIN)");
        if (timeoutUs == 0) return MC_TRY_AGAIN;
        auto pred = [&]{ return !c->outQ.empty() || c->err; };
        if (timeoutUs < 0) c->cv.wait(l, pred);
        else c->cv.wait_for(l, std::chrono::microseconds(timeoutUs), pred);
    }
    if (c->outQ.empty()) return MC_TRY_AGAIN;
    Codec::Out o = c->outQ.front(); c->outQ.pop_front(); l.unlock();
    if (info && bi_off) {
        env->SetIntField(info, bi_off, o.attr.offset);
        env->SetIntField(info, bi_size, o.attr.size);
        env->SetLongField(info, bi_pts, o.attr.pts);
        env->SetIntField(info, bi_flags, (o.attr.flags & OH_FLAG_EOS) ? MC_FLAG_EOS : 0);
    }
    return (jint)o.idx;
}

// releaseOutputBuffer(int index, boolean render, boolean updatePTS, long time)
static void nReleaseOutput(JNIEnv* env, jobject thiz, jint index, jboolean, jboolean, jlong) {
    Codec* c = getCodec(env, thiz); if (!c || !c->codec) return;
    { std::lock_guard<std::mutex> l(c->mu);
      c->outBufs.erase((uint32_t)index); }
    p_FreeOutput(c->codec, (uint32_t)index);
}

// getOutputFormatNative(int index) / getFormatNative(boolean input) -> Map<String,Object>
static jobject buildFormatMap(JNIEnv* env, Codec* c) {
    jclass mapC = env->FindClass("java/util/HashMap");
    jobject m = env->NewObject(mapC, env->GetMethodID(mapC, "<init>", "()V"));
    jmethodID put = env->GetMethodID(mapC, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jclass intC = env->FindClass("java/lang/Integer");
    jmethodID intNew = env->GetStaticMethodID(intC, "valueOf", "(I)Ljava/lang/Integer;");
    auto putInt = [&](const char* k, int v){ jstring jk=env->NewStringUTF(k); jobject jv=env->CallStaticObjectMethod(intC,intNew,v); env->CallObjectMethod(m,put,jk,jv); env->DeleteLocalRef(jk); env->DeleteLocalRef(jv); };
    auto putStr = [&](const char* k, const char* v){ jstring jk=env->NewStringUTF(k); jstring jv=env->NewStringUTF(v); env->CallObjectMethod(m,put,jk,jv); env->DeleteLocalRef(jk); env->DeleteLocalRef(jv); };
    int sr=c?c->sampleRate:44100, ch=c?c->channels:2;
    if (c && c->codec && p_GetOutputDesc && p_FmtGetInt) { OH_AVFormat* f=p_GetOutputDesc(c->codec); if(f){ int v; if(p_FmtGetInt(f,"sample_rate",&v))sr=v; if(p_FmtGetInt(f,"channel_count",&v))ch=v; if(p_FmtDestroy)p_FmtDestroy(f);} }
    putStr("mime", "audio/raw");
    putInt("sample-rate", sr);
    putInt("channel-count", ch);
    putInt("pcm-encoding", 2 /*ENCODING_PCM_16BIT*/);
    return m;
}
static jobject nGetOutputFormat(JNIEnv* env, jobject thiz, jint) { MCLOG("getOutputFormatNative called"); return buildFormatMap(env, getCodec(env, thiz)); }
static jobject nGetFormat(JNIEnv* env, jobject thiz, jboolean in) { MCLOG("getFormatNative(input=%d) called", in); return buildFormatMap(env, getCodec(env, thiz)); }

static void nStop(JNIEnv* env, jobject thiz) { Codec* c=getCodec(env,thiz); if(c&&c->codec&&g_ok) p_Stop(c->codec); }
static void nFlush(JNIEnv* env, jobject thiz) { Codec* c=getCodec(env,thiz); if(c){ {std::lock_guard<std::mutex> l(c->mu); c->inIdx.clear(); c->outQ.clear(); c->inBufs.clear(); c->outBufs.clear();} if(c->codec&&g_ok) p_Flush(c->codec);} }
static void nRelease(JNIEnv* env, jobject thiz) { Codec* c=getCodec(env,thiz); if(c){ {std::lock_guard<std::mutex> l(c->mu);} if(c->codec&&g_ok) p_Destroy(c->codec); if(f_ctx) env->SetLongField(thiz,f_ctx,0); delete c; } }
static void nInit(JNIEnv*, jclass) {}
static void nReset(JNIEnv* env, jobject thiz) { nFlush(env, thiz); }
static void nSetCallback(JNIEnv* env, jobject thiz, jobject callback) {
    Codec* c = getCodec(env, thiz); if (!c) return;
    if (!g_vm) env->GetJavaVM(&g_vm);
    if (c->javaCodec) env->DeleteGlobalRef(c->javaCodec);
    if (c->cb) env->DeleteGlobalRef(c->cb);
    c->javaCodec = env->NewGlobalRef(thiz);
    c->cb = callback ? env->NewGlobalRef(callback) : nullptr;
    if (callback && !cb_onInput) {  // cache Callback + BufferInfo + MediaFormat IDs once
        jclass cbC = env->GetObjectClass(callback);
        cb_onInput  = env->GetMethodID(cbC, "onInputBufferAvailable", "(Landroid/media/MediaCodec;I)V");
        cb_onOutput = env->GetMethodID(cbC, "onOutputBufferAvailable", "(Landroid/media/MediaCodec;ILandroid/media/MediaCodec$BufferInfo;)V");
        cb_onFormat = env->GetMethodID(cbC, "onOutputFormatChanged", "(Landroid/media/MediaCodec;Landroid/media/MediaFormat;)V");
        jclass bi = env->FindClass("android/media/MediaCodec$BufferInfo");
        g_biClass = (jclass)env->NewGlobalRef(bi);
        g_biCtor = env->GetMethodID(bi, "<init>", "()V");
        g_biSet  = env->GetMethodID(bi, "set", "(IIJI)V");
        jclass mf = env->FindClass("android/media/MediaFormat");
        g_mfClass = (jclass)env->NewGlobalRef(mf);
        g_mfCreateAudio = env->GetStaticMethodID(mf, "createAudioFormat", "(Ljava/lang/String;II)Landroid/media/MediaFormat;");
        g_mfSetInt = env->GetMethodID(mf, "setInteger", "(Ljava/lang/String;I)V");
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    MCLOG("setCallback cb=%p (async mode)", (void*)callback);
}

// ================= MediaCodecList (report an MP3 decoder) =================
static jint mclCount(JNIEnv*, jclass) { return 1; }
static void mclInit(JNIEnv*, jclass) {}
static jstring mclGetName(JNIEnv* env, jclass, jint) { return env->NewStringUTF("OH.audio.mp3.decoder"); }
static jstring mclGetCanonical(JNIEnv* env, jclass, jint) { return env->NewStringUTF("OH.audio.mp3.decoder"); }
static jint mclGetAttributes(JNIEnv*, jclass, jint) { return 0; } // 0 = decoder (not encoder)
// Only our own component matches; anything else must miss, or callers resolve foreign codec names
// to index 0 and then query capabilities we never provide.
static jint mclFindByName(JNIEnv* env, jclass, jstring name) {
    if (name == nullptr) return -1;
    const char* n = env->GetStringUTFChars(name, nullptr);
    if (n == nullptr) return -1;
    int r = (strcmp(n, "OH.audio.mp3.decoder") == 0) ? 0 : -1;
    env->ReleaseStringUTFChars(name, n);
    return r;
}
// §497: cached at registration time. Looking java/lang/String up lazily inside the native was the
// actual crash: ART's NewObjectArray JniAbortF()s -- ABORTS the process, it does not throw -- if the
// element class is null, and FindClass can fail here because these natives run on the app's
// ExoPlayer:Playback thread, not the thread that registered them. Faultlog:
//   #07 art::JNI<false>::NewObjectArray(...)+500  <- JniAbortF -> Abort, Tid Name:ExoPlayer:Playb
static jclass g_strClass = nullptr;   // global ref

static jobjectArray mclSupportedTypes(JNIEnv* env, jclass, jint) {
    // Never enter ART's JNI with an exception pending: most calls abort rather than throw.
    if (env->ExceptionCheck()) env->ExceptionClear();
    jclass strC = g_strClass;
    if (strC == nullptr) {                       // registration-time caching did not happen
        strC = env->FindClass("java/lang/String");
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    if (strC == nullptr) { MCERR("no java/lang/String — returning empty type list"); return nullptr; }
    // ★AOSP's own pattern (frameworks/base/media/jni/android_media_MediaCodecList.cpp): allocate with
    // a NULL initializer, then SetObjectArrayElement. Passing the jstring as the initializer is what
    // aborted: in this libart, NewObjectArray+500 is the JniAbortF branch for an initializer ART
    // considers incompatible with the component class — and a JNI abort KILLS THE PROCESS rather than
    // throwing. Caching the class (my first attempt) did not touch that branch at all.
    jobjectArray arr = env->NewObjectArray(1, strC, nullptr);
    if (arr == nullptr) return nullptr;          // exception already pending; let it propagate
    jstring mime = env->NewStringUTF("audio/mpeg");
    if (mime == nullptr) return nullptr;
    env->SetObjectArrayElement(arr, 0, mime);
    env->DeleteLocalRef(mime);
    return arr;
}
// getCodecCapabilities(index, type) -> a minimal CodecCapabilities for the mime.
// ExoPlayer's MediaCodecUtil only queries isFeatureRequired/isFeatureSupported.
static jobject mclGetCaps(JNIEnv* env, jclass, jint, jstring type) {
    jclass ccC = env->FindClass("android/media/MediaCodecInfo$CodecCapabilities");
    if (!ccC) { if(env->ExceptionCheck())env->ExceptionClear(); return nullptr; }
    const char* t = type ? env->GetStringUTFChars(type, nullptr) : nullptr;
    jstring mime = env->NewStringUTF((t && *t) ? t : "audio/mpeg");
    if (type && t) env->ReleaseStringUTFChars(type, t);
    jmethodID cfpl = env->GetStaticMethodID(ccC, "createFromProfileLevel",
        "(Ljava/lang/String;II)Landroid/media/MediaCodecInfo$CodecCapabilities;");
    jobject cc = cfpl ? env->CallStaticObjectMethod(ccC, cfpl, mime, 0, 0) : nullptr;
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (!cc) {  // createFromProfileLevel returned null (mError) -> build a bare one
        jmethodID ctor0 = env->GetMethodID(ccC, "<init>", "()V");
        if (ctor0) {
            cc = env->NewObject(ccC, ctor0);
            if (env->ExceptionCheck()) env->ExceptionClear();
            jfieldID plF = env->GetFieldID(ccC, "profileLevels", "[Landroid/media/MediaCodecInfo$CodecProfileLevel;");
            if (plF && cc) {
                jclass plC = env->FindClass("android/media/MediaCodecInfo$CodecProfileLevel");
                if (env->ExceptionCheck()) env->ExceptionClear();
                if (plC != nullptr) {            // null element class -> NewObjectArray ABORTS
                    jobjectArray empty = env->NewObjectArray(0, plC, nullptr);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    if (empty != nullptr) env->SetObjectField(cc, plF, empty);
                }
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
    }
    MCLOG("getCodecCapabilities -> %p", (void*)cc);
    return cc;
}
static jobject mclGlobalSettings(JNIEnv* env, jclass) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    jclass mapC = env->FindClass("java/util/HashMap");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (mapC == nullptr) { MCERR("no HashMap — global settings null"); return nullptr; }
    jmethodID ctor = env->GetMethodID(mapC, "<init>", "()V");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (ctor == nullptr) return nullptr;         // NewObject with a null methodID also ABORTS
    jobject m = env->NewObject(mapC, ctor);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return m;
}

extern "C" int register_MediaCodec_shim(JNIEnv* env) {
    resolveByteOrder(env);   // §515: resolve once, not lazily in getBuffer
    jclass mc = env->FindClass("android/media/MediaCodec");
    if (!mc) { if(env->ExceptionCheck())env->ExceptionClear(); MCERR("no MediaCodec class"); return -1; }
    f_ctx = env->GetFieldID(mc, "mNativeContext", "J");
    if (!f_ctx) { if(env->ExceptionCheck())env->ExceptionClear(); MCERR("no mNativeContext"); }
    // BufferInfo field IDs
    jclass bi = env->FindClass("android/media/MediaCodec$BufferInfo");
    if (bi) { bi_off=env->GetFieldID(bi,"offset","I"); bi_size=env->GetFieldID(bi,"size","I"); bi_pts=env->GetFieldID(bi,"presentationTimeUs","J"); bi_flags=env->GetFieldID(bi,"flags","I"); }
    JNINativeMethod m[] = {
        {"native_init","()V",(void*)nInit},
        {"native_setup","(Ljava/lang/String;ZZII)V",(void*)nSetup},
        {"native_configure","([Ljava/lang/String;[Ljava/lang/Object;Landroid/view/Surface;Landroid/media/MediaCrypto;Landroid/os/IHwBinder;I)V",(void*)nConfigure},
        {"native_start","()V",(void*)nStart},
        {"native_stop","()V",(void*)nStop},
        {"native_flush","()V",(void*)nFlush},
        {"native_reset","()V",(void*)nReset},
        {"native_release","()V",(void*)nRelease},
        {"native_finalize","()V",(void*)nRelease},
        {"native_dequeueInputBuffer","(J)I",(void*)nDequeueInput},
        {"native_dequeueOutputBuffer","(Landroid/media/MediaCodec$BufferInfo;J)I",(void*)nDequeueOutput},
        {"native_queueInputBuffer","(IIIJI)V",(void*)nQueueInput},
        {"releaseOutputBuffer","(IZZJ)V",(void*)nReleaseOutput},
        {"getBuffer","(ZI)Ljava/nio/ByteBuffer;",(void*)nGetBuffer},
        {"getOutputFormatNative","(I)Ljava/util/Map;",(void*)nGetOutputFormat},
        {"getFormatNative","(Z)Ljava/util/Map;",(void*)nGetFormat},
        {"native_setCallback","(Landroid/media/MediaCodec$Callback;)V",(void*)nSetCallback},
    };
    int rc = env->RegisterNatives(mc, m, sizeof(m)/sizeof(m[0]));
    if (env->ExceptionCheck()) env->ExceptionClear();
    MCLOG("register MediaCodec rc=%d", rc);

    // §495: this half had NO logging, so when MediaCodec registered fine but MediaCodecList did not,
    // the shim still reported success and ExoPlayer went on to abort inside getSupportedTypes.
    jclass mcl = env->FindClass("android/media/MediaCodecList");
    if (env->ExceptionCheck()) env->ExceptionClear();
    MCLOG("FindClass(MediaCodecList) -> %p", (void*)mcl);
    if (mcl) {
        JNINativeMethod ml[] = {
            {"native_init","()V",(void*)mclInit},
            {"native_getCodecCount","()I",(void*)mclCount},
            {"getCodecName","(I)Ljava/lang/String;",(void*)mclGetName},
            {"getCanonicalName","(I)Ljava/lang/String;",(void*)mclGetCanonical},
            {"getAttributes","(I)I",(void*)mclGetAttributes},
            {"findCodecByName","(Ljava/lang/String;)I",(void*)mclFindByName},
            {"getSupportedTypes","(I)[Ljava/lang/String;",(void*)mclSupportedTypes},
            {"getCodecCapabilities","(ILjava/lang/String;)Landroid/media/MediaCodecInfo$CodecCapabilities;",(void*)mclGetCaps},
            {"native_getGlobalSettings","()Ljava/util/Map;",(void*)mclGlobalSettings},
        };
        if (g_strClass == nullptr) {
            jclass sc = env->FindClass("java/lang/String");
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (sc != nullptr) { g_strClass = (jclass)env->NewGlobalRef(sc); }
            MCLOG("cached java/lang/String -> %p", (void*)g_strClass);
        }
        int rc2 = env->RegisterNatives(mcl, ml, sizeof(ml)/sizeof(ml[0]));
        if (env->ExceptionCheck()) env->ExceptionClear();
        MCLOG("register MediaCodecList rc=%d (%d methods)", rc2, (int)(sizeof(ml)/sizeof(ml[0])));
        // A nonzero rc means a signature did not match; report which ones exist so the mismatch is
        // identifiable rather than silent.
        if (rc2 != 0) {
            for (size_t i = 0; i < sizeof(ml)/sizeof(ml[0]); i++) {
                int one = env->RegisterNatives(mcl, &ml[i], 1);
                if (env->ExceptionCheck()) env->ExceptionClear();
                MCLOG("  %s%s -> rc=%d", ml[i].name, ml[i].signature, one);
            }
        }
    } else {
        MCERR("MediaCodecList class NOT FOUND -- its natives stay unbound and ExoPlayer will abort");
    }
    return 0;
}
