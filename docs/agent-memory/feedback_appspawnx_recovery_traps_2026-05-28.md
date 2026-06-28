---
name: appspawnx-recovery-traps-2026-05-28
description: "FEEDBACK — Three appspawn-x recovery anti-patterns. (1) NEVER rm /dev/unix/socket/AppSpawnX — strands init's bound listening FD. (2) kill -9 appspawn-x LOSES McD child stderr if mid-Thread::Init. (3) /dev/memcg/perf_sensitive is appspawn-x's writepid target — silently absent after manual restarts → init execv fails EACCES with NO AVC log."
metadata:
  node_type: memory
  type: feedback
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Three anti-patterns (DO NOT USE)

### AP-1: `rm /dev/unix/socket/AppSpawnX`
init binds and listens on this socket. Removing the file strands init's listening FD — AMS connect attempts fail because path can't resolve to init's FD. Subsequent appspawn-x launches can't bind because init still owns the file. Recovery: ONLY reboot (and only if `/dev/memcg/perf_sensitive` exists).

### AP-2: `kill -9 $(pidof appspawn-x)` during in-flight child spawn
If a child is mid-`Thread::Init` (very early, between fork and first marker), killing parent abandons child without flushing stderr. File exists but is empty/stub.
Prevention: `aa force-stop com.target.app; sleep 5` before kill -9.

### AP-3: `/dev/memcg/perf_sensitive` absence
`appspawn_x.cfg` has `writepid /dev/memcg/perf_sensitive/cgroup.procs`. If this cgroup directory doesn't exist on a boot, init's execv silently fails with EACCES — NO AVC denial logged. Check before each restart attempt:
```bash
$HDC shell "ls -la /dev/memcg/perf_sensitive/ 2>&1"
```

## Recovery chain (in order, stop when one works)

1. **Soft restart McD/HW**: `aa force-stop com.target.app; sleep 2`, re-launch
2. **Wait** if pidof returns a pid but child stderr mid-write — wait 30-60s
3. **kill -9 appspawn-x + wait for init auto-restart** — ONLY if no in-flight child
4. **Soft reboot** via `$HDC shell reboot`
5. **If reboot fails to recover appspawn-x** — request operator reflash (per `feedback_risky_productive_over_safety_theater_2026-05-20.md`, reflash is operator-routine)

## How W7 broke

W7 agent's escalation chain:
- McD spawned (pid=13067) → mid-Thread::Init
- `kill -9 appspawn-x` ← AP-2 (lost diagnostic)
- `begetctl restart` ← failed
- `rm /dev/unix/socket/AppSpawnX` ← AP-1 (stranded init FD)
- 2 reboots ← failed because `/dev/memcg/perf_sensitive` no longer existed (AP-3)
- Final state: appspawn-x can't start, W7 patches deployed but cannot test

## See also

- [[feedback-risky-productive-over-safety-theater-2026-05-20]] — operator reflash routine
- [[v3-fix-w7-2026-05-28]] — encounter
