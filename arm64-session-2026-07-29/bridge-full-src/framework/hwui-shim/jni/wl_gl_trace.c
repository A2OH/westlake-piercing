/*
 * wl_gl_trace.c — WESTLAKE §283r
 *
 * The hwui RenderThread spins at 100% CPU (state R, stime-dominated) and §283q ruled out the
 * shim's window API, EGL, sync_wait() and page faults.  What is left is GL.  Both libhwui (15
 * undefined gl* syms) and libskia_canvaskit (108 GL and EGL syms) resolve GL through the global
 * scope, and this shim is FIRST in libhwui's DT_NEEDED, so defining them here interposes both.
 *
 * Neither imports glClientWaitSync/glFenceSync, so a GL fence-object spin is ruled out — but BOTH
 * import **glFinish**, which blocks until the GPU drains and which busy-polls an ioctl in many
 * drivers.  That matches the signature exactly (100% CPU, high stime, no syscall ever sampled
 * because the task is always on-CPU).
 *
 * glFinish therefore gets ENTER/EXIT markers, not just a counter: "ENTER" with no matching "EXIT"
 * identifies it as the hang. The rest get counters.
 */
#include <stdio.h>
#include <dlfcn.h>

static void *wl_gl_handle(void) {
    static void *h = (void *)0;
    static int tried = 0;
    if (!tried) {
        tried = 1;
        h = dlopen("libGLESv3.so", 0x00002 /*RTLD_NOW*/ | 0x00100 /*RTLD_GLOBAL*/);
        if (!h) h = dlopen("libGLESv2.so", 0x00002 | 0x00100);
        fprintf(stderr, "[WESTLAKE-GLTRACE] libGLES handle=%p\n", h);
        fflush(stderr);
    }
    return h;
}

static void *wl_gl_sym(const char *name) {
    void *h = wl_gl_handle();
    void *p = h ? dlsym(h, name) : (void *)0;
    if (!p) {
        fprintf(stderr, "[WESTLAKE-GLTRACE] !! cannot resolve %s\n", name);
        fflush(stderr);
    }
    return p;
}

static void wl_gl_tick(const char *name, unsigned long *n) {
    unsigned long v = ++(*n);
    if (v == 1UL || v == 100UL || v == 1000UL || (v % 20000UL) == 0UL) {
        fprintf(stderr, "[WESTLAKE-GLTRACE] %s calls=%lu\n", name, v);
        fflush(stderr);
    }
}

/* ★glFinish: bracketed, because the interesting outcome is "entered and never returned". */
void glFinish(void) {
    static unsigned long n = 0;
    static void (*real)(void) = 0;
    unsigned long v = ++n;
    fprintf(stderr, "[WESTLAKE-GLTRACE] glFinish ENTER #%lu\n", v);
    fflush(stderr);
    if (!real) real = (void (*)(void)) wl_gl_sym("glFinish");
    if (real) real();
    fprintf(stderr, "[WESTLAKE-GLTRACE] glFinish EXIT  #%lu\n", v);
    fflush(stderr);
}

void glFlush(void) {
    static unsigned long n = 0;
    static void (*real)(void) = 0;
    wl_gl_tick("glFlush", &n);
    if (!real) real = (void (*)(void)) wl_gl_sym("glFlush");
    if (real) real();
}

unsigned int glGetError(void) {
    static unsigned long n = 0;
    static unsigned int (*real)(void) = 0;
    wl_gl_tick("glGetError", &n);
    if (!real) real = (unsigned int (*)(void)) wl_gl_sym("glGetError");
    return real ? real() : 0u;
}

unsigned int glCheckFramebufferStatus(unsigned int target) {
    static unsigned long n = 0;
    static unsigned int (*real)(unsigned int) = 0;
    wl_gl_tick("glCheckFramebufferStatus", &n);
    if (!real) real = (unsigned int (*)(unsigned int)) wl_gl_sym("glCheckFramebufferStatus");
    return real ? real(target) : 0u;
}
