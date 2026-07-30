/*
 * wl_egl_trace.c — WESTLAKE §283q
 *
 * hwui's RenderThread spins at ~100% CPU (utime+182/stime+436 ticks per 6s) holding the mutex
 * the UI thread waits on, and it logs nothing.  libhwui CANNOT be rebuilt on this board, so it
 * cannot be instrumented directly — but it imports 26 EGL symbols as UNDEFINED, and this shim
 * is FIRST in its DT_NEEDED (the same shadowing that caused the §283m bug).  So defining them
 * here interposes them ahead of libEGL.
 *
 * Every wrapper counts its calls and forwards to the real libEGL entry point resolved lazily by
 * dlsym.  Whichever counter runs away IS the spin.  Prime suspect: eglClientWaitSyncKHR — if a
 * fence never signals and hwui polls it with a zero timeout, that is exactly 100% CPU with high
 * system time and no logging.
 *
 * ★Forwarding must never fail silently: if the real symbol cannot be resolved we say so once,
 * loudly, and return a benign value.
 */
#include <stdint.h>
#include <stdio.h>
#include <dlfcn.h>

static void *wl_egl_handle(void) {
    static void *h = (void *)0;
    static int tried = 0;
    if (!tried) {
        tried = 1;
        h = dlopen("libEGL.so", 0x00002 /*RTLD_NOW*/ | 0x00100 /*RTLD_GLOBAL*/);
        if (!h) h = dlopen("libEGL_impl.so", 0x00002);
        fprintf(stderr, "[WESTLAKE-EGLTRACE] libEGL handle=%p\n", h);
        fflush(stderr);
    }
    return h;
}

static void *wl_egl_sym(const char *name) {
    void *h = wl_egl_handle();
    void *p = h ? dlsym(h, name) : (void *)0;
    if (!p) {
        fprintf(stderr, "[WESTLAKE-EGLTRACE] !! could not resolve %s\n", name);
        fflush(stderr);
    }
    return p;
}

/* Per-symbol call counter; prints on a geometric-ish schedule so a runaway is obvious
 * without drowning the log. */
static void wl_tick(const char *name, unsigned long *n) {
    unsigned long v = ++(*n);
    if (v == 1UL || v == 100UL || v == 1000UL || (v % 20000UL) == 0UL) {
        fprintf(stderr, "[WESTLAKE-EGLTRACE] %s calls=%lu\n", name, v);
        fflush(stderr);
    }
}

#define WL_EGL_FWD(ret, name, params, args, deflt)                       \
    ret name params {                                                    \
        static unsigned long n = 0;                                      \
        static ret (*real) params = 0;                                   \
        wl_tick(#name, &n);                                              \
        if (!real) real = (ret (*) params) wl_egl_sym(#name);            \
        if (!real) return deflt;                                         \
        return real args;                                                \
    }

typedef void *WLptr;

/* ---- the render-loop hot path (fences + current + query) ---- */
WL_EGL_FWD(int,   eglClientWaitSyncKHR, (WLptr d, WLptr s, int f, unsigned long long t), (d, s, f, t), 0)
WL_EGL_FWD(int,   eglWaitSyncKHR,       (WLptr d, WLptr s, int f),                        (d, s, f),    0)
WL_EGL_FWD(WLptr, eglCreateSyncKHR,     (WLptr d, unsigned int ty, const int *a),         (d, ty, a),   (WLptr)0)
WL_EGL_FWD(unsigned int, eglDestroySyncKHR, (WLptr d, WLptr s),                           (d, s),       0)
WL_EGL_FWD(int,   eglDupNativeFenceFDANDROID, (WLptr d, WLptr s),                         (d, s),      -1)
WL_EGL_FWD(unsigned int, eglMakeCurrent, (WLptr d, WLptr dr, WLptr rd, WLptr c),          (d, dr, rd, c), 0)
WL_EGL_FWD(unsigned int, eglQuerySurface, (WLptr d, WLptr s, int a, int *v),              (d, s, a, v), 0)
WL_EGL_FWD(unsigned int, eglSurfaceAttrib, (WLptr d, WLptr s, int a, int v),              (d, s, a, v), 0)
WL_EGL_FWD(unsigned int, eglSwapInterval, (WLptr d, int i),                               (d, i),       0)
WL_EGL_FWD(unsigned int, eglSetDamageRegionKHR, (WLptr d, WLptr s, int *r, int cnt),      (d, s, r, cnt), 0)
WL_EGL_FWD(int,   eglGetError,          (void),                                           (),           0x3000 /*EGL_SUCCESS*/)
