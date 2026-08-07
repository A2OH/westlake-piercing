// ============================================================================
// libcore_io_Memory.cpp
//
// JNI bindings for libcore.io.Memory. Mirrors AOSP
// libcore/luni/src/main/native/libcore_io_Memory.cpp, which upstream lives in
// libjavacore.so — a library this port does not ship. libart binds part of
// libcore itself, but not this class, so the bulk copy primitives behind
// java.nio.DirectByteBuffer were simply absent.
//
// §504 (2026-08-04): this is what stopped noice from ever producing sound.
//
//   java.nio.DirectByteBuffer.put(byte[], int, int)
//       -> libcore.io.Memory.pokeByteArray(address, src, offset, length)
//
// is the copy ExoPlayer performs in MediaCodecRenderer.feedInputBuffer() to
// move an MP3 sample out of the extractor's byte[] and into the direct input
// ByteBuffer returned by MediaCodec.getInputBuffer(). Unbound, it raised
//
//   UnsatisfiedLinkError: No implementation found for
//     void libcore.io.Memory.pokeByteArray(long, byte[], int, int)
//
// on the "ExoPlayer:Playback" HandlerThread. ExoPlayerImplInternal.handleMessage
// catches ExoPlaybackException / IOException / RuntimeException — but NOT Error,
// so the UnsatisfiedLinkError escaped Looper.loop() and killed the thread. And
// because ThreadGroup.uncaughtException is a PFCUT no-op in this runtime, the
// death was never reported: the thread simply vanished. Downstream, the app
// had already taken input buffers 0..5 and now never returned any, so the OH
// decoder spun forever on
//
//   AVBufferQueue: (wait_for(), 314): wait for free buffer, timeout = 50000
//
// which is why the codec looked alive while queueInputBuffer stayed at zero.
//
// ★Registered one method at a time on purpose. RegisterNatives fails the WHOLE
// batch if any single method is missing from the class, and this runtime's
// libcore is a partial port — a batch call would let one absent method keep
// pokeByteArray unbound and silently reinstate the bug.
// ============================================================================

#include "AndroidRuntime.h"

#include <jni.h>
#include <cstdio>
#include <cstring>
#include <cstdint>

namespace android {

namespace {

inline void* addr(jlong a) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(a));
}

// DirectByteBuffer.put(byte[], int, int) — copy heap array -> native memory.
void Memory_pokeByteArray(JNIEnv* env, jclass /*clazz*/, jlong dstAddress,
                          jbyteArray src, jint offset, jint length) {
    env->GetByteArrayRegion(src, offset, length,
                            reinterpret_cast<jbyte*>(addr(dstAddress)));
}

// DirectByteBuffer.get(byte[], int, int) — copy native memory -> heap array.
// Needed on the way back out: DefaultAudioSink drains the codec's direct
// output buffer, so binding only the poke direction would just move the same
// UnsatisfiedLinkError one step later in the pipeline.
void Memory_peekByteArray(JNIEnv* env, jclass /*clazz*/, jlong srcAddress,
                          jbyteArray dst, jint dstOffset, jint byteCount) {
    env->SetByteArrayRegion(dst, dstOffset, byteCount,
                            reinterpret_cast<const jbyte*>(addr(srcAddress)));
}

// AOSP passes either a byte[] or a direct ByteBuffer here, so resolve both.
// Mirrors libcore's ScopedBytesRO/RW, minus the exception plumbing.
struct Bytes {
    JNIEnv* env;
    jbyteArray array = nullptr;
    jbyte* ptr = nullptr;

    Bytes(JNIEnv* e, jobject obj) : env(e) {
        if (obj == nullptr) return;
        jclass byteArrayClass = e->FindClass("[B");
        if (byteArrayClass != nullptr && e->IsInstanceOf(obj, byteArrayClass)) {
            array = reinterpret_cast<jbyteArray>(obj);
            ptr = e->GetByteArrayElements(array, nullptr);
        } else {
            ptr = reinterpret_cast<jbyte*>(e->GetDirectBufferAddress(obj));
        }
        if (byteArrayClass != nullptr) e->DeleteLocalRef(byteArrayClass);
    }
    ~Bytes() {
        if (array != nullptr && ptr != nullptr) {
            env->ReleaseByteArrayElements(array, ptr, 0);
        }
    }
};

void Memory_memmove(JNIEnv* env, jclass /*clazz*/, jobject dstObject, jint dstOffset,
                    jobject srcObject, jint srcOffset, jlong byteCount) {
    Bytes dst(env, dstObject);
    if (dst.ptr == nullptr) return;
    Bytes src(env, srcObject);
    if (src.ptr == nullptr) return;
    memmove(dst.ptr + dstOffset, src.ptr + srcOffset, static_cast<size_t>(byteCount));
}

struct MemoryMethod {
    const char* name;
    const char* sig;
    void* fn;
    bool essential;   // §523: binding failure here must fail the whole registration
};

const MemoryMethod kMemoryMethods[] = {
    // pokeByteArray is the whole reason this file exists: DirectByteBuffer.put() has no other
    // implementation here, and silently continuing without it reinstates the original bug.
    { "pokeByteArray", "(J[BII)V", reinterpret_cast<void*>(Memory_pokeByteArray), true },
    { "peekByteArray", "(J[BII)V", reinterpret_cast<void*>(Memory_peekByteArray), true },
    { "memmove", "(Ljava/lang/Object;ILjava/lang/Object;IJ)V",
      reinterpret_cast<void*>(Memory_memmove), false },
};

}  // namespace

int register_libcore_io_Memory(JNIEnv* env) {
    jclass clazz = env->FindClass("libcore/io/Memory");
    if (clazz == nullptr) {
        env->ExceptionClear();
        fprintf(stderr, "[WESTLAKE-504] libcore/io/Memory not found; skipping\n");
        return -1;
    }

    int bound = 0, essentialFail = 0;
    for (const MemoryMethod& m : kMemoryMethods) {
        JNINativeMethod one = { m.name, m.sig, m.fn };
        if (env->RegisterNatives(clazz, &one, 1) == JNI_OK) {
            ++bound;
        } else if (m.essential) {
            env->ExceptionClear();
            ++essentialFail;
            fprintf(stderr, "[WESTLAKE-523] ESSENTIAL Memory.%s%s FAILED to bind\n", m.name, m.sig);
        } else {
            // Absent or already-satisfied methods are not fatal: the point of
            // registering singly is that one failure cannot mask the others.
            env->ExceptionClear();
            fprintf(stderr, "[WESTLAKE-504] Memory.%s%s not registered\n", m.name, m.sig);
        }
    }
    fprintf(stderr, "[WESTLAKE-504] libcore.io.Memory bound %d/%d (essential failures=%d)\n",
            bound, static_cast<int>(sizeof(kMemoryMethods) / sizeof(kMemoryMethods[0])),
            essentialFail);

    env->DeleteLocalRef(clazz);
    // §523: "bound > 0" reported success even when pokeByteArray — the only method that matters —
    // had failed. Success now requires every essential method.
    return essentialFail == 0 ? 0 : -1;
}

}  // namespace android
