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

## ★DECISIVE: OSR entry works, normal prologue entry fails => the PROLOGUE PROBE is the trigger
round 0 returns a correct checksum because run() is entered INTERPRETED and then OSR-compiled: OSR
jumps INTO the compiled body mid-loop and SKIPS the prologue. round 1 calls the now-compiled run()
via its NORMAL entry, which runs the prologue (`sub x16,sp,#0x2000; ldr wzr,[x16]`) and throws SOE.
Same compiled code, same thread, same stack — the ONLY difference is whether the prologue runs.
=> The prologue stack-overflow check is the trigger, full stop. (The geometry "it can't fault"
paradox is unresolved — likely ART's fault-handler guard-region bookkeeping misclassifies the probe
address as an overflow, or stack_end used by the handler differs from the mapped stack — but the
OSR-vs-normal split proves the prologue is where it dies.)

## FIX DIRECTION (concrete)
The probe is emitted by the JIT codegen frame-entry (CodeGeneratorARM64::GenerateFrameEntry /
GenerateStackOverflowCheck). Options, in order:
 1. Binary-patch that codegen path so JIT-compiled prologues either skip the probe or use a correct
    limit (one libart patch fixes ALL compiled methods; precedent: §534/§551 code-cave patches).
 2. Or fix ART's fault-handler guard-region / stack_end bookkeeping so the probe address is not
    misclassified as overflow.
Reproduce/verify with the headless bench (round 0 OK, round 1 SOE). Instrument ThrowStackOverflow
Error @0x8574e8 (log LR) only if needed to choose between 1 and 2.

## §578 — codegen probe patched & VERIFIED, but the probe was a RED HERRING
Patched CodeGeneratorARM64::GenerateFrameEntry @0xb7ee4c: `movz w1,#0x2000`->`movz w1,#0`
(reserved bytes 8192->0), so the JIT emits `sub temp,sp,#0; ldr wzr,[temp]` == `ldr [sp]` (SP is
always mapped, can never fault). VERIFIED in freshly-compiled code: prologue is now
`d10003f0 (sub x16,sp,#0); b940021f (ldr wzr,[x16])`.
RESULT: the headless leaf bench STILL throws StackOverflowError at round 1. So the compiled
prologue probe is NOT the cause. (recipes/patch578.py; reverted from the deployed baseline.)

## The REAL culprit: an inlined explicit CheckStackOverflow on the interp->COMPILED invoke path
interpreter::CheckStackOverflow @0xa51268 (inlined into the invoke path, no direct callers):
    x9 = self->stack_end_           ; [Thread+160]
    x8 = runtime[+1353] (a byte)    ; reserved-units
    x19 = stack_end + (x8<<13) + space(x1)
    if (SP < x19) ThrowStackOverflowError
Round 0 (interp->interp run) passes; round 1 (interp->COMPILED run) throws. Same thread, same
stack_end, same runtime byte — so the differing term is **space (x1)**: the interpreter reserves a
spuriously huge `space` when the callee is COMPILED, so SP(near top) < stack_end + reserved + space
=> spurious SOE. The runtime[+1353] byte is NOT globally huge (round 0 uses it and passes).

### NEXT (the actual fix)
Find where the interp->compiled invoke path (ArtInterpreterToCompiledCodeBridge / DoCall's compiled
branch / art_quick_invoke_stub setup) computes the `space` passed to the inlined CheckStackOverflow,
and correct it (it's reading the compiled frame size / required stack wrong — likely a bad field
offset or a sign/scale error yielding ~stack-size). That is the single value to fix. Reproduce with
the headless leaf bench (round 0 OK / round 1 SOE); §578 confirms it is NOT the compiled prologue.

