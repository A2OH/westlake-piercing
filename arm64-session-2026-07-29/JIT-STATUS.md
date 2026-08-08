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
