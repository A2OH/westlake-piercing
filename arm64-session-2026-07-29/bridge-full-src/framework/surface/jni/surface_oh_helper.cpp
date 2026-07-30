/*
 * surface_oh_helper.cpp — P13.2.b helper
 *
 * Provides createInProcessProducer() — builds an OH producer Surface backed by
 * an in-process consumer. The dmabuf round-trip works (RequestBuffer/FlushBuffer
 * succeed, virAddr is mmapped), but the queued buffers go to an in-process
 * consumer that we ignore — pixels are not visible on screen.
 *
 * This bypasses oh_surface_bridge.cpp (which would create an RSSurfaceNode that
 * eventually displays via RenderService) because that file pulls in the entire
 * RS client header chain which has broken Skia includes (skcms.h at wrong path).
 *
 * Wiring to actual display = P13.2.c (requires WindowManagerService → SceneSession
 * → RSSurfaceNode integration that's beyond P13.2.b scope).
 */
#include "iconsumer_surface.h"
#include "surface.h"
#include "ibuffer_producer.h"
#include "surface_buffer.h"
#include "ibuffer_consumer_listener.h"
#include "sync_fence.h"
#include "surface_type.h"
#include "external_window.h"   // OHNativeWindow + OH_NativeWindow_NativeWindowHandleOpt
#include <android/log.h>
#include <cstdio>
#include <cstring>

#define LOG_TAG "OH_SurfaceHelper"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" {

// Returns an OHOS::Surface* (raw pointer wrapped via OHOS::sptr internally).
// Caller stores the sptr via the helpers below to keep ref count alive.
// Returns nullptr on failure.
void* surface_oh_create_in_process_producer(const char* name) {
    auto consumer = OHOS::IConsumerSurface::Create(name ? name : "OHAdapterSurface");
    if (!consumer) {
        LOGE("create_in_process_producer: IConsumerSurface::Create failed");
        return nullptr;
    }

    // Hold the consumer alive in a static map keyed by the producer pointer,
    // because once the consumer is dropped the producer becomes useless.
    // We leak the consumer ref intentionally — Surface lifetime is tied to
    // the JNI Surface object lifetime which we don't always have visibility into.
    OHOS::sptr<OHOS::IBufferProducer> producerIface = consumer->GetProducer();
    if (!producerIface) {
        LOGE("create_in_process_producer: GetProducer failed");
        return nullptr;
    }

    OHOS::sptr<OHOS::Surface> producer = OHOS::Surface::CreateSurfaceAsProducer(producerIface);
    if (!producer) {
        LOGE("create_in_process_producer: CreateSurfaceAsProducer failed");
        return nullptr;
    }

    // Set queue size for triple buffering
    producer->SetQueueSize(3);

    // Pin both refs by leaking sptr's increment.
    // We add a ref then return the raw pointer; caller manages release via _release.
    OHOS::Surface* raw = producer.GetRefPtr();
    raw->IncStrongRef(nullptr);
    consumer->IncStrongRef(nullptr);  // pin consumer

    LOGI("create_in_process_producer: ok, name=%s", name);
    return raw;
}

void surface_oh_release_producer(void* producerRaw) {
    if (!producerRaw) return;
    auto* p = reinterpret_cast<OHOS::Surface*>(producerRaw);
    p->DecStrongRef(nullptr);
}

}  // extern "C"

/* =====================================================================
 * AImageReader equivalent (OHOS primitives) — off-screen GPU render-target
 * readback, used by libhwui ThreadedRenderer.createHardwareBitmap().
 *
 * Android's createHardwareBitmap renders a RenderNode into an AImageReader's
 * producer ANativeWindow, then reads the result back as an AHardwareBuffer.
 * OHOS has no AImageReader, so this provides the same capability with OHOS
 * primitives: an in-process IConsumerSurface gives a producer Surface (wrapped
 * to an AOSP-ABI ANativeWindow via oh_anw_wrap, exactly like the on-screen
 * window path). hwui renders+swaps into it; we AcquireBuffer on the consumer
 * side, wait the GPU fence, and expose the dmabuf's mapped CPU pixels so the
 * caller can build a Bitmap. CPU readback (vs. an EGLImage GPU import) keeps
 * this robust and decoupled from Skia's hardware-buffer import path.
 * ===================================================================== */
