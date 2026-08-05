/**
 * appspawn-x child process implementation.
 *
 * Handles the complete child process lifecycle after fork():
 * 1. Apply OH DAC credentials (UID/GID/groups)
 * 2. Set up filesystem sandbox (mount namespace, bind mounts)
 * 3. Set SELinux security context
 * 4. Configure OH AccessToken for permission enforcement
 * 5. Initialize the Android-OH adapter layer
 * 6. Launch ActivityThread.main() to start the Android app
 */

#include "child_main.h"
#include "appspawnx_runtime.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <dlfcn.h>
#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#include <fcntl.h>
#include <grp.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <algorithm>

// B.30 (2026-04-29): real adaptation for SELinux + AccessToken so child secon
// transitions out of u:r:appspawn:s0 (which can't query SA 180/501/4607).
// hap_restorecon: HapContext::HapDomainSetcontext routes APL → setcon.
// token_setproc: SetSelfTokenID applies the OH access token from TLV.
#include "hap_restorecon.h"          // selinux_adapter:libhap_restorecon
#include "token_setproc.h"           // access_token:libtokensetproc_shared

namespace appspawnx {
// ── §525: name the JAVA method behind an interpreter SEGV ──────────────────────────────────────
// The §436 invoke-interface wall kills this child intermittently, and every fix for it (§440 app dex,
// §464 framework dex, §524 cdn) needed the EXACT call site. But the crash backtrace is all libart:
//
//   [WESTLAKE-CHILDSEGV] #0 sig=11 addr=0x45 ... art::interpreter::DoCall<false>
//     fr00 ... InstructionHandler<...>::INVOKE_INTERFACE
//
// i.e. "somewhere in an interface call" — leaving 19 candidate sites to guess between.
//
// DoCall's first parameter IS the ArtMethod*, so on aarch64 x0 still holds it when the fault happens
// early in that function. libart exports art::ArtMethod::PrettyMethod(ArtMethod*, bool), and this
// binary is built against the same OHOS libc++ (__h ABI) as libart, so it can be called directly —
// turning the crash into a named Java method.
//
// ⚠️This runs in a signal handler after a memory fault, so it is deliberately defensive:
//   * registers print FIRST, so a raw ArtMethod* survives even if resolution itself faults;
//   * a re-entry flag stops a fault inside PrettyMethod from looping;
//   * obviously-bad pointers are rejected before being dereferenced.
static volatile int wl_naming_in_progress = 0;

static void wl_name_java_method(void* ctx) {
    ucontext_t* uc = (ucontext_t*) ctx;
    if (uc == nullptr) return;
    {
        char rb[256];
        int n = snprintf(rb, sizeof rb,
            "[WESTLAKE-525] x0=%#lx x1=%#lx x2=%#lx x3=%#lx x29=%#lx x30=%#lx\n",
            (unsigned long) uc->uc_mcontext.regs[0], (unsigned long) uc->uc_mcontext.regs[1],
            (unsigned long) uc->uc_mcontext.regs[2], (unsigned long) uc->uc_mcontext.regs[3],
            (unsigned long) uc->uc_mcontext.regs[29], (unsigned long) uc->uc_mcontext.regs[30]);
        if (n > 0) { ssize_t w = write(2, rb, (size_t) n); (void) w; }
    }
    if (wl_naming_in_progress) return;
    wl_naming_in_progress = 1;

    unsigned long m = (unsigned long) uc->uc_mcontext.regs[0];
    if (m < 0x10000 || (m & 3) != 0) { wl_naming_in_progress = 0; return; }

    using PrettyFn = std::string (*)(void*, bool);
    static PrettyFn pretty = nullptr;
    if (pretty == nullptr)
        pretty = (PrettyFn) dlsym(RTLD_DEFAULT, "_ZN3art9ArtMethod12PrettyMethodEPS0_b");
    if (pretty == nullptr) {
        const char* nf = "[WESTLAKE-525] PrettyMethod not exported; use x0 above\n";
        ssize_t w = write(2, nf, strlen(nf)); (void) w;
        wl_naming_in_progress = 0; return;
    }
    std::string name = pretty((void*) m, true);
    char nb[512];
    int n = snprintf(nb, sizeof nb, "[WESTLAKE-525] JAVA METHOD AT FAULT: %s\n", name.c_str());
    if (n > 0) { ssize_t w = write(2, nb, (size_t) n); (void) w; }
    wl_naming_in_progress = 0;
}



// ---------------------------------------------------------------------------
// run  –  child process main entry point (does not return)
// ---------------------------------------------------------------------------
[[noreturn]] void ChildMain::run(const SpawnMsg& msg, AppSpawnXRuntime* runtime) {
    pid_t myPid = getpid();
    LOGI("Child process started, pid=%d uid=%d bundle=%s",
         myPid, msg.uid, msg.bundleName.c_str());

    // Diagnostic (kept until HelloWorld UI works, per memory
    // feedback_keep_cp_instrumentation.md): redirect native fd 1/2 to a
    // per-pid file so libart's LOG(FATAL)/CHECK output (otherwise
    // discarded into the inherited /dev/null fd 2) becomes readable
    // post-mortem.  Try multiple paths because appspawn:s0 SELinux
    // domain may deny write to /data/local/tmp; /data/service/el1/public/appspawnx
    // is created by appspawn_x.cfg specifically for our use.
    LOGI("[CHILD] entering stderr redirect probe");
    {
        const char* candPaths[] = {
            "/data/service/el1/public/appspawnx",
            "/data/misc/appspawnx",
            "/data/local/tmp",
            "/data/log",
        };
        int errFd = -1;
        char chosenPath[128] = {0};
        for (const char* dir : candPaths) {
            snprintf(chosenPath, sizeof(chosenPath),
                     "%s/adapter_child_%d.stderr", dir, myPid);
            errFd = open(chosenPath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (errFd >= 0) {
                LOGI("[CHILD] opened %s fd=%d", chosenPath, errFd);
                break;
            }
            LOGW("[CHILD] open(%s) failed: errno=%d %s",
                 chosenPath, errno, strerror(errno));
        }
        if (errFd >= 0) {
            dup2(errFd, 2);
            dup2(errFd, 1);
            if (errFd != 1 && errFd != 2) close(errFd);
            setvbuf(stderr, nullptr, _IOLBF, 0);
            LOGI("[CHILD] native stderr redirected to %s", chosenPath);
            // WESTLAKE 2026-07-22 (§145): OH's libdfx/musl-sigchain handler converts faults into a
            // clean exit(1) and may write no cppcrash, so a fault can masquerade as a normal exit
            // (this misled the investigation twice). Install our own handler FIRST so every fatal
            // signal is reported with its address before anything can swallow it, then re-raise.
            {
                struct sigaction wl_sa;
                memset(&wl_sa, 0, sizeof(wl_sa));
                wl_sa.sa_flags = SA_SIGINFO;
                wl_sa.sa_sigaction = [](int sig, siginfo_t* si, void*) {
                    char buf[192];
                    int n = snprintf(buf, sizeof(buf),
                                     "\n[WESTLAKE-FATALSIG] signal=%d code=%d addr=%p pid=%d\n",
                                     sig, si != nullptr ? si->si_code : 0,
                                     si != nullptr ? si->si_addr : nullptr, (int)getpid());
                    ssize_t ignored = write(2, buf, n); (void)ignored;
                    signal(sig, SIG_DFL);
                    raise(sig);
                };
                for (int wl_s : {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT}) {
                    sigaction(wl_s, &wl_sa, nullptr);
                }
                fprintf(stderr, "[WESTLAKE-FATALSIG] handlers installed\n"); fflush(stderr);
                // WESTLAKE §330: the sigaction() handlers above NEVER FIRE — OHOS musl INTERCEPTS
                // sigaction(), so ART/sigchain remains the real handler and our report is skipped
                // (proven in-process in §324: install returned rc=0 with prev=libart, yet ~26k
                // faults/s flowed past unseen). musl exports the same sigchain API ART uses, and
                // "special" handlers run BEFORE the user sigaction — register there too so a fatal
                // signal is ALWAYS reported with its address/PC before anything can swallow it.
                // Returns false = report only, never claim the fault, chain unchanged.
                {
                    struct wl_sig_action {
                        bool (*sca_sigaction)(int, siginfo_t*, void*);
                        sigset_t sca_mask;
                        int sca_flags;
                    };
                    typedef void (*add_ssh_t)(int, struct wl_sig_action*);
                    add_ssh_t wl_add = (add_ssh_t) dlsym(RTLD_DEFAULT, "add_special_signal_handler");
                    if (wl_add != nullptr) {
                        static struct wl_sig_action wl_acts[2];
                        int wl_i = 0;
                        for (int wl_s : {SIGSEGV, SIGBUS}) {
                            struct wl_sig_action* a = &wl_acts[wl_i++];
                            memset(a, 0, sizeof(*a));
                            sigemptyset(&a->sca_mask);
                            a->sca_flags = 0;
                            a->sca_sigaction = [](int sig, siginfo_t* si, void* ctx) -> bool {
                                static volatile int wl_n = 0;
                                if (wl_n < 6) {
                                    int k = wl_n++;
                                    unsigned long pc = 0, lr = 0;
                                    ucontext_t* uc = (ucontext_t*) ctx;
                                    if (uc != nullptr) {
                                        pc = (unsigned long) uc->uc_mcontext.pc;
                                        lr = (unsigned long) uc->uc_mcontext.regs[30];
                                    }
                                    // §330b: resolve PC to a library+offset with dladdr — the child
                                    // dies too fast to snapshot /proc/<pid>/maps from a shell, and raw
                                    // ASLR addresses are useless across runs.
                                    Dl_info wl_di;
                                    const char* wl_lib = "?"; unsigned long wl_off = 0;
                                    if (dladdr((void*) pc, &wl_di) != 0 && wl_di.dli_fname != nullptr) {
                                        wl_lib = wl_di.dli_fname;
                                        const char* sl = strrchr(wl_lib, '/');
                                        if (sl != nullptr) wl_lib = sl + 1;
                                        wl_off = pc - (unsigned long) wl_di.dli_fbase;
                                    }
                                    char b[320];
                                    int n2 = snprintf(b, sizeof b,
                                        "[WESTLAKE-CHILDSEGV] #%d sig=%d code=%d addr=%p pc=%#lx (%s+%#lx) sym=%s tid=%d\n",
                                        k, sig, si ? si->si_code : -1, si ? si->si_addr : nullptr,
                                        pc, wl_lib, wl_off,
                                        (dladdr((void*) pc, &wl_di) != 0 && wl_di.dli_sname) ? wl_di.dli_sname : "-",
                                        (int) gettid());
                                    if (n2 > 0) { ssize_t w = write(2, b, (size_t) n2); (void) w; }
                                    // §525: turn "somewhere in an interface call" into a named site
                                    if (sig == 11) wl_name_java_method(ctx);
                                    // frame-pointer walk (aarch64: [fp]=caller fp, [fp+8]=ret addr)
                                    if (uc != nullptr) {
                                        unsigned long fp = (unsigned long) uc->uc_mcontext.regs[29];
                                        for (int d = 0; d < 12 && fp != 0 && (fp & 7) == 0; ++d) {
                                            unsigned long nx = *(unsigned long*) fp;
                                            unsigned long ra = *(unsigned long*) (fp + 8);
                                            if (ra == 0) break;
                                            Dl_info fdi;
                                            const char* flib = "?"; unsigned long foff = 0;
                                            const char* fsym = "-";
                                            if (dladdr((void*) ra, &fdi) != 0 && fdi.dli_fname != nullptr) {
                                                flib = fdi.dli_fname;
                                                const char* s2 = strrchr(flib, '/');
                                                if (s2 != nullptr) flib = s2 + 1;
                                                foff = ra - (unsigned long) fdi.dli_fbase;
                                                if (fdi.dli_sname != nullptr) fsym = fdi.dli_sname;
                                            }
                                            char fb[256];
                                            int fl = snprintf(fb, sizeof fb,
                                                    "[WESTLAKE-CHILDSEGV]   fr%02d ra=%#lx (%s+%#lx) %s\n",
                                                    d, ra, flib, foff, fsym);
                                            if (fl > 0) { ssize_t w2 = write(2, fb, (size_t) fl); (void) w2; }
                                            if (nx <= fp) break;
                                            fp = nx;
                                        }
                                    }
                                }
                                return false;
                            };
                            wl_add(wl_s, a);
                        }
                        const char* m = "[WESTLAKE-FATALSIG] sigchain special handlers registered\n";
                        ssize_t w = write(2, m, strlen(m)); (void) w;
                    } else {
                        const char* m = "[WESTLAKE-FATALSIG] add_special_signal_handler NOT FOUND\n";
                        ssize_t w = write(2, m, strlen(m)); (void) w;
                    }
                }
            }
        } else {
            LOGE("[CHILD] all stderr redirect paths failed; ART abort msgs lost");
        }
    }

    // 2026-05-02 G2.14n+: mark this process as the child so that the lazy
    // Typeface init path in libhwui's typeface_minimal_stub (calling
    // oh_create_default_skia_handle in liboh_hwui_shim.so) is allowed to do
    // real Skia work.  In the parent process, this flag stays false and the
    // shim refuses to call SkFontMgr_New_OHOS — which would spawn IPC worker
    // threads that ZygoteHooks.preFork()'s waitUntilAllThreadsStopped()
    // would hang on forever.
    //
    // Resolved via dlsym (not direct link) to avoid creating a load-time
    // dependency from appspawn-x → liboh_hwui_shim.so (the shim is loaded
    // transitively via libhwui via liboh_adapter_bridge.so).
    {
        using MarkFn = void (*)(void);
        MarkFn fn = reinterpret_cast<MarkFn>(
            dlsym(RTLD_DEFAULT, "oh_typeface_mark_child"));
        if (fn) {
            fn();
            LOGI("[CHILD] oh_typeface_mark_child() invoked");
        } else {
            LOGW("[CHILD] oh_typeface_mark_child symbol not found — Typeface "
                 "real-init won't fire (font rendering will degrade)");
        }
    }

    // G2.14h (2026-05-01): apply AccessToken FIRST, while still in appspawn:s0
    // domain AND still root uid. SetSelfTokenID requires either uid==0 or
    // CAP_SYS_RESOURCE; both are dropped by applyDac, both are restricted by
    // applySELinux's transition to normal_hap:s0. OH's stock appspawn applies
    // token first (see appspawn_service.c init child sequence).
    //
    // Step 1: Apply OH AccessToken (must run while uid==0 + appspawn:s0)
    int ret = applyAccessToken(msg);
    if (ret != 0) {
        LOGE("applyAccessToken failed, ret=%d – aborting child", ret);
        _exit(13);
    }

    // Step 2: Apply DAC credentials (must be done early, before sandbox)
    ret = applyDac(msg);
    if (ret != 0) {
        LOGE("applyDac failed, ret=%d – aborting child", ret);
        _exit(10);
    }

    // Step 3: Set up filesystem sandbox
    ret = applySandbox(msg);
    if (ret != 0) {
        LOGE("applySandbox failed, ret=%d – aborting child", ret);
        _exit(11);
    }

    // Step 4: Set SELinux context
    ret = applySELinux(msg);
    if (ret != 0) {
        LOGE("applySELinux failed, ret=%d – aborting child", ret);
        _exit(12);
    }

    // Step 4.5 (B.33 2026-04-29): ZygoteHooks.postForkChild + postForkCommon.
    // Order matters: must run AFTER applySELinux/applyAccessToken (steps 3-4)
    // because Daemons.startPostZygoteFork inside postForkCommon spawns ART
    // daemon threads, and setcon in applySELinux requires the process to be
    // single-threaded (kernel returns EPERM otherwise → -SELINUX_SET_CONTEXT_ERROR
    // rc=-7).  But MUST run BEFORE any env->CallStaticVoidMethod in initChild,
    // since without postForkChild the child's ART state is inconsistent (locks
    // held by dead parent daemon TIDs) and Java calls deadlock in epoll_wait
    // on dead daemon notifications.
    if (runtime->zygotePostForkChild() != 0) {
        LOGW("zygotePostForkChild non-zero — daemon threads may be missing in child");
    }
    // 2026-07-09 BISECT: the child dies DURING CallStaticVoidMethod(initChild) (no
    // [CM-CK] return), right after a Throwable.printStackTrace — smells like a daemon
    // thread restarted here exit_group'ing. Gate off the daemon-restart to test: if
    // the child then reaches J_initChild_TOP, the restarted daemon is the killer.
    // postForkCommon is REQUIRED (skipping it → child dies at J_initChild_TOP, the
    // B.33 epoll_wait/dead-daemon-lock issue). WITH it, the child reaches deep into
    // initChild (installActivityManagerStub). It is NOT the fork-path killer.
    if (runtime->zygotePostForkCommon() != 0) {
        LOGW("zygotePostForkCommon non-zero — daemon restart may be incomplete");
    }

    // 2026-07-09: restart the sigchain re-assert thread in the forked CHILD, now that
    // postForkChild/setcon are done (child may be multi-threaded). This keeps ART's
    // fault handler on top of OHOS libdfx so the child's framework null/suspend-check
    // SEGVs are recovered by ART instead of libdfx exit(1)-ing (the initChild death).
    // Must run AFTER setcon (single-threaded requirement) — hence here, not in atfork.
    // Give the CHILD the SAME SIG_DFL {SEGV,BUS,ABRT,external} reclaimer that keeps
    // the PARENT alive (the killer is a SEGV that only manifests as a death when a
    // handler is installed; SIG_DFL neutralizes it). The re-assert thread doesn't
    // survive fork, so start a fresh one here in the forked app process.
    // 2026-07-09 EXPERIMENT: child SIG_DFL reclaimer DISABLED. It SIG_DFL'd SEGV/BUS
    // (breaking ART's inherited implicit-null-check fault recovery) and raced ART's
    // handler — a suspected cause of the variable-timing death. Let ART's inherited
    // fault handler stand alone in the child.
    if (getenv("ASX_CHILD_RECLAIMER")) {
        pthread_t childReclaimer;
        if (pthread_create(&childReclaimer, nullptr, [](void*) -> void* {
            for (;;) { signal(SIGSEGV, SIG_DFL); signal(SIGBUS, SIG_DFL); signal(SIGABRT, SIG_DFL);
                signal(SIGTERM, SIG_DFL); signal(SIGXCPU, SIG_DFL); usleep(20000); }
            return nullptr;
        }, nullptr) == 0) { pthread_detach(childReclaimer);
            fprintf(stderr, "[CHILD] SIG_DFL reclaimer started\n"); fflush(stderr); }
    } else {
        // Keep ART's fault handler IN FRONT of OHOS libdfx (which else intercepts the
        // framework's implicit-null-check SEGVs and exit(1)s the child). If ART's
        // NullPointerHandler gets the SEGV and the arm64 pc-edit survives sigreturn,
        // the null check is recovered and the child survives past installActivityManagerStub.
        void (*startReassert)() =
            reinterpret_cast<void(*)()>(dlsym(RTLD_DEFAULT, "SigchainStartReassert"));
        if (startReassert) { startReassert(); fprintf(stderr, "[CHILD] sigchain re-assert (ART front-of-chain)\n"); }
        else fprintf(stderr, "[CHILD] SigchainStartReassert not found\n");
        fflush(stderr);
    }

    // Step 5: Post-fork runtime initialization (OH IPC setup)
    runtime->onChildInit();

    // Step 6: Initialize adapter layer (OHEnvironment.initialize)
    JNIEnv* env = runtime->getJNIEnv();
    if (!env) {
        LOGE("JNIEnv is null after onChildInit – aborting child");
        _exit(14);
    }

    // 2026-07-09: re-run AndroidRuntime::startReg in the CHILD. The child fork-inherits
    // MOST of the parent's framework-native registrations, but LOSES some (notably
    // android.os.Binder.clearCallingIdentity + adapter natives) → ActivityThread.main
    // dies with UnsatisfiedLinkError deep in the launch. Re-registering here (dlsym the
    // same exported startReg the parent uses) restores them. RegisterNatives overwrites,
    // so this is idempotent for the ones already inherited.
    {
        // 2026-07-11: MUST use RTLD_NOLOAD (bridge already loaded via System.loadLibrary)
        // to fetch the EXISTING handle. Plain RTLD_NOW forces a fresh full-resolution
        // pass that FAILS on arm64 (some global-scope symbol is unresolved under NOW) →
        // dlopen returns null → startReg is silently skipped → MessageQueue/ApkAssets
        // natives never get registered → ActivityThread.main dies with UnsatisfiedLinkError.
        // The parent uses the same RTLD_NOLOAD|RTLD_NOW trick (appspawnx_runtime.cpp:636).
        void* libRt = dlopen("liboh_adapter_bridge.so", RTLD_NOLOAD | RTLD_NOW);
        if (!libRt) libRt = dlopen("liboh_adapter_bridge.so", RTLD_NOLOAD | RTLD_LAZY);
        if (!libRt) libRt = dlopen(nullptr, RTLD_LAZY);
        fprintf(stderr, "[CHILD] startReg re-register: libRt=%p (%s)\n",
                libRt, libRt ? "ok" : dlerror()); fflush(stderr);
        if (libRt) {
            using StartReg_t = int (*)(JNIEnv*);
            auto childStartReg = reinterpret_cast<StartReg_t>(
                dlsym(libRt, "_ZN7android14AndroidRuntime8startRegEP7_JNIEnv"));
            if (childStartReg) {
                int rc = childStartReg(env);
                if (env->ExceptionCheck()) env->ExceptionClear();
                fprintf(stderr, "[CHILD] re-ran startReg in child, rc=%d\n", rc); fflush(stderr);
            } else {
                fprintf(stderr, "[CHILD] startReg symbol not found for child re-register\n");
                fflush(stderr);
            }
        }
    }

    ret = initAdapterLayer(env, runtime);
    if (ret != 0) {
        LOGW("initAdapterLayer failed, ret=%d – continuing anyway", ret);
        // Non-fatal: the app may work without the adapter in some cases
    }

    // Step 7: Set process name for debugging (shows in ps output)
    if (!msg.procName.empty()) {
        prctl(PR_SET_NAME, msg.procName.c_str(), 0, 0, 0);
    }

    // Step 8: Launch the Android ActivityThread event loop
    LOGI("Launching ActivityThread for %s", msg.procName.c_str());
    // WESTLAKE (arm64 board, 2026-07-21) — run it on a BIG-STACK pthread.
    //
    // musl reports only __default_stacksize (128 KB) from pthread_getattr_np() for the
    // INITIAL thread, regardless of the real 8 MB rlimit stack.  ART's Thread::InitStackHwm
    // believes it, so anything deep on the child's main thread dies with
    //   "java.lang.StackOverflowError: stack size 124KB"   (128 KB - guard)
    // — observed killing android.graphics.Typeface.<clinit> on its font-mmap path
    // (MappedByteBuffer/DirectByteBuffer), which then leaves sDefaultTypeface null and
    // fails ensureBindApplication.  Patching the ELF PT_GNU_STACK and `ulimit -s` both
    // failed to move it (OHOS musl doesn't raise __default_stacksize from either).
    // A pthread created with an explicit stacksize DOES get an honest attr, so run the
    // whole ActivityThread/Looper on one.  It never returns, so just join it.
    {
        struct LaunchCtx {
            const SpawnMsg* msg; AppSpawnXRuntime* runtime;
        };
        static LaunchCtx ctx{&msg, runtime};   // enclosing fn is static: no `this`
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, 8 * 1024 * 1024);
        pthread_t javaTid;
        int prc = pthread_create(&javaTid, &attr, [](void* a) -> void* {
            auto* c = static_cast<LaunchCtx*>(a);
            JNIEnv* tenv = nullptr;
            JavaVM* vm = c->runtime->getJavaVM();
            if (vm != nullptr && vm->AttachCurrentThread(&tenv, nullptr) != JNI_OK) {
                fprintf(stderr, "[CM-BIGSTACK] AttachCurrentThread FAILED\n"); fflush(stderr);
                return nullptr;
            }
            size_t ss = 0; pthread_attr_t ga;
            if (pthread_getattr_np(pthread_self(), &ga) == 0) {
                pthread_attr_getstacksize(&ga, &ss); pthread_attr_destroy(&ga);
            }
            fprintf(stderr, "[CM-BIGSTACK] ActivityThread on dedicated pthread, stack=%zu bytes\n", ss);
            fflush(stderr);
            ChildMain::launchActivityThread(tenv, *c->msg, c->runtime);
            return nullptr;
        }, &ctx);
        pthread_attr_destroy(&attr);
        if (prc == 0) {
            pthread_join(javaTid, nullptr);
        } else {
            fprintf(stderr, "[CM-BIGSTACK] pthread_create failed (%d) — falling back to main thread\n", prc);
            fflush(stderr);
            launchActivityThread(env, msg, runtime);
        }
    }

    // Should never reach here – launchActivityThread enters an infinite loop
    LOGE("launchActivityThread returned unexpectedly – exiting");
    fprintf(stderr, "[CM-EXIT] launchActivityThread RETURNED — child_main:_exit(1)\n"); fflush(stderr);
    _exit(1);
}

// ---------------------------------------------------------------------------
// applyDac  –  set UID, GID, and supplementary groups
// ---------------------------------------------------------------------------
int ChildMain::applyDac(const SpawnMsg& msg) {
    LOGI("Applying DAC: uid=%d gid=%d gids_count=%zu",
         msg.uid, msg.gid, msg.gids.size());

    // [NET-FIX 2026-07-04] Grant internet at the spawn boundary, the OHOS-native
    // way. Adapter (Android) apps never enter OHOS's permission system — they
    // install from an APK with no OHOS bundle config, so their access-token holds
    // no ohos.permission.INTERNET, and native appspawn/AMS therefore DENY them
    // (every socket()/getaddrinfo -> EPERM, crashing networked apps). Android apps
    // universally expect INTERNET, so grant it here for adapter children exactly as
    // native appspawn does for an OHOS app that holds INTERNET: add the inet
    // supplementary gids, and (still fully root, below) set the netsys eBPF
    // oh_sock_permission_map[uid]=1 — the actual per-uid socket gate.
    std::vector<gid_t> gidArray(msg.gids.begin(), msg.gids.end());
    for (gid_t g : {static_cast<gid_t>(3003) /*AID_INET*/,
                    static_cast<gid_t>(3004) /*AID_NET_RAW*/}) {
        if (std::find(gidArray.begin(), gidArray.end(), g) == gidArray.end()) {
            gidArray.push_back(g);
        }
    }
    if (setgroups(gidArray.size(), gidArray.data()) < 0) {
        LOGE("setgroups(%zu groups) failed: %s", gidArray.size(), strerror(errno));
        return -1;
    }
    LOGD("Set %zu supplementary groups (incl inet 3003/3004)", gidArray.size());

    // Grant the netsys socket-permission map for this uid while still fully root
    // (before setresgid/setresuid). The eBPF map is the real socket gate and the
    // grant persists for the process's lifetime. The child is single-threaded here
    // (ART daemon threads restart later, in zygotePostForkChild) and no seccomp is
    // applied until applySandbox, so fork+exec of the existing bpfgrant tool is
    // safe. Best-effort: a failure only means the app falls back to its offline UI.
    if (msg.uid >= 0) {
        pid_t gp = fork();
        if (gp == 0) {
            char uidStr[16];
            snprintf(uidStr, sizeof(uidStr), "%d", msg.uid);
            execl("/system/bin/bpfgrant", "bpfgrant", uidStr, "oh_sock_permission_map",
                  static_cast<char*>(nullptr));
            _exit(127);
        } else if (gp > 0) {
            int st = 0;
            waitpid(gp, &st, 0);
            LOGI("[NET-FIX] granted oh_sock_permission_map[%d]=1 at spawn (rc=%d)",
                 msg.uid, st);
        } else {
            LOGW("[NET-FIX] fork for bpfgrant failed: %s — internet may be denied",
                 strerror(errno));
        }
    }

    // WESTLAKE §364: optionally KEEP the privileged token.
    // Measured chain: our SSM sub-session is only mounted under an app WindowScene, and the only
    // WindowScene we can borrow belongs to noice's ArkTS ability — which AMS then kills with
    // `LIFECYCLE_TIMEOUT / ability:EntryAbility foreground timeout` about 5s after we create the
    // session (proven: the identical run with sessions disabled produces no sysfreeze at all), and
    // when it dies its WindowScene and every node under it — ours included — is destroyed.
    // A privileged caller does not need to borrow anything: SSM's CheckSystemWindowPermission and
    // RS's CheckCreateNodeAndSurface both branch on IsSystemCalling()/isNonSystemAppCalling, so with
    // a native token we can ask for an APP_MAIN_WINDOW that owns its own WindowScene.
    // Dev-board bring-up aid, opt-in only.
    if (getenv("WL_NO_DROP_UID") != nullptr) {
        LOGI("[WESTLAKE-§364] WL_NO_DROP_UID set - keeping privileged uid/gid (uid=%d)", getuid());
        fprintf(stderr, "[WESTLAKE-KEEPUID] staying uid=%d gid=%d\n", getuid(), getgid());
        fflush(stderr);
        return 0;
    }

    // Set primary GID (must be done before setuid to avoid permission issues)
    if (msg.gid >= 0) {
        if (setresgid(msg.gid, msg.gid, msg.gid) < 0) {
            LOGE("setresgid(%d) failed: %s", msg.gid, strerror(errno));
            return -1;
        }
        LOGD("Set GID to %d", msg.gid);
    }

    // Set UID (this drops root privileges – do this last)
    if (msg.uid >= 0) {
        if (setresuid(msg.uid, msg.uid, msg.uid) < 0) {
            LOGE("setresuid(%d) failed: %s", msg.uid, strerror(errno));
            return -1;
        }
        LOGD("Set UID to %d", msg.uid);
    }

    // Verify we actually dropped root
    if (msg.uid > 0 && getuid() == 0) {
        LOGE("Failed to drop root – uid is still 0");
        return -1;
    }

    // Disable ability to regain root via setuid binaries
    if (msg.uid > 0) {
        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
            LOGW("prctl(PR_SET_NO_NEW_PRIVS) failed: %s", strerror(errno));
            // Non-fatal
        }
    }

