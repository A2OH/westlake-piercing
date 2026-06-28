---
name: catalog-zombie-missions-findings
description: "Why OHOS recents shows multiple zombie io.material.catalog entries with names but no thumbnails: mission_data_storage.cpp is STUBBED (in-memory only) + multiton launch mode + the WMS-focus compositing wall blocks snapshot capture"
metadata: 
  node_type: memory
  type: project
  originSessionId: 5f7b1af5-9cec-4260-888a-283e2b779990
---

Investigated 2026-06-25 (user: "why zombie catalog instances after power-on, names but no thumbnail?"). Source-grounded answer (OHOS tree + deployed binary).

**ROOT CAUSE — mission persistence is STUBBED on this build.** `foundation/ability/ability_runtime/services/abilitymgr/src/mission_data_storage.cpp` is locally modified from upstream's 565 lines down to a **16-line no-op**: `SaveMissionInfo(){}`, `SaveMissionSnapshot(){}`, `GetMissionSnapshot()→false`, `LoadAllMissionInfo()→true` (loads nothing). Verified in the deployed `out/rk3568/.../system/lib/platformsdk/libabilityms.z.so` (built 2026-04-04, AFTER the 2026-03-16 stub) — ZERO of the upstream persistence log strings present. So mission records + snapshots are NEVER written to disk; they live ONLY in AMS in-memory `missionInfoList_`. `TaskDataPersistenceMgr` (unmodified) just delegates to the stub; no RDB/sqlite path.

**Why MANY zombies:** the catalog ability is STANDARD/multiton (standard|multiton → LaunchMode::STANDARD). `MissionListManager::GetReusedStandardMission` (mission_list_manager.cpp:810) returns null unless `startRecent` (normal aa start/tap = false) → **every launch makes a NEW mission**. Dozens of session launches → dozens of records. On process death `HandleAbilityDiedByDefault` (:2633) KEEPS the record with `runningState=-1` (zombie) unless removeMissionAfterTerminate/excludeFromMissions (Android adapter abilities have neither); `DoesNotShowInTheMissionList` doesn't filter runningState=-1 → zombies shown.

**Why name but NO thumbnail:** NAME = BMS `appInfo.appName` (AmsMissionManager.ts:87, always present). THUMBNAIL needs a snapshot, fails 2 ways: (a) the snapshot store is the no-op stub (GetMissionSnapshot→false); (b) live WMS fallback `SnapshotController::GetSnapshot` (window_manager/.../snapshot_controller.cpp:31) fails — dead proc = null token, and even alive the catalog window never sets `firstFrameAvailable_` (the WMS-focus/displayId compositing wall) so `SurfaceDraw::GetSurfaceSnapshot` has no buffer. Launcher catches the empty SnapShotInfo → `RecentMissionCard.ets` renders `Image(undefined)` = blank + the name. (Legacy launcher recents; scene_board_enabled=false.)

**KEY NUANCE:** stubbed persistence + `MissionInfoMgr::Init` clears the list on boot → records CANNOT survive a true full power-cycle (in-memory only). A genuine reboot = 0 missions. So zombies seen "right after power on" are in-memory records from the current session's launches (AMS not fully restarted, or launches after the boot) — NOT disk-persisted. (Device peek to confirm `.json`/`.jpg` absent on disk was pending — board unreachable.)

**CLEANUP:** recents "Clear All" → `missionManager.clearAllMissions()` → AMS CleanAllMissions → DeleteAllMissionInfos (needs MANAGE_MISSIONS; launcher has it); single swipe → `clearMission(id)`; OR full reboot (clears in-memory list).
**PREVENTION:** set the catalog adapter ability launch mode to **`singleton`** in the entry.hap module.json (see [[adapter-launcher-icon-entryhap-fix]]) → repeated launches reuse one mission (GetSingletonMissionByName). Optionally `removeMissionAfterTerminate:true` → dead record deleted immediately. NEITHER restores thumbnails (needs un-stubbing mission_data_storage AND fixing the compositing wall — both required). Related: [[catalog-ime-bridge-impl]] (the WMS-focus wall), [[catalog-perf-jit-aot-findings]].
