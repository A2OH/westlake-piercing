// ============================================================================
// android_view_InputChannel.cpp
//
// JNI binding for android.view.InputChannel. Mirrors AOSP
// frameworks/base/core/jni/android_view_InputChannel.cpp.
//
// Java side (frameworks/base/core/java/android/view/InputChannel.java):
//   private static native long[] nativeOpenInputChannelPair(String name);   // (Ljava/lang/String;)[J
//   private static native long   nativeGetFinalizer();                       // ()J
//   private        native void   nativeDispose(long channel);                // (J)V
//   private        native long   nativeReadFromParcel(Parcel parcel);        // (Landroid/os/Parcel;)J
//   private        native void   nativeWriteToParcel(Parcel parcel, long ch);// (Landroid/os/Parcel;J)V
//   private        native long   nativeDup(long channel);                    // (J)J
//   private        native IBinder nativeGetToken(long channel);              // (J)Landroid/os/IBinder;
//   private        native String nativeGetName(long channel);                // (J)Ljava/lang/String;
//
// AOSP impl uses libgui InputTransport (socketpair-based fd transport between
// system_server and app for input event delivery).  OH has its own input
// subsystem (MMI / MultiModalInput) which the adapter partially routes through
// `framework/window/java/InputEventBridge.java` + `framework/core/jni/adapter_bridge.cpp`
// (Java_adapter_InputEventBridge_nativeRegisterInputChannel).  However, the
// android.view.InputChannel JNI itself has never been registered, so any
// AOSP code path that does `new InputChannel()` / `addToDisplay` / IPC parcel
// transport blows up with UnsatisfiedLinkError.
//
// Adapter strategy (NO_INPUT_EVENT_TRANSPORT baseline):
//   * Each InputChannel native side is an `OhInputChannel` struct holding only
//     a name (for debug) and a magic tag.  No actual socketpair, no fd, no
//     event transport.
//   * `nativeOpenInputChannelPair` returns a long[2] with two newly-allocated
//     OhInputChannel instances ("name (server)" + "name (client)").
//   * `nativeReadFromParcel` allocates an empty channel — the Parcel data is
//     ignored because adapter-side write/read is symmetric (both sides see
//     this same impl, and write is no-op).  Apps that traverse this path
//     don't actually need the data to be byte-correct, only that the result
//     is a non-zero pointer that survives to the dispose call.
//   * `nativeDup` allocates a fresh channel with the same name.
//   * `nativeGetToken` returns null — Java callers null-check.
//   * `nativeGetName` returns the cached UTF-8 name string.
//   * `nativeDispose` frees the struct (idempotent on null).
//   * `nativeGetFinalizer` returns a no-op free function pointer. NativeAllocationRegistry
//     does NOT treat 0 as no-op — it dereferences and calls the function pointer
//     directly, so returning 0 causes SIGSEGV when ReferenceQueueD daemon picks
//     up the Cleaner. (G2.14k 2026-05-01 fix.)
//   * `nativeWriteToParcel` is a no-op (we don't transport channels via
//     Parcel; the read side ignores the bytes anyway).
//
// This is sufficient for HelloWorld's ViewRootImpl → WindowSession.addToDisplay
// → InputChannel-pair-creation path to NOT crash.  Apps that depend on real
// input event delivery (touch, key events) won't see events; they will run
// without input until OH MMI hookup is wired.  That's an acceptable adapter
// state for the bring-up phase — the alternative (full MMI plumbing) is multi-
// week work and isn't in HelloWorld's critical path.
//
// Future P2: expose OhInputChannel.fd and back nativeReadFromParcel /
// nativeDup with a real OH MMI input event channel so touch/key events
// propagate from OH MMI service → app's main thread Choreographer.
// ============================================================================

#include "AndroidRuntime.h"

#include <jni.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