    LOGI("DAC applied: running as uid=%d gid=%d", getuid(), getgid());
    return 0;
}

// ---------------------------------------------------------------------------
// applySandbox  –  create mount namespace and set up filesystem isolation
// ---------------------------------------------------------------------------
int ChildMain::applySandbox(const SpawnMsg& msg) {
    // Skip sandbox if NO_SANDBOX flag is set (for debugging)
    if (msg.hasFlag(StartFlags::NO_SANDBOX)) {
        LOGW("Sandbox disabled by NO_SANDBOX flag");
        return 0;
    }

    LOGI("Setting up sandbox for %s", msg.bundleName.c_str());

    // TODO: Implement Android-specific sandbox mounts:
    //
    // 1. Create new mount namespace:
    //    unshare(CLONE_NEWNS)
    //
    // 2. Make root mount private to prevent propagation:
    //    mount("", "/", NULL, MS_REC | MS_PRIVATE, NULL)
    //
    // 3. Bind mount APK directory for app code access:
    //    mount("/data/app/<bundleName>/", "<sandbox>/app/", NULL, MS_BIND, NULL)
    //
    // 4. Bind mount app data directory:
    //    mount("/data/data/<bundleName>/", "<sandbox>/data/", NULL, MS_BIND, NULL)
    //
    // 5. Mount tmpfs for /dev and create minimal device nodes
    //
    // 6. Bind mount shared libraries:
    //    mount("/system/lib64/", "<sandbox>/system/lib64/", NULL, MS_BIND | MS_RDONLY, NULL)
    //
    // 7. Apply OH sandbox profile from JSON config
    //
    // In production, this links against OH appspawn sandbox library:
    //   - SetAppSandboxProperty(msg)
    //   - AppSpawnSandboxCfg_Parse(configPath)

    LOGD("Sandbox setup: TODO – filesystem isolation not yet implemented");
    LOGD("  APK path: %s", msg.apkPath.c_str());
    LOGD("  Native libs: %s", msg.nativeLibPaths.c_str());
    LOGD("  Bundle data: /data/data/%s/", msg.bundleName.c_str());

    return 0;
}

