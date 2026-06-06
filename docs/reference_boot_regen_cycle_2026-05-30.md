---
name: reference-boot-regen-cycle-2026-05-30
description: Validated local boot-regen + BCP-jar smali-patch cycle — the repeatable tool for fixing framework/adapter-method walls
metadata: 
  node_type: memory
  type: reference
  originSessionId: 422676c7-2c64-4e1a-8602-735c79264cef
---

The repeatable, fully-local cycle for fixing any **BCP-jar-level wall** (a framework/adapter Java method that's missing or a null-returning stub). Validated end-to-end 2026-05-30 (getActiveNetwork + getInstallSourceInfo): jar edit → regen → deploy → HW-gate-pass → app advances. ~5-8 min/cycle. Scripts at `/tmp/nflx-boot/{regen,deploy_nflx,regen2,deploy2}.sh`; evidence `docs/engine/V3-NFLX-BOOT-EVIDENCE/`.

**dex2oat env (runs locally, NO HBC/Alex per [[feedback_no_builds_on_hbc_or_alex_2026-05-25]]):** `$HOME/tools/dex2oat64` + `$HOME/tools/lib64/libsigchain.so`. Invoke with `LD_LIBRARY_PATH=$HOME/tools/lib64 LD_PRELOAD=<sigchain> dex2oat64 --android-root=/system --instruction-set=arm <--dex-file/--dex-location per jar> --oat-file=boot.oat --image=boot.art --base=0x70000000 --runtime-arg -Xms64m --runtime-arg -Xmx512m --compiler-filter=speed`. Produces 30 segments (boot.{art,oat,vdex} + 9× boot-<jar>.{art,oat,vdex}) in ~14s. `[DBG]`/`Skipping class …previously found in` warnings are NORMAL (instrumented libart + BCP first-jar-wins dedup).

**Consistency rule:** regen from the **device's OWN exact jars** (pull all 10 via `hdc file recv`), swap only the jar you patched. Guarantees the boot image matches `/system`'s jars (avoids `InitWithoutImage: Class mismatch` per [[reference_local_build_infra_2026-05-25]]).

**BCP order — VERIFY before every regen** (B-6 SIGABRT-storm trap): `hdc shell strings /system/bin/appspawn-x | grep 'framework/.*\.jar'` gives the authoritative `kBootClasspath`. 2026-05-30 order: core-oj core-libart core-icu4j okhttp bouncycastle apache-xml adapter-mainline-stubs framework adapter-runtime-bcp oh-adapter-framework. See [[feedback_bcp_first_jar_wins_2026-05-25]].

**Surgical BCP-jar patch via smali (NO full javac/d8 build needed for 1-method changes):** jars are classes.dex-packed. `java -jar /tmp/v3-fix-d/baksmali.jar d <jar> -o out` → edit the .smali → `java -jar /tmp/v3-fix-d/smali.jar a out -o classes.dex` → `zip <jarcopy> classes.dex` (preserves manifest). Always re-baksmali the new jar to verify the edit landed. (To which jar a class belongs: `unzip -p $j.jar classes.dex | grep -ac <Class>` — NOT the zip listing, which only shows `classes.dex`.)

**Deploy (whole boot image = ONE ATOMIC UNIT, all 30 + the jar):** stage to `/data` with **send + size-verify + retry** — `hdc.exe` over WSL silently drops large files (boot-framework.* is 51/37/23MB); verify `ls -l` remote size == local, retry ≤5×, abort BEFORE touching `/system` if <30/30. Then stop appspawn-x (`aa force-stop <app>; kill -9 $(pidof appspawn-x)`), `cp` staging→`/system/android/framework[/arm]`, `chcon u:object_r:system_file:s0` + `chmod 0644`, `rm -rf /data/misc/appspawnx/dalvik-cache/*`, `sync`, restart via `/data/local/tmp/start_asx.sh`, then re-`chmod 0666`+`chcon appspawn_socket` the AppSpawnX socket + `setenforce 0`. Recovery traps: [[feedback_appspawnx_recovery_traps_2026-05-28]].

**ALWAYS:** snapshot current boot+jar to `/data/local/tmp/<rollback>` first; HW-gate (HelloWorld `onResume`, zero `mark_sweep|Fatal|cppcrash|Class mismatch|ValidateOatFile|InitWithoutImage`) → auto-rollback on fail. Snapshot the *current working* state each cycle so a failure rolls back to the latest win, not the original.
