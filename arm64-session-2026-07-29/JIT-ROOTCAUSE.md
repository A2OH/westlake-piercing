# JIT root cause — FOUND (2026-08-08, headless bench)

## The finding (definitive, deterministic)
A minimal **headless pure-arithmetic bench** (com.android.internal.os.WlJitBench, in the BCP,
run from child_main on a dedicated attached thread after JIT-enable — APPSPAWNX_JIT_BENCH=1)
proves the JIT failure is NOT recursion, NOT coroutines, NOT atomics, NOT the app:

    round 0 (interpreted): run(2_000_000) returns a real checksum — WORKS.
    round 1 (now compiled): java.lang.StackOverflowError: stack size 15MB  (16MB thread stack)

`run()` in the leaf variant is a self-contained loop with ZERO method calls and ZERO recursion —
it cannot consume 15MB. So **the JIT-compiled method's stack-overflow check misfires on ENTRY**:
the first compiled frame immediately "overflows". The SOE message tracks the thread stack size
(16MB->"15MB", 64MB->"63MB", 8MB main->"8182KB"), i.e. reserved ~1MB, check trips with SP near
the top of a fresh stack. The interpreter check on the SAME thread/stack passes (round 0), so
stack_end is correct for the interpreter — the COMPILED prologue's limit/probe is what's wrong.

## This retroactively explains every prior JIT symptom
- noice main thread SOE at initChild (8182KB) the moment JIT compiles a startup method.
- "eats 63MB / unbounded recursion" — WRONG reading; it never actually recursed. The check just
  trips on the first compiled frame and the huge "size" is the message echoing the stack size.
- coroutine/AtomicInteger "fingerprint" — those were just the hot methods that compiled first.

## Fix attempts
- APPSPAWNX_EXPLICIT_CHECKS=1 (-Ximplicit-checks:none -Xexplicit-checks:all): APPLIED (§561 marker
  in hilog x3) but did NOT fix it — still SOE. So it is not merely implicit(guard-page)-vs-explicit;
  the stack LIMIT the compiled code compares against (or how the JIT compiler computes the check)
  is wrong for this port's threads.
- Bigger stack only changes the reported number.

## The reproducer (use this to fix)
Deterministic, isolated, no lottery, no noice UI:
  APPSPAWNX_FORCE_JIT=1 APPSPAWNX_JIT_VERBOSE=1 APPSPAWNX_JIT_DELAY_MS=40000 APPSPAWNX_JIT_BENCH=1
  [APPSPAWNX_JIT_BENCH_STACK_MB=16] [APPSPAWNX_EXPLICIT_CHECKS=1]
Watch: "[JIT-BENCH] round 0 checksum=..." (interp OK) then round 1 SOE (compiled check misfire).
jitbench/src/.../WlJitBench.java; merge into oh-adapter-framework.jar via tools/DexMerge; the
runner is wl_jit_bench_start()/wl_jit_bench_thread() in appspawn-x child_main.cpp.
⚠️CONFOUND: noice's own main thread also SOEs at initChild once JIT is live, and can kill the child
before the bench logs — to read the bench cleanly, need noice's ActivityThread quiescent or a
truly headless launch.

## NEXT
Inspect the JIT-compiled prologue's stack check: disassemble a compiled method from the code cache
(or read the arm64 codegen path) to see whether it does `str wzr,[sp,#-N]` (implicit probe) or
`cmp sp, [tr,#stack_end_off]` (explicit), and what limit/register it uses. Then correct the limit
(Thread::tlsPtr_.stack_end / the compiler's reserved-bytes) so the first compiled frame passes.