namespace android {
namespace {

// 2026-05-18 Phase 2: real socketpair backing for OH MMI input event delivery.
// Server side held by OHInputBridge (in liboh_adapter_bridge.so), client side
// held by InputEventReceiver (in this same so, liboh_android_runtime.so).
// fd == -1 for channels not paired (e.g., from-parcel / dup placeholders).
struct OhInputChannel {
    int32_t magic;       // 'OHIC' = 0x4F484943 — sanity-check tag
    int64_t id;          // unique id for debug logs
    std::string name;
    int fd;              // socketpair endpoint fd, or -1 if no real transport
};

constexpr int32_t kOhInputChannelMagic = 0x4F484943;
std::atomic<int64_t> g_channelId{1};

OhInputChannel* alloc_channel(const char* name) {
    auto* c = new OhInputChannel();
    c->magic = kOhInputChannelMagic;
    c->id = g_channelId.fetch_add(1);
    c->name = (name != nullptr) ? name : "";
    c->fd = -1;
    return c;
}

OhInputChannel* as_channel(jlong p) {
    auto* c = reinterpret_cast<OhInputChannel*>(p);
    if (!c || c->magic != kOhInputChannelMagic) return nullptr;
    return c;
}

// ----------------------------------------------------------------------------
// JNI methods.
// ----------------------------------------------------------------------------

jlongArray JNICALL
IC_nativeOpenInputChannelPair(JNIEnv* env, jclass /*clazz*/, jstring nameObj) {
    std::string baseName;
    if (nameObj != nullptr) {
        const char* utf = env->GetStringUTFChars(nameObj, nullptr);
        if (utf) {
            baseName = utf;
            env->ReleaseStringUTFChars(nameObj, utf);
        }
    }
    if (baseName.empty()) baseName = "anonymous";

    // 2026-05-18 Phase 2: real socketpair. AOSP InputTransport uses
    // AF_UNIX SOCK_SEQPACKET (boundary-preserving, reliable, message-mode).
    // SOCK_NONBLOCK + SOCK_CLOEXEC mirror AOSP InputChannel::openInputChannelPair.
    int fds[2] = {-1, -1};
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC,
                   0, fds) != 0) {
        fprintf(stderr, "[liboh_android_runtime] nativeOpenInputChannelPair: "
                "socketpair failed errno=%d (%s)\n", errno, strerror(errno));
        return nullptr;
    }

    OhInputChannel* server = alloc_channel((baseName + " (server)").c_str());
    OhInputChannel* client = alloc_channel((baseName + " (client)").c_str());
    server->fd = fds[0];
    client->fd = fds[1];

    jlongArray arr = env->NewLongArray(2);
    if (arr == nullptr) {
        ::close(fds[0]);
        ::close(fds[1]);
        delete server;
        delete client;
        return nullptr;
    }
    jlong slots[2] = {
        reinterpret_cast<jlong>(server),
        reinterpret_cast<jlong>(client),
    };
    env->SetLongArrayRegion(arr, 0, 2, slots);
    return arr;
}

// G2.14k (2026-05-01): MUST return a real function pointer, NOT 0.
// NativeAllocationRegistry.applyFreeFunction(freeFunction, nativePtr) is implemented
// as: `((void(*)(void*))freeFunction)(nativePtr)` — calling 0 triggers SIGSEGV at pc=0
// when the Cleaner daemon thread eventually processes the reference (typically
// ~25-30s after first GC cycle). Empirically observed: ReferenceQueueD SIGSEGV
// inside art_quick_generic_jni_trampoline after blx to NULL pointer.
static void IC_noop_free(void* /*p*/) {
    // Intentional no-op. OhInputChannel cleanup happens via explicit nativeDispose
    // call from Java, so the Cleaner finalizer path is informational only.
}

jlong JNICALL
IC_nativeGetFinalizer(JNIEnv* /*env*/, jclass /*clazz*/) {
    return reinterpret_cast<jlong>(&IC_noop_free);
}

void JNICALL
IC_nativeDispose(JNIEnv* /*env*/, jobject /*obj*/, jlong channel) {
    OhInputChannel* c = as_channel(channel);
    if (c != nullptr) {
        // Close socketpair endpoint if we own one. nativeRegisterInputChannel
        // (adapter side) dup()s the fd, so closing here doesn't break the
        // server-side bridge.
        if (c->fd >= 0) {
            ::close(c->fd);
            c->fd = -1;
        }
        delete c;
    }
}

jlong JNICALL
IC_nativeReadFromParcel(JNIEnv* /*env*/, jobject /*obj*/, jobject /*parcel*/) {
    // We don't actually transport channels via Parcel in adapter — both
    // write and read sides are no-ops/sentinels.  Allocate a fresh channel
    // with a placeholder name so subsequent nativeGetName / nativeDispose
    // work.
    OhInputChannel* c = alloc_channel("from-parcel");
    return reinterpret_cast<jlong>(c);
}

void JNICALL
IC_nativeWriteToParcel(JNIEnv* /*env*/, jobject /*obj*/,
                        jobject /*parcel*/, jlong /*channel*/) {
    // No-op — read side allocates fresh channel rather than deserializing.
}

