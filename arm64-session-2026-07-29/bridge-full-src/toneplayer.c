/* toneplayer.c - validate OHOS OH_AudioRenderer backend (the AudioTrack shim's target).
 * Plays a 440Hz stereo sine for ~5s. arm32 / dlopen libohaudio. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

typedef struct OH_AudioStreamBuilderStruct OH_AudioStreamBuilder;
typedef struct OH_AudioRendererStruct OH_AudioRenderer;
typedef struct {
    int32_t (*OnWriteData)(OH_AudioRenderer*, void*, void*, int32_t);
    int32_t (*OnStreamEvent)(OH_AudioRenderer*, void*, int32_t);
    int32_t (*OnInterruptEvent)(OH_AudioRenderer*, void*, int32_t, int32_t);
    int32_t (*OnError)(OH_AudioRenderer*, void*, int32_t);
} OH_AudioRenderer_Callbacks;
enum { TYPE_RENDERER = 1, FMT_S16LE = 1, USAGE_MUSIC = 1, LAT_NORMAL = 0 };

static double g_phase = 0.0;
static const double FREQ = 440.0, RATE = 48000.0;

static int32_t onWriteData(OH_AudioRenderer* r, void* u, void* buf, int32_t len) {
    (void)r; (void)u;
    int16_t* p = (int16_t*)buf;
    int frames = len / 4; /* 2ch * 2bytes */
    double inc = 2.0 * M_PI * FREQ / RATE;
    for (int i = 0; i < frames; i++) {
        int16_t s = (int16_t)(sin(g_phase) * 9000.0);
        p[i*2] = s; p[i*2+1] = s;
        g_phase += inc; if (g_phase > 2*M_PI) g_phase -= 2*M_PI;
    }
    return 0;
}

int main(void) {
    void* h = dlopen("/system/lib/ndk/libohaudio.so", RTLD_NOW);
    if (!h) { printf("dlopen fail: %s\n", dlerror()); return 1; }
    int32_t (*Create)(OH_AudioStreamBuilder**, int32_t)            = dlsym(h, "OH_AudioStreamBuilder_Create");
    int32_t (*SetRate)(OH_AudioStreamBuilder*, int32_t)            = dlsym(h, "OH_AudioStreamBuilder_SetSamplingRate");
    int32_t (*SetChan)(OH_AudioStreamBuilder*, int32_t)            = dlsym(h, "OH_AudioStreamBuilder_SetChannelCount");
    int32_t (*SetFmt)(OH_AudioStreamBuilder*, int32_t)             = dlsym(h, "OH_AudioStreamBuilder_SetSampleFormat");
    int32_t (*SetInfo)(OH_AudioStreamBuilder*, int32_t)           = dlsym(h, "OH_AudioStreamBuilder_SetRendererInfo");
    int32_t (*SetLat)(OH_AudioStreamBuilder*, int32_t)            = dlsym(h, "OH_AudioStreamBuilder_SetLatencyMode");
    int32_t (*SetCb)(OH_AudioStreamBuilder*, OH_AudioRenderer_Callbacks, void*) = dlsym(h, "OH_AudioStreamBuilder_SetRendererCallback");
    int32_t (*Gen)(OH_AudioStreamBuilder*, OH_AudioRenderer**)     = dlsym(h, "OH_AudioStreamBuilder_GenerateRenderer");
    int32_t (*Start)(OH_AudioRenderer*)                            = dlsym(h, "OH_AudioRenderer_Start");
    int32_t (*Stop)(OH_AudioRenderer*)                            = dlsym(h, "OH_AudioRenderer_Stop");
    int32_t (*Release)(OH_AudioRenderer*)                          = dlsym(h, "OH_AudioRenderer_Release");
    int32_t (*SetVol)(OH_AudioRenderer*, float)                    = dlsym(h, "OH_AudioRenderer_SetVolume");
    printf("syms: Create=%p Gen=%p Start=%p\n", (void*)Create, (void*)Gen, (void*)Start);
    if (!Create||!SetRate||!SetChan||!SetFmt||!Gen||!Start) { printf("missing syms\n"); return 2; }

    OH_AudioStreamBuilder* b = NULL;
    int rc = Create(&b, TYPE_RENDERER); printf("Create rc=%d b=%p\n", rc, (void*)b);
    if (rc != 0 || !b) return 3;
    SetRate(b, 48000); SetChan(b, 2); SetFmt(b, FMT_S16LE);
    if (SetInfo) printf("SetInfo rc=%d\n", SetInfo(b, USAGE_MUSIC));
    if (SetLat) SetLat(b, LAT_NORMAL);
    OH_AudioRenderer_Callbacks cbs; memset(&cbs, 0, sizeof cbs);
    cbs.OnWriteData = onWriteData;
    rc = SetCb(b, cbs, NULL); printf("SetCb rc=%d\n", rc);
    OH_AudioRenderer* r = NULL;
    rc = Gen(b, &r); printf("GenerateRenderer rc=%d r=%p\n", rc, (void*)r);
    if (rc != 0 || !r) return 4;
    if (SetVol) SetVol(r, 1.0f);
    rc = Start(r); printf("Start rc=%d -> playing 440Hz tone 5s...\n", rc);
    if (rc != 0) return 5;
    sleep(5);
    Stop(r); Release(r);
    printf("DONE (tone played)\n");
    return 0;
}
