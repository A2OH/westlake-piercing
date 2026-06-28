---
name: Full OHOS Integration Status
description: Dalvik VM fully integrated with OHOS init, ueventd, Skia, fb0, input — auto-starts on boot
type: project
---

## Full OHOS Integration (completed 2026-03-21)

All 5 integration items completed:

### 1. Dalvik as OHOS init service
- `dalvikvm.cfg` created by `prepare_images.sh` step 7
- Auto-starts on boot via init service framework
- Runs `/system/bin/run_dalvikvm.sh` wrapper script
- Verified: dalvikvm PID 88 auto-started alongside samgr, hilogd, softbus

### 2. Skia/OH_Drawing deployed
- `lib2d_graphics.z.so` + `libskia_canvaskit.z.so` deployed to `system/lib/platformsdk/`
- `prepare_images.sh` step 6 copies from build output
- Has real OH_Drawing symbols: Canvas, Bitmap, Font, TextBlob, Path, etc.

### 3. ueventd auto-creates /dev/fb0
- Rule added to both source (`base/startup/init/ueventd/etc/ueventd.config`) and built config
- `/dev/fb0 0666 0 1003` — no manual mknod needed
- Also: `dalvikvm.cfg` post-fs-data job runs `mknod` as fallback

### 4. Canvas → fb0 direct blit
- CanvasViewDumper.java writes rendered pixels directly to `/dev/fb0`
- No external `dd` or blit tool needed
- Supports mouse wheel scrolling via `/dev/input/event*` (60px/tick)

### 5. C-side open() fix
- Root cause: `/data/a2oh/` directory didn't exist when C code ran
- Fix: added `mkdir("/data/a2oh", 0777)` before `open()` + errno logging
- Both flush_framebuffer() and canvasDestroy() patched

### Boot sequence (no manual commands)
kernel → init → ueventd (creates /dev/fb0) → samgr/hilogd/softbus/hdcd →
dalvikvm service → ShowcaseActivity → Canvas render → fb0 blit → VNC display → scroll loop

### Key files
- `openharmony-wsl/scripts/prepare_images.sh` — 11-step image prep with all integrations
- `openharmony-wsl/scripts/CanvasViewDumper.java` — fb0 blit + scroll
- `openharmony-wsl/scripts/qemu_boot_full_vnc.sh` — boot script

### Remaining gaps (not critical)
- render_service compositor (currently raw fb0 write) — OHOS uses producer/consumer BufferQueue
- multimodal_input service for touch routing (currently raw /dev/input read)
- Foundation service keeps crashing (not critical, separate issue)

**Why:** These are plumbing integrations — the core pipeline works end-to-end.
**How to apply:** Use `prepare_images.sh` after any OHOS rebuild. All integration is automated.