extern "C" {
void* CreateNativeWindowFromSurface(void* pSurface);                 // libnative_window
void* oh_anw_wrap(void* ohNativeWindow);                             // oh_anativewindow_shim.cpp
void  oh_anw_destroy(struct ANativeWindow* aosp);                    // oh_anativewindow_shim.cpp
}
// OH_NativeWindow_NativeWindowHandleOpt(OHNativeWindow*, int, ...) comes from external_window.h

namespace {
class OhReadbackListener : public OHOS::IBufferConsumerListener {
public:
    void OnBufferAvailable() override {}   // we poll via AcquireBuffer post-render
};
struct OhImageReader {
    OHOS::sptr<OHOS::IConsumerSurface>        consumer;
    OHOS::sptr<OHOS::Surface>                 producer;
    OHOS::sptr<OHOS::IBufferConsumerListener> listener;
    OHOS::sptr<OHOS::SurfaceBuffer>           acquired;
    void* ohnw = nullptr;       // OH NativeWindow (CreateNativeWindowFromSurface)
    void* aospWindow = nullptr; // AOSP-ABI ANativeWindow (oh_anw_wrap)
    int   width = 0, height = 0, format = 0;
};
constexpr uint64_t kReadbackUsage =
    OHOS::BUFFER_USAGE_CPU_READ | OHOS::BUFFER_USAGE_MEM_DMA |
    OHOS::BUFFER_USAGE_HW_RENDER | OHOS::BUFFER_USAGE_HW_TEXTURE;
}  // namespace

extern "C" {

// Create an off-screen render target. Returns an opaque handle or null.
void* oh_imagereader_create(int32_t width, int32_t height, int32_t format, uint64_t usage) {
    if (width <= 0 || height <= 0) return nullptr;
    auto consumer = OHOS::IConsumerSurface::Create("oh-hwbitmap-readback");
    if (!consumer) { LOGE("imagereader_create: IConsumerSurface::Create failed"); return nullptr; }
    auto* r = new OhImageReader();
    r->consumer = consumer;
    r->listener = new OhReadbackListener();
    OHOS::sptr<OHOS::IBufferConsumerListener> l = r->listener;
    consumer->RegisterConsumerListener(l);
    consumer->SetDefaultWidthAndHeight(width, height);
    consumer->SetDefaultUsage(usage ? usage : kReadbackUsage);
    auto producerIface = consumer->GetProducer();
    if (!producerIface) { LOGE("imagereader_create: GetProducer failed"); delete r; return nullptr; }
    r->producer = OHOS::Surface::CreateSurfaceAsProducer(producerIface);
    if (!r->producer) { LOGE("imagereader_create: CreateSurfaceAsProducer failed"); delete r; return nullptr; }
    r->producer->SetQueueSize(3);
    r->width = width; r->height = height; r->format = format;
    LOGI("imagereader_create: ok %dx%d fmt=%d", width, height, format);
    return r;
}

// Get the AOSP-ABI ANativeWindow (producer) for hwui to render into.
void* oh_imagereader_get_window(void* handle) {
    auto* r = reinterpret_cast<OhImageReader*>(handle);
    if (!r || !r->producer) return nullptr;
    void* nw = CreateNativeWindowFromSurface(&r->producer);
    if (!nw) { LOGE("imagereader_get_window: CreateNativeWindowFromSurface failed"); return nullptr; }
    OH_NativeWindow_NativeWindowHandleOpt(reinterpret_cast<OHNativeWindow*>(nw), 3 /*SET_FORMAT*/, 12 /*RGBA_8888*/);
    OH_NativeWindow_NativeWindowHandleOpt(reinterpret_cast<OHNativeWindow*>(nw), 5 /*SET_USAGE*/, (uint64_t)kReadbackUsage);
    r->ohnw = nw;
    r->aospWindow = oh_anw_wrap(nw);
    if (!r->aospWindow) { LOGE("imagereader_get_window: oh_anw_wrap failed"); return nullptr; }
    LOGI("imagereader_get_window: aospWindow=%p oh=%p", r->aospWindow, nw);
    return r->aospWindow;
}

// Acquire the rendered frame; returns mapped CPU pixel pointer (RGBA_8888) or null.
// Fills *outW/*outH (px) and *outStride (bytes per row of the dmabuf).
void* oh_imagereader_acquire(void* handle, int32_t* outW, int32_t* outH, int32_t* outStride) {
    auto* r = reinterpret_cast<OhImageReader*>(handle);
    if (!r || !r->consumer) return nullptr;
    OHOS::sptr<OHOS::SurfaceBuffer> buf;
    int32_t fenceFd = -1;
    int64_t ts = 0;
    OHOS::Rect damage = {0, 0, 0, 0};
    OHOS::GSError ret = r->consumer->AcquireBuffer(buf, fenceFd, ts, damage);
    if (static_cast<int>(ret) != 0 || buf == nullptr) {
        LOGE("imagereader_acquire: AcquireBuffer ret=%d", static_cast<int>(ret));
        return nullptr;
    }
    if (fenceFd >= 0) {
        OHOS::sptr<OHOS::SyncFence> f = new OHOS::SyncFence(fenceFd);  // takes ownership of fd
        f->Wait(3000);  // wait for GPU render completion before CPU read
    }
    r->acquired = buf;
    void* va = buf->GetVirAddr();
    if (!va) { buf->Map(); va = buf->GetVirAddr(); }
    if (outW)      *outW = buf->GetWidth();
    if (outH)      *outH = buf->GetHeight();
    if (outStride) *outStride = buf->GetStride();
    LOGI("imagereader_acquire: buf %dx%d stride=%d va=%p", buf->GetWidth(), buf->GetHeight(), buf->GetStride(), va);
    return va;
}

void oh_imagereader_destroy(void* handle) {
    auto* r = reinterpret_cast<OhImageReader*>(handle);
    if (!r) return;
    if (r->acquired != nullptr && r->consumer != nullptr) {
        r->consumer->ReleaseBuffer(r->acquired, -1);  // legacy single-slot path
    }
    // Free the native-window wrappers created in oh_imagereader_get_window, else they leak
    // once per reader (createHardwareBitmap makes a fresh reader for every bitmap).
    if (r->aospWindow) oh_anw_destroy(reinterpret_cast<struct ANativeWindow*>(r->aospWindow));
    if (r->ohnw) OH_NativeWindow_DestroyNativeWindow(reinterpret_cast<OHNativeWindow*>(r->ohnw));
    delete r;  // sptr members auto-release
}

}  // extern "C"

