/*
 * wl_looper_trace.c — WESTLAKE §283s
 *
 * §283q/§283r ruled out, by counters that never fired: the shim's 22 ANativeWindow_* entries,
 * 11 interposed EGL entries, 4 interposed GL entries (incl. glFinish), sync_wait(), and page
 * faults.  So the spinning RenderThread makes NO call out of libhwui/skia... except that hwui's
 * ThreadBase IS a Looper loop, and libhwui imports Looper::pollOnce UNDEFINED from libutils.
 * A RenderThread whose looper is polled with timeoutMillis == 0 in a tight loop is exactly
 * "state R, 100% CPU, stime-dominated, never caught in a syscall by /proc sampling".
 *
 * Interpose the C++ member function by its MANGLED name via an asm label, count calls, bucket by
 * timeout, and forward to the real implementation in libutils.so.
 *
 *   android::Looper::pollOnce(int timeoutMillis, int* outFd, int* outEvents, void** outData)
 *     -> _ZN7android6Looper8pollOnceEiPiS1_PPv
 */
#include <stdio.h>
#include <dlfcn.h>

static void *wl_utils_handle(void) {
    static void *h = (void *)0;
    static int tried = 0;
    if (!tried) {
        tried = 1;
        h = dlopen("libutils.so", 0x00002 /*RTLD_NOW*/ | 0x00100 /*RTLD_GLOBAL*/);
        fprintf(stderr, "[WESTLAKE-LOOPER] libutils handle=%p\n", h);
        fflush(stderr);
    }
    return h;
}

int wl_looper_pollOnce(void *self, int timeoutMillis, int *outFd, int *outEvents, void **outData)
    __asm__("_ZN7android6Looper8pollOnceEiPiS1_PPv");

int wl_looper_pollOnce(void *self, int timeoutMillis, int *outFd, int *outEvents, void **outData) {
    static unsigned long n = 0;
    static unsigned long zero_timeout = 0;
    static int (*real)(void *, int, int *, int *, void **) = 0;
    unsigned long v = ++n;
    if (timeoutMillis == 0) zero_timeout++;
    if (v == 1UL || v == 100UL || v == 1000UL || (v % 20000UL) == 0UL) {
        fprintf(stderr, "[WESTLAKE-LOOPER] pollOnce calls=%lu (timeout==0: %lu) latest_timeout=%d self=%p\n",
                v, zero_timeout, timeoutMillis, self);
        fflush(stderr);
    }
    if (!real) {
        void *h = wl_utils_handle();
        real = (int (*)(void *, int, int *, int *, void **))
                   (h ? dlsym(h, "_ZN7android6Looper8pollOnceEiPiS1_PPv") : (void *)0);
        if (!real) {
            fprintf(stderr, "[WESTLAKE-LOOPER] !! cannot resolve real pollOnce\n");
            fflush(stderr);
        }
    }
    if (!real) return -1;
    return real(self, timeoutMillis, outFd, outEvents, outData);
}
