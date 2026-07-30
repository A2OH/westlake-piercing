// wlicu.c — is ICU 66 actually usable on this board, and does it survive a fork?
//
// Written to isolate a startup SIGSEGV in ucnv_fromUnicode_66 (addr=0x5c) that appeared after a
// power cycle. The bridge binds the converter symbols in the appspawn-x PARENT and the forked child
// inherits g_cnv.ok=true, so the question is whether ICU's data mapping is still valid post-fork.
//
// build: $OHOS_LLVM/bin/clang --target=aarch64-linux-ohos --sysroot=$NDK/sysroot -O1 \
//          -o wlicu wlicu.c -ldl
// run:   ICU_DATA=/data/local/tmp/asx LD_LIBRARY_PATH=/data/local/tmp/asx:/system/lib64 ./wlicu
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/wait.h>

typedef void* (*fn_open)(const char*, int*);
typedef void  (*fn_close)(void*);
typedef void  (*fn_from)(void*, char**, const char*, const unsigned short**,
                         const unsigned short*, int*, char, int*);

static void* lib;
static fn_open  u_open;
static fn_close u_close;
static fn_from  u_from;

static void* sym(const char* base) {
    static const char* suf[] = { "", "_66", "_74", "_72", "_70", NULL };
    char b[128];
    for (int i = 0; suf[i]; i++) {
        snprintf(b, sizeof b, "%s%s", base, suf[i]);
        void* p = dlsym(lib, b);
        if (p) { printf("    %s -> %s\n", base, b); return p; }
    }
    printf("    %s -> MISSING\n", base);
    return NULL;
}

// Encode "hi" through the named charset. Returns 0 on success.
static int try_encode(const char* name, const char* tag) {
    int st = 0;
    void* c = u_open(name, &st);
    printf("  [%s] ucnv_open(%s) -> %p st=%d\n", tag, name, c, st);
    if (!c || st > 0) return 1;
    const unsigned short src[2] = { 'h', 'i' };
    const unsigned short* sp = src;
    char out[16]; char* tp = out;
    st = 0;
    u_from(c, &tp, out + sizeof out, &sp, src + 2, NULL, 1, &st);
    printf("  [%s] fromUnicode wrote %ld byte(s) st=%d\n", tag, (long)(tp - out), st);
    u_close(c);
    return (st > 0);
}

int main(void) {
    printf("ICU_DATA=%s\n", getenv("ICU_DATA") ? getenv("ICU_DATA") : "(unset)");
    const char* names[] = { "libicuuc.so", "/data/local/tmp/asx/libicuuc.so", NULL };
    for (int i = 0; names[i] && !lib; i++) {
        lib = dlopen(names[i], RTLD_NOW | RTLD_GLOBAL);
        printf("dlopen(%s) -> %p%s\n", names[i], lib, lib ? "" : dlerror());
    }
    if (!lib) return 2;
    u_open  = (fn_open)  sym("ucnv_open");
    u_close = (fn_close) sym("ucnv_close");
    u_from  = (fn_from)  sym("ucnv_fromUnicode");
    if (!u_open || !u_close || !u_from) return 3;

    // 1. plain use in this process
    int pre = try_encode("UTF-8", "parent/UTF-8");
    pre    |= try_encode("ISO-8859-1", "parent/latin1");

    // 2. the case the bridge actually hits: open BEFORE fork, use AFTER
    int st = 0;
    void* inherited = u_open("UTF-8", &st);
    printf("pre-fork ucnv_open -> %p st=%d\n", inherited, st);

    pid_t p = fork();
    if (p == 0) {
        // child: use the inherited converter, then a freshly opened one
        const unsigned short src[2] = { 'h', 'i' };
        const unsigned short* sp = src;
        char out[16]; char* tp = out; int cst = 0;
        printf("  [child] using INHERITED converter %p\n", inherited);
        fflush(stdout);
        u_from(inherited, &tp, out + sizeof out, &sp, src + 2, NULL, 1, &cst);
        printf("  [child] inherited fromUnicode wrote %ld st=%d\n", (long)(tp - out), cst);
        int r = try_encode("UTF-8", "child/fresh");
        fflush(stdout);
        _exit(r);
    }
    int status = 0; waitpid(p, &status, 0);
    printf("child exit status=0x%x (signalled=%d sig=%d)\n", status,
           WIFSIGNALED(status), WIFSIGNALED(status) ? WTERMSIG(status) : 0);
    u_close(inherited);
    printf("RESULT parent_ok=%d\n", pre == 0);
    return 0;
}