/* =====================================================================
 * AImageReader NDK ABI shim — real implementation backed by the OHOS
 * IConsumerSurface readback above. Bridges android/media AImageReader_* /
 * AImage_* to OHOS so the stock AImageReader path works (hwui
 * createHardwareBitmap, and any other AImageReader NDK user) instead of the
 * compile-time no-op header stubs that left the window uninitialized.
 * CPU readback only — the GPU AHardwareBuffer import path (SkSurfaces::
 * WrapAndroidHardwareBuffer / SkImages::DeferredFromAHardwareBuffer) is
 * stubbed on OHOS, so AImage_getHardwareBuffer reports unsupported; callers
 * read pixels via AImage_getPlaneData.
 * ===================================================================== */
struct ANativeWindow;
struct AHardwareBuffer;
typedef int oh_media_status_t;
// Opaque NDK handles ARE these structs (callers only hold pointers). Each AImage OWNS its
// acquired OHOS buffer (holds an sptr ref to the consumer so ReleaseBuffer is valid even if
// the reader is deleted first), so multiple outstanding images are independent and there is
// no shared single-buffer slot to clobber.
struct AImageReader { void* oh; int32_t w, h, fmt; };
struct AImage {
    OHOS::sptr<OHOS::IConsumerSurface> consumer;   // ref keeps the consumer alive for ReleaseBuffer
    OHOS::sptr<OHOS::SurfaceBuffer>    buf;         // this image's own acquired buffer
    void* va; int32_t w, h, stride, fmt;
};