// ---------------------------------------------------------------------------
// applySELinux  –  set the SELinux security context for the process
// ---------------------------------------------------------------------------
int ChildMain::applySELinux(const SpawnMsg& msg) {
    // B.30 (2026-04-29): real adaptation via OH HapContext::HapDomainSetcontext.
    //
    // Prior B.29 attempt failed because child stayed in u:r:appspawn:s0 (parent
    // domain) and couldn't query OH SA 180 (DeviceInfo) / SA 501 (BMS) /
    // SA 4607 (WindowSession) — all selinux denied.  AMS LIFECYCLE_HALF_TIMEOUT
    // 5s killed the child.
    //
    // OH's libhap_restorecon resolves apl + packageName + hapFlags + uid →
    // app domain context (e.g. u:r:normal_hap:s0:c<uid>) via the same lookup
    // table OH native appspawn uses (sehap_contexts file).  After setcon the
    // child has SA query permissions appropriate for its APL.

    // Default APL if TLV didn't supply one (text-format spawn fallback).
    std::string apl = msg.apl;
    if (apl.empty()) {
        apl = "normal";
    }

    ::HapDomainInfo info;
    info.apl = apl;
    info.packageName = msg.bundleName;
    info.hapFlags = msg.hapFlags;
    info.uid = static_cast<uint32_t>(msg.uid);

    LOGI("applySELinux: apl=%s pkg=%s hapFlags=%llu uid=%u",
         info.apl.c_str(), info.packageName.c_str(),
         static_cast<unsigned long long>(info.hapFlags), info.uid);

    ::HapContext hapContext;
    int rc = hapContext.HapDomainSetcontext(info);
    if (rc != 0) {
        LOGE("HapDomainSetcontext failed rc=%{public}d errno=%{public}d (apl=%{public}s pkg=%{public}s) — child remains in %{public}s",
             rc, errno, info.apl.c_str(), info.packageName.c_str(), "u:r:appspawn:s0");
        return rc;
    }
    LOGI("applySELinux: child secon transitioned successfully (apl=%s)",
         info.apl.c_str());
    return 0;
}

