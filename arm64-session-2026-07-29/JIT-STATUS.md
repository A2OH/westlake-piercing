# JIT status (2026-08-07) — read before touching the JIT again

## Corrected claim
An earlier commit said "§571: THE JIT NOW SURVIVES". **That was from a single run and is wrong.**

## Measured behaviour
| condition | result |
|---|---|
| JIT on, app IDLE | survives 4/4, but **compiles 0 methods** |
| JIT on, app UNDER LOAD (driving tab switches) | **dies with StackOverflowError** |
| §571 libart, NO JIT, same load | 6/6 taps, SOE=0, alive, matches baseline |

With the §568 layout storm fixed, an idle app never gets hot enough to compile — so "it survived"
runs are usually runs where the JIT did nothing. **Always report `Compiling method` count alongside
survival**; survival with compiled=0 proves nothing.

## What is settled
* The JIT compiles correctly and needs no libart rebuild (compiler is statically linked in).
* §568 relayout-storm fix: idle CPU 88% -> ~3.2%. Solid, independent win.
* §571 un-swallowing uncaught exceptions is **necessary and safe** (stable without JIT) but
  **not sufficient**.
* §570 gives a working way to mark methods non-compilable (kAccCompileDontBother = 0x02000000,
  ArtMethod::access_flags_ at offset 4, jmethodID == ArtMethod*).
* The JIT env does NOT break the input channel (2/3 launches fine) — that was the launch lottery.

## Valid baseline (no JIT, 6/6 taps delivered, adaptive settle)
    presets tab  ~120 ticks   (~1.2 CPU-s)
    library tab  ~5550 ticks  (~55 CPU-s)   <- 31 SVG cards
"A tab switch" is NOT one number; always say which tab.

## ★★NEW (2026-08-07, later): the crash is NOT caused by compiled code

Under load with the JIT enabled the child died on the FIRST tap with
**`Compiling method` count = 0** — the JIT had compiled *nothing*. So executing compiled code is
NOT the trigger; merely **enabling the JIT** is. That kills the whole "compiled code bypasses the
interpreter's §436 repair" theory as the direct cause.

What enabling actually changes for INTERPRETED execution: with the JIT live, ART's interpreter
calls the JIT bookkeeping path (`Jit::MethodEntered` -> `AddSamples` -> hotness / ProfilingInfo
allocation) on method entry. That is the code newly in play, and it runs alongside westlake's PFCUT
machinery in `DoCall`. **Investigate that interaction next**, not the compiler.

⚠️TOOLING BUG FOUND: `LOGE("... %{public}s", ...)` **drops its string arguments** in this build —
the §569 stack dumper prints bare `at ` lines and `[JIT-570]` logs an empty message. The frame
count is real (`stack depth = 5 frames`) but the names never appear. **Switch those to
`fprintf(stderr, ...)`** (which the rest of the bridge uses successfully) before trusting any
string logged through LOGE.

## Next
The failure is under load and involves the coroutine path. Options: package-prefix exclusion via
§570 over the loaded-class list, or find why compiled coroutine dispatch recurses (the §436
interface-dispatch repair lives in the interpreter's DoCall and compiled code bypasses it).
Use scripts/host/bench.sh — it refuses to produce numbers from a launch with no input channel.


## 2026-08-07 (session 4) — fully characterized, still open

Reproduced deterministically: **chan=1 launch + JIT live + ONE tab tap => unbounded recursion,
child dies.** Ruled out, with evidence:
* NOT compiled code — `compiled=0` at death (JIT compiled nothing; the tap path is interpreted).
* NOT nterp — `CanRuntimeUseNterp` is `mov w0,wzr; ret` (hard-off), nterp never runs.
* NOT the §436 interface-repair loop — no PFCUT-IFACE/PROXY logging at death.
* NOT per-frame overhead / headroom — `stack size 63MB` on a 64MB stack = the WHOLE stack is
  consumed => genuinely unbounded. Bigger stack only delays (8MB, 64MB, 256MB all die).
* NOT startup sync — JIT live at 20s with no tap SURVIVES; only the TAP path triggers it.
* NOT the JIT env breaking launch — 2/3 JIT-env launches got a channel (it's the normal lottery).

The recursion is **silent interpreted method invocation** (no per-invoke logging). The last
intrinsic logged before every death is
    [PFCUT] AtomicInteger.compareAndSet intrinsic current=-536870911   (0xE0000001)
i.e. a j.u.c/Kotlin CAS retry that appears never to converge with the JIT on — a concrete lead.

### Why it's hard to see
`getStackTrace()` returns 0 usable frames because the 63MB overflow leaves no room to walk the
stack; the §569/§572 dumper then prints only the 5-frame secondary trip (Arrays.copyOf /
StringBuilder.append during initChild's Looper). The frame COUNT is real; the deep frames never
survive.

### NEXT (the only reliable way to name it)
Binary-patch ART's `ThrowStackOverflowError` (or the switch-interpreter's stack-overflow check) to
emit a NATIVE backtrace at throw time, BEFORE unwinding — precedent: the §525/§534 style hooks. Or
enlarge `kStackOverflowReservedBytes` so `getStackTrace()` can walk. Either names the recursing
Java method; then §570-exclude it or fix the CAS intrinsic under JIT.
