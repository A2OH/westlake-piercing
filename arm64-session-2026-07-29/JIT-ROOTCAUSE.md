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

## Prologue disassembled (2026-08-08) + geometry — the SOE is SPURIOUS, not a simple bad limit
Dumped the JIT-compiled WlJitBench.run() from the live code cache (/memfd:jit-cache) via a memread
helper (jitbench/memread.c) on a headless parked process (APPSPAWNX_HEADLESS_BENCH=1 skips noice's
ActivityThread so the process stays alive — that removed the confound of noice's main thread dying).

Compiled prologue, BOTH baseline (kind2) and optimized (kind0):
    sub x16, sp, #0x2000     ; x16 = sp - 8192   (kStackOverflowReservedBytes)
    ldr wzr, [x16]           ; ART implicit stack-overflow probe
    ... normal frame setup (small frame ~176-336B) ...

Thread geometry at the fault (logged from the bench thread):
    stack lo=0x7f0a4da000  hi=0x7f0b4d8000  (16 MB, ONE contiguous rw-p, no internal PROT_NONE)
    SP at round-1 call ≈ 0x7eeda17648  (near the TOP)
    => sp-0x2000 is deep inside mapped memory; the probe CANNOT fault on this stack.

SOE stack trace (via getStackTrace in the bench): depth=1, "at WlJitBench.bench" — i.e. thrown at
the ENTRY of the compiled callee before its frame exists (attributed to the caller).

### Conclusions (correcting the earlier "fix the stack_end limit" plan)
- It is NOT the implicit probe (stack fully mapped, SP near top).
- It is NOT a simple wrong Thread::stack_end: the interpreter uses stack_end and round 0 (interpreted)
  works on the SAME thread/stack.
- APPSPAWNX_EXPLICIT_CHECKS=1 does NOT change the JIT codegen (prologue identical) — that option is
  for the AOT compiler; the JIT always emits the implicit probe. So that lever is a dead end.
- => The SOE is thrown by an EXPLICIT check on the interpreter->compiled INVOKE transition (or in
  art_quick_invoke_stub / the compiled-entry bridge), spuriously, only when the callee is compiled.
  round 0 (interp->interp) works; round 1 (interp->COMPILED run) throws at entry.

### NEXT (decisive): instrument ThrowStackOverflowError @0x8574e8
Binary-patch/trampoline it to log SP, LR (caller), and Thread::stack_end at throw time. That names
the exact check (implicit fault handler vs explicit invoke-path check) and the limit it used. Then
correct THAT limit/path. The headless bench is the deterministic reproducer (round 0 OK, round 1
throws) — no noice, no lottery.
