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

## §577 — remove ALL 74 DoCall needles: strstr 28%->2.3%, but a launch-reliability regression
Goal: strip the entire per-invoke needle classifier, add back only what noice needs. Nulled all 74
`bl strstr` -> `mov x0,xzr` (recipes/patch577.py + all_needle_sites.json), on the §576 base.

### What the needle chain actually is
DoCall<false>/<true> run 37 strstr each = a McDonald's-era PACKAGE CLASSIFIER (needles: mcdonalds*,
newrelic, androidx/*, com/google/gson, java/lang/reflect, kotlin/*, SurfaceControl). The REAL hook
dispatch (atomics/Unsafe/arraycopy/gson/lifecycle/proxy/interface/charset) lives in DoCallCommon,
which has its OWN strcmp dispatch and runs INDEPENDENTLY. Proven: with all 74 needles nulled the
load-bearing hooks still fire (arraycopy/Atomic/Unsafe/PFCUT-IFACE/PFCUT-PROXY/Charset all non-zero).

### Which needle-gated hooks noice actually uses (from the log)
FIRE: Gson (89), androidx Lifecycle (17), Hilt (15), MethodHandles (7).
NEVER fire: KotlinReflection, WorkManager, Splash, SavedState, ClassNewInstance,
ClassGetDeclaredField (0 each) — pure dead weight for noice.

### MEASURED (§577, good launch)
  strstr CPU share  ~28% -> **~2.3%**  (interpreter now spends its time in ExecuteSwitchImplCpp)
  render, all 5 tabs, play UI (Unsaved-Preset bar), save FAB: all work; SOE=0; alive.

### ⚠️THE REGRESSION (this is what must be "added back")
§577 degrades LAUNCH RELIABILITY: 5+5 relaunch attempts failed to get an input channel
(side-channels never started), then reverting to §576 got chan=1 on the FIRST try on the same board.
So >=1 of the 54 non-McDonald's needles gates a hook the window/session/adoption path needs. That is
the concrete "add back for noice" item. (Basic once-good launch worked, so it is a rate regression,
not a hard break.)

### NEXT (finish the goal)
Bisect the 54 non-mcd needle sites: null half, measure channel-success rate over ~5 launches;
narrow to the offending needle(s); re-provide ONLY that hook (cheaply gated) and keep the rest
removed. Audio also still needs a clean confirm (ExoPlayer engages; audio-out is slow-start ~min).

### Deployed baseline: §576 (dc01de55...) — proven safe, ~13% library-tab win. §577 NOT deployed.

---

## ★★§589 (2026-08-09): needle classifier cut 54 -> 14 sites, **library tab 4951 -> ~3650 ticks (-26%)**

### First: the needle list everyone had been guessing at is now EXTRACTED
`recipes/extract_needles.py` resolves the literal behind every `bl strstr` in DoCall<false>/<true>.
**14 distinct needles, 54 live sites** (each scanned on EVERY invoke), all confirmed against objdump:
```
4x androidx/activity   4x androidx/appcompat  4x androidx/core/app   4x androidx/core/splashscreen
4x androidx/core/view  4x androidx/fragment   4x androidx/lifecycle  4x androidx/loader
4x androidx/savedstate 4x com/google/gson     4x java/lang/reflect   4x kotlin/Lazy
4x kotlin/jvm/internal 2x SurfaceControl
```
⚠️**THE TRAP that cost two wrong extractions:** `.text` maps `file_off = vaddr-0x1000`, but `.rodata`
maps **IDENTITY** in this .so. Applying the .text rule to a .rodata pointer lands 4096 bytes away and
yields a *different but plausible-looking* string — that is how a pass "found" needles such as
`CumulativeLoggerLock`, `Zip: bad uncompressed length`, `string_id.index_`. **Always resolve
vaddr->offset from the section headers.** (Fixed in extract_needles.py.)

### The patch
`recipes/patch589.py` nulls (`bl strstr` -> `mov x0,xzr`) the **10 needles the audit measured as never
firing for noice** (40 sites), and KEEPS the 4 plausibly load-bearing ones (14 sites):
`SurfaceControl` (prime suspect for the §577 launch regression — window/session path),
`com/google/gson` (fires 89x), `androidx/lifecycle` (17x), `java/lang/reflect` (MethodHandles 7x).

### MEASURED (post-reboot, clean state) — libart md5 0100cd5fa46ca5d5854e86025cd4c66a
```
library tab   §576 4901/4973/4979 (~4951)  ->  §589 3587/3682/3681 (~3650)   -26%
presets tab   §576 179/106/115             ->  §589 147/88/90
launch        first try (relaunch.sh attempt 1/4, side-channels=1)
render        124523 bytes (live UI)       taps 6/6 delivered
hooks alive   PFCUT-IFACE 286  PFCUT-PROXY 736  arraycopy 1225  Atomic 3100
              Unsafe 5915  Charset 474  Gson 81
errors        NoSuchMethodError/ClassNotFound/AbstractMethodError/StackOverflow = 0
              JNIMISS 19 == §576 control; "FATAL" hits are only [WESTLAKE-FATALSIG] setup lines
```
Cumulative: §575 (2 redundant hooks) + §576 (20 mcd needles) + §589 (40 dead needles).

### ⚠️§577's "LAUNCH REGRESSION" IS NOW SUSPECT — it was probably measurement noise
§577 was rejected because "5+5 relaunch attempts failed to get an input channel". Re-measured here
with a **control**, under the same repeated-`pkill -9` harness:
```
  §589  1/5 launches got a side-channel
  §576  1/5   <-- THE CONTROL IS IDENTICAL
```
So that harness degrades launch reliability *regardless of libart* (consistent with the known
"SIGKILL cycles degrade the input channel" rule) — a treatment measured with no control. After a
**reboot**, §589 launched on the first attempt. §577 may well be safe; its rejection rests on the
same confound. ★Never judge launch reliability without a same-harness control, and prefer post-reboot.

### NEXT
Test the remaining 4 needles individually **post-reboot with a control** (not the pkill harness).
If `SurfaceControl` alone is the launch-critical one, nulling gson/lifecycle/reflect gets close to
§577's strstr ~2.3% for a further win. Do them ONE AT A TIME — all three currently fire, so each
carries real behavior risk.

## §590 (2026-08-09) — per-invoke `getenv` nulled: MEASURED NO-OP, **not shipped**

hiperf on §589 showed `getenv` at 2.46%. Cause is real: westlake predicates read diagnostic env vars
*per invoke* (`bl getenv` inside DoCall<false>/<true> and the Execute path), and musl's getenv is a
linear scan (§557 already added an index cache in appspawn-x and it was still 2.46%).
`recipes/patch590.py` nulls the 8 per-invoke sites whose vars are all UNSET in the child
(`WESTLAKE_NO_PROXYFIX`, `WL_BADREF`, `WESTLAKE_TRACE_TZ`, `WESTLAKE_TRACE_INTERP_JNI`,
`WESTLAKE_UNSAFE_JNI_DIAG` — verified against /proc/<pid>/environ and testnoice.sh), leaving the
load-time reads (`LD_LIBRARY_PATH` etc.) alone.

RESULT: library tab **3630 vs §589's 3650 — no change** (within noise). Profile confirms why:
getenv only fell 2.46% -> 1.84%, so most callers are OUTSIDE the interpreter region scanned
(0xa40000-0xac0000). **Reverted; §589 remains the baseline.** Keep patch590.py only as a record —
re-running it would permanently disable those debug vars for no gain.
★Lesson repeated: a symbol's profile share is not automatically recoverable; find the CALLERS first.

## ★PROFILE OF RECORD (§589, hiperf, library-tab switch) — where the time actually goes
```
  8.09%  art::mirror::Class::DescriptorEquals      <-- BIGGEST single item ("15 per invoke")
  7.71%  strstr           (appspawn-x, §532 fast)  <-- the last 14 needle sites
  4.73%  strcmp           (musl)                   <-- the "84 per invoke"
  4.43%  DoCallCommon<false>
  3.67%  ExecuteSwitchImplCpp<false>               <-- the ONLY line doing real work
  2.89%  __emutls_get_address (hook TLS)
  2.64%  DoCall<false>
  2.46%  getenv          (see §590 - not recoverable there)
  2.29%  Class::GetDescriptor      1.91% ArtMethod::GetNameView    1.54% Signature::operator==
  by module: libart 26.3%  appspawn-x 10.2%  musl 4.7%  libc++ 2.9%
```
**The interpreter spends ~31% on PFCUT predicates and 3.67% interpreting.** That is the headline
number for any future perf work.

### NEXT LEVER (ranked by the profile, not by guesswork)
1. **`DescriptorEquals` + `GetDescriptor` ≈ 10.4%** — the audit's single pre-filter gate at the top
   of DoCallCommon's dispatch would remove most of this AND the 4.73% strcmp: one cheap
   "is this class in any hooked package?" test instead of 61 calls + 84 strcmp + 15 DescriptorEquals.
   Highest value, needs a code cave.
2. **`strstr` 7.71%** — the last 14 needle sites (SurfaceControl/gson/lifecycle/reflect). Test ONE
   AT A TIME post-reboot with a control; all three non-SurfaceControl ones currently fire.
3. `__emutls_get_address` 2.89% — per-invoke TLS in the hooks; falls out of (1) if the gate
   short-circuits before the hook bodies.

⚠️LAUNCH MEASUREMENT (re-confirmed the hard way): reliability is a LOTTERY dominated by SIGKILL churn
and the known residual SEGV — §589 was seen at 1st-try-good, 4/4-fail, and good-on-retry across
sessions, and §576 fails the same way. A dead child logs no SIGSEGV here; it just stops mid-log, so
**check `ps` for the process, not the log, before calling a launch "channel-less".** Never grade a
patch on launch attempts without a same-harness control.

## ★★★§591 (2026-08-09) — ALL 54 needles nulled. §577 WAS WRONGLY REJECTED. **library 4951 -> ~3270 (-34%)**

`recipes/patch591.py` = §589 + the last 14 sites (`SurfaceControl`, `com/google/gson`,
`androidx/lifecycle`, `java/lang/reflect`) — i.e. the full §577 configuration, retested with the
methodology §577 lacked.

### MEASURED (post-reboot, libart md5 5c243589ed18adb0308bb7db7af283a4)
```
library tab   §576 ~4951  ->  §589 ~3650  ->  §591 3256/3287/3384/3436/3480  (~3270-3430)
presets tab   §576 ~120   ->  §591 141/87
strstr share  §589 7.71%  ->  §591 2.94%      (matches §577's reported ~2.3%)
launch        GOOD on round 1 post-reboot, twice, on two separate boots
5-tab walk    124438 / 72837 / 62993 / 49737 / 81066 bytes
              == BYTE-IDENTICAL to the §589 control on 4 of 5 tabs
hooks         Gson 82  Lifecycle 17  MethodHandle 8  PFCUT-IFACE 286  PFCUT-PROXY 736
              Unsafe 5943  Atomic 3181   -- i.e. UNCHANGED from §589
```

### Why nulling "load-bearing" needles is safe (the thing §577 never established)
The DoCall needles are a **redundant classifier**. The real hook dispatch lives in DoCallCommon and
runs off its OWN strcmp chain, independently. Proof: with all 54 needles nulled, Gson still fires 82x,
Lifecycle 17x, MethodHandles 8x — the same counts as §589. The needle scan was pure per-invoke waste.

### ⚠️THE NEAR-MISS — read this before rejecting any future patch
The §591 walk logged `NoSuchMethodError: No InvokeType(2) method setProperty ... tagsoup.Parser`
tagged `[INITCHILD-FAIL]`, absent from the previous §589 run. That looked like a clear functional
regression, and §591 was about to be rejected for it.

Running the **identical walk on §589 as a control** produced `NoSuchMethodError=3, tagsoup=3,
INITCHILD-FAIL=2` — **exactly the same**. The error is pre-existing; the earlier §589 run simply
never visited the tab that parses HTML (bench touches 2 tabs, the walk touches 5). n=1-vs-n=1
comparisons of DIFFERENT workloads are worthless.

That is the second time this exact mistake nearly cost a real win (the first was §577 itself, and a
third variant produced a wrong JIT verdict). **Rule: same harness, same coverage, run the control.**

### Cumulative
§575 (2 redundant hooks) + §576 (20 mcd needles) + §589 (40 dead needles) + §591 (last 14)
= the entire DoCall needle classifier is gone. **Library tab 4951 -> ~3270 ticks, -34%.**

### NEXT (profile-ranked, unchanged)
`DescriptorEquals` is now the top item at **9.92%** (+ `GetDescriptor` 2.68%, `strcmp` 5.22%, which is
largely called FROM it). It resolves the descriptor through the dex (dependent loads + ULEB128) and
ends in strcmp, once per comparison. ⚠️The audit's "single pre-filter gate at DoCallCommon" is NOT
straightforward: the 11 DescriptorEquals and 84 strcmp sites are scattered across all 17KB of
DoCallCommon, interleaved with the real invoke logic — there is no contiguous block to jump over.
The tractable idea is **memoizing DescriptorEquals** (consecutive calls query the same class), which
needs writable storage + thread-safety analysis. Treat as a project, not a patch.

## ⛔§592 (2026-08-09) — disabling "dead" hooks ABORTS the runtime. Reverted. Read before retrying.

After §591 removed the whole needle classifier, the remaining dispatch is **57 distinct PFCut hooks /
61 call sites** still called per invoke from DoCallCommon (list: `recipes/patch592.py`). §592 disabled
the 20 .text symbols of the 11 hooks with the best "dead for noice" evidence — the McDonald's set
(McdLogger/JustFlipEvent/PerfAnalytics/NetworkBoundary), NewRelic, Realm, and the four the audit had
measured firing **0** times (KotlinReflection, AndroidxWorkManager, AndroidxSplash, ClassNewInstance,
ClassGetDeclaredField) — by the standard `mov w0,wzr; ret` entry patch.

RESULT: **12 consecutive failed launches** across 3 rounds post-reboot (§591 gets a good launch on
round 1-2 every time). The child ABORTS:
```
runtime.cc:1863] Found invalid root: 0 Type=RootType(2) thread_id=1   (xN)
[WESTLAKE-FATALSIG] signal=6 code=-6
```

### Why — the assumption behind "disable a hook" is WRONG for this class of hook
"Returning false = not handled, do the normal invoke" is only safe when the hook is a *predicate*.
Several of these are **implementations**: `ClassNewInstance`, `ClassGetDeclaredField`,
`WorkManagerConstructorLite` produce an OBJECT because the normal path does not work in this port —
that is precisely why the hook exists. Disabled, the normal invoke yields nothing, a null lands where
a live reference belongs, and the next GC root check aborts the runtime.

⚠️Also note the audit's "fires 0 times" evidence was **log-based** and therefore only covers hooks
that log; it is NOT proof a hook is unused. Do not treat it as such again.

### If retrying, do this instead
Split the set: the pure **`*Noop`** hooks for other apps (Mcd*, NewRelic, Realm, AndroidxSplash) are
the defensible subset — they suppress foreign behaviour and produce nothing. Keep every
`*Intrinsic`/`*Constructor`/`*Fallback` that returns a value. Test the Noop-only subset ALONE,
post-reboot, against a §591 control. Expected gain is small (a handful of the 61 calls), so weigh it
against the risk before spending a cycle.

### Deployed baseline remains §591 (5c243589ed18adb0308bb7db7af283a4) — library ~3270 ticks, -34%.

---

# ★★★★★THE LAUNCH LOTTERY IS THE §436 SEGV — AND IT HANGS RATHER THAN CRASHES (2026-08-10)

Chased all session as "noise". It is not noise: it is a **deterministic, diagnosable defect**, and it
is the residual of §436/§551.

## Symptoms and how to recognise it
A failed launch is NOT a crash and NOT a missing channel:
```
child process        ALIVE (a `ps` check says LIVE - the log just stops)
side-channels        0, CreateWindow never reached
adapter_child log    FROZEN at ~385KB, byte-identical across failures
one thread           "SharedPreferenc" pegged at ~500 ticks/4s (state=R), everything else asleep
```
Measured 6 consecutive launches: attempt 1 good, attempts 2-6 all failed identically.

## Root cause (direct capture, not deduction)
`hilog -P <pid>` on the stuck child shows a **SIGSEGV storm** — thousands per second, same millisecond:
```
MUSL-SIGCHAIN: signal_chain_handler call usr sigaction for signal: 11
DfxSignalHandler :: signo(11), si_code(1), pid(...), tid(...)      <- repeats forever
```
and westlake's own handler names the fault:
```
[WESTLAKE-CHILDSEGV] #0 sig=11 code=1 addr=0x45 pc=libart.so+0xa897a4 sym=DoCall<false>
    fr00 InstructionHandler<...>::INVOKE_INTERFACE
    fr01 ExecuteSwitchImplCpp<false>   fr03 ArtInterpreterToInterpreterBridge   fr05 DoCall<false>
```
**`addr=0x45` is the §436 signature** — `FindVirtualMethodForInterface` returns a corrupt ArtMethod
whose `declaring_class_ == 5`, so the field load touches 5+0x40 = 0x45. §551 guarded the POINTER but
NOT THAT FIELD (memory records exactly this), so a residual remains.

## Why it HANGS instead of crashing — the important part
The SEGV handler chain runs, returns, and the faulting instruction **re-executes and faults again,
forever**. Nothing dies, so every "hang"/"no input channel" symptom is really this loop. Corroborated:
`minflt` flat (26375 -> 26375, no page-fault storm), 84% of samples in `DoCall<false>` (interpreting),
and the syscall traffic is `writev -> unix_dgram_sendmsg` = the signal handlers **spamming hilog**.
This is consistent with the recorded OHOS quirk that *sigreturn drops the handler's PC edit*
(startup-flaky-getapplicationinfo-fix-2026-06-29), so the handler can never redirect past the fault.

⚠️`WESTLAKE-CHILDSEGV` stops at 78 lines — the logger CAPS. A frozen stderr log does NOT mean the
process is idle. Check `hilog`, thread CPU, and `ps` before concluding anything.

## Why this poisoned every measurement this session
Every "launch reliability" number - including §577's rejection - was really sampling how often this
loop is hit. It is also why §589/§576 both score ~1/5 under `pkill` churn yet launch first-try after a
reboot. **Launch counts are not a valid signal for grading a libart patch.** ⇒ [[always-run-the-control]]

## THE FIX (well-defined, next session)
Extend the §551 code-cave guard in `FindVirtualMethodForInterface` to validate the **`declaring_class_`
field**, not just the ArtMethod pointer — reject/repair when it is not a plausible heap pointer (the
observed bad value is 5). Precedent + constraints: §551's cave at pad `0xfd9818`; ⚠️`cbz`/`b.cond`
reach only ±1MB.
Secondary (cheap, independent): make westlake's SEGV handler **fail fast** after N faults at the same
PC instead of returning forever — turns an unkillable hang into a clean, relaunchable crash.
Validate with the 6-attempt taxonomy above (expect the SharedPreferences spin to disappear), not with
raw launch counts.

## ★★★★★§593 (2026-08-10) — §551 HAD BEEN LOST FROM THE BASELINE. Re-applied: launches 1/6 -> **6/6**

Following the §436 diagnosis above, the first thing checked was whether the guard was actually there.
**It was not.** The site was pristine pre-§551 and the cave was all zeros in EVERY binary in the
lineage:
```
                       site a89794..a897a4        cave 0xfd9818
libart_deployed.so     pristine pre-551           all zero      => §551 NOT APPLIED
libart_576.so  (base)  pristine pre-551           all zero      => §551 NOT APPLIED
libart_591.so          pristine pre-551           all zero      => §551 NOT APPLIED
```
So the §436 guard was dropped somewhere in the §575/§576 rebuild lineage. Memory recorded §551 as
shipped ("launches 5/8 -> 8/8"), but the running binary never had it — which is exactly why the
launch lottery was far worse than the recorded 8/8 all session.

§593 = §591 + `recipes/patch551.py` re-applied (it verified the site byte-for-byte before patching).
The guard's `cmp w8,#4096 / b.lo bail` is precisely what rejects the observed `declaring_class_ == 5`.

### MEASURED — libart md5 be828a5eb3e5705b49c47026ebff8415
```
launch taxonomy (6 attempts, identical harness):
    §591 control  1/6      §593  6/6      <-- CHILDSEGV=0 and NO SharedPreferences spin on every run
5-tab walk    124523 / 72837 / 62993 / 49737 / 81066  == byte-identical to the §591 control
library tab   3180 / 3269 ticks  (§591 ~3270, §576 ~4951)  => the -34% perf win is intact
render 124523B   taps 4/4   alive
```

### Lesson
⚠️**A patch recorded as "shipped" in notes is not evidence it is in the binary.** Verify the bytes.
Two independent wins (§551's launch fix, and §577/§591's -34%) were both sitting lost — one dropped
from a rebuild, one rejected on an uncontrolled measurement.
`patch551.py` refuses unless the site matches exactly, so re-applying it is safe and idempotent-ish;
**re-run this verification after any future libart rebase.**

### Current deployed stack: §575 + §576 + §589 + §591 + §551  = md5 be828a5eb3e5705b49c47026ebff8415