// ---------------------------------------------------------------------------
// applyAccessToken  –  set OH AccessToken for permission enforcement
// ---------------------------------------------------------------------------
int ChildMain::applyAccessToken(const SpawnMsg& msg) {
    // B.30 (2026-04-29): real adaptation via OH SetSelfTokenID
    // (libtokensetproc_shared).  AMS ships the AccessTokenIdEx via TLV;
    // applying it lets OH SAs (BMS / DeviceInfo / WindowSession) authorize
    // this process for permissions granted to bundleName at install time.
    //
    // Without this, OH IPC checks (samgr->GetSystemAbility) succeed past
    // SELinux but fail at AccessToken layer with "permission denied" errors.

    // G2.14h (2026-05-01) — when TLV has 0, query AccessTokenKit directly.
    //
    // Why TLV has 0 even after libbms v3 patch (which calls AllocHapToken at install
    // and writes accessTokenId into both top-level applicationInfo and
    // InnerBundleUserInfo for userId 0/100):
    //   AppMS::StartProcess at app_mgr_service_inner.cpp:4814 OVERRIDES the
    //   bundleInfo.applicationInfo.accessTokenId/Ex with the result of
    //   AccessTokenKit::GetHapTokenIDEx(GetUserIdByUid(uid), bundleInfo.name, appIndex).
    //   Empirically that GetHapTokenIDEx call returns 0 for our HelloWorld even
    //   though `atm dump -t -b com.example.helloworld` shows the token registered
    //   at install time. Cause not yet pinned (caller-permission? IPC race?), but
    //   the kit call from CHILD context returns the correct registered token, so
    //   we use that as a self-heal path.
    //
    // History:
    //   2026-04-29 B.31 probe verified: OH AppMS DID send OH_TLV_ACCESS_TOKEN_INFO,
    //   but the value is genuinely 0 (libapk_installer never calls
    //   AccessTokenKit::AllocHapToken at install time).
    //
    // Earlier (pre-G2.14g) behavior was to early-return when 0, leaving the
    // child with appspawn-x's INHERITED system token. This caused
    // JudgeSelfCalled mismatch in OH AMS:
    //   * IPCSkeleton::GetCallingTokenID() returns child's inherited (non-zero) token
    //   * abilityRecord->GetApplicationInfo().accessTokenId stored = 0 (from BMS)
    //   * 非零 != 0 → CHECK_PERMISSION_FAILED (rc=2097177)
    //   * Symptom: AbilityTransitionDone(state=FOREGROUND) rejected → AMS times out
    //     ability load → 30s LIFECYCLE_TIMEOUT kills HelloWorld.
    //
    // Fix: explicitly SetSelfTokenID(0) so child's calling token matches
    // AMS-stored 0. JudgeSelfCalled then sees callingTokenId == tokenID
    // (both 0) and returns true.
    //
    // Long-term P4 fix (still unaddressed): integrate AccessTokenKit into
    // libapk_installer so both BMS and child have a real non-zero HAP token.
    // For HelloWorld baseline, "both 0" is acceptable — JudgeSelfCalled
    // passes; OH services that gate by specific permissions still deny but
    // the lifecycle path is unblocked.
    if (msg.accessTokenIdEx == 0) {
        LOGW("applyAccessToken: accessTokenIdEx==0 in TLV — kernel will reject SetSelfTokenID; skipping "
             "(child keeps parent's inherited token; libbms v3 + AppMS bundleInfo override patch should fix)");
        return 0;
    }

    LOGI("applyAccessToken: id=0x%llx apl=%s",
         static_cast<unsigned long long>(msg.accessTokenIdEx),
         msg.apl.c_str());

    int rc = SetSelfTokenID(msg.accessTokenIdEx);
    if (rc != 0) {
        LOGE("SetSelfTokenID(0x%llx) failed rc=%d",
             static_cast<unsigned long long>(msg.accessTokenIdEx), rc);
        return 0;
    }
    LOGI("applyAccessToken: SetSelfTokenID(0x%llx) OK",
         static_cast<unsigned long long>(msg.accessTokenIdEx));
    return 0;
}