extern "C" {

oh_media_status_t AImageReader_newWithUsage(int32_t width, int32_t height, int32_t format,
                                            uint64_t usage, int32_t /*maxImages*/,
                                            AImageReader** reader) {
    if (!reader || width <= 0 || height <= 0) return -10000;
    if (format != 1 /*AIMAGE_FORMAT_RGBA_8888*/) {   // only RGBA_8888 CPU readback is implemented
        LOGE("AImageReader_newWithUsage: unsupported format %d (only RGBA_8888)", format);
        return -10000;
    }
    void* oh = oh_imagereader_create(width, height, 12 /*OHOS PIXEL_FMT_RGBA_8888*/, usage);
    if (!oh) { LOGE("AImageReader_newWithUsage: oh_imagereader_create failed"); return -10000; }
    *reader = new AImageReader{oh, width, height, format};
    LOGI("AImageReader_newWithUsage: ok %dx%d fmt=%d", width, height, format);
    return 0;  // AMEDIA_OK
}

oh_media_status_t AImageReader_new(int32_t width, int32_t height, int32_t format,
                                   int32_t maxImages, AImageReader** reader) {
    return AImageReader_newWithUsage(width, height, format, 0, maxImages, reader);
}

void AImageReader_delete(AImageReader* reader) {
    if (!reader) return;
    if (reader->oh) oh_imagereader_destroy(reader->oh);
    delete reader;
}

oh_media_status_t AImageReader_getWindow(AImageReader* reader, ANativeWindow** window) {
    if (!reader || !window) return -10000;
    void* w = oh_imagereader_get_window(reader->oh);
    if (!w) { LOGE("AImageReader_getWindow: failed"); return -10000; }
    *window = reinterpret_cast<ANativeWindow*>(w);
    return 0;
}

// Acquire directly off the reader's consumer so each AImage owns its own buffer + release.
static oh_media_status_t ohshim_acquire(AImageReader* reader, AImage** image) {
    if (!reader || !image) return -10000;
    auto* r = reinterpret_cast<OhImageReader*>(reader->oh);
    if (!r || !r->consumer) return -10000;
    OHOS::sptr<OHOS::SurfaceBuffer> buf;
    int32_t fenceFd = -1; int64_t ts = 0; OHOS::Rect damage = {0, 0, 0, 0};
    OHOS::GSError ret = r->consumer->AcquireBuffer(buf, fenceFd, ts, damage);
    if (static_cast<int>(ret) != 0 || buf == nullptr) { LOGE("AImage acquire: AcquireBuffer ret=%d", static_cast<int>(ret)); return -10000; }
    if (fenceFd >= 0) {
        OHOS::sptr<OHOS::SyncFence> f = new OHOS::SyncFence(fenceFd);  // takes fd ownership
        f->Wait(3000);  // wait GPU render completion before CPU read
    }
    void* va = buf->GetVirAddr();
    if (!va) { buf->Map(); va = buf->GetVirAddr(); }
    if (!va) { r->consumer->ReleaseBuffer(buf, -1); LOGE("AImage acquire: map failed"); return -10000; }
    *image = new AImage{r->consumer, buf, va, buf->GetWidth(), buf->GetHeight(), buf->GetStride(), reader->fmt};
    return 0;
}
oh_media_status_t AImageReader_acquireNextImage(AImageReader* reader, AImage** image)   { return ohshim_acquire(reader, image); }
oh_media_status_t AImageReader_acquireLatestImage(AImageReader* reader, AImage** image) { return ohshim_acquire(reader, image); }

void AImage_delete(AImage* image) {
    if (!image) return;
    if (image->consumer != nullptr && image->buf != nullptr)
        image->consumer->ReleaseBuffer(image->buf, -1);   // this image's own buffer
    delete image;
}

oh_media_status_t AImage_getPlaneData(const AImage* image, int planeIdx, uint8_t** data, int* dataLength) {
    if (!image || !data || !dataLength || planeIdx != 0) return -10000;   // RGBA_8888 is single-plane
    *data = reinterpret_cast<uint8_t*>(image->va);
    *dataLength = image->stride * image->h;
    return 0;
}
oh_media_status_t AImage_getPlaneRowStride(const AImage* image, int planeIdx, int32_t* rowStride) {
    if (!image || !rowStride || planeIdx != 0) return -10000; *rowStride = image->stride; return 0;
}
oh_media_status_t AImage_getWidth(const AImage* image, int32_t* w)  { if (!image || !w) return -10000; *w = image->w; return 0; }
oh_media_status_t AImage_getHeight(const AImage* image, int32_t* h) { if (!image || !h) return -10000; *h = image->h; return 0; }
oh_media_status_t AImage_getFormat(const AImage* image, int32_t* f) { if (!image || !f) return -10000; *f = image->fmt; return 0; }
oh_media_status_t AImage_getHardwareBuffer(const AImage* /*image*/, AHardwareBuffer** buffer) {
    // OHOS GPU AHardwareBuffer import is stubbed; callers must use AImage_getPlaneData (CPU).
    if (buffer) *buffer = nullptr;
    return -10000;
}

}  // extern "C"
