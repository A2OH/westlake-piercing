---
name: measurement-channels
description: CRITICAL — hilog grep counts are echo-contaminated phantoms; the real app channel is per-child stderr. Re-validate any hilog-based signal.
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 422676c7-2c64-4e1a-8602-735c79264cef
---

The session's key noice signals (`createSurface=1`, `W20-badalloc=1`, `W20-FIX-fired=1`) were **measurement phantoms**, not real.

**The contamination:** `deploy_w20.sh` measured via `hdc shell "hilog -x | grep -ac 'LINKMETHODS-BADALLOC'"`. But OHOS `HDC_LOG` echoes the dispatched command string (which literally contains `LINKMETHODS-BADALLOC` / `createSurface`) **into the same hilog being grepped**. So `grep -c` matches its own command echo → always returns ≥1. Both signals were guaranteed false positives.

**Why it matters (and the limit of this correction):** the *count* was a phantom, but **W20 itself is REAL** — confirmed in the per-child stderr of a DEEP run: `[W12F-LINKMETHODS-BADALLOC] CULPRIT: m8.e`. Don't over-correct: the contaminated COUNT ≠ "no bad_alloc". noice's runs are flaky — early-death runs don't reach W20 (which misled me once), DEEP runs (full bind + MainActivity FOREGROUND) DO hit it. The W20 guard builds (3c8335c0/fefc3c01/53932c26) targeted the wrong containers (arena-backed), so `W20-FIX` fired 0×; the real runaway is an `ss_lists` cycle (std::vector, operator new). See [[noice-real-blocker-initchild]].

**How to apply — use the RIGHT channels:**
- App (appspawn-x child) ART/init/link output → **`/data/service/el1/public/appspawnx/adapter_child_<pid>.stderr`** (per-child stderr; `start_asx.sh` rm's these on boot). Newest file = last-launched app. This is the gold channel for createSurface/bad_alloc/onCreate/link errors.
- Parent Zygote stderr → `/data/local/tmp/asx_run.err` (NOT the app — its last child is whatever forked last, e.g. helloworld from the HW gate).
- Native mmap runaway (W20) → the **allocprobe** (`/data/local/tmp/liballocprobe.so`, real mmap hook) — never echo-contaminated; the gold standard for the W20 runaway.
- If you must grep hilog, **capture to a file first** (`hilog -x > f.txt`) then grep the file locally, OR exclude echoes with `grep -av 'HDC_LOG\|ExecuteCommand'`. Never `hdc shell "hilog | grep -c <literal you're searching>"`.
- `[AESPROBE]`/`System.err.println` from the child are buffered and **lost on abrupt death**; CloseGuard `[alog:System]` warnings survive (different path). Don't infer "didn't run" from missing buffered stderr.
- **REFINED (3rd burn, 2026-06-01): the per-child stderr FILE is itself BUFFERED for Java `System.err`** — it can LOSE the uncaught exception / `J_invokeStaticMain_main_threw` (e.g. a grep on the file said `AppCompat-err=0` when the app HAD thrown it). For app **Java exceptions / death cause**, use **HILOG** via the `C00f00/AppSpawnXJava [stderr]` mirror (+ `C00f00/AppSpawnXInit`) — that path is flushed. **Native `fprintf(stderr)`** (FIX-VTABLE-A, `[W20-FIX]`, `[W12F-LINKMETHODS-BADALLOC]`, `[G2.5-SLA-PRE]`) IS flushed → reliable in the child-stderr file (so W20's verification stands; the theme "pass" did not). Rule: native-fprintf markers → child-stderr file OK; Java exceptions → hilog only.
- Watch appspawn-x count: `pidof appspawn-x` returning 2+ pids = the parent + live forked children (or a stale 2nd instance from kill/restart churn) → can cause flaky launch failures. Deploy boot-image changes via REBOOT, not kill/restart.

**Why this happened:** I trusted a grep-count signal without verifying the channel. The user was right to demand rigor. Always confirm WHERE a signal is logged before trusting a count.
