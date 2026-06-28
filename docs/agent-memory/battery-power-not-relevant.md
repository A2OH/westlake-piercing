---
name: battery-power-not-relevant
description: "The DAYU200 board is wire-powered (no battery) — the \"11%\" reading is bogus; NEVER blame battery/power/lockscreen for flaky or blank launches"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 5f7b1af5-9cec-4260-888a-283e2b779990
---

The DAYU200 board is **powered by wire, no battery** — the "11%" battery reading is bogus. **Do NOT attribute any issue (white screen, flaky launch, blank/foreground-render failure, the swipe-up lockscreen) to battery / low power / "battery-driven aggressive lockscreen".** The user has corrected this twice ("power is not an issue, board is powered by wire no battery").

**Why:** prior notes (e.g. catalog demo memory) blamed "low battery 11% → aggressive lockscreen blocks foreground render" — that is WRONG and sent me chasing a non-cause. The lockscreen that appears is just the ordinary screen-lock (swipe-up dismisses it), unrelated to power.

**How to apply:** when a launch is blank/flaky/white, diagnose the REAL layers — appspawn-x running + Phase 4, AMS→appspawn-x spawn routing, the cold-launch/compositing path — never power/battery. Treat "11%" as noise. Related: [[catalog-newboard-WORKING-2026-06-27]], [[adapter-app-launch-bringup]].
