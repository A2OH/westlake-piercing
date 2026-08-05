# The westlake ABI-shim pattern

*Distilled from the audio bring-up (§504-§517), which took one app from "no sound, cause unknown" to
continuous playback. Every fix was a bridge shim or a two-method framework patch; no app logic was
rewritten and libart was never rebuilt.*

The standing principle is **adapt at ABI boundaries**. This document is about the part that
principle does not tell you: *how a shim can be wrong even when it looks right*, and the order in
which to find out.

---

## 1. The four defect classes at a boundary

Class 1 and 2 are already understood on this project. Classes 3 and 4 are what cost this session
weeks, and they are the ones to internalise.

### Class 1 — Missing binding
The native is simply not registered.

```
UnsatisfiedLinkError: No implementation found for
  void libcore.io.Memory.pokeByteArray(long, byte[], int, int)
```

★**An unbound native is not an inert no-op. It is a thread-killing `Error`.**
`UnsatisfiedLinkError` extends `Error`, and frameworks catch `RuntimeException`, not `Error`.
ExoPlayer's `handleMessage` catches `ExoPlaybackException` / `IOException` / `RuntimeException`, so
the throw escapes `Looper.loop()` and the thread dies. Because `ThreadGroup.uncaughtException` is a
no-op in this runtime, **nothing is reported** — the thread just vanishes from `/proc/<pid>/task`.

⇒ On this runtime, **a thread disappearing silently means an `Error`, not a clean quit.**
⇒ Detection is free: `grep -a JNIMISS <child stderr>` names every native the runtime tried to call
and could not find. It had the answer for weeks before anyone read it.

Instances: §504 `libcore.io.Memory.pokeByteArray`, §505 `AudioTrack.native_setPlayerIId`,
§506 `native_enableDeviceCallback`, §506c `native_get_timestamp`.

### Class 2 — ABI mismatch
bionic↔musl, symbol versioning, calling convention. See `bionic-musl-class2-abi-detection`.
DEX is blind to it; analyse the ELF `.dynsym`.

### Class 3 — Bound, called, returns… and still wrong ★the expensive one
The signature matches and the call succeeds. The **value or object violates the platform contract**.
No symbol check, no JNIMISS, no exception at the boundary — the failure surfaces far away, in the
caller's logic, usually as an assertion or a stall.

Four sub-kinds, all hit in this session:

| sub-kind | what was wrong | how it presented |
|---|---|---|
| **Representation** | `NewDirectByteBuffer` yields **BIG_ENDIAN**; AOSP's MediaCodec calls `.order(nativeOrder())`, we did not (§515) | `Assertions.checkArgument(order == LITTLE_ENDIAN)` → **message-less** `IllegalArgumentException` |
| **Identity** | returned a *fresh* ByteBuffer per call; real `MediaCodec` caches one per index (§510) | `checkArgument(outputBuffer == null \|\| buffer == outputBuffer)` — an **identity** test |
| **Accounting** | `getPlaybackHeadPosition` returned the renderer's total consumption, which counts underrun **silence** (§514) | position reported 8916 frames while the app had written **0 bytes** |
| **Signalling** | `read()` returned `-1` for timeout and error as well as EOF (§517) | Java maps `-1` to end-of-stream ⇒ silent truncation |

★**The tell for Class 3 is a message-less exception or a stall with everything alive.** A platform
error carries text; `Assertions.checkArgument(boolean)` does not. If you see a bare
`IllegalArgumentException`, stop inferring and go read the caller's bytecode.

★**Ask "what does this value MEAN", not "does this compile".** Every Class-3 defect was a correct
signature carrying a wrong meaning.

### Class 4 — The peer does not exist
The native is fine; the *service* it talks to is absent. `AudioTrack.<init>` registers the player
with AudioService (§505/§508); `MediaRouter` reads a `WifiDisplayStatus` from the display service
(§513). Neither exists here.

★**Supply the honest "nothing there" answer. Do not disable the caller.**
A default-constructed `WifiDisplayStatus` is `FEATURE_STATE_UNAVAILABLE / NOT_SCANNING /
NOT_CONNECTED` — literally true on this board — so `MediaRouter` takes its real no-wifi-display path.
That is `tools/PatchReturnNew.java`. `PatchNoOp` is the wrong tool for an object-returning method
because **its type-default IS null — the bug you are trying to fix.**

⚠️Contrast with §478, which was reverted as over-reach: that blanked methods whose throws were
*inert*, purely to stop them saturating a diagnostic probe. Editing the platform to fix your own
instrumentation is not the same as removing a call whose remote peer does not exist.

