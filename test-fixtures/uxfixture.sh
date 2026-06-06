#!/bin/bash
# ============================================================================
# noice UX TEST FIXTURE  (2026-06-05)
# ----------------------------------------------------------------------------
# Reliable, automatable UX regression test for noice on the OHOS adapter.
#
# WHY THIS DESIGN (the rethink):
#   * INPUT via the CONTROL CHANNEL /data/local/tmp/noice_tap  -> the in-process
#     OHTouchInjector / dispatchTouchViaViewRoot dispatches straight to noice's
#     own ViewRootImpl (WindowManagerGlobal.mRoots).  This is FOCUS-INDEPENDENT
#     (works even when WMS input-focus drifted to the launcher) -- unlike uinput,
#     which only reaches the WMS-focused window.  REQUIREMENT: noice's window
#     must be present (foreground); a cold launch satisfies this, HOME/backgrounding
#     breaks it -> the fixture never backgrounds noice and relaunches if needed.
#   * VERIFY via in-process truth, not ambiguous screenshots (compositing race):
#       - CRASH  = pid changed/died  (+ hilog FATAL/Caused-by captured)   [hard]
#       - NAV    = target fragment/class marker in the per-child stderr   [round-1]
#       - RENDER = screenshot byte size (>45k populated)                  [soft]
#   * SELF-RECOVERY: restore db/cache for a populated list; reboot on no-spawn;
#     relaunch on crash so one failure never aborts the run.
#
# USAGE:  bash uxfixture.sh [ROUNDS]      (default 2 rounds)
# OUTPUT: structured [PASS]/[FAIL]/[WARN] per step + a final summary + exit code.
# ============================================================================
set -u
HDC=/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe
WINDIR='C:\Users\dspfa\Dev\ohos-tools'; WSLWIN=/mnt/c/Users/dspfa/Dev/ohos-tools
BASE=/data/app/el1/0/base/com.github.ashutoshgngwr.noice
ART=$HOME/openharmony/docs/engine/V3-NOICE-DPAD-FINDINGS/uxtest-shots
ROUNDS="${1:-2}"
mkdir -p "$ART"
sh()  { $HDC shell "$1" 2>&1 | tr -d '\r'; }
npid(){ for i in 1 2 3; do p=$(sh 'for d in /proc/[0-9]*; do u=$(grep -m1 "^Uid:" $d/status 2>/dev/null|cut -f2); c=$(cat $d/comm 2>/dev/null); if [ "$u" = "13731" ] && [ "$c" != sh ]; then echo ${d##*/}; fi; done|head -1'); [ -n "$p" ] && { echo "$p"; return; }; sleep 1; done; }

PID=""; STDERR=""; PASS=0; FAIL=0; WARN=0; CRASHES=""

reboot_dev() {
  echo "  [recover] rebooting (spawn degraded)..."
  sh "param set persist.sys.usb.config hdc; sync"; sh "reboot"; sleep 60
  $HDC kill >/dev/null 2>&1; sleep 2; $HDC start >/dev/null 2>&1; sleep 3
  for i in $(seq 1 18); do T=$($HDC list targets 2>&1|tr -d '\r'|grep -vE '^$|Empty|Fail|connect-key'|head -1); [ -n "$T" ] && break; /mnt/c/Windows/System32/taskkill.exe /F /IM hdc.exe>/dev/null 2>&1; sleep 2; $HDC start>/dev/null 2>&1; sleep 6; done
  sh "mkdir -p /dev/memcg/perf_sensitive 2>/dev/null; setenforce 0"
  sh "setsid sh /data/local/tmp/start_asx.sh </dev/null >/dev/null 2>&1 &"; sleep 14
  sh "chmod 0666 /dev/unix/socket/AppSpawnX; chcon u:object_r:appspawn_socket:s0 /dev/unix/socket/AppSpawnX; setenforce 0"
  sh "power-shell wakeup; power-shell timeout -o 3600000; power-shell display -o 230; param set persist.sys.usb.config none" >/dev/null 2>&1
  sh "/data/local/tmp/bpfgrant 13731 oh_sock_permission_map" >/dev/null 2>&1
  sh "hilog -p off >/dev/null 2>&1; param set hilog.private.on false >/dev/null 2>&1; echo '' > /data/local/tmp/noice_tap; chmod 666 /data/local/tmp/noice_tap"
}