// ---------------------------------------------------------------------------
// initAdapterLayer  –  initialize the Android-OH adapter bridge
// ---------------------------------------------------------------------------
int ChildMain::initAdapterLayer(JNIEnv* env, AppSpawnXRuntime* runtime) {
    LOGI("Initializing adapter layer (OHEnvironment)");

    // Find OHEnvironment via the PathClassLoader that loaded it in the parent.
    // Using env->FindClass here would go through the bootstrap classloader,
    // producing a *different* class object whose <clinit> would re-attempt
    // System.loadLibrary("oh_adapter_bridge") — but the .so is already bound
    // to the parent's PathClassLoader, triggering UnsatisfiedLinkError:
    //   "already opened by ClassLoader 0x1cf; can't open in ClassLoader 0(null)"
    // Going through the cached PathClassLoader returns the same class the
    // parent already initialized (inherited via fork); <clinit> does not re-run.
    jclass ohEnvClass = runtime
        ? runtime->loadClassViaPath(env, "adapter.core.OHEnvironment")
        : env->FindClass("adapter/core/OHEnvironment");
    if (!ohEnvClass) {
        LOGW("OHEnvironment class not found – adapter layer not in classpath");
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        return -1;
    }

    // Get the static initialize() method
    jmethodID initMethod = env->GetStaticMethodID(
        ohEnvClass, "initialize", "()V");
    if (!initMethod) {
        LOGE("OHEnvironment.initialize() method not found");
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        env->DeleteLocalRef(ohEnvClass);
        return -1;
    }

    // Call OHEnvironment.initialize()
    // This loads liboh_adapter_bridge.so, sets up the OH service connections,
    // and registers the adapter service stubs
    env->CallStaticVoidMethod(ohEnvClass, initMethod);

    if (env->ExceptionCheck()) {
        LOGE("Exception during OHEnvironment.initialize():");
        env->ExceptionDescribe();
        env->ExceptionClear();
        env->DeleteLocalRef(ohEnvClass);
        return -1;
    }

    env->DeleteLocalRef(ohEnvClass);
    LOGI("Adapter layer initialized successfully");
    return 0;
}

