/* deathcatch.c — LD_PRELOAD diagnostic for the noice clean-death wall.
 * arm32 / arm-linux-ohos / musl.
 *
 * Interposes the process-teardown paths (exit family, abort, pthread_exit,
 * raw exit_group syscall) and chains a fatal-signal handler, logging the
 * immediate caller + a one-shot /proc/self/maps so the caller address can be
 * resolved offline. Pure observation: every hook calls through to the real
 * function (or re-raises), so process behavior is unchanged.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <unwind.h>

/* ---- native backtrace via the C++ unwinder (CFI-based, robust to tailcalls) ---- */
struct bt_state { unsigned long* ips; int cur; int max; };
static _Unwind_Reason_Code bt_cb(struct _Unwind_Context* ctx, void* arg) {
    struct bt_state* s = (struct bt_state*)arg;
    if (s->cur >= s->max) return _URC_END_OF_STACK;
    s->ips[s->cur++] = (unsigned long)_Unwind_GetIP(ctx);
    return _URC_NO_REASON;
}
static void numcat(char* b, int* n, long v);
static void hexcat(char* b, int* n, unsigned long v);
static void log_backtrace(const char* what) {
    unsigned long ips[24];
    struct bt_state st = { ips, 0, 24 };
    _Unwind_Backtrace(bt_cb, &st);
    char b[1024]; int n = 0;
    const char* t = "[DEATHCATCH] BT ";
    for (const char* p = t; *p; p++) b[n++] = *p;
    for (const char* p = what; *p; p++) b[n++] = *p;
    b[n++] = ':';
    for (int i = 0; i < st.cur && n < 980; i++) {
        b[n++] = ' ';
        hexcat(b, &n, ips[i]);
    }
    b[n++] = '\n';
    write(2, b, n);
}

static int g_maps_dumped = 0;

static void hexcat(char* b, int* n, unsigned long v) {
    static const char* H = "0123456789abcdef";
    b[(*n)++] = '0'; b[(*n)++] = 'x';
    int started = 0;
    for (int i = 28; i >= 0; i -= 4) {
        int d = (v >> i) & 0xf;
        if (d || started || i == 0) { b[(*n)++] = H[d]; started = 1; }
    }
}

static void numcat(char* b, int* n, long v) {
    if (v < 0) { b[(*n)++] = '-'; v = -v; }
    char tmp[24]; int t = 0;
    if (v == 0) tmp[t++] = '0';
    while (v) { tmp[t++] = '0' + (v % 10); v /= 10; }
    while (t) b[(*n)++] = tmp[--t];
}

#define DC_FD 2

static void dump_maps_once(void) {
    if (g_maps_dumped) return;
    g_maps_dumped = 1;
    int mf = open("/proc/self/maps", O_RDONLY);
    if (mf < 0) return;
    char hdr[64]; int hn = 0;
    const char* h = "\n--- MAPS pid=";
    for (const char* p = h; *p; p++) hdr[hn++] = *p;
    numcat(hdr, &hn, (long)getpid());
    hdr[hn++] = ' '; hdr[hn++] = '-'; hdr[hn++] = '-'; hdr[hn++] = '-'; hdr[hn++] = '\n';
    write(DC_FD, hdr, hn);
    char buf[4096]; ssize_t r;
    while ((r = read(mf, buf, sizeof buf)) > 0) write(DC_FD, buf, r);
    close(mf);
    const char* e = "--- END MAPS ---\n";
    write(DC_FD, e, strlen(e));
}

static void logline(const char* what, unsigned long caller, long extra, const char* extra_name) {
    dump_maps_once();
    char b[256]; int n = 0;
    const char* tag = "[DEATHCATCH] pid=";
    for (const char* p = tag; *p; p++) b[n++] = *p;
    numcat(b, &n, (long)getpid());
    b[n++] = ' ';
    for (const char* p = what; *p; p++) b[n++] = *p;
    b[n++] = ' '; b[n++] = 'c'; b[n++] = 'a'; b[n++] = 'l'; b[n++] = 'l'; b[n++] = 'e'; b[n++] = 'r'; b[n++] = '=';
    hexcat(b, &n, caller);
    if (extra_name) {
        b[n++] = ' ';
        for (const char* p = extra_name; *p; p++) b[n++] = *p;
        b[n++] = '=';
        numcat(b, &n, extra);
    }
    b[n++] = '\n';
    write(DC_FD, b, n);
}