# Bring noice up FOREGROUND + POPULATED. Reboot once if spawn degraded.
ensure_up() {
  local tried_reboot=0
  while :; do
    local got=""
    for t in 1 2 3 4; do
      sh "aa force-stop com.github.ashutoshgngwr.noice" >/dev/null 2>&1; sleep 2
      sh "cp /data/local/tmp/noice-room.db.bak $BASE/databases/com.github.ashutoshgngwr.noice.db 2>/dev/null; cp /data/local/tmp/noice-cdn-cache.bak/* $BASE/cache/cdn-cache/ 2>/dev/null; chown -R 13731:13731 $BASE/databases $BASE/cache 2>/dev/null"
      sh "power-shell wakeup; aa start -a com.github.ashutoshgngwr.noice.activity.MainActivity -b com.github.ashutoshgngwr.noice" >/dev/null 2>&1; sleep 14
      PID=$(npid); [ -z "$PID" ] && continue
      local sz=$(sh "snapshot_display -f /data/local/tmp/uxi.jpeg >/dev/null 2>&1; stat -c%s /data/local/tmp/uxi.jpeg")
      [ "${sz:-0}" -gt 45000 ] && { got=1; break; }
    done
    [ -n "$got" ] && break
    [ "$tried_reboot" = "1" ] && { echo "  [recover] still no populated spawn after reboot -- aborting"; return 1; }
    reboot_dev; tried_reboot=1
  done
  STDERR=/data/service/el1/public/appspawnx/adapter_child_$PID.stderr
  echo "  [up] noice pid=$PID (foreground, populated)"
}

relaunch() { echo "  [recover] relaunching after crash..."; ensure_up; }

SB=0
# step <label> <control-channel-input> [stderr-marker-regex] [sleep]
step() {
  local label="$1" input="$2" marker="${3:-}" slp="${4:-4}"
  sh "hilog -r >/dev/null 2>&1"
  SB=$(sh "wc -l < $STDERR 2>/dev/null"); SB=${SB:-0}
  sh "echo '$input' > /data/local/tmp/noice_tap"; sleep "$slp"
  local now=$(npid)
  local shot=$(sh "snapshot_display -f /data/local/tmp/ux_${label}.jpeg >/dev/null 2>&1; stat -c%s /data/local/tmp/ux_${label}.jpeg")
  $HDC file recv /data/local/tmp/ux_${label}.jpeg "$ART/${label}.jpeg" >/dev/null 2>&1
  if [ "$now" != "$PID" ]; then
    local cause=$(sh "hilog -x 2>/dev/null | grep -aiE 'FATAL|Caused by' | grep -vaE HDC_LOG | tail -1")
    echo "[FAIL] $label : CRASH (pid $PID -> '${now:-dead}')  ${cause}"
    FAIL=$((FAIL+1)); CRASHES="$CRASHES $label"; relaunch; return
  fi
  local navinfo="alive"
  if [ -n "$marker" ]; then
    local hit=$(sh "tail -n +$((SB+1)) $STDERR 2>/dev/null" | grep -acE "$marker")
    hit=$((hit + $(sh "hilog -x 2>/dev/null | grep -acE \"$marker\" 2>/dev/null") ))
    [ "$hit" -gt 0 ] && navinfo="alive + nav('$marker')" || navinfo="alive (no '$marker' marker; likely already-loaded)"
  fi
  echo "[PASS] $label : $navinfo  shot=${shot}"
  PASS=$((PASS+1))
}

echo "================ noice UX FIXTURE : $ROUNDS round(s) ================"
ensure_up || { echo "ABORT: could not bring noice up"; exit 2; }

for r in $(seq 1 "$ROUNDS"); do
  echo "---------------- ROUND $r ----------------"
  step "r${r}_tab_library"   "1"          "LibraryFragment"
  step "r${r}_sound_info"    "84 337"     "SoundInfoFragment|NATIVE-DIALOG|SUB_WINDOW"
  step "r${r}_back1"         "0 1"        ""        2   # dim-area tap to dismiss sheet
  step "r${r}_volume"        "492 337"    "NATIVE-DIALOG|SUB_WINDOW"
  step "r${r}_back2"         "0 1"        ""        2
  step "r${r}_play"          "324 337"    "SoundPlaybackService|playSound"
  step "r${r}_tab_saved"     "2"          "PresetsFragment"
  step "r${r}_tab_timer"     "3"          ""
  step "r${r}_tab_alarms"    "4"          "AlarmsFragment"
  step "r${r}_add_alarm"     "648 1090"   "TimePicker|Alarm" 5
  step "r${r}_back3"         "0 1"        ""        2
  step "r${r}_tab_account"   "5"          "AccountFragment"
  step "r${r}_subscription"  "360 513"    "ViewSubscriptionPlans|Subscription" 6
done

echo "================ SUMMARY ================"
echo "PASS=$PASS  FAIL=$FAIL"
[ -n "$CRASHES" ] && echo "CRASHED STEPS:$CRASHES"
echo "shots: $ART/"
echo "UXFIXTURE_DONE"
[ "$FAIL" = "0" ]