// ---------------------------------------------------------------------------
// launchActivityThread  –  enter the Android app event loop
// ---------------------------------------------------------------------------
void ChildMain::launchActivityThread(JNIEnv* env, const SpawnMsg& msg,
                                     AppSpawnXRuntime* runtime) {
    // Determine the target class to launch
    std::string targetClass = msg.targetClass;
    if (targetClass.empty()) {
        targetClass = "android.app.ActivityThread";
    }

    LOGI("Launching target class: %s (proc=%s, sdkVersion=%d)",
         targetClass.c_str(), msg.procName.c_str(), msg.targetSdkVersion);

    // Call AppSpawnXInit.initChild(procName, targetClass, targetSdkVersion)
    // This Java method performs:
    //   1. Set the process name (Process.setArgV0)
    //   2. Call RuntimeInit.commonInit() for thread handlers, timezone, etc.
    //   3. Find the target class and invoke its main() method
    //   4. For ActivityThread, this enters Looper.loop() which blocks forever

    // Prefer the parent-cached global ref (resolved via PathClassLoader at
    // runtime startup). Fall back to runtime->loadClassViaPath (also uses
    // the PathClassLoader) or plain FindClass if everything else failed.
    jclass initClass = nullptr;
    jclass cachedGlobal = runtime ? runtime->getAppSpawnXInitClass() : nullptr;
    if (cachedGlobal) {
        // Global ref is valid across fork; use it directly (no local ref needed
        // for invoking static methods, but we'll keep the pattern consistent).
        initClass = cachedGlobal;
    } else if (runtime) {
        initClass = runtime->loadClassViaPath(
            env, "com.android.internal.os.AppSpawnXInit");
    } else {
        initClass = env->FindClass("com/android/internal/os/AppSpawnXInit");
    }
    bool initClassIsLocal = (initClass != cachedGlobal);
    if (!initClass) {
        LOGE("AppSpawnXInit class not found – cannot launch ActivityThread");
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }

        // Fallback: try to launch ActivityThread.main() directly
        LOGW("Attempting direct ActivityThread.main() launch as fallback");

        // Convert dotted class name to JNI format (replace . with /)
        std::string jniClassName = targetClass;
        for (char& c : jniClassName) {
            if (c == '.') c = '/';
        }

        jclass targetJniClass = env->FindClass(jniClassName.c_str());
        if (!targetJniClass) {
            LOGE("Cannot find class %s", jniClassName.c_str());
            if (env->ExceptionCheck()) {
                env->ExceptionDescribe();
                env->ExceptionClear();
            }
            return;
        }

        jmethodID mainMethod = env->GetStaticMethodID(
            targetJniClass, "main", "([Ljava/lang/String;)V");
        if (!mainMethod) {
            LOGE("Cannot find main([String) in %s", jniClassName.c_str());
            if (env->ExceptionCheck()) {
                env->ExceptionDescribe();
                env->ExceptionClear();
            }
            env->DeleteLocalRef(targetJniClass);
            return;
        }

        // Create empty String[] for main() argument
        jclass stringClass = env->FindClass("java/lang/String");
        jobjectArray emptyArgs = env->NewObjectArray(0, stringClass, nullptr);

        LOGI("Calling %s.main() directly", targetClass.c_str());
        env->CallStaticVoidMethod(targetJniClass, mainMethod, emptyArgs);

        if (env->ExceptionCheck()) {
            LOGE("Exception in %s.main():", targetClass.c_str());
            env->ExceptionDescribe();
            env->ExceptionClear();
        }

        env->DeleteLocalRef(emptyArgs);
        env->DeleteLocalRef(stringClass);
        env->DeleteLocalRef(targetJniClass);
        return;
    }

    // Use AppSpawnXInit.initChild for proper initialization
    jmethodID initChildMethod = env->GetStaticMethodID(
        initClass, "initChild",
        "(Ljava/lang/String;Ljava/lang/String;I)V");
    if (!initChildMethod) {
        LOGE("AppSpawnXInit.initChild() method not found");
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        if (initClassIsLocal) env->DeleteLocalRef(initClass);
        return;
    }

    // 2026-07-09 DIRECT-LAUNCH: arm the BMS/AMS-free launch path for noice by
    // exporting env that AppSpawnXInit.initChild reads to post directLaunchNoBms.
    // apkPath uses msg.apkPath when the spawn carried it, else the staged default.
    if (msg.bundleName == "com.github.ashutoshgngwr.noice") {
        const char* apk = !msg.apkPath.empty() ? msg.apkPath.c_str()
                                               : "/data/local/tmp/asx/noice.apk";
        setenv("ASX_APK_PATH", apk, 1);
        setenv("ASX_LAUNCH_PKG", msg.bundleName.c_str(), 1);
        setenv("ASX_LAUNCH_ACTIVITY",
               "com.github.ashutoshgngwr.noice.activity.MainActivity", 1);
        setenv("ASX_DIRECT_LAUNCH", "1", 1);
        LOGI("[DIRECT-LAUNCH] armed for noice: apk=%s", apk);
    }

    // Convert arguments to Java strings
    jstring jProcName = env->NewStringUTF(msg.procName.c_str());
    jstring jTargetClass = env->NewStringUTF(targetClass.c_str());

    LOGI("Calling AppSpawnXInit.initChild(\"%s\", \"%s\", %d)",
         msg.procName.c_str(), targetClass.c_str(), msg.targetSdkVersion);
    LOGI("[CHILD_CK] CK_BEFORE_initChild_call (about to enter Java)");

    // 2026-07-09: force ART's fault handler to the front of the signal chain right
    // before the first post-fork Java call, in case libdfx re-installed its handler
    // during the child's init (postForkChild/onChildInit/adapter) and the 20ms
    // re-assert hasn't re-claimed it yet. This is the boundary where the child dies.
    {
        void (*ensureFront)(int) =
            reinterpret_cast<void(*)(int)>(dlsym(RTLD_DEFAULT, "EnsureFrontOfChain"));
        if (ensureFront) { ensureFront(SIGSEGV); ensureFront(SIGBUS); }
        fprintf(stderr, "[CHILD] EnsureFrontOfChain(SEGV/BUS) before initChild: %s\n",
                ensureFront ? "done" : "symbol-missing"); fflush(stderr);
    }
    // 2026-07-09 DIAG: is the initChild-entry death a SEGV or a clean exit? Force
    // SIG_DFL for SEGV/BUS just for this boundary — if a crash dump appears, it's a
    // fault (and names the PC); if the death stays clean (exit, no dump), it's a
    // different mechanism (ART clean-exit / abort). REMOVE after diagnosis.
    if (getenv("ASX_CHILD_SIGDFL_DIAG")) {
        signal(SIGSEGV, SIG_DFL);
        signal(SIGBUS, SIG_DFL);
        fprintf(stderr, "[CHILD] SIG_DFL SEGV/BUS diag armed before initChild\n"); fflush(stderr);
    }

    env->CallStaticVoidMethod(initClass, initChildMethod,
                              jProcName, jTargetClass,
                              static_cast<jint>(msg.targetSdkVersion));

    // 2026-04-29 B.31: stderr was /dev/null under init service so we never
    // saw whether initChild returned.  Switched to LOGI/LOGE (hilog) so we
    // know if Java side returned, threw, or hung in Looper.loop().
    LOGI("[CHILD_CK] CK_AFTER_initChild_call (Java returned!)");
    fprintf(stderr, "[CM-CK] initChild CallStaticVoidMethod RETURNED (exc=%d) — "
            "ActivityThread.main/Looper.loop exited or threw\n",
            (int)env->ExceptionCheck()); fflush(stderr);
    if (env->ExceptionCheck()) {
        LOGE("[CHILD_CK] Exception in AppSpawnXInit.initChild():");
        // 2026-05-02 G2.14r: ExceptionDescribe() writes to ART's stderr which
        // appspawn-x maps to /dev/null under "console":0 cfg.  Manually fetch
        // exception class name + message via JNI and log to hilog.
        jthrowable thr = env->ExceptionOccurred();
        env->ExceptionClear();
        if (thr != nullptr) {
            jclass thrClass = env->GetObjectClass(thr);
            jmethodID getMsg = env->GetMethodID(thrClass, "getMessage", "()Ljava/lang/String;");
            jclass classClass = env->GetObjectClass(thrClass);
            jmethodID getName = env->GetMethodID(classClass, "getName", "()Ljava/lang/String;");
            jstring jClassName = (jstring)env->CallObjectMethod(thrClass, getName);
            jstring jMsg = getMsg ? (jstring)env->CallObjectMethod(thr, getMsg) : nullptr;
            if (env->ExceptionCheck()) env->ExceptionClear();
            const char* className = jClassName ? env->GetStringUTFChars(jClassName, nullptr) : "<no class>";
            const char* msg = jMsg ? env->GetStringUTFChars(jMsg, nullptr) : "<no message>";
            LOGE("[CHILD_CK] Exception type: %{public}s", className);
            LOGE("[CHILD_CK] Exception message: %{public}s", msg);
            fprintf(stderr, "[CM-EXC] initChild threw: %s: %s\n", className, msg); fflush(stderr);
            if (jClassName) env->ReleaseStringUTFChars(jClassName, className);
            if (jMsg) env->ReleaseStringUTFChars(jMsg, msg);
            // dump first 16 stack frames
            jmethodID getStackTrace = env->GetMethodID(thrClass, "getStackTrace",
                                                      "()[Ljava/lang/StackTraceElement;");
            if (getStackTrace) {
                jobjectArray frames = (jobjectArray)env->CallObjectMethod(thr, getStackTrace);
                if (env->ExceptionCheck()) env->ExceptionClear();
                if (frames) {
                    jsize n = env->GetArrayLength(frames);
                    if (n > 16) n = 16;
                    jclass steClass = env->FindClass("java/lang/StackTraceElement");
                    jmethodID toStr = env->GetMethodID(steClass, "toString", "()Ljava/lang/String;");
                    for (jsize i = 0; i < n; ++i) {
                        jobject ste = env->GetObjectArrayElement(frames, i);
                        jstring jStr = (jstring)env->CallObjectMethod(ste, toStr);
                        const char* s = env->GetStringUTFChars(jStr, nullptr);
                        LOGE("[CHILD_CK]   at %{public}s", s);
                        env->ReleaseStringUTFChars(jStr, s);
                        env->DeleteLocalRef(jStr);
                        env->DeleteLocalRef(ste);
                    }
                    env->DeleteLocalRef(frames);
                }
            }
            env->DeleteLocalRef(thr);
        }
        LOGE("[CHILD_CK] Exception cleared — child will exit");
    } else {
        LOGE("[CHILD_CK] AppSpawnXInit.initChild() returned WITHOUT exception (event loop exited unexpectedly)");
    }

    env->DeleteLocalRef(jProcName);
    env->DeleteLocalRef(jTargetClass);
    if (initClassIsLocal) env->DeleteLocalRef(initClass);
}

} // namespace appspawnx
