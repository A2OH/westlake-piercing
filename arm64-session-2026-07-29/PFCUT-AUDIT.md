# PFCUT hook audit (2026-08-08) — is each interpreter hook redundant now?

## Scale (validates "wrong layer" critique)
The deployed libart carries **~130 distinct PFCut hooks** (395 symbols incl. templates): a whole
app-compat layer built INSIDE the interpreter — Gson, Kotlin reflection, Realm, Hilt, WorkManager,
charset/locale/filesystem/Unsafe/atomic intrinsics, proxy repair, NewRelic/analytics no-ops, etc.

## Dispatch structure (the tax)
`DoCallCommon<false>` @0xa48640 (~17KB) inline-dispatches per invoke:
  **61 PFCut hook calls + 84 strcmp + 15 DescriptorEquals**, each `bl hook; tbnz w0,#0,handled`.
Returning w0=0 (false) from a hook = "not handled, do the normal invoke". So a hook is disabled by
patching its entry to `mov w0,wzr; ret` (precedent §571). This is the audit lever.

## Results
| hook | verdict | evidence |
|---|---|---|
| **PFCutTryProxyInvoke<0/1>** (0xa5db7c/0xa7d23c) | **REDUNDANT — removed (§575)** | disabled: app renders, tab-switches, `NoSuchMethodError=0`, `InvokeType(4)=0`. §440 (dex→WlProxy) + §551 (FindVirtualMethodForInterface) cover it. |
| **PFCutTrySystemTimeIntrinsic** (0xa62080) | **REDUNDANT — removed (§575)** | disabled: app works, 0 clock errors. §534 fixed the underlying clock; fall-through is correct. |
| **PFCUT-IFACE** interface-repair (DoInvoke, "repaired null interface method") | **STILL NEEDED** | 182 live repairs of *null* interface methods with proxy hook off. §551 fixed the *corrupt* ArtMethod case, NOT the null case. Do not remove. |
| **AtomicInteger intrinsic** (PFCutTryAtomicIntegerOrBooleanIntrinsic) | **INLINED into DoCallCommon** | no standalone symbol — can't entry-patch. It is the JIT-death fingerprint (last log `AtomicInteger.compareAndSet current=0xE0000001`). Needs the pre-filter/right-layer approach, not a hook-disable. |
| PFCutTryAtomicLongIntrinsic<0/1> (0xa548ec/0xa74bf4) | untested | standalone; disablable if needed. |

## JIT
Disabling proxy+systemTime does NOT make the JIT survive (still SOE on first tap, compiled=0) —
expected, since neither logs at JIT death. The JIT culprit is on the atomics/coroutine path
(inlined), consistent with the 0xE0000001 scheduler-ctl CAS.

## The real perf lever (not these 2 hooks)
Removing 2 of 61 hooks is ~3% of the dispatch chain — not a breakthrough. The right-layer-within-
libart fix for the 44% predicate tax is a SINGLE cheap pre-filter gate at the top of DoCallCommon's
dispatch: "does this method's declaring class belong to any hooked package?" — short-circuiting the
99.9% of invokes that match nothing, instead of 61 calls + 84 strcmp each. That also shrinks the
JIT-interaction surface. Next step for both perf and JIT.

## ★★HEADLINE (2026-08-08): this libart's PFCUT layer was built for the McDONALD'S app
The 37 per-invoke strstr needles in DoCall<false>/<true> are NOT generic — they are literal
McDonald's-app class names: `mcdonalds`, `Lcom/mcdonalds/mcdcoreapp/common/activity/SplashActivity`,
`Hilt_SplashActivity`, `McdLauncherActivity`, plus `newrelic` (their analytics). The rest are
framework/library prefixes (androidx/*, com/google/gson, java/lang/reflect, kotlin/jvm/internal,
kotlin/Lazy) and `SurfaceControl`. So the ~130-hook machinery is largely another app's baggage that
noice drags through on every invoke.

### §576 — null the dead McDonald's/NewRelic needle scans (safe, measured)
noice dex has **0 mcdonalds, 0 newrelic** classes, so those strstr always return null. Replaced
`bl strstr` -> `mov x0,xzr` (x0=null = strstr's no-match result; the following `cbnz x0,handler`
falls through identically) at **20 sites** (10 per DoCall template). recipes/patch576.py +
mcd_sites.json.

MEASURED (on §575 base + §576):
  strstr CPU share  ~28% -> ~24%
  library tab       ~5550 -> ~4810 ticks  (~13% faster)   presets ~120 -> ~108
  app fully works (renders, tab-switches, 6/6 taps), no behavior change.

### Cumulative safe removals so far
  §575: proxy-invoke + systemTime hooks (redundant via §440/§551/§534)
  §576: 20 McDonald's/NewRelic dead needle scans
Deployed baseline libart md5 = dc01de5509ae10a426370f2002b59294.

### Continued path
The remaining ~17 needles/DoCall (androidx/kotlin/gson/reflect/SurfaceControl) still scan per invoke.
Audit each: does noice actually need that compat hook, or is it McDonald's behavior that merely
also matches noice's androidx/kotlin methods (applying the wrong app's shim)? The endgame is either
(a) a single pre-filter gate at DoCall's `cbz x28` point, or (b) recognizing the whole PFCUT layer is
the wrong app's and stripping it to only what noice needs (interface-repair is confirmed needed).
JIT still blocked by the inlined atomics/coroutine path.