## §579 — localized to the INTERPRETER->COMPILED INVOKE TRANSITION (systematic elimination)
Ruled out, each tested on the headless leaf bench (round 0 OK / round 1 SOE "at bench"):
 - §578 compiled prologue probe: patched reserved 8192->0, VERIFIED in compiled code, still SOE.
 - §579 invoke_depth cap (EnterInterpreterFromInvoke @0xaaf4ec cmp#50->#4095): still SOE.
   (only 1 of 25 cmp#50 sites is an SOE depth-guard; raising it changed nothing.)
 - §579b bench() marked NON-COMPILABLE (kAccCompileDontBother on its ArtMethod): so INTERPRETED
   bench calls COMPILED run() in a loop — STILL SOE at round 1. run() compiles (kind1+0), round 0
   (interp->interp) works, round 1 (interp->COMPILED run) throws.

=> DEFINITIVE: the spurious SOE is thrown on the INTERPRETER->COMPILED invoke transition — an
interpreted caller invoking a JIT-compiled callee. NOT the callee's prologue (patched away), NOT
recursion, NOT bench being compiled. OSR entry into the same compiled code works; only the normal
interp->compiled invoke throws.

### Throw site candidates (next: disassemble + fix)
ArtInterpreterToCompiledCodeBridge @0xa51870 has NO stack check itself, so the throw is in what it
calls:
  - art::ArtMethod::Invoke @0x876c0c  ← has a WESTLAKE emutls `stale_quick_repair_count` (repairs
    "stale" quick/compiled entry points). Prime suspect: on a freshly-JIT-compiled method it may
    mis-handle the quick entry / recompute stack wrong.
  - art_quick_invoke_stub @0xfda330 / art_quick_invoke_stub_internal @0xf9e3dc (hand-written asm).
Instrument ThrowStackOverflowError @0x8574e8 to log LR (one code-cave trampoline) to pin the exact
site, then fix. Reproducer: headless leaf bench, APPSPAWNX_JIT_BENCH_NOCOMPILE_OUTER=1 makes it the
clean interp->compiled case.

## ★★★ROOT CAUSE FOUND (2026-08-08): the quick-invoke ABI is a STUB (aborts)
`art_quick_invoke_stub_internal` — the entrypoint that invokes JIT/AOT-compiled code from
C++/the interpreter — is a westlake ABORT STUB:
    art_quick_invoke_stub_internal @0xf9e3dc: fwrite("FATAL: stub entrypoint
    art_quick_invoke_stub_internal called\n"); abort();
It is one of only 3 such stubs (also ExecuteMterpImpl, art_quick_invoke_polymorphic_with_hidden_
receiver). So the westlake port NEVER IMPLEMENTED the quick calling convention — a compiled method
CANNOT be invoked via the normal path. The only way compiled code runs is **OSR** (on-stack
replacement patches the interpreter to jump INTO compiled code mid-method, bypassing the stub).

### This explains EVERY symptom of the whole investigation
 - OSR entry works (round 0): OSR doesn't use the invoke stub.
 - Normal invoke of a compiled method throws/fails (round 1): westlake routes around the abort stub
   by forcing interpretation, and that fallback throws the spurious SOE when the callee already has
   compiled code.
 - `compiled=0` survivors: nothing compiled => nothing invoked via the missing stub.
 - noice dies once the JIT compiles its hot methods: they then get invoked normally => broken path.
 - The compiled prologue probe, invoke_depth cap, `space`, stack_end — ALL red herrings. The stack
   is fine; the problem is that compiled methods are unreachable by normal call.

### THE REAL FIX (well-defined but substantial)
Implement art_quick_invoke_stub_internal for arm64 (the quick calling convention): build the quick
frame, marshal args from the ShadowFrame/arg array per the shorty, set the thread/marking registers,
`blr` the compiled entry, store the JValue result, tear down. This is the standard AOSP
quick_entrypoints_arm64 stub — port it into libart via a code cave / a linked-in implementation
(the deployed libart has room; precedent: binary-patched entrypoints). Until then the JIT can only
ever help hot LOOPS (OSR), never method-call-heavy code, and enabling it app-wide is unsafe (any
compiled method that gets normally invoked hits the fallback SOE).

## ⛔CORRECTION (2026-08-08): the "abort-stub root cause" was WRONG
art_quick_invoke_stub_internal @0xf9e3dc (the abort stub) has ZERO callers — it is DEAD code, an
unused alternate entry that was safely stubbed. ArtMethod::Invoke @0x876c0c calls the REAL,
fully-implemented art_quick_invoke_stub @0xfda330 (bl at 0x876d44), which marshals args by shorty
and calls the compiled entry — and it NEVER calls ThrowStackOverflowError (verified: 0 throw calls
in 0xfda330..0xfda5e0). So the quick-invoke ABI IS implemented and IS used; implementing _internal
would fix nothing.

=> The SOE root cause is STILL NOT identified. Disproven theories this session, each with evidence:
   recursion, the compiled prologue probe (§578, patched+verified), the invoke_depth cap (§579),
   the "space"/CheckStackOverflow term, AND the abort-stub. Do NOT build on any of them.

### The one reliable next step (stop guessing)
Instrument ThrowStackOverflowError @0x8574e8 with a code-cave trampoline that logs LR (x30) via a
write(2,...) syscall, run the headless bench (deterministic: round 0 OK / round 1 SOE), and read the
caller address. That NAMES the throwing instruction directly — every indirect deduction this session
has been wrong at least once, so only direct capture should be trusted from here.

## ★★★THE ACTUAL MECHANISM (2026-08-08, via LR capture — the reliable method finally used)
Instrumented ThrowStackOverflowError @0x8574e8 with a code-cave trampoline (recipes/patch580.py +
cave.s) that logs the caller's LR to stderr. Ran the headless bench. Captured LR=runtime 0x7fbced19f4;
libart base 0x7fbc480000 => **file vaddr 0xa519f4**, inside ArtInterpreterToCompiledCodeBridge,
right after `bl ArtInterpreterToInterpreterBridge` (the SOE is a tail-throw from inside it).

So for a COMPILED callee, ArtInterpreterToCompiledCodeBridge @0xa51870 does NOT run the compiled
code — it REDIRECTS to ArtInterpreterToInterpreterBridge @0xabaf84 (force-interpret). That bridge's
stack check @0xabafb4:
    ldr x8,[self,#160]=stack_end ; ldrb w9,[runtime,#1353] ; add x8,x8,w9,lsl#13 ; cmp x29,x8 ; b.lo throw
fires spuriously (self->stack_end_ is wrong-high for the thread; SP is actually near the top).

### §581: neutralizing that b.lo (@0xabafb8 -> nop) REMOVES the SOE
Confirmed on the bench: round 1 no longer throws (SOE=0). BUT it then HANGS (round 1 never completes,
process alive, no progress) — because the force-interpret path re-interprets the already-compiled
run() and (likely) loops on OSR-into-already-compiled. So neutralizing the check only moves the
symptom.

### THE REAL PICTURE
westlake's ArtInterpreterToCompiledCodeBridge FORCE-INTERPRETS compiled methods instead of invoking
their compiled code. Consequences:
 - The JIT provides ZERO benefit on normal method calls (they're re-interpreted).
 - Only OSR ever runs compiled code (hot loops within one method).
 - The force-interpret path is buggy: spurious SOE (the stack check), and a hang when the target is
   already compiled.

### THE REAL FIX
Make ArtInterpreterToCompiledCodeBridge actually invoke the compiled code (call the REAL
art_quick_invoke_stub @0xfda330 with the method's quick entry point) for compiled callees, instead of
redirecting to ArtInterpreterToInterpreterBridge. That is the change that makes the JIT actually run
under load. Next: examine the bridge's interpret-vs-compiled decision (0xa518a4..0xa51934) and route
compiled methods to the quick stub. Reproducer: headless bench.

---

## §583–§586 DEFINITIVE: the JIT compiles fine but this libart cannot EXECUTE compiled code

Everything below is from **direct capture on the deterministic headless bench**
(`APPSPAWNX_FORCE_JIT=1 APPSPAWNX_HEADLESS_BENCH=1 APPSPAWNX_JIT_BENCH=1 APPSPAWNX_JIT_BENCH_STACK_MB=16`),
not deduction. Every earlier "root cause" that was a guess is superseded by these measurements.

### §582–§583 stack_end is CORRECT; the SOE is a REAL 3-frame infinite recursion
- §582 measured `self->stack_end_` = 0x7ef3e9c000 = stack_lo + 0x5000 (20 KB) — **correct**. So the
  "wrong stack_end" theory is dead; the SOE check is doing its job.
- §583 frame-walk trampoline captured the return-address cycle at the throw (base 0x7f80800000):
  ```
  0xab0684  Execute                       (after `bl ToCompiledCodeBridge`)
  0xa519f4  ArtInterpreterToCompiledCodeBridge (after `bl ToInterpreterBridge`)
  0xabaff4  ArtInterpreterToInterpreterBridge  (after `bl Execute`)
  → repeats until the 16 MB stack is exhausted
  ```
  So the recursion is **Execute → ToCompiledCodeBridge → ToInterpreterBridge → Execute → …**. For a
  COMPILED callee, ToCompiledCodeBridge force-interprets (via ToInterpreterBridge), which re-enters
  Execute, which re-routes the same compiled method back to ToCompiledCodeBridge. Infinite.

### The gate: Execute @0xab0650 → Jit::CanInvokeCompiledCode
Execute calls `Jit::CanInvokeCompiledCode(method)` @0x95fcd8; if true it pops the shadow frame and
calls ToCompiledCodeBridge (0xab0680). ToCompiledCodeBridge, for a **non-native** method with
caller==null, goes a51934→a51938→a5193c→a519f0 `bl ToInterpreterBridge` (force-interpret). Native
methods instead go to a51a28 (run the entry point). So JIT-compiled **Java** methods are never run.

### §585 (CanInvokeCompiledCode→false) breaks the recursion → then §574 (OSR off) → bench COMPLETES
- §585 alone: rounds 0,1,2 progress (first time past round 1, SOE=0) but then **spins at round 2**
  (bench thread pegged 371 ticks/3s). The spin is the OTHER compiled-execution path — OSR into the
  now-compiled run().
- §585 + §574 (no-op `Jit::MaybeDoOnStackReplacement` @0x9602d8): bench runs cleanly to completion
  (round 2→40→80→…→280→400, correct checksums, SOE=0, no spin). **This proves both compiled-execution
  paths are the blockers**: invoke-bridge (recursion) AND OSR (spin). With both disabled, execution is
  pure interpretation → stable but **zero JIT benefit** (compiled code never runs).

### §586 (route non-native invokes to the real compiled path a51a28) → correct but 64× SLOWER
- The JIT DOES publish compiled code to `ArtMethod+24` (JitCodeCache::Commit → UpdateMethodsCode,
  verified intact @0x95585c; UpdateMethodsCodeImpl @0x6e49bc writes method+24). The a51a28 path
  loads+validates that entry and calls it.
- §586 flips a51938 `tbnz w8,#8,a51a28` → unconditional `b a51a28`, sending compiled Java methods to
  that path. Result: **no crash, correct checksums, but ~48 s/round vs 0.75 s interpreted (64×
  slower)** — because jumping to a51a28 also bypasses the compilation-enqueue (a51a1c), so run() never
  actually compiles and every invoke pays the heavyweight compiled-ABI → interpreter-bridge round trip.

### CONCLUSION (fully verified, three independent execution paths)
The JIT **compiler works** (correct machine code, published to entry points). This specific
binary-only westlake libart **cannot execute compiled code** by any path:
  1. Invoke via ToCompiledCodeBridge → **infinite recursion / SOE** (force-interprets, re-enters Execute).
  2. OSR via MaybeDoOnStackReplacement → **infinite spin**.
  3. Real compiled-invoke path (a51a28) → **correct but 64× slower** (and starves compilation).
This is the "compiler and runtime don't agree" hypothesis, **confirmed** across three mechanisms.
Making compiled code actually run fast requires rebuilding libart so compiler↔runtime agree (⛔the
deployed `e1af9bb5…`/`dc01de55…` is not reproducible from source) or a rebuild-scale reimplementation
of the compiled-invoke ABI. **Binary patching cannot get there.**

**Therefore the performance path is NOT the JIT** — it is the interpreter-tax reductions already
shipped (§532 musl `strstr`; §575/§576 PFCUT redundant-hook removal). The JIT-enable hack (§528)
should stay OFF: enabling it only burns CPU compiling code that can never run, and risks the recursion
crash. All experimental patches (§584–§586, §574, §585) are REVERTED; board restored to the §576
baseline (libart md5 `dc01de5509ae10a426370f2002b59294`, framework.jar `3babe086…`) and verified with a
live launch (screenshot 124523 bytes, side-channels up).

---

## §587 STATIC RE-AUDIT (2026-08-09) — root cause sharpened; one §586 claim RETRACTED

Re-read the whole investigation and re-derived the mechanism from disassembly alone. All sites below
were verified **byte-identical between `libart_deployed.so` and the deployed `libart_576.so`**
(`dc01de55…`), so the analysis applies to what is actually running.

### The two facts that pin the root cause
1. **`Jit::CanInvokeCompiledCode` @0x95fcd8 is three instructions:**
   ```
   ldr x0,[x0,#8]     ; jit->code_cache_
   ldr x1,[x1,#24]    ; method->entry_point_from_quick_compiled_code_
   b   JitCodeCache::ContainsPc
   ```
   It returns true **iff the method's entry point already points into the JIT code cache**. So when
   the gate fires, JIT code *is* installed at the entry point — confirmed, not inferred. (This also
   independently confirms `JitCodeCache::Commit → UpdateMethodsCode` really does publish code.)
2. **`ArtInterpreterToCompiledCodeBridge` @0xa51870 has EXACTLY ONE CALLER** — `Execute` @0xab0680,
   reached only after that gate passes (`tbz w0,#0` @0xab0654 skips it otherwise).

### => The root cause, stated exactly
The bridge exists *solely* to run compiled code, and is *only* entered for methods that provably have
compiled code — yet its non-native path (a51934 → a51938 → a5193c → a519f0) **never reads the entry
point at all** and unconditionally force-interprets via `ArtInterpreterToInterpreterBridge`. Only
`tbnz w8,#8` (kAccNative) reaches the real invoke path at a51a28.

So westlake **gutted the callee side of the compiled-invoke path but left the caller-side gate that
routes methods into it intact.** Force-interpreting does not change the entry point, so the gate's
condition stays true and the same method is routed back in forever:
`Execute → ToCompiledCodeBridge → ToInterpreterBridge → Execute → …` (frame-walk confirmed, §583).
The StackOverflowError is a **correct** stack check catching this real recursion — not a misfire.

### ⛔RETRACTION: the §586 slowdown explanation was WRONG
§586 (a51938 `tbnz w8,#8,a51a28` → unconditional `b a51a28`) was reported as "64× slower because it
bypasses the compile-enqueue at a51a1c, so run() never compiles and every invoke pays a heavyweight
compiled-ABI→interpreter round trip on uncompiled methods." **That cannot be right**: with a single
caller gated on `CanInvokeCompiledCode`, *every* method reaching a51a28 under §586 had live JIT code
at its entry point. There is no uncompiled-method case on that path. Also note Execute passes
`caller = xzr` (@0xab0674), so the bridge always jumps a518a0 → a51934, skipping the hotness/enqueue
block entirely — with or without §586. The enqueue was never on this path.

=> **The §586 64× slowdown is UNEXPLAINED.** Do not repeat the "never compiles" story.

### What this changes about §586's status
§586 is not a wrong-shaped fix — it is the **semantically correct** one (route compiled methods to the
real compiled-entry path), and it produced **correct checksums**. It is a functionally-correct fix with
an unexplained performance pathology. That is a much better starting point than "dead end", and it is
where any future attempt should resume: instrument the a51a28 → a51b50 path (arg marshaling walks the
shorty/descriptor per call; a51f18 is the entry-point-validation-failure branch and was never examined)
to find where the time goes on the deterministic headless bench.

### Unchanged conclusions
- OSR (`MaybeDoOnStackReplacement` @0x9602d8) independently spins — a second broken execution path.
- §585+§574 (both paths off) = stable, correct, but pure interpretation ⇒ zero JIT benefit.
- Keep §528 JIT-enable OFF; the shipped perf path remains §532 + §575/§576.

---

## ★★★★★§588 TRUE ROOT CAUSE (2026-08-09, codex-assisted): compiled code is NATIVE-ONLY by design

A second-opinion review (codex gpt-5.6-sol, max effort) caught a **fatal flaw in my §586 experiment**,
and following it up produced the real answer. This supersedes §583–§587's verdict.

### The experimental flaw
§586 was measured with **OSR still enabled**, yet §585 had already shown OSR independently enters a
pathological CPU-bound state. So the "64× slower" number **conflated two variables**. The 2×2 matrix
was missing its decisive cell:

| config | compiled entry | OSR | result |
|---|---|---|---|
| §585+§574 | off | off | pure interpret — 50k rounds, **193 s** |
| §585 alone | off | on  | spin |
| §586 alone | on  | on  | "64× slower" ← **CONFOUNDED, retracted** |
| **§586+§574** | **on** | **off** | **stable, correct, 50k rounds, 194 s** ← was never run |

**§586+§574 is NOT 64× slower — it is exactly interpreter speed (194 s vs 193 s).** The 64× was OSR.

### The measurement that pins it
With `APPSPAWNX_JIT_VERBOSE=1`, the JIT **does compile the method under test**:
```
Compiling method long com.android.internal.os.WlJitBench.run(int) kind=CompilationKind(1)
Compiling method long com.android.internal.os.WlJitBench.run(int) kind=CompilationKind(0)
```
So: compiled code exists, entry point is published (`CanInvokeCompiledCode` returns true), §586 routes
to the real path — **and it still runs at exactly interpreter speed.** Something interprets it anyway.

### ROOT CAUSE: every layer gates compiled execution on kAccNative (bit 8)
`ArtMethod::Invoke` @0x876c0c — the function §586's path tail-calls (@0xa51c78) — contains:
```
876cfc: tbnz w9,#8, 0x876d10     ; NATIVE?  -> ... -> 876d44: bl art_quick_invoke_stub  (FAST)
        (else, non-native Java)  ->  876d74: bl interpreter::EnterInterpreterFromInvoke  (INTERPRET)
```
The **same** native-only gate as the bridge (`tbnz w8,#8` @0xa51938). Also at 0x876e30 (taken when the
`Runtime+706` byte is 0) it again sets up `EnterInterpreterFromInvoke` with `w5=1` (stay_in_interpreter).

=> **`art_quick_invoke_stub` is reserved for NATIVE (JNI) methods. Compiled JAVA code is unreachable at
every layer** — the interpreter bridge AND `ArtMethod::Invoke`. This is systematic and deliberate, not
a contradiction between two sites (§587's framing was too narrow).

### Why westlake did this (coherent with the PFCUT audit)
The interpreter carries **~130 app-compat hooks inline in `DoCall`/`DoCallCommon`**. Executing compiled
Java code would bypass all of them. Pinning Java execution to the interpreter is what keeps those hooks
live. **The JIT is architecturally incompatible with this build's design**, which is why the JIT was
force-disabled at source level in the first place (§528 undoes exactly that).

### Corrected conclusions
- ⛔"the JIT compiles but the runtime cannot execute compiled code" — RIGHT conclusion, WRONG mechanism.
  It is not three broken paths; it is **one design rule: compiled == native-only**.
- ⛔The §586 "64× slower" and its explanation are **both retracted** (confound + wrong cause).
- ✅§586+§574 is **safe and correct** (50k rounds, correct checksums, no SOE) but delivers **zero
  speedup**, because `ArtMethod::Invoke` interprets non-native methods regardless.
- A real JIT would require patching *every* kAccNative gate on the execution path **and** would then
  bypass the ~130 compat hooks the app depends on. That is a rebuild-scale redesign, not a patch.
- ★Unchanged: keep §528 JIT-enable OFF; perf path = §532 + §575/§576.

### Method note (codex's warning, worth keeping)
Scanning `bl 0xa51870` proves *direct* call sites only — not `b`/tail-calls, ADRP+ADD address-taking,
relocations or function-pointer tables. Don't treat "single caller" as a production invariant without
checking those. Also: correct benchmark checksums prove the integer path only, not full quick-ABI
semantics (reference args under moving GC, wide/FP, synchronized, exceptions, deopt, proxies).

---

# ★★★★★§594-§597 (2026-08-10): THE JIT NOW RUNS COMPILED CODE UNDER HEAVY LOAD

§588 concluded "compiled Java is unreachable by design". That was **half right**: the design does gate
compiled execution on `kAccNative`, but there are **TWO** such gates and §586 had only opened the
first. Opening both — plus fixing what then trapped — gets compiled Java code executing.

## The three patches (recipes/patch594.py, patch596.py, patch597.py; applied on §593)
| § | site | change | why |
|---|---|---|---|
| §586 | `ToCompiledCodeBridge` 0xa51938 | `tbnz w8,#8` -> `b 0xa51a28` | gate 1: stop force-interpreting non-native |
| **§594** | `ArtMethod::Invoke` 0x876cfc | `tbnz w9,#8` -> `b 0x876d10` | **gate 2** — this is the one §588 missed. Gate 1's path tail-calls ArtMethod::Invoke, which re-interpreted everything; that is exactly why §586+§574 measured *exactly* interpreter speed |
| §596 | `CodeGenerator::GenerateNullCheck` 0xc6bde8 | `ldrb w8,[x8,#226]` -> `mov w8,wzr` | explicit NULL checks (turned out NOT to be the fault source — kept, harmless) |
| **§597** | `InstructionCodeGeneratorARM64::GenerateSuspendCheck` 0xb829fc | `ldrb w8,[x8,#228]` -> `mov w8,wzr` | **explicit SUSPEND checks — this killed the fault storm** |

## The fault storm, found and fixed
With both gates open, compiled code ran but was ~17x SLOWER than interpreting. Cause, measured:
```
  hilog signo(11):  35,923 per 5 s   (~7,200 SIGSEGV/second)
  fault:            addr=0, FIXED pc inside the JIT code cache (0x460005f8)
  profile:          el0_svc_common 17%, _raw_spin_unlock_irq 11%, unwind_frame 4.5%,
                    walk_stackframe 3.6%   -- i.e. the SIGNAL PATH, not the program
```
ART's implicit **suspend** check is a load through a register pointing at a page that is mprotected to
request suspension; that mechanism is not set up in this port, so the load hits address 0 and traps on
**every loop back-edge**. On OHOS each trap costs a musl sigchain + DFX handler round trip that
**unwinds the stack**, so a hot loop crawls. §597 switches the JIT to explicit suspend checks:
```
  SIGSEGV rate  35,846 per 5s  ->  0        SEGV=0, SOE=0
```
⚠️§596 (null checks) alone changed nothing (35,846 vs 35,923) — proof the storm was suspend checks.
This is the mechanism behind the old note "OHOS sigreturn drops the W15 PC-edit ⇒ implicit-null fatal".

## RESULT — headless bench, heavy load (50,000 rounds x 2,000 iters)
```
  DONE all 50000 rounds — SURVIVED heavy compute load     <-- first time ever
  SEGV=0  SOE=0  correct checksums  process alive
  compiled code genuinely executing (ExecuteSwitch 0.00%, faulting pc inside the JIT cache)
  elapsed 324 s   vs   193 s interpreted
```
So the JIT **runs, compiles and executes compiled code under sustained heavy load without crashing** —
previously it died with a StackOverflowError or spun forever. It is still ~1.7x slower than the
interpreter on this benchmark; explicit suspend checks cost a load+test+branch per back-edge, and the
per-invoke quick-stub marshalling is paid on every call.

## ⚠️LIMIT: the GRAPHICAL app does not render with the JIT on
noice under §597 + `APPSPAWNX_FORCE_JIT=1`, clean boot: side-channels up, `CreateWindow=1`, 4/4 taps
delivered, **313 methods compiled**, SEGV=0, SOE=0, process alive — but the screen stays black (36907 B)
and tab taps cost ~103 ticks instead of ~3200, i.e. the UI is not drawing. Headless compute is fine;
the render path is not. **Unsolved — that is the next JIT question, and it is NOT the old SOE/recursion.**

## Deployment
§594/§596/§597 are **NOT deployed**. The board runs the §593 stack (`be828a5e…`) with the JIT OFF,
which is still the right default: the JIT gives no speed win yet and breaks rendering. The patches are
kept as recipes so the work can resume from a known-good, reproducible point.

---

# ★★★★★§598 (2026-08-10) CORRECTION: THE JIT IS **41× FASTER**, NOT SLOWER

The "324 s with JIT vs 193 s interpreted" figure reported above is **WRONG and retracted**. Both
numbers were WALL CLOCK measured by a host-side polling loop (`sleep 4` + an `hdc` round trip per
iteration, ~0.5 s each) around a bench that also writes a progress line every 40 rounds to a file on
the device. Wall clock measured my polling and the bench's log I/O — not the computation.

Re-measured with **process CPU time** (`utime+stime` from `/proc/<pid>/stat`, which includes exited
threads and is immune to both artefacts). Same libart (§597), same workload, same bench binary; the
ONLY variable is `APPSPAWNX_FORCE_JIT`:

```
workload: 50,000 rounds x 2,000 iters = 100,000,000 iterations

  JIT OFF (pure interpretation) : 3227 ticks = 32.27 s CPU   (~323 ns/iteration)
  JIT ON  (§597, compiled)      :   78 ticks =  0.78 s CPU   (~7.8 ns/iteration)
                                   ------------------------
                                   SPEEDUP = 41.4x
```
Both runs report `DONE all 50000 rounds` and the **identical checksum 220067813341938944**, so the
compiled code is correct as well as fast. ~12 cycles/iteration at this clock is exactly what compiled
arithmetic should cost, and ~323 ns/iteration is exactly what a switch interpreter should cost.

## So the full JIT result is:
1. §586 + §594 open BOTH `kAccNative` gates -> compiled Java actually executes.
2. §597 makes suspend checks explicit -> the ~7,200/s SIGSEGV storm goes to **0**.
3. Net: **41× faster on compute**, correct results, survives sustained heavy load, no crash.

## ⚠️Still true, and now the ONLY blocker: the GRAPHICAL app does not render with the JIT on
Controlled this session: §597 with JIT **off** renders normally (124 842 B); §597 with JIT **on**,
clean boot, gives a black screen while the window is created, taps are delivered, ~313 methods compile
and nothing crashes. So the gate/check patches are innocent — executing compiled code breaks the
render path specifically. That is the one remaining problem between here and a shipping JIT.

## ★METHOD (this is the third time the same mistake was made)
**Never time this device with host-side wall clock.** An `hdc` round trip is 0.2-0.5 s and the bench
logs over a socket/file; both dwarf the work. Use `/proc/<pid>/stat` CPU ticks, and vary ONE variable.
The same error class produced the §577 rejection and the §586 "64× slower" claim.
⇒ [[always-run-the-control]]

## §598b — the 41× VERIFIED three ways (and the marginal ratio is 89×)

The 41× was cross-checked, because "compiled code beats an interpreter" needed to survive the
alternative explanation that the compiler simply skipped the work.

**1. Linearity (proves the work is really executed).** JIT ON, 20,000 rounds, varying iters:
```
   iters=1000 -> 26 ticks     iters=2000 -> 37 ticks     iters=4000 -> 56 ticks
   => CPU = ~14 fixed + ~0.0105 per iters-unit : linear. Nothing is being elided.
```
**2. Marginal ratio (removes startup + JIT compilation overhead from both sides).** Cost of the SAME
extra 20,000,000 iterations:
```
   interpreter  1629-649 =  980 ticks  ->  490 ns/iteration
   JIT            37-26  =   11 ticks  ->  5.5 ns/iteration
   => steady-state ratio 89x   (end-to-end on 50k x 2k, including warm-up, is 41x)
```
**3. Correctness.** Identical checksums between JIT and interpreter at every configuration.

Both absolute numbers are physically sane: 5.5 ns/iter ~ 10 cycles for the ~10-12 arm64 instructions
of a xorshift+multiply body; 490 ns/iter ~ 880 cycles for ~15 dex bytecodes ~ 59 cycles/bytecode,
which is what a **C++ switch interpreter** costs (this port has nterp hard-off, so there is no fast
interpreter path — see CanRuntimeUseNterp returning 0).

### ⚠️Do NOT generalise 41-89x to the app
`WlJitBench.run` is a pure 64-bit integer ALU loop with **zero calls, zero allocation, zero field
access** — the best case for a compiler and the worst case for an interpreter. Published figures put
the *average* interpreter-vs-JIT gap nearer ~10x, and JIT's large wins are specifically in tight
numeric inner loops (which is exactly this shape). Real UI/app code is call- and allocation-heavy and
would see far less. The ratio is also inflated here because this port's interpreter is unusually slow
(switch interpreter + nterp off + the PFCUT per-invoke tax).

---

# §599 (2026-08-10) AOT attempt — image BUILT, but it cannot beat the JIT here, and cannot be activated yet

Asked whether AOT would beat the JIT's 41x. Findings, in order of how much they matter:

### 1. AOT on this port is restricted to a WEAKER compiler config than the JIT
`--compiler-filter=speed` **SEGVs dex2oat** on this port's patched jars — reproduced exactly
(`Fatal signal 11 (SIGSEGV), code 1 (SEGV_MAPERR) fault addr 0x4`, rc=139), confirming §318. The only
configuration that builds is the §318 one: **`--compiler-filter=space` with `--inline-max-code-units=0`
(inlining OFF)**. The JIT meanwhile compiles at *speed* quality WITH inlining (`kind=0` optimizing).
=> For a hot loop already JIT-compiled, **AOT here would produce WORSE code, not better**. AOT's real
advantages are startup latency and full coverage — neither of which this benchmark measures.
Note also both use the SAME optimizing backend, so AOT has no peak-throughput edge by construction.

### 2. An image WAS successfully built from the CURRENT device jars
`recipes/build_aot_bootimage.sh <filter> <inline-units>` (space/0 works, speed/32 crashes). Built from
the 10 live BCP jars pulled off the board (incl. WlJitBench):
`boot.art 5,041,168 B` + `boot.oat 8,532,568 B` + per-jar .art/.oat/.vdex, rc=0, ~23 s.
⚠️pull jars via `hdc file recv` into `.shots/` first — hdc.exe cannot write to /tmp under WSL.

### 3. Activation blocker (why it was not measured)
```
  our image stamps    : art\n 114   oat\n 247
  deployed libart wants: art\n 118   oat\n 247      <- OAT matches, IMAGE differs
```
§318 solved this by **rebuilding libart to v114** — forbidden here (⛔not reproducible) and it would
discard every binary patch (§551/§589/§591/§594/§597). The version is only a 2-byte constant in libart
(sites 0x58faa8 and 0x5a3483), so it is patchable instead — but activation ALSO needs a `-Ximage=`
runtime option that appspawn-x's child does not currently pass (no `Ximage` anywhere in child_main.cpp),
i.e. a bridge rebuild; and §318's deeper wall (IMT incompatibility -> AbstractMethodError before
bindApplication) is unrelated to the kAccNative gates and probably still stands.

### Verdict
**Do not expect AOT to beat the 41x.** Same backend, weaker permitted settings, and the JIT's warm-up
here is only ~14 of 78 ticks, so there is almost no headroom for AOT to recover. The sensible use of
AOT on this port is *startup/coverage*, and it should be revisited only after the JIT's rendering bug
is fixed — at which point the version patch + `-Ximage` plumbing is a well-scoped task.

---

# ★★★★★§600 (2026-08-10) THE REBUILD WORKS — and the old "rebuild SIGSEGVs" was the §436 BUG

Tested rather than assumed. `A2OH/art-latest` **can rebuild libart for OHOS arm64 today.**

## Build: clean
All 5 prerequisites present (aosp-art-15, aosp-android-11/art, art-latest/stubs, the OHOS clang
prebuilt, the ohos-sysroot). `make -f Makefile.ohos-arm64 -j16 all` → **231/231 runtime objects,
0 errors**, with the patched sources compiling as patches (`runtime.cc (patched)`,
`interpreter_common.cc (patched)`, `class_linker.cc (patched)`, …). The Makefile compiles
`patches/**.cc` directly over the AOSP tree — it does NOT need patches installed into aosp-art-15,
so the old *"48 patches are not installed in the ART tree"* objection does not apply to this path.
Link with `bridge-build-arm64/build_libart_so_arm64.sh` → `out/libart.so`, rc=0.

## The rebuilt binary matches the deployed one structurally
```
                    rebuilt        deployed §593
  size              22,554,336     22,552,176   (0.01% apart)
  PFCut functions           93              93   IDENTICAL
  undefined symbols        458             458   IDENTICAL
```
So the tree really is the source of the running binary (⇒ [[rebuild-libart-reassessed-2026-08-10]]:
only 7 `PFCutRealm*State` macro accessors were ever unaccounted for).

## ★It RUNS — and the historical blocker is now identified
Deployed the rebuilt libart, 3 launches:
```
  run1: chan=0  child=LIVE  SEGV=78  addr=0x45   <- §436
  run2: chan=0  child=LIVE  SEGV=78  addr=0x45   <- §436
  run3: chan=1  child=LIVE  SEGV=0   log 2.5MB   <- FULL, USABLE LAUNCH
```
**A rebuilt libart launches the app.** The July-27 note *"a full rebuild reaches parity yet still
SIGSEGVs the child"* was never a rebuild defect — it is the **§436 launch lottery** (corrupt ArtMethod,
`declaring_class_==5` ⇒ `addr=0x45`), the same defect that §551/§593 guards and that this session
root-caused ⇒ [[segv436-imt-guard-2026-08-06]]. The rebuild reproduces it simply because the guard is
a BINARY patch (code cave) and therefore absent from a source build.

## NEXT — this unblocks everything
Port the §436 guard to SOURCE. The faulting shape is the `method->GetDeclaringClass() != nullptr &&
method->GetDeclaringClass()->DescriptorEquals(...)` idiom (e.g. interpreter_common.cc:176,186,196):
the null test passes for the garbage value 5, then the inline load at class+0x40 faults at 0x45.
Fix = a plausibility check (non-null AND >= 0x1000 AND aligned) instead of `!= nullptr`. Then:
rebuild → `verify-libart-guards.sh`-equivalent → 6-attempt launch taxonomy (expect 6/6) → 5-tab walk
→ CPU-tick bench. Once that lands, the whole binary-patch stack (§589/§591/§594/§596/§597) should be
re-expressed in source, where the two blocked goals — **memoizing DescriptorEquals** and the **JIT
render bug** — actually become tractable.

⚠️Board left on §593 (`be828a5e…`), verified: usable launch, render 124,975 B.

## §600b — does the JIT work FROM THE art-latest SOURCE BUILD? **No — and the source says why**

Tested the freshly rebuilt libart (no binary patches) with `APPSPAWNX_FORCE_JIT=1` on the same bench:
```
  JIT IS LIVE = yes, 3 methods compiled
  RESULT: StackOverflowError at WlJitBench.bench, DONE=0   <- the original §436-era crash
  (JIT-patched libart finishes the same bench in 78 ticks; pure interpreter = 3227)
```
So a stock build of this repo **crashes when the JIT is enabled** — the §583 recursion
(Execute → ToCompiledCodeBridge → ToInterpreterBridge → Execute) — which is exactly the symptom that
began this whole investigation.

### The source states the reason, in `patches/runtime/art_method.cc:424-432`
```cpp
// Force interpreter for ALL non-native Java methods.
// Without AOT compilation or JIT, there is no compiled code to execute.
// Entry points may contain Nterp stubs (just 'ret') or uninitialized values,
// causing SIGBUS when the invoke stub jumps to them.
bool force_interpreter_path = false;
if (!IsNative() && IsInvokable() && !IsProxyMethod()) { force_interpreter_path = true; }
```
This is **§594's gate in source form**. ⚠️It is a deliberate SAFETY MEASURE for the JIT-off
configuration, *not* an architectural impossibility — §588's "compiled Java is unreachable by design"
was right that it is deliberate and wrong about why. Its own premise ("there is no compiled code to
execute") is FALSE once the JIT is live, so the guard must become conditional.

### The source fix (all three, now precisely located)
1. `art_method.cc` — gate `force_interpreter_path` on the method NOT having live JIT code, i.e. the
   same test `Jit::CanInvokeCompiledCode` already performs (`code_cache->ContainsPc(entry_point)`).
   = §594.
2. the `ArtInterpreterToCompiledCodeBridge` non-native force-interpret = §586.
3. explicit suspend checks (or set up the implicit suspend-check page this port never initialises)
   = §597 — without it, compiled code takes ~7,200 SIGSEGV/s.
Plus the §436 guard (§551) so launches are not a lottery. With those four, a source build should
reproduce the measured **41× compute win**, and the JIT render bug becomes debuggable in source.

## ★★★★★§601 (2026-08-10) JIT ENABLED **IN SOURCE**, rebuilt, benchmarked: **10.3× — no binary patches**

`recipes/601-enable-jit-in-source.patch` (63 lines, 3 files) turns the JIT on in `A2OH/art-latest`:
| file | change | replaces |
|---|---|---|
| `patches/runtime/art_method.cc` | `force_interpreter_path = !has_jit_code` instead of unconditional `true` — using the same `jit->GetCodeCache()->ContainsPc(entry_point)` test the file already uses at line 932 | §594 |
| `patches/runtime/interpreter/interpreter_common.cc` | the bridge no longer force-interprets a method that HAS live JIT code (+ `jit_code_cache.h` include) | §586 |
| `patches/runtime/runtime.cc` | `implicit_suspend_checks_ = false` on arm64 — this port never installs the suspend-check page, so the implicit check faulted at address 0 on every loop back-edge | §597 |

Rebuilt (`make -f Makefile.ohos-arm64 -j16 all` → 231/231, **0 errors**) and linked. MEASURED on the
same build, same workload, only `APPSPAWNX_FORCE_JIT` varied:
```
  JIT OFF : 3131 ticks     JIT ON : 305 ticks     =>  10.3x
  DONE=1 both, SOE=0, SEGV=0, identical checksum 220067813341938944
  JIT live, 4 methods compiled (WlJitBench x2)
```
**The StackOverflowError is gone at source level** — the crash a stock build still produces (§600b).

⚠️305 vs the binary-patched 78 ticks (41×): the source build is ~4× off the patched one. Not yet
explained — candidates are startup/compile overhead counted in-process, `implicit_null_checks_` still
true (the §596 equivalent was NOT ported, though §596 measured as a no-op), or CompilerOptions being
snapshotted before the runtime flag is applied. Worth closing, but the headline stands: **a clean
source build now runs compiled Java correctly and ~10× faster, with no binary patching.**

NEXT: port the §436 guard (§551) to source for launch reliability, then retest the app — the render
bug is now debuggable in source.

### §601b — CORRECTION: the source build is **31×**, on par with the binary-patched one (not 10.3×)

The "10.3× (305 vs 3131 ticks)" above is **retracted** — the 305 was a bad reading (two appspawn-x
processes were alive; the CPU was read from the wrong one, and 305 does not fit the build's own
linearity model, which predicts ~105 for that workload).

Clean measurement, SAME source build, 20,000 rounds x 2,000 iters, only `APPSPAWNX_FORCE_JIT` varied:
```
  source build   JIT OFF 1311 ticks   JIT ON 42 ticks  =>  31.2x
  binary-patched JIT OFF 1629 ticks   JIT ON 37 ticks  =>  44.0x
```
And the decomposition shows they are the SAME engine:
```
  marginal cost per +20M iterations : source 12 ticks | patched 11 ticks   <- compiled loop identical
  fixed cost (startup + 20k invokes): source 18 ticks | patched ~15 ticks
```
The residual spread (42 vs 37, 1311 vs 1629) is run-to-run variance, not a real difference — note the
*interpreter* numbers differ in the opposite direction to any real effect, which is the giveaway.
Also confirmed: **0 SIGSEGV/5s** on the source build, so the implicit-null-check path is NOT faulting
and §596 does not need porting.

**Conclusion: the source-enabled JIT performs on par with the binary-patched one (~31-44× on this
benchmark). Binary patching bought nothing that source cannot.**

## §602 (2026-08-10) AOT on the source build — plumbing BUILT, measurement NOT obtained

With the tree now buildable, both AOT blockers became source fixes (`recipes/602-aot-image-support.patch`):
- `patches/runtime/oat/image.cc`: `kImageVersion` **118 → 114**, so the images our prebuilt x86→arm64
  dex2oat stamps are accepted (OAT v247 already matched). §318 did this by hand-edit + relink; it is
  in source now.
- `patches/runtime/runtime.cc`: when `-Ximage` is absent (appspawn-x's child passes none),
  `WESTLAKE_BOOT_IMAGE=<path/boot.art>` supplies one. **Inert unless the env var is set.**
Rebuilt clean (231/231, 0 errors); verified in the binary: accepts image version `114`, and the
`WESTLAKE_BOOT_IMAGE` hook is present.

⚠️**The AOT run did not actually happen.** `hdc file send <localdir> <remotedir>` nested the 30 image
files under `…/bootimg/.shots/…`, so the `find … boot.art` came back empty, `WESTLAKE_BOOT_IMAGE` was
set to an empty string, and the runtime started **imageless** (`WESTLAKE-AOT` marker count = 0). The
2167-tick result from that run is therefore an unloaded-image control, NOT AOT — do not quote it.

### To finish (small, well-defined)
1. Push the image with an explicit remote path per file (or `hdc file send dir/. dest`) and CONFIRM
   `boot.art` is directly in the target dir before running.
2. Run with `WESTLAKE_BOOT_IMAGE=<dir>/boot.art` and check `grep -a WESTLAKE-AOT` is non-zero — that
   marker is the only proof the image loaded.
3. Compare CPU ticks against the same build's interpreter (1311) and JIT (42) at 20k×2k.
⚠️Expect AOT to LOSE to the JIT here: the only dex2oat config that builds on this port is
`--compiler-filter=space --inline-max-code-units=0` (speed SEGVs, reproduced), i.e. size-optimised and
non-inlining, versus the JIT's speed-quality inlining output. §318's IMT wall may also still bite.

### §602b — AOT still NOT achieved. Two obstacles cleared, one precise bug left.

CLEARED:
1. ⚠️**`hdc file send` replicates the LOCAL RELATIVE PATH under the remote dir.** `hdc file send
   .shots/boot.art /data/local/tmp/asx/bimg` lands at `bimg/.shots/boot.art`, NOT `bimg/boot.art`.
   And because `.shots` is dot-prefixed, a plain `ls` shows the dir as EMPTY — which is what made this
   look like a failed transfer twice. Verify with `ls -a`, or `find <dir> -name boot.art`.
   All 30 image files are pushed correctly this way (confirmed: 30 present).
2. The image itself is fine and the libart accepts it (built binary reports image version `114`).

REMAINING BUG (one line): §602's hook is guarded by `if (image_locations_.empty())`, but
`Opt::Image` has a NON-EMPTY default, so the branch never runs — `WESTLAKE-AOT` marker = 0 on every
attempt, i.e. the image is never loaded.
**Fix:** make the env var OVERRIDE rather than fill in:
```cpp
  const char* wl_img = getenv("WESTLAKE_BOOT_IMAGE");
  if (wl_img != nullptr && wl_img[0] != '\0') {
    image_locations_.clear();
    image_locations_.push_back(std::string(wl_img));
    fprintf(stderr, "[WESTLAKE-AOT] using boot image: %s\n", wl_img);
  }
```
Then rebuild, run with `WESTLAKE_BOOT_IMAGE=/data/local/tmp/asx/bimg/.shots/boot.art`, and **check the
`WESTLAKE-AOT` marker is non-zero before believing any number** — every AOT timing so far was an
imageless control and must not be quoted.

Expect AOT to LOSE to the JIT regardless (space filter + inlining OFF is the only dex2oat config that
builds here), and §318's IMT wall may still bite once an image genuinely loads.

### §602c — AOT still not loading; codex review says the whole approach is wrong-headed

Fixed the `image_locations_.empty()` guard (now an unconditional OVERRIDE), rebuilt (0 errors,
`overriding` string verified present in the binary), re-pushed all 30 image files (confirmed present),
deployed the new libart (md5 `9dde7736…`) — **`WESTLAKE-AOT` marker is still 0**, so the image is
still not loaded. Env propagation to the appspawn-x child is the next thing to verify (the
`/proc/<pid>/environ` check returned empty for a KNOWN-GOOD control var too, so that probe itself was
unreliable — re-do it while the child is definitely alive).

### ★codex (gpt-5.6-sol, max) — the decisive point
> **"Rebuild host dex2oat from the exact target ART source/configuration and remove the
> `kImageVersion` downgrade."**

That reframes everything: forcing libart to accept a **v114** image emitted by a *mismatched prebuilt*
dex2oat is treating the symptom. The version number is a proxy for **serialized-layout compatibility**
— and §318's "IMT incompatibility / AbstractMethodError" is exactly what a mismatched image producer
yields. `art-latest`'s Makefile already has dex2oat targets and a `build-ohos-arm64/dex2oat` dir, so
building the matching compiler is plausible. Note the `speed`-filter SEGV is then also explained as a
prebuilt-compiler defect, not a property of our jars.

Codex's other conclusions, worth keeping:
- **AOT will not win steady-state throughput here.** The boot image holds BCP/framework code; the JIT
  gets RTI, devirtualization, inlining and OSR. Do NOT assume the JIT recompiles a lower-quality AOT
  method — a warmed method's entrypoint may stay inside `boot.oat`.
- **AOT's real wins on this port**: pre-`bindApplication` and cold-start framework work, first
  invocation before JIT thresholds, short-lived appspawn children, deterministic latency, less JIT
  CPU, and shared read-only `.art`/`.oat` pages across forked children.
- **Mixing a boot image with the JIT is normal ART operation** — they coexist by design. Port traps to
  watch: any remaining force-interpreter condition, global deopt, missing `PreZygoteFork`/post-fork
  JIT hooks, W^X / icache-flush / signal-chain differences on musl, per-child JIT caches losing
  sharing, an over-aggressive `instruction_set_features_bitmap_` (SIGILL).
- Suggested triage matrix for the AbstractMethodError when it returns: image vs no-image × `-Xint` vs
  quick, plus a deliberately COLLIDING interface pair chosen with the target runtime's own
  `ImTable::GetImtIndex` — "only the collision case fails" would pin `ImtConflictTable`/the conflict
  trampoline.
- For a clean control, try a **`verify`**-filter boot image if it builds (no AOT code, isolates image
  ABI from codegen).

### Recommended next step (revised)
Build dex2oat from `art-latest` for the host, regenerate the image, and **revert `kImageVersion` to
118** rather than downgrading libart. That is the shortest path to an image that is compatible by
construction instead of by coercion.

### §602d — narrowing why the image never loads (state at handoff)

Ruled OUT this round:
- ❌"wrong libart is loaded" — `/proc/<pid>/maps` confirms the child maps
  **`/data/local/tmp/asx/libart.so`**, i.e. exactly what we deploy.
- ❌"image files missing/mispushed" — all 30 present at `/data/local/tmp/asx/bimg/.shots/`
  (remember: `hdc file send` replicates the LOCAL RELATIVE PATH, and `ls` hides the dot-dir; use `ls -a`).
- ❌"version mismatch" — libart now accepts **114**, which is what this dex2oat stamps. Not the blocker.
- ❌"the child gets a sanitised environment" — `child_main.cpp` uses `setenv(...)` and never
  `clearenv`/`execve` with a fresh env, so the child INHERITS. `APPSPAWNX_*` vars demonstrably work.
- ⚠️`/proc/<pid>/environ` probes are unreliable here — the device has **no `tr`** (and the control var
  came back empty too). Do not trust that probe; it is not evidence either way.

STILL UNEXPLAINED: `WESTLAKE-AOT` marker = 0 with the correct libart deployed and the env var set.

**Next diagnostic (cheap, decisive):** move the `fprintf` OUT of the `if` so it prints
unconditionally right after `image_locations_ = runtime_options.ReleaseOrDefault(Opt::Image);`,
e.g. `[WESTLAKE-AOT] hook reached, env=%s, locations=%zu`. That answers in one run whether the code
path executes at all — if it never prints, the child's Runtime is created through a path that does
not reach this line (and `-Ximage` must be injected elsewhere, e.g. into the runtime options vector
the JNI_CreateJavaVM caller builds in appspawn-x). If it prints with a null env, the variable is not
reaching the child after all.

Only after the image LOADS does codex's main point apply: **rebuild dex2oat from art-latest and
revert `kImageVersion` to 118**, so the image is compatible by construction rather than by coercion —
that is what addresses §318's IMT/AbstractMethodError class of failure.

### ★§602e ANSWER: `Runtime::Init`'s image-options line is NEVER EXECUTED in this child

The unconditional probe settled it. Built libart with an `fprintf` placed immediately after
`image_locations_ = runtime_options.ReleaseOrDefault(Opt::Image);` (verified `hook reached` is in the
binary; md5 `30d65613…`), deployed it, ran the bench:
```
  bench ran fine        : 58 JIT-BENCH lines   (so the child DID start on this libart)
  "WESTLAKE-AOT" marker : 0                    (the probe NEVER printed)
```
So the failure was never the env var, the guard, the image files, or the version — **that line of
`Runtime::Init` simply does not run in the appspawn-x child.** Every earlier hypothesis was downstream
of a code path that is not taken.

**Therefore `-Ximage` cannot be injected there.** It must go where this child actually configures the
VM: the runtime-options vector appspawn-x builds before `JNI_CreateJavaVM` (`child_main.cpp` — the same
place that already sets `-Xbootclasspath`). Find that vector and append
`-Ximage:<path>/boot.art`; that is a BRIDGE rebuild, not a libart one, and it is allowed.

Corollary worth keeping: this also explains why §318 needed its own deploy topology — the child's VM
init is not stock `Runtime::Init` flow.

### Status of the AOT effort at handoff
- ✅ boot image builds from live jars (`recipes/build_aot_bootimage.sh`; `space`+no-inline only)
- ✅ libart accepts image v114 (`recipes/602-aot-image-support.patch`)
- ❌ image never loaded — root cause now identified (wrong injection point, above)
- ⚠️ ALL AOT timings so far are imageless controls. Do not quote any of them.
- ★ then apply codex's main point: build dex2oat from art-latest and revert `kImageVersion` to 118,
  so the image is compatible by construction (that is what addresses §318's IMT/AbstractMethodError).

---

# ★★★★★§603 (2026-08-10) AOT: §602e WAS WRONG — THE IMAGE **LOADS**. Three real blockers, two cleared.

Question asked: *why does source-built ART 15 support the JIT but not load an AOT image?*
Answer: the two are **not comparable**, and the recorded root cause was an artefact of reading the
wrong log.

## The structural asymmetry (this is the actual answer)
- **JIT is child-side and self-contained.** It is created post-fork in the child
  (`child_main.cpp:230 wl_create_jit_after_fork`) and the §601 patches sit on paths the child runs
  (`art_method.cc`, `interpreter_common.cc`). It depends on no external artefact.
- **The AOT boot image is parent-side and one-shot.** `appspawn-x` is a zygote: `main.cpp` Phase 2
  calls `runtime.startVm()` → `JNI_CreateJavaVM` → `Runtime::Init` **once, in the PARENT, before any
  child exists**; children are `fork()`ed later. So image loading happens in a different process,
  logs to a different file, and depends on an **external artefact** that must agree with the runtime
  in three independent ways — header version, internal dex/vdex consistency, and file location policy.
  All three failed, and the first two fail **silently into imageless fallback**.

## ⛔RETRACTED: "`Runtime::Init`'s image line is never executed in this child" (§602e)
FALSE. Two independent proofs:
1. **Live**: with the §602 libart deployed, `/data/local/tmp/asx/asx.err` contains
   `[WESTLAKE-AOT] hook reached: env=… locations=1` and `[IMG] LoadBootImage entered, image_locations=1`.
2. **Was available all along**: §601's `implicit_suspend_checks_ = false` is `runtime.cc:4582` — the
   **same `Runtime::Init`**, 325 lines *after* the image line at 4257 — and it demonstrably worked
   (SIGSEGV 35 846/5 s → 0). If 4582 runs, 4257 ran.
★The marker was grepped in the CHILD's stderr / hilog. `Runtime::Init` runs in the **parent**, whose
stderr is `/data/local/tmp/asx/asx.err`. ⇒ **For anything at VM-init time, read `asx.err`, not the
child log.**

## ⛔RETRACTED: "`-Ximage` must be injected in `child_main.cpp` (a bridge rebuild)"
FALSE — it is **already plumbed and always passed**:
- `main.cpp:96` `setenv("ANDROID_BOOT_IMAGE", "/data/local/tmp/asx/fw/boot.art", 1)`
- `appspawnx_runtime.cpp:184-190` → `options.push_back("-Ximage:" + bootImage)`
- live: `option[3]: -Ximage:/data/local/tmp/asx/fw/boot.art` → `parsed_options.cc:656` → `locations=1`.
§602 grepped **`child_main.cpp`** — the wrong file; VM options are built in `appspawnx_runtime.cpp`.
The `WESTLAKE_BOOT_IMAGE` hook is therefore redundant (still handy as an override; used below).

## The real blocker chain
### L1 — image header version (cleared by the source build)
Deployed §593 (`be828a5e…`) wants `art\n118`; the device image stamps `114` ⇒
`Image header … is invalid` ⇒ `InitWithoutImage`. Byte-checked the device `boot.art` against **every**
`ImageHeader::IsValid()` term — magic ✅, `image_reservation_size_=0x052e4000` 16 K-aligned ✅,
`begin(0x70000000)/size(0x4c6078)` ✅, `oat_file_begin<oat_data_begin<oat_data_end<oat_file_end` ✅ —
**version was the only failing check.** §602's `kImageVersion 118→114` clears it.
Version table: `art-latest/build/bin/dex2oat` (Jul 22) stamps **image 114 / oat 247 / vdex 027**;
source libart expects 114 (§602) / 247 / 027; **stock AOSP-15 = 118**. OAT+VDEX already agreed — only
the image number was coerced, exactly as codex diagnosed.

### L2 — the deployed image is unusable: built with NO `--dex-location` (cleared by rebuilding)
With 114 accepted, the header passes and it dies one stage later:
`OatDexFile #0 for '$WLROOT/bridge-build-arm64/fwjars/apache-xml.jar' with invalid dex file magic`
— a **host path** baked into the oat. Cause: `bridge-build-arm64/build_fw_bootimg.sh` passes only
`--dex-file=$FW/$j.jar`. (`recipes/build_aot_bootimage.sh` does pass `--dex-location` — use that one.)
⚠️the dex IS present and valid in the vdex (section kind=1 @0x48, `dex\n039`), so "invalid magic" here
means location/offset disagreement, not a missing dex.
Rebuilt from the **10 live BCP jars** with `--dex-location=/data/local/tmp/asx/fw/<jar>`:
`filter=verify inline=0` → rc=0, 31 files, `boot.art 5 041 168 B`, `boot.oat 54 832 B` (~4 s).
⚠️**`--compiler-filter=space` now SEGVs dex2oat too** (`fault addr 0x3c052ade`, rc=139) — §599's
"space is the config that builds" is **no longer true** with the current jars. `verify` builds.

### ✅RESULT: THE BOOT IMAGE LOADS (first time on this port)
`WESTLAKE_BOOT_IMAGE=/data/local/tmp/asx/aotimg/boot.art`:
```
[WESTLAKE-AOT] using boot image: …/aotimg/boot.art (overriding 1 default(s))
[IMG] Reserving 0x6d63000 bytes at compiled address 0x70000000 → Reservation OK
[IMG] Loaded boot.art @0x70000000 … + all 9 components (boot-framework.art size=0x1e35914)
[IMG] MaybeRelocateSpaces: actual=compiled=0x70000000 diff=0
[IMG] Cleared 594 / 334 stale native JNI entry points
[WESTLAKE-TOPO] InitFromBootImage entered          <-- not InitWithoutImage
```

### L3 — NEW and current: ART's zygote trusted-oat policy ABORTS the runtime
```
oat_file_manager.cc:87] Check failed: in_memory || !only_use_system_oat_files_ ||
  LocationIsTrusted(…) || !oat_file->IsExecutable()
  Registering a non /system oat file: /data/local/tmp/asx/aotimg/boot.oat  android-root=/system/android
Runtime aborting...
```
`runtime.cc:4326  if (is_zygote_ || Opt::OnlyUseTrustedOatFiles) oat_file_manager_->SetOnlyUseTrustedOatFiles();`
appspawn-x **must** pass `-Xzygote` (else `PreZygoteFork` CHECK-aborts), so this always fires, and the
image lives under `/data`, not `ANDROID_ROOT` (`/system/android`). `/system` is **read-only**
(`mkdir /system/android/framework` → EROFS; 822 M free) and has no `framework/` dir.
**Fix, in order:** (a) one-line source change — don't call `SetOnlyUseTrustedOatFiles()` (or extend
`LocationIsTrusted` to the asx dir); (b) remount `/system` rw and stage the ~58 MB image at
`/system/android/framework/` (the path the arm32 deploy used).
⚠️**§318's IMT / AbstractMethodError wall is still UNTESTED** — it lies beyond this abort.

## Method notes
★Read `asx.err` (parent) for anything at VM init; the child log cannot contain it.
⛔`pkill -9 -f appspawn-x` inside an `hdc shell` **kills your own shell** (its cmdline contains the
pattern) — use `kill -9 $(pidof appspawn-x)`.
★Board restored to §593 (`be828a5e…`), imageless, `Ready to accept`, verified.

## §603b — L3 fixed in source; §318's wall STILL STANDS (as a class-roots defect), and why

### ✅L3 fix (source, opt-in)
`patches/runtime/runtime.cc:4326` — the `SetOnlyUseTrustedOatFiles()` call is now gated on
`getenv("WESTLAKE_TRUST_ALL_OAT")` being **unset**, so default behaviour is byte-for-byte unchanged and
the relaxation is opt-in. Backup: `runtime.cc.pre603`.
Rebuild `make -f Makefile.ohos-arm64 -j16 all` = **231/231, 0 errors**; link → `out/libart.so`
md5 **`3c3f71b0…`**; byte-verified (`trusted-oat policy DISABLED` + `WESTLAKE_TRUST_ALL_OAT` present).
★CONTROL run (new libart, NO image env): imageless, `Ready to accept`, no abort, marker absent — so the
binary itself is sane and only the env var changes behaviour.

### L4 (new, current): image loads, then `CreateProxyConstructor` CHECK-fails
`WESTLAKE_BOOT_IMAGE=…/aotimg/boot.art WESTLAKE_TRUST_ALL_OAT=1`:
```
[WESTLAKE-AOT] trusted-oat policy DISABLED (WESTLAKE_TRUST_ALL_OAT set)
[WESTLAKE-TOPO] InitFromBootImage entered
… ~34 600 more log lines (vs ~120 before) …
class_linker.cc:6351] Check failed: proxy_class->NumDirectMethods() == 21u
                      (proxy_class->NumDirectMethods()=0, 21u=21)
Runtime aborting...
```
`ClassLinker::CreateProxyConstructor` @6348 does `GetClassRoot<mirror::Proxy>(this)` and requires
`java.lang.reflect.Proxy` to have 21 direct methods; from the image it has **0**. So the image's
**class-roots content does not match what this runtime expects** — the same producer/consumer family as
§318's IMT/AbstractMethodError. ⇒ **§318's wall still stands**, now with an exact, reproducible signature
instead of a vague one.

### ★★The reason, proven: the ACTIVE host dex2oat is the APRIL build
```
  build/bin/dex2oat                (ACTIVE, Apr 3)   image=114  oat=247  vdex=027
  build/bin/dex2oat.apr3.bak                         image=114  oat=247  vdex=027
  build/bin/dex2oat.jul22-rebuilt.bak                image=118  oat=247  vdex=027
  libart source (stock AOSP-15)                      image=118
```
So §602's `kImageVersion 118→114` downgrade exists **only to accommodate a stale compiler** — codex's
"rebuild dex2oat and revert to 118" is correct, and now proven rather than argued.

### ⛔BUT the v118 binary on disk is NOT usable — it is an INSTRUMENTED DEBUG build
`dex2oat.jul22-rebuilt.bak` has the §318-era tracing compiled in (`[WESTLAKE-NEXT]`, `[WESTLAKE-IMT]`,
`[WESTLAKE-NTERPENTRY]`). Building the same image with it: **9.5 min, rc=1, no `boot.art`**, and a
**51 GB** `d2o.err` (394 032 945 lines) — it emits one `[UNSAFE-JNI] compareAndSetInt` line per iteration
of a CAS spin during unstarted-runtime clinit. That is why the project reverted to the apr3 binary.
⚠️**Always cap dex2oat's stderr** (`| head -c 50M`, or a filtered redirect) — this filled 51 GB in
9 minutes. (Cleaned up; disk back to 52%.)

### NEXT (well-scoped)
1. Build a **clean** host dex2oat from `art-latest` (no WESTLAKE tracing) — the `Makefile` host target,
   not the `.bak`.
2. Regenerate the image with it (stamps 118), revert `kImageVersion` **114→118** in
   `patches/runtime/oat/image.cc`, rebuild libart.
3. Re-run with `WESTLAKE_BOOT_IMAGE` + `WESTLAKE_TRUST_ALL_OAT` and check whether the
   `NumDirectMethods()==21` CHECK now passes. That is the decisive test of the producer/consumer theory.
4. If it still fails, codex's triage matrix applies (image vs no-image × `-Xint` vs quick, plus a
   deliberately colliding interface pair) — and a `verify`-filter image is already the clean control.

★Board restored and verified: libart `be828a5e…` (§593), pid live, `Ready to accept`=1, aborts=0.

## §603c — the v118 (correct-by-construction) route is blocked by a dex2oat clinit SPIN

Followed codex's recommendation to its end: use a tree-matched dex2oat and revert `kImageVersion` to
118 instead of coercing libart down to 114.

### Done
- `patches/runtime/oat/image.cc`: `kImageVersion` **114 → 118** (stock). Rebuilt 231/231, 0 errors,
  linked → libart **`fecfe16f…`**, byte-verified (accepts `118`, L3 gate + AOT hook present).
- Found the right compiler already on disk: **`build/bin/dex2oat.jul22-rebuilt-GATED.bak`**
  (image **118**, oat 247, vdex 027 — matches the source tree). ⚠️I first used
  `dex2oat.jul22-rebuilt.bak` (UNGATED) by mistake — that is the one that wrote the 51 GB log.

### ⛔BLOCKED: the tree-matched dex2oat never finishes — it SPINS in unstarted-runtime clinit
| run | args | outcome |
|---|---|---|
| ungated | `-Xmx256m` | 9.5 min, rc=1, **51 GB** d2o.err (394 032 945 lines) |
| gated | `-Xmx256m` | 9.5 min, rc=1, killed by the **dex2oat watchdog** at 570 s, no `boot.art` |
| gated | `-Xmx1g` | instant abort: `heap.cc:616 Check failed: main_mem_map_1.IsValid() Failed anonymous mmap(nil, 1073741824)` ⇒ **do not raise -Xmx** |
| gated | `--watchdog-timeout=5400000`, `-Xmx256m` | **45 min at 99.9 % CPU, utime 2746 s / stime 0.2 s, RSS frozen at 129 128 kB, d2o.err frozen at 202 547 B, 0 `.art`** ⇒ killed manually |
A frozen RSS + frozen log + 100 % user CPU for 42 minutes is a **spin**, not slow progress.

### What the spin is NOT (checked, so nobody re-checks it)
- ⛔NOT a missing `Unsafe` handler. `compareAndSetInt` **is** in the dispatch table
  (`unstarted_runtime_list.h:121 JdkUnsafeCompareAndSetInt`) and is implemented (a forwarder to
  `UnstartedJNIJdkUnsafeCompareAndSwapInt`).
- ⛔NOT the `[UNSAFE-JNI]` log itself. That `LOG(WARNING)` sits at the **top of
  `UnstartedRuntime::Jni`** (`unstarted_runtime.cc:2470-2486`), so it fires on **every** JNI call the
  unstarted interpreter makes — handled or not. ★So 394 M `compareAndSetInt` lines mean **a CAS loop
  executed 394 M times**, not "the method is unimplemented". The gate
  (`WESTLAKE_UNSAFE_JNI_DIAG`, added 2026-07-22) only silences it; the loop still runs.
- ⛔NOT memory. `-Xmx1g` cannot even be reserved; 256 MB is the working configuration.

### ★The comparison that explains the L4 Proxy failure
```
  dex2oat (Apr 3, v114) : "dex2oat took 2.745s (2.469s cpu)", java alloc 21 MB, arena alloc 0 B  -> image OK
  dex2oat (Jul 22, v118): 487 s in class loading, 38 392 classes loaded, "Classes initialized: 0"  -> spins
```
A 10-jar boot image over a 40 MB `framework.jar` cannot honestly be built in **2.7 seconds**. The April
compiler produces a **structurally valid but semantically under-populated** image — which is exactly
why the runtime later finds `java.lang.reflect.Proxy` with **0 direct methods** (§603b L4). So the two
compilers fail in opposite ways: April finishes but under-populates; July populates but never finishes.

### State / next
- `kImageVersion` is left at the stock **118** — correct-by-construction, and it matches the deployed
  §593 binary. Revert to 114 only to load an April-built (broken) image.
- **The gate to a real AOT image is now the dex2oat clinit spin**, not anything in libart or the
  bridge. Next step is to identify the spinning loop: run the gated dex2oat under a debugger (or add a
  bounded iteration counter + `PrettyMethod` dump in `UnstartedRuntime::Jni`) and name the class whose
  `<clinit>` never terminates. Everything downstream (§603's L1/L2/L3 fixes) is already in place and
  verified, so that single fix should produce a loadable, correct image.
- ★Board untouched and verified: libart `be828a5e…` (§593), pid live, `Ready to accept`=1.

## §603d — A.1 settled: a FRESH tree-built dex2oat spins too ⇒ real bug, not a stale binary

Rebuilt the host compiler from the current tree (`make -j16 all && make link`) to test codex's
"compatible by construction" route properly.

### Two tree defects found and fixed on the way
1. **The host link is missing a symbol the tree references.** `dex2oat` built but died instantly with
   `undefined symbol: _ZN3art11interpreter20g_westlake_infl_gateE`. Cause: `Makefile.ohos-arm64`
   **globs** `patches/**.cc` (so it gets the definition in `interpreter_common.cc:11548`), while the
   host `Makefile` uses an **explicit per-file patch list** that includes the patched `thread.cc`
   (which references the symbol at :4868) but NOT `interpreter_common.cc`. The host link passes
   `-Wl,--unresolved-symbols=ignore-all`, so it only surfaced at RUN time.
   ✅Fixed host-side in `stubs/link_stubs.cc` (backup `.pre603d`) — a `false` definition, rather than
   dragging the whole PFCUT interpreter into the compiler; the gate is read only by a diagnostic
   probe in `Thread::SetException`. ⚠️`stubs/link_stubs.cc` is host-only (arm64 uses
   `link_stubs_arm64.cc`), so this cannot affect the device build.
2. Fresh binary: `ceec8afb…`, stamps **image 118 / oat 247 / vdex 027** — matches libart `fecfe16f…`.

### ⛔RESULT: it spins IDENTICALLY to the July binary
```
   40 s : 99.7 % CPU, RSS 129 204 kB, d2o.err 254 891 B
 37 min : 99.9 % CPU, RSS 129 204 kB, d2o.err 254 891 B, 0 *.art
```
RSS and log **byte-identical** across 37 minutes ⇒ a spin. So the compiler defect is in the **current
tree**, not in a stale binary, and rebuilding it does not help. ★A.1 is answered.

### What the spin is (bounded diagnostic, capped at 150 MB)
`WESTLAKE_UNSAFE_JNI_DIAG=1`, stderr `| head -c 150000000` (head exits at the cap; dex2oat dies on
SIGPIPE). Evidence kept in `.shots/aotdiag/spin-context.txt` + `spin-head.txt`:
- **1 085 440** `[UNSAFE-JNI] method=compareAndSetInt class=Ljdk/internal/misc/Unsafe;` lines, and the
  last 200 KB is **1448/1448 identical** ⇒ an unbounded CAS retry loop in the unstarted-runtime
  interpreter.
- Last classes seen initializing before the storm: `Ljava/lang/UNIXProcess;` (via
  `Executors.newCachedThreadPool` → `ThreadPoolExecutor`, whose `ctl` is an `AtomicInteger` driven by
  exactly this CAS-retry shape) and `Ljava/security/PKCS12Attribute;`.
- ⚠️**Hypothesis, not proven**: naming the exact non-terminating `<clinit>` needs a per-class log
  before `EnsureInitialized` (a source change + host rebuild). Only failures are logged today, and the
  spinning clinit never fails.

### ⛔The `[WESTLAKE-BADTYPE]` lines in that log are FALSE POSITIVES — do not chase them
334 of them appear right before the storm, which looks damning. The probe
(`aosp-art-15/runtime/mirror/dex_cache.cc:218-239`, §119, written for an arm32 app bug) flags a
resolved-type pointer if **all 8 bytes are printable-or-zero**, or unaligned, or has high bits set.
Every flagged value here (`0x41337e20`, `0x412c2358`, `0x42007a60`, …) is **4-byte aligned and < 2^48**
— they trip only the ASCII clause, i.e. they are ordinary host-heap pointers whose bytes happen to be
printable. Not corruption.

### Verdict on Track A
The AOT chain is now: L1 ✅, L2 ✅, L3 ✅, **L4 = the image is under-populated because the only compiler
that finishes (Apr-3, 2.7 s) doesn't do the work, and the compiler that does the work never finishes.**
Fixing that is a dex2oat/unstarted-runtime debugging project, and it competes with the JIT render bug —
which gates a **measured 31× win** and would very likely block AOT-compiled code too (both go through
the same `kAccNative` gates §586/§594/§601). ★Recommendation: park AOT here and take the render bug.
★Board untouched throughout: libart `be828a5e…` (§593), pid live.

---

# ★★★★★§603e (2026-08-10) THE JIT DOES **NOT** BREAK RENDERING — that blocker was a screenshot artifact

The "one remaining problem between here and a shipping JIT" (§597/§598/§601: *"screen stays black
(36907 B), the UI never draws"*) is **WRONG and retracted**. The app renders fine with the JIT on.

## Setup (a real control this time)
Rebuilt the binary-patched JIT stack from recipes on the deployed §593 (`be828a5e…`), so the §436
guard is present and launches are not a lottery:
- ★**`recipes/patch586.py` had never existed** — §594/§596/§597 were scripted but gate 1 was not, so the
  JIT stack was not reproducible from recipes. Written this session (`a51938: tbnz w8,#8 -> b 0xa51a28`).
- §586 + §594 + §597 on §593 ⇒ **`72942da6…`**, all three sites byte-verified.
- Harness `ab_render.sh` (on device): same binary, same boot path, **the only variable is
  `APPSPAWNX_FORCE_JIT`**; waits on the `side-channels started` marker instead of a fixed sleep.

## Result
```
                    screenshot   compiled   child   sidechan  CreateWindow  SEGV
  JIT OFF (control)  124 835 B          0   ALIVE          1             1     0
  JIT ON             113 830 B       2270   ALIVE          1             1     0
  JIT ON (repeat)    113 830 B       2265   ALIVE          1             1     0
```
★**Both screenshots show the fully drawn Library screen** — title, LIFE header, six sound rows with
info/download/play/volume controls, the shuffle FAB and the five-tab bottom bar. Inspected visually,
not by byte count.

## Why the old reading was wrong
**36 907 bytes is the pre-draw screen, and it has nothing to do with the JIT.** I reproduced it exactly
— 36 907 B — on a **JIT-OFF** run whose screenshot was taken 25 s after spawn, before the app had drawn.
The JIT makes startup slower (2 270 methods compiled), so a fixed-delay screenshot lands before first
draw. ⇒ ★**never screenshot this app on a timer; wait for `side-channels started` + settle.**
The companion claim *"tab taps cost ~103 ticks instead of ~3200"* has the same explanation: the taps
landed before the UI existed.

## The REAL defect, and it is much narrower
The only visual difference is that the **per-row SVG artwork is missing** under JIT (birds, crickets,
heartbeat, purring cat, café, seagulls). Everything else is pixel-comparable. That accounts for the
113 830 vs 124 835 byte gap (a simpler image compresses smaller).

Log diff (distinct message shapes, JIT/VSYNC noise stripped, `ctrl.stderr` vs `jit.stderr`):
```
  ArrayStoreException      ctrl 0    jit 3      <-- JIT-ONLY
  PFCUT arraycopy intrinsic ctrl 1117  jit 1079  (both, so the interpreter path is still live)
  androidsvg mentions      ctrl 48   jit 69
```
JIT-only lines:
```
[CHILD_CK] J_invokeStaticMain_main_threw: java.lang.ArrayStoreException:
    java.lang.String cannot be stored in an array of type java.lang.String[]
[INITCHILD-FAIL] java.lang.reflect.InvocationTargetException: null
[INITCHILD-FAIL]   caused by: java.lang.ArrayStoreException: (same)
```
★**Storing a `String` into a `String[]` is always legal** — this is an *impossible* exception, i.e. a
**type-check that returns the wrong answer in/beneath compiled code**. It never occurs with the JIT off.

### Leading hypothesis (plausible, NOT yet proven)
The artwork is rendered by **`com.caverock.androidsvg`**, whose parser methods are in the compiled set
(`SVGParser$f.i()`, `SVG$u.f(byte)`, `KXmlParser.readValue/next`, …). The interpreter carries a
**`[PFCUT] System.arraycopy` intrinsic** with its own element type checks, visibly handling the
`Object[] -> TypedArray[]` case. §588 predicted exactly this failure mode: **compiled code bypasses the
PFCUT compat hooks**, so `System.arraycopy` (or an `aput-object`/`checkcast`) falls through to a path
whose assignability check is broken on this port ⇒ bogus ArrayStoreException ⇒ the SVG load aborts ⇒
no artwork, while the rest of the UI still draws.
⚠️The causal link between the ArrayStoreException and the missing artwork is **inferred, not shown** —
no Java frames are printed at that throw site.

### NEXT (decisive, small)
1. Print a **stack trace** at the ArrayStoreException throw site (the §571 exception plumbing already
   surfaces uncaught exceptions; this needs the frames). That names the array store and confirms or
   kills the SVG link in one run.
2. If it is `System.arraycopy`: compare the PFCUT intrinsic's element check against
   `Class::IsAssignableFrom` on this port — an impossible `String`→`String[]` failure means
   assignability is answering wrongly for identical classes (duplicate `Class` objects, or a bad
   read of `klass_`/component type).
3. Re-shoot the A/B afterwards; the artwork is the pass/fail oracle.

★Board restored and verified: libart `be828a5e…` (§593), pid live, `Ready to accept`=1.
★Artifacts: `.shots/shot_CONTROL.jpeg`, `.shots/shot_JITON.jpeg`, `.shots/shapes_{ctrl,jit}.txt`,
device `/data/local/tmp/asx/{ctrl,jit}.stderr`, harness `/data/local/tmp/asx/ab_render.sh`.

## ★★★★★§603f — ROOT CAUSE of the JIT artwork bug: TWO `java.lang.String` Class objects

Added an ASE-DIAG at `ThrowArrayStoreException` that dumps the Java stack **and** the class-pointer
identity, because the whole question was whether the comparison is broken or its inputs are.

### ⚠️Build-system trap found on the way (cost one rebuild)
`patches/runtime/common_throws.cc` is **DEAD** — it is in **neither** Makefile's patch list, so both
builds compile `aosp-art-15/runtime/common_throws.cc` instead. Editing the `patches/` copy produces a
binary with none of the change in it (byte-checked: the marker strings were absent).
★★**Correction to §600/§603d**: the arm64 Makefile does **not** glob `patches/**.cc` — it uses an
explicit per-file list too (25 entries). Several WESTLAKE probes therefore live directly in the AOSP
tree (e.g. the NPE-DIAG in `common_throws.cc`, the BADTYPE probe in `mirror/dex_cache.cc`).
★**Before editing any patched file, check it is actually in a Makefile, then byte-verify the marker.**

### ✅Also ported: the §436 guard to SOURCE (§551 in C++)
`interpreter_common.cc`: new `PFCutDeclaringClassPlausible(ArtMethod*)` — non-null, 4-byte-aligned
method, declaring class `>= 0x1000` and aligned — replacing the bare `X->GetDeclaringClass() != nullptr`
at **all 8 sites**. ⚠️do the replacement with a word-boundary regex: a plain `method->` replace corrupts
`called_method->` into `called_PFCut…(method)` (it compiled to garbage names and had to be redone).
Result: the source build **launched first try** (channels up, `CreateWindow=1`, SEGV=0) where §600
measured 1/3. Build `24c2fc6c…` = §601 JIT + §603 trust gate + kImageVersion 118 + ASE-DIAG + §436 guard.

### ★★★THE ANSWER
```
[WESTLAKE-ASE-DIAG] #1 element=0x1400c800 (java.lang.String)
                       array=0x14000660 (java.lang.String[])
                       component=0x10780  (java.lang.String)   element_eq_component=0
```
identical on all occurrences. **`element_class` and `array_class->GetComponentType()` are two DIFFERENT
`Class` objects that both describe `java.lang.String`.** So the assignability test is *correct* — its
**inputs** are wrong. This is a **class-identity / duplicate-Class defect**, not a compiler bug.
⚠️`0x10780` is implausibly low for a heap object, so the component-type *field* may itself be corrupt
rather than pointing at a legitimately-created second String class. Distinguishing those is the next
probe (print the component's own class + class loader).

### ★And the stack proves the artwork link (previously only inferred)
```
  at com.android.org.kxml2.io.KXmlParser.parseStartTag(KXmlParser.java:1142)
  at com.android.org.kxml2.io.KXmlParser.next / nextToken
  at com.caverock.androidsvg.SVGParser.G(SVGParser.java:261)
  at com.caverock.androidsvg.SVGParser.h(SVGParser.java:54)
  at f3.n.run -> Handler -> Looper -> ActivityThread.main
```
`KXmlParser.parseStartTag:1142` is the attribute-array growth
(`String[] bigger = new String[...]; System.arraycopy(attributes, 0, bigger, 0, …)`).
⇒ **SVG parse → XML attribute array grow → arraycopy → bogus ArrayStoreException → the SVG never
loads → the row artwork is missing.** Hypothesis §603e is now **confirmed end to end**.

### Why only with the JIT on — §588 was right
The interpreter services `System.arraycopy` through the **PFCUT intrinsic** (visible in both logs), which
does not perform the element-wise assignability check. When the caller is JIT-compiled, the call goes to
ART's real `System.arraycopy`, which **does** check — and the pre-existing duplicate-Class defect finally
becomes visible. **The JIT did not create this bug; it uncovered one the PFCUT hook was masking.**

### NEXT
1. Probe where the second String class comes from: log `GetClassLoader()` and the defining dex file for
   both `element_class` and the component, plus `class_linker`'s class-table entry for
   `Ljava/lang/String;`. Duplicate BCP class vs corrupt field is the fork in the road.
2. If duplicated: fix the class-loading path (this port seeds `ServiceManager`/BCP in unusual ways).
   If corrupt: find who writes the array class's component-type field.
3. Oracle stays the artwork: `ab_render.sh JITON …` and compare against 124 835 B.
★Board restored: libart `be828a5e…` (§593), pid live, `Ready to accept`=1.

## ★★★★★§603g — FULL ROOT CAUSE: `String[]`'s component type points at a **RETIRED** String class

Extended the ASE-DIAG to dump class identity (meta-class, loader, dex, status) for all three classes.
Build `2e7322a2…`. Result, identical on every occurrence:
```
element  : cls=0x1400c800 meta=0x103c0(java.lang.Class) meta_ok=1 loader=0 status=15 dex=…/core-oj.jar
array    : cls=0x14000660 meta=0x103c0(java.lang.Class) meta_ok=1 loader=0 status=15 array=1
component: cls=0x10780    meta=0x103c0(java.lang.Class) meta_ok=1 loader=0 status=1  dex=…/core-oj.jar
```
So the "corrupt field" branch is **dead**: both are genuine `java.lang.Class` instances, same meta-class,
same **boot** loader (0), same defining dex (`core-oj.jar`). They are **two real `java.lang.String`
class objects**, and the decisive difference is the STATUS (`runtime/class_status.h`):
```
  status 15 = kVisiblyInitialized   <- the live String (element)
  status  1 = kRetired  "Retired, should not be used. Use the newly cloned one instead."
                                    <- what String[].GetComponentType() returns
```

### The mechanism, located exactly in source
1. `class_linker.cc:1099-1103` (`InitWithoutImage`, the path this port uses) creates `java.lang.String[]`
   and binds `object_array_string->SetComponentType(java_lang_String.Get())` — the **early** String object.
2. `ClassLinker::LinkClass` later finds the class is a **temp**, clones it into a correctly-sized object
   and retires the original: `:7563 FixupTemporaryDeclaringClass(klass, h_new_class)` →
   `:7566 table->UpdateClass(h_new_class, …)` → `:7584 SetStatus(klass, ClassStatus::kRetired)`.
3. ★`FixupTemporaryDeclaringClass` only repoints the **declaring class of fields and methods**
   (`:7311-7318`), and `UpdateClass` only fixes the **class table**. **Nothing updates any array class's
   `component_type_`.**
⇒ `String.class` resolves to the new object, while `String[].getComponentType()` still returns the
retired one ⇒ `element != component` ⇒ the array-store check correctly refuses ⇒
`ArrayStoreException: java.lang.String cannot be stored in an array of type java.lang.String[]`.
(Address layout corroborates: the retired String `0x10780` sits beside `java.lang.Class` `0x103c0` in the
early boot allocation region, while the live String `0x1400c800` is in the main heap.)

### Why it only shows with the JIT on
Interpreted `System.arraycopy` is served by the **PFCUT intrinsic**, which does not perform the
element-wise assignability check. A JIT-compiled caller reaches ART's real `System.arraycopy`, which
does — so a **pre-existing class-linking bug becomes visible**. The JIT is the messenger.

### The fix (three candidates, cheapest last is probably the correct one)
1. **Targeted**: in the retire path (`class_linker.cc` ~7563-7584), after `UpdateClass`, repoint any array
   class whose `component_type_ == klass` to `h_new_class` — at minimum the `InitWithoutImage` roots
   (`kJavaLangStringArrayClass`, and check `Object[]`/`Class[]` for the same pattern).
2. **General**: do that fixup for every array class found in the class table.
3. ★**Root-of-the-root**: ask why `java.lang.String` is a temp at all. `InitWithoutImage` allocates it with
   `mirror::String::ClassSize(image_pointer_size_)`, which should be exact, so stock ART never re-clones
   it. Something in this port makes String's linked size differ from the early estimate. Fixing THAT
   removes the retire entirely and is the smallest correct change.

### Why this matters far beyond the artwork
A boot image is a frozen snapshot of class objects and their identity relationships. **A retired-class
reference cannot survive serialization**, and the two AOT failures already seen are the same shape:
`CreateProxyConstructor` finding `Proxy.NumDirectMethods()==0` (a class root pointing at the wrong
object) and §318's IMT/`AbstractMethodError`. ⇒ **Fix class identity before spending anything more on
AOT.** ★Oracle for the fix: the row artwork (`ab_render.sh JITON …`; 124 835 B good / 113 830 B broken).

★Board restored: libart `be828a5e…` (§593), pid live, `Ready to accept`=1.

## ★★★★★§603h — **FIXED**: the JIT now renders the app correctly

### The fix (5 lines of logic, `patches/runtime/class_linker.cc` in `CheckSystemClass`)
The port already *detected* the String mismatch and repaired the **class root**
(`SetClassRoot(kJavaLangString, c2)`), but nothing repaired the **array classes** bound to the old
object back in `InitWithoutImage:1103`. Added, right after the root repair: sweep
`kJavaLangStringArrayClass` / `kObjectArrayClass` / `kClassArrayClass` and repoint any whose
`GetComponentType() == c1` to `c2`.

### Why the mismatch happens at all (the size skew, from the port's own diagnostic)
```
c1 (pre-allocated) ptr=0x10780    classSize=784 status=1 numVTable=81 numMethods=0
c2 (from DEX)      ptr=0x1400c800 classSize=808 status=7 numVTable=-1 numMethods=131 numSFields=7
```
`mirror::String::ClassSize()` (compiled from aosp-art-15) predicts **784**; this port's
`core-oj.jar` String needs **808** (131 methods / 7 static fields). So `LinkClass` must clone into a
bigger object and retire the original — **BCP-jar vs ART-version skew, inherent to the port**, which is
also why stock ART's `LOG(FATAL) << "Class mismatch … most likely a broken build"` was long ago
downgraded here to `LOG(WARNING) … (continuing for standalone dex2oat)`. Repairing consistently is the
right call; making it fatal again is not an option for this port.

### VERIFIED — same build, JIT ON
```
                     screenshot   ArrayStoreException  INITCHILD-FAIL  androidsvg
  before (§603e-g)   113 830 B            3                  2             69
  after  (§603h)     125 132 B            0                  0             48   <- matches control
  JIT-OFF control    124 835 B            0                  0             48
```
★**Visually confirmed**: the per-row SVG artwork (birds, crickets, heartbeat, purring cat, café,
seagulls) is BACK with the JIT on — `.shots/shot_FIXJIT2.jpeg`. Parent log shows the repair firing:
`[§603h] [Ljava/lang/String; component retargeted 0x10780 (retired) -> 0x1400c800`
(only `String[]` matched; `Object[]`/`Class[]` were already correct).
⇒ **The JIT's last known functional blocker is closed.** libart `15149894…`.

### ⚠️Still open: the §436 source guard is INCOMPLETE
The first FIXJIT attempt failed to launch with the classic §436 signature — child ALIVE, stderr frozen
at 386 890 B, `CHILDSEGV` capped at 78, `sig=11 addr=0x45 pc=…interpreter::DoCall … INVOKE_INTERFACE`.
The retry succeeded. So §603f's port covered only the 8 `X->GetDeclaringClass() != nullptr` sites; the
faulting deref in `DoCall` is a **different** `GetDeclaringClass()` use not guarded by that idiom.
★NEXT: find the remaining deref(s) on the `DoCall`/`INVOKE_INTERFACE` path (compare against what the
§551 code cave guards at 0xa89794) and route them through `PFCutDeclaringClassPlausible` too, then run
the 6-attempt launch taxonomy expecting 6/6.

### Status of the JIT overall
✅compiles, ✅executes (31-44× on compute), ✅survives sustained load, ✅**renders the app correctly**.
Remaining before it could ship: the §436 guard completion (launch reliability), and the source build
still lacks the binary-only PFCUT perf patches (§589/§591, −34%), so an app-level perf comparison is
not yet apples-to-apples.
★Board restored: libart `be828a5e…` (§593), pid live, `Ready to accept`=1.

## §603i + MERGE — mini-debug-info abort fixed; all JIT fixes consolidated into source

### §603i: the JIT aborted the app mid-use, and it was the DEBUG INFO, not the code
Walking noice's tab bar with the JIT on killed the child:
```
xz_utils.cc:90] Check failed: res == 0 (res=-1, 0=0)   [held: "JIT native debug entries", "Jit code cache"]
Runtime aborting...   ->  [WESTLAKE-FATALSIG] signal=6
```
Every JIT-compiled method adds a mini-debug-info ELF and the periodic repack XZ-compresses them;
compression eventually fails here and the CHECK is fatal. `kDefaultGenerateMiniDebugInfo` is **true** in
stock ART (`compiler/driver/compiler_options.h:64`). Nothing on this port consumes that data (no
debugger, no simpleperf symbolization), so it is pure overhead plus a fatal failure mode ⇒ set **false**.
✅After: the same 5-tab walk survives (ALIVE ×5), no abort.
⚠️★**The Makefile has NO header dependency tracking** — editing the header rebuilt nothing and the md5
was unchanged. Had to `rm build-ohos-arm64/compiler/driver/compiler_options.o` by hand. Always
byte-verify a marker after any header edit.

### ⚠️Tab navigation does not repaint on the SOURCE build — and it is NOT the JIT
After the abort was fixed, tab taps are delivered (`dispatchTouchViaViewRoot … delivered`) and the app
stays alive, but the screen freezes after one navigation (byte-identical screenshots).
★**CONTROL RUN, same build, JIT OFF: identical behaviour** (frozen at 76 261 B across 3 taps).
So this is a **source-vs-binary-patch gap**, not a JIT regression — the source build lacks the
binary-only stack (§589/§591 PFCUT perf, §551/§593, …) that makes the deployed §593 fully interactive.
⇒ Do not chase it as a JIT bug. (Fourth time the control has overturned a JIT verdict this session.)

### MERGE — everything is now in source
`recipes/603-jit-complete.patch` (555 lines, 8 files) + `recipes/603-jit-complete.md` (manifest).
Covers §601 (enable) + §603f (§436 guard, partial) + §603h (render fix) + §603i (mini-debug-info)
+ §602/§603c (image v118) + §603 (trust gate) + §603d (host link symbol).
★**The patch spans TWO trees** — `art-latest/patches/**` and `aosp-art-15/**` — because
`Makefile.ohos-arm64` uses an explicit 25-entry list, NOT a `patches/**.cc` glob. Part A/Part B are
marked in the patch header.
★Board restored: libart `be828a5e…` (§593), pid live, `Ready to accept`=1.