#define CALLER() ((unsigned long)__builtin_return_address(0))

/* ---- exit family ---- */
typedef void (*exit_fn)(int);
void exit(int code) {
    logline("exit", CALLER(), code, "code");
    log_backtrace("exit");
    static exit_fn real = 0;
    if (!real) real = (exit_fn)dlsym(RTLD_NEXT, "exit");
    real(code);
    __builtin_unreachable();
}
void _exit(int code) {
    logline("_exit", CALLER(), code, "code");
    log_backtrace("_exit");
    static exit_fn real = 0;
    if (!real) real = (exit_fn)dlsym(RTLD_NEXT, "_exit");
    real(code);
    __builtin_unreachable();
}
void _Exit(int code) {
    logline("_Exit", CALLER(), code, "code");
    log_backtrace("_Exit");
    static exit_fn real = 0;
    if (!real) real = (exit_fn)dlsym(RTLD_NEXT, "_Exit");
    real(code);
    __builtin_unreachable();
}

/* ---- abort ---- */
typedef void (*abort_fn)(void);
void abort(void) {
    logline("abort", CALLER(), 0, 0);
    log_backtrace("abort");
    static abort_fn real = 0;
    if (!real) real = (abort_fn)dlsym(RTLD_NEXT, "abort");
    real();
    __builtin_unreachable();
}

/* ---- pthread_exit ---- */
typedef void (*pte_fn)(void*);
void pthread_exit(void* rv) {
    logline("pthread_exit", CALLER(), (long)(intptr_t)rv, "rv");
    static pte_fn real = 0;
    if (!real) real = (pte_fn)dlsym(RTLD_NEXT, "pthread_exit");
    real(rv);
    __builtin_unreachable();
}

/* ---- raw syscall (catch exit_group=248 / exit=1 issued directly) ---- */
typedef long (*sc_fn)(long, ...);
long syscall(long num, ...) {
    va_list ap; va_start(ap, num);
    long a0 = va_arg(ap, long), a1 = va_arg(ap, long), a2 = va_arg(ap, long);
    long a3 = va_arg(ap, long), a4 = va_arg(ap, long), a5 = va_arg(ap, long);
    va_end(ap);
    if (num == 248 /*exit_group*/ || num == 1 /*exit*/)
        { logline("syscall_exit_group", CALLER(), num, "nr"); log_backtrace("sc_exit"); }
    static sc_fn real = 0;
    if (!real) real = (sc_fn)dlsym(RTLD_NEXT, "syscall");
    return real(num, a0, a1, a2, a3, a4, a5);
}

/* ---- chained fatal-signal handler ---- */
static void on_fatal(int sig, siginfo_t* si, void* uc) {
    {
        dump_maps_once();
        char b[160]; int n = 0;
        const char* tag = "[DEATHCATCH] pid=";
        for (const char* p = tag; *p; p++) b[n++] = *p;
        numcat(b, &n, (long)getpid());
        const char* s = " FATAL_SIGNAL sig=";
        for (const char* p = s; *p; p++) b[n++] = *p;
        numcat(b, &n, sig);
        const char* a = " addr=";
        for (const char* p = a; *p; p++) b[n++] = *p;
        hexcat(b, &n, si ? (unsigned long)(uintptr_t)si->si_addr : 0);
        const char* c = " code=";
        for (const char* p = c; *p; p++) b[n++] = *p;
        numcat(b, &n, si ? si->si_code : 0);
        b[n++] = '\n';
        write(DC_FD, b, n);
    }
    /* restore default and re-raise: preserve original fatal behavior */
    signal(sig, SIG_DFL);
    raise(sig);
}

__attribute__((constructor))
static void dc_init(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_fatal;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    /* installed before ART's runtime init -> ART's libsigchain chains to us
     * for faults it does NOT claim (genuine unhandled crashes). */
    sigaction(SIGSEGV, &sa, 0);
    sigaction(SIGBUS, &sa, 0);
    sigaction(SIGABRT, &sa, 0);
    sigaction(SIGILL, &sa, 0);
    sigaction(SIGFPE, &sa, 0);
    sigaction(SIGTRAP, &sa, 0);
    /* no logline here: keep g_maps_dumped=0 across fork so each child dumps
     * its OWN maps on its first hook (needed to resolve the caller address). */
}