---

## 2. The procedure

1. **Let the runtime name the gap.** `grep -a JNIMISS` first, always. Intuition is slower and wrong
   more often.
2. **Bind one at a time.** A static dex scan (`tools/FindClassRefs.java`) lists every method of a
   class the app can reach — use it to *anticipate*. Use JNIMISS to *decide*.
   ★**Reachable ≠ needed.** Binding 14 AudioTrack natives at once because they were reachable hung
   startup for 5 minutes against a 44 s boot; only one had ever been named.
3. **Bound but still broken ⇒ assume Class 3.** Disassemble the caller. `tools/Disasm.java` on the
   minified method found the byte-order assertion at instruction `[153]` in about ten minutes, after
   days of inference had failed.
4. **Prefer the honest answer to the plausible one.** `get_timestamp` → "unavailable" makes ExoPlayer
   fall back to `getPlaybackHeadPosition()`, which is real. `is_direct_output_supported` → false keeps
   it on the decode path that works. Fabricated values trade a loud crash for silent drift.
5. **Instrument the boundary before concluding anything** — see §3.
6. **Objects: supply. Features: never disable.**

---

## 3. Verification discipline — every rule here was bought with a wrong conclusion

★**Prove the process is alive before reporting a hang.** A dead process fakes *every* stall signature
at once: frozen SQL counts, a suspend that never resumes, unbalanced `BEGIN`/`COMMIT`. Hours went
into "Room is deadlocked" that was measured ~85 min after the app had died.
`ps -A | grep -c '[a]shu'`, or the `alive=` field in `run_*.log`.

★**Absence in a log proves nothing until you have confirmed the log would show presence.** This one
recurred *four times*:
- a hang measured against a dead process
- `writeCalls=0` from a path with **no logging at all**
- `writeCalls=0` again from logging **only one of two** write variants
- `cbOutput` "frozen at 6" that was a **hard-capped log** (`if (n++ < 6)`), not a stall — and §510 was
  built on that number

⇒ **Never cap a diagnostic counter.** Log the first few, then every Nth, and always carry a running
total. A count that cannot exceed its cap reads exactly like a stall.

★**Control your own instrumentation.** `hilog -b DEBUG` is a *global* base level: it enables debug for
every OpenHarmony subsystem, floods the buffer, and stopped the child booting — which then
"confirmed" a bridge regression that did not exist. If you need app-level logs, map them at the
bridge (§516: Android `VERBOSE`/`DEBUG` → HiLog `INFO`), so you add one app's lines instead of the
whole platform's.

★**Sample 3+ runs.** This board's variance broke several single-run conclusions, including the
attribution of the final fix.

★**Distrust your own greps.** `grep -c queueInput` matched both a shim marker and an app trace, which
turned a count of 2 into an apparent 99.

---

## 4. Diagnostic channels on this port

| channel | carries | read with |
|---|---|---|
| `adapter_child_<pid>.stderr` | westlake markers, libart probes, **JNIMISS** | `grep -a` |
| hilog domain `0xD000F00` → `C00f00/<TAG>` | **everything the app logs via `android.util.Log`** | `scripts/host/wl_applog.sh` |
| stock Android phone | the golden trace for any behaviour | `adb logcat -v threadtime` |

★These are **different sinks**. Reading only stderr made the app look silent for months while it was
describing exactly where it stopped. `android_util_Log.cpp` routes `println_native` to HiLog.

★**Keep a reference device.** The same APK on stock Android answered "what should happen" instantly
and repeatedly — the golden play trace, and the segment transition firing at exactly +32.2 s where
the board froze. `reference-traces/`.

★**Get the app's source when it is available.** It decoded every minified name (`j(ByteBuffer;JI)Z`
= `handleBuffer`, `Lf4/a;->b(Z)V` = `Assertions.checkArgument`), which is what made the byte-order
assertion findable at all.

---

## 5. Applying this to the next subsystem

For any Android API not yet working here:

1. Exercise it and `grep -a JNIMISS` → Class 1 list. Bind, one at a time, honest values only.
2. Re-exercise. Still broken with everything bound ⇒ Class 3 or 4.
3. For Class 4, identify the absent peer and return its honest "unavailable" state.
4. For Class 3, disassemble the caller at the failure point and ask what the value *means*:
   byte order, object identity, units//epoch of a counter, and error-vs-terminal signalling are the
   four that have bitten so far.
5. Verify against the reference device before believing any of it.
