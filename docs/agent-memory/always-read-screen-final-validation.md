---
name: always-read-screen-final-validation
description: Always capture + visually read the device screen as the final validation; never declare success on synthetic/log proof alone
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

ALWAYS read the actual device screen (snapshot_display → recv → view the image) as the FINAL validation of any noice/UI/connectivity claim. Never declare success based on synthetic tests, log markers, or HTTP-code probes alone.

**Why:** I claimed "connectivity SOLVED" because the NetTest harness (a synthetic `HttpsURLConnection`) returned `HTTP 200` and noice's okhttp3 `sslConnect` succeeded in tls.log — but the actual noice screen still showed **"无法获取订阅计划。网络无法访问。"** (couldn't fetch plans — network unreachable). The synthetic proof did not reflect what the app/user actually sees. The user corrected: "no read the screen, still failed to access network" + "remember always read screen as the final validation."

**How to apply:** For every "it works / fixed / renders / loads" claim about noice (UI, nav, data loading, connectivity), end with a `snapshot_display -f ... ` → `hdc file recv` (relative path) → Read the .jpeg and describe what's actually on screen. Watch the focus/compositing race (launcher ~77k vs noice content vs the 24973 error page) — re-foreground + re-capture if it caught the launcher. Treat logs/NetTest/HTTP codes as intermediate evidence only; the screen is ground truth. Cross-ref [[noice-dpad-consumer-keystub]].