jlong JNICALL
IC_nativeDup(JNIEnv* /*env*/, jobject /*obj*/, jlong channel) {
    OhInputChannel* c = as_channel(channel);
    const char* name = (c != nullptr) ? c->name.c_str() : "duplicate";
    OhInputChannel* dupCh = alloc_channel(name);
    // 2026-05-18 Phase 2: dup the underlying socketpair fd so the cloned
    // InputChannel object holds an independent fd referencing the same
    // socket endpoint. AOSP's InputChannel.copyTo() relies on this to
    // hand off a working channel to InputEventReceiver — previously this
    // stub returned a placeholder with fd=-1 which made our worker thread
    // bail out with "no client fd".
    if (c != nullptr && c->fd >= 0) {
        int newFd = ::dup(c->fd);
        if (newFd < 0) {
            fprintf(stderr, "[liboh_android_runtime] nativeDup: dup(fd=%d) failed "
                    "errno=%d (%s)\n", c->fd, errno, strerror(errno));
        } else {
            dupCh->fd = newFd;
        }
    }
    return reinterpret_cast<jlong>(dupCh);
}

jobject JNICALL
IC_nativeGetToken(JNIEnv* /*env*/, jobject /*obj*/, jlong /*channel*/) {
    // AOSP returns the IBinder identity token used by InputDispatcher to
    // match channels to windows.  Adapter has no InputDispatcher; return
    // null — Java callers null-check (see ViewRootImpl.setView et al.).
    return nullptr;
}

jstring JNICALL
IC_nativeGetName(JNIEnv* env, jobject /*obj*/, jlong channel) {
    OhInputChannel* c = as_channel(channel);
    const char* name = (c != nullptr) ? c->name.c_str() : "<invalid>";
    return env->NewStringUTF(name);
}

const JNINativeMethod kInputChannelMethods[] = {
    { "nativeOpenInputChannelPair", "(Ljava/lang/String;)[J",
      reinterpret_cast<void*>(IC_nativeOpenInputChannelPair) },
    { "nativeGetFinalizer", "()J",
      reinterpret_cast<void*>(IC_nativeGetFinalizer) },
    { "nativeDispose", "(J)V",
      reinterpret_cast<void*>(IC_nativeDispose) },
    { "nativeReadFromParcel", "(Landroid/os/Parcel;)J",
      reinterpret_cast<void*>(IC_nativeReadFromParcel) },
    { "nativeWriteToParcel", "(Landroid/os/Parcel;J)V",
      reinterpret_cast<void*>(IC_nativeWriteToParcel) },
    { "nativeDup", "(J)J",
      reinterpret_cast<void*>(IC_nativeDup) },
    { "nativeGetToken", "(J)Landroid/os/IBinder;",
      reinterpret_cast<void*>(IC_nativeGetToken) },
    { "nativeGetName", "(J)Ljava/lang/String;",
      reinterpret_cast<void*>(IC_nativeGetName) },
};

// Internal helper exposed as extern "C" below.
static int channel_fd_unchecked(jlong p) {
    auto* c = as_channel(p);
    return (c != nullptr) ? c->fd : -1;
}

}  // namespace

// ----------------------------------------------------------------------------
// 2026-05-18 Phase 2: cross-binary accessor.
//
// Adapter bridge (liboh_adapter_bridge.so) needs the server-side socketpair
// fd to write Android InputMessage bytes. Java InputChannel.mPtr exposes the
// OhInputChannel* but the struct definition lives in this .so (private to
// translation unit). Export this resolver so adapter_bridge can dlsym it and
// retrieve fd cleanly without duplicating the struct or going through Java
// reflection on a non-existent InputChannel.getFd() method.
//
// Returns -1 if the pointer is null / invalid / not yet paired.
// ----------------------------------------------------------------------------
extern "C" __attribute__((visibility("default")))
int oh_input_channel_get_fd(jlong native_ptr) {
    return android::channel_fd_unchecked(native_ptr);
}

int register_android_view_InputChannel(JNIEnv* env) {
    jclass clazz = env->FindClass("android/view/InputChannel");
    if (clazz == nullptr) {
        fprintf(stderr, "[liboh_android_runtime] register_android_view_InputChannel:"
                        " FindClass(android/view/InputChannel) failed\n");
        return -1;
    }
    jint rc = env->RegisterNatives(clazz, kInputChannelMethods,
                                    sizeof(kInputChannelMethods)
                                        / sizeof(kInputChannelMethods[0]));
    env->DeleteLocalRef(clazz);
    if (rc != JNI_OK) {
        fprintf(stderr, "[liboh_android_runtime] register_android_view_InputChannel:"
                        " RegisterNatives rc=%d\n", (int)rc);
        return -1;
    }
    fprintf(stderr, "[liboh_android_runtime] register_android_view_InputChannel:"
                    " 8 methods (NO_INPUT_EVENT_TRANSPORT baseline)\n");
    return 0;
}

}  // namespace android
