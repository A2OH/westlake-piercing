# Catalog cold-boot "bad-boot" = OHOS font-config cJSON hang, NOT AESKeyGenProbe (2026-06-25)

**The long-standing "~50% AESKeyGenProbe bad-boot" belief is WRONG. Root-caused + FIXED.**

## What the bad-boot actually is
On a cold boot, launching the Material Catalog intermittently FREEZES on the FIRST launch
(`AAFWK LIFECYCLE_TIMEOUT` / `LIFECYCLE_HALF_TIMEOUT` → AMS watchdog kills the app at ~10s).
Rigorous freeze-aware measurement: **2/8 = 25%** bad (NOT 50%).

**TRUE root cause** (caught with `dumpcatcher -p <catalogpid>` on the LIVE hung process —
the watchdog sysfreeze dump is unreliable, it often captures the `foundation`/system_server
stack instead of the catalog's own — see feedback_sysfreeze_vs_stderr_diagnostic):
```
#00 cJSON_GetArrayItem+22                         /system/lib/chipset-sdk-sp/libcjson.z.so
#01-02 librosen_text.z.so
#03 OHOS::Rosen::Symbol::DefaultSymbolConfig::ParseConfigOfHmSymbol(const char*)
#04 skia::text::HmSymbolConfig_OHOS::LoadSymbolConfig(const char*, SkString)   libskia_canvaskit.z.so
#05 FontConfig_OHOS::loadHMSymbol(SymbolLoadMode)
#06 FontConfig_OHOS::FontConfig_OHOS(...)
#07 SkFontMgr_OHOS::SkFontMgr_OHOS(const char*)
#08 SkFontMgr::RefDefault()
#09 libhwui android::fonts::createMinikinFontSkia(...)
#10 libhwui android::Font_Builder_build(...)         ← catalog's first android.graphics.fonts.Font.Builder.build()
#11 boot-framework.oat
```
The OHOS Rosen/Skia font manager (`SkFontMgr::RefDefault()`, a **per-process singleton**, re-run
in every appspawn-x fork) parses the HarmonyOS symbol-font config the first time the app renders
text. The deployed `libskia_canvaskit.z.so` reads **`/system/fonts/hm_symbol_config_next.json`
(6.3 MB)** (confirmed via `strings`; the small `hm_symbol_config.json` 15 KB is the OLD path, not
used by the deployed binary). The cJSON parser indexes array elements with `cJSON_GetArrayItem`
(O(index) linked-list walk) inside nested per-symbol loops → effectively **O(n²) over thousands of
symbols**. Under cold-boot 4-core contention this borderline-slow parse intermittently exceeds the
~10s lifecycle watchdog → 25% freeze. (Slow-but-finite, not a true deadlock.)

## Why AESKeyGenProbe was a RED HERRING
`adapter.diag.AESKeyGenProbe.run()` (invoke still LIVE at AppSpawnXInit.smali line 828 in arb
`c026e80c`; NOT nop'd) **completes in ~15 ms** (`[AESPROBE] provider count=6 ... [AESPROBE] done`,
all within one millisecond-range in the hilog) and is followed by ~8 s of further SUCCESSFUL init
before the lifecycle hang. It was only ever the *last named init marker before the freeze*, so prior
sessions mis-blamed it. Nop'ing it does NOTHING for the bad-boot. **Do not waste a boot-regen on it.**
(NOTE on the proxy-LinkMethods hang: that was a REAL but SEPARATE bug, already fixed earlier the same
day via libart W22-PROXY-SKIP in `2813065e` — see the 2026-06-25 START-HERE bug #2. The old
sf-6420 06:12 proxy stack predates/aliases that fix. The deployed `2813065e` already carries
`|| klass->IsProxyClass()` at class_linker.cc ~9306, so proxy linking no longer hangs. The
REMAINING 25% bad-boot is the font-config cJSON hang documented here — a distinct, later cause.)

## THE FIX (deployed, validated) — shrink the symbol config
Replace the 6.3 MB `/system/fonts/hm_symbol_config_next.json` with a **structurally-valid minimal**
config so the parse is instant:
```json
{ "name":"HM Symbol Layers Grouping", "version":"2.0",
  "common_animations":[], "special_animations":[], "symbol_layers_grouping":[] }
```
(The parser loops `for(i<root.size())` over each top-level array → empty arrays = 0 iterations, no
crash. Verified the parser tolerates empty sections.) Deploy: remount,rw / → `cp` (preserves SELinux
ctx `u:object_r:system_fonts_file:s0`) → chmod 644 → remount,ro. **Only decorative HM-symbol glyphs
are lost — normal text rendering is unaffected** (launcher + lockscreen render perfectly post-fix).
- Original 6.3 MB backed up: `/data/local/tmp/pre-symbolfix/hm_symbol_config_next.json` (md5 6ed9f4d6).
- Minimal config md5: 425290bd (144 bytes). Fully reversible: restore backup + reboot.

## RESULT
Bad-boot rate: **25% (2/8) BEFORE → 0% (16/16) AFTER**. Cold-boot → swipe-unlock → tap catalog icon
→ catalog renders + navigable, every time. Evidence:
`docs/engine/V3-CATALOG-DEMO-READY/` (01 launcher post-fix, 02 coldboot-unlock-icon,
03 tap-icon-catalog-renders, badboot-live-hang-stack-cJSON-fontconfig.txt).

## KEY LESSONS
- **For an AAFWK LIFECYCLE_TIMEOUT, get the stack with `dumpcatcher -p <pid>` on the LIVE process**
  (poll every ~1-2s right after launch, before the ~10s watchdog kill). The sysfreeze faultlog often
  dumps the wrong process. `processdump` is disabled → use `dumpcatcher` (`-p pid` = all threads).
- Catalog process: name truncates to `io.material.cat`; `pidof` FAILS (empty cmdline) → detect via
  `ps -A | grep material`. A "drew=1 in newest stderr" check can FALSE-PASS a bad-boot (watchdog
  kills the frozen first launch, AMS/`aa start` respawns a 2nd that draws) — verify the FIRST pid is
  stable + sysfreeze count == 0.
- This is a generic OHOS-app cold-boot risk (any fresh-fork app's first text render hits
  `SkFontMgr::RefDefault()` → this parse), not catalog-specific.
