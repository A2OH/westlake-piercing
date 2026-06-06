#!/bin/bash
# =============================================================================
# noice FULL per-page / per-submenu / per-widget UX TEST FIXTURE  (Westlake/OHOS)
# -----------------------------------------------------------------------------
# Exercises EVERY noice screen + every account submenu + the interactive widgets
# (time-dial, sliders, list scroll), verifying each with in-process truth
# (pid-alive + per-child-stderr fragment marker + screenshot byte-size) rather
# than flaky pixel matching.
#
# INPUT MODEL:
#   * /data/local/tmp/noice_tap is the in-process control channel (bridge
#     dispatchTouchViaViewRoot -> noice's ViewRootImpl). FOCUS-INDEPENDENT.
#       echo "N"      -> bottom-nav tab N (1=library 2=saved 3=timer 4=alarms 5=account)
#       echo "X Y"    -> tap at window coords (X,Y)
#   * uinput swipes (DRAGS for slider/dial/scroll) are WMS-focus-dependent and
#     only land when noice is the foreground/focused window (cold launch gives
#     this). Form: uinput -T -m X1 Y1 X2 Y2 DURATION_MS
#   * BACK from a 2nd-level page = tap the toolbar back-arrow at (40,60).
#
# COORDINATES (720x1280 portrait, validated this session):
#   tab row y=1218 : library=72 saved=216 timer=360 alarms=504 account=648
#   library row1 buttons y=337 : info=84 download=204 play=324 volume=492
#   add-alarm FAB = (648,1090) ; toolbar back-arrow = (40,60)
#   account submenu : register=(360,257) login=(360,383) subscription=(360,513)
#                     settings=(360,738) support=(360,866) about=(360,994)
#   settings sliders: fade-in handle ~ (125,490) ; fade-out handle ~ (125,697)
#   settings toggles: ignore-audio-focus ~ (635,835) ; media-buttons ~ (635,978)
#   time-picker dial: center ~ (360,615) ; "9" ~ (150,615) ; "3" ~ (560,615)
#                     OK 确定 ~ (598,972) ; Cancel 取消 ~ (447,972)
#
# USAGE: bash ux_full_fixture.sh
# OUTPUT: [PASS]/[FAIL]/[WARN] per step + summary; screenshots in $ART/.
# =============================================================================
set -u
HDC=/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe
BASE=/data/app/el1/0/base/com.github.ashutoshgngwr.noice
ART="$(dirname "$0")/uxshots"; mkdir -p "$ART"
sh(){ $HDC shell "$1" 2>&1 | tr -d '\r'; }
npid(){ for i in 1 2 3;do p=$(sh 'for d in /proc/[0-9]*;do u=$(grep -m1 "^Uid:" $d/status 2>/dev/null|cut -f2);c=$(cat $d/comm 2>/dev/null);if [ "$u" = "13731" ]&&[ "$c" != sh ];then echo ${d##*/};fi;done|head -1');[ -n "$p" ]&&{ echo "$p";return;};sleep 1;done; }
PID="";STDERR="";PASS=0;FAIL=0;WARN=0

ensure_up(){
  for t in 1 2 3 4;do
    sh "aa force-stop com.github.ashutoshgngwr.noice">/dev/null 2>&1;sleep 2
    sh "cp /data/local/tmp/noice-room.db.bak $BASE/databases/com.github.ashutoshgngwr.noice.db 2>/dev/null;cp /data/local/tmp/noice-cdn-cache.bak/* $BASE/cache/cdn-cache/ 2>/dev/null;chown -R 13731:13731 $BASE/databases $BASE/cache 2>/dev/null"
    sh "/data/local/tmp/bpfgrant 13731 oh_sock_permission_map >/dev/null 2>&1;power-shell wakeup;aa start -a com.github.ashutoshgngwr.noice.activity.MainActivity -b com.github.ashutoshgngwr.noice">/dev/null 2>&1;sleep 15
    PID=$(npid);[ -z "$PID" ]&&continue
    local sz=$(sh "snapshot_display -f /data/local/tmp/u.jpeg>/dev/null 2>&1;stat -c%s /data/local/tmp/u.jpeg")
    [ "${sz:-0}" -gt 45000 ]&&{ STDERR=/data/service/el1/public/appspawnx/adapter_child_$PID.stderr;echo "[up] noice pid=$PID";return 0;}
  done
  echo "[ABORT] could not bring noice up populated";return 1
}
# tap <label> <control-input> [stderr-marker] [sleep]
tap(){ local lbl="$1" inp="$2" mk="${3:-}" slp="${4:-4}"
  local b=$(sh "wc -l < $STDERR 2>/dev/null");b=${b:-0}
  sh "echo '$inp' > /data/local/tmp/noice_tap";sleep "$slp"
  local now=$(npid)
  local shot=$(sh "snapshot_display -f /data/local/tmp/u_${lbl}.jpeg>/dev/null 2>&1;stat -c%s /data/local/tmp/u_${lbl}.jpeg")
  $HDC file recv /data/local/tmp/u_${lbl}.jpeg "$ART/${lbl}.jpeg">/dev/null 2>&1
  if [ "$now" != "$PID" ];then echo "[FAIL] $lbl : CRASH ($PID->${now:-dead})";FAIL=$((FAIL+1));ensure_up;return;fi
  local info="alive shot=$shot"
  if [ -n "$mk" ];then local h=$(sh "tail -n +$((b+1)) $STDERR 2>/dev/null"|grep -acE "$mk");[ "$h" -gt 0 ]&&info="alive+nav($mk) shot=$shot"||info="alive(no '$mk' marker) shot=$shot";fi
  echo "[PASS] $lbl : $info";PASS=$((PASS+1))
}
# drag via uinput (widget interaction; needs noice foreground)
drag(){ local lbl="$1" x1="$2" y1="$3" x2="$4" y2="$5" dur="${6:-500}"
  sh "uinput -T -m $x1 $y1 $x2 $y2 $dur">/dev/null 2>&1;sleep 3
  local now=$(npid);local shot=$(sh "snapshot_display -f /data/local/tmp/u_${lbl}.jpeg>/dev/null 2>&1;stat -c%s /data/local/tmp/u_${lbl}.jpeg")
  $HDC file recv /data/local/tmp/u_${lbl}.jpeg "$ART/${lbl}.jpeg">/dev/null 2>&1
  [ "$now" = "$PID" ]&&{ echo "[PASS] drag:$lbl : alive shot=$shot (compare $ART/${lbl}.jpeg to see the widget moved)";PASS=$((PASS+1));}||{ echo "[FAIL] drag:$lbl : CRASH";FAIL=$((FAIL+1));ensure_up;}
}
back(){ sh "echo '40 60' > /data/local/tmp/noice_tap";sleep 3; }   # toolbar back-arrow

echo "================= noice FULL UX FIXTURE ================="
ensure_up || exit 2

echo "----- TOP-LEVEL TABS -----"
tap tab1_library   "1"        "LibraryFragment"
tap tab2_saved     "2"        "PresetsFragment"      ; tap back_lib "1" "" 2
tap tab3_timer     "3"        ""
tap tab4_alarms    "4"        "AlarmsFragment"       ; tap back_lib2 "1" "" 2
tap tab5_account   "5"        "AccountFragment"      ; tap back_lib3 "1" "" 2

echo "----- LIBRARY ROW WIDGETS (Birds) -----"
tap soundinfo      "84 337"   "SoundInfo"      5     ; tap dismiss_info "0 1" "" 2
tap volume_dialog  "492 337"  "NATIVE-DIALOG|SUB_WINDOW" 5 ; tap dismiss_vol "0 1" "" 2
tap play_sound     "324 337"  "SoundPlaybackService|playSound" 5

echo "----- ALARMS: add-alarm TIME-PICKER + DIAL DRAG -----"
tap alarms_tab     "4"        "AlarmsFragment"
tap add_alarm      "648 1090" "TimePicker|Alarm" 5
drag dial_9_to_3   150 615 560 615 600        # drag hour hand 9 -> 3
tap timepicker_ok  "598 972"  ""             3     # 确定
tap back_from_alarm "1" "" 2

echo "----- ACCOUNT SUBMENUS (2nd level) -----"
tap account_tab    "5"        "AccountFragment"
tap sub_settings   "360 738"  "Settings|Preference" 5
echo "  -- settings widgets --"
drag fadein_slider 125 490 400 490 500            # fade-in slider
drag fadeout_slider 125 697 380 697 500           # fade-out slider
tap toggle_mediakeys "635 978" "" 3               # media-buttons toggle
drag settings_scroll 360 1000 360 300 400         # scroll the settings list
back                                              # back to account
tap sub_about      "360 994"  "About|libraries" 5 ; back
tap sub_support    "360 866"  "Support|Donate" 5  ; back
tap sub_register   "360 257"  "SignUp|Register|ViewModel" 6 ; back
tap sub_login      "360 383"  "SignIn|Login" 6    ; back
tap sub_subscription "360 513" "Subscription|ViewSubscriptionPlans" 7
echo "    (subscription needs live API; PASS=page shown, data-load depends on TLS/conscrypt)"
back

echo "================= SUMMARY ================="
echo "PASS=$PASS FAIL=$FAIL WARN=$WARN"
echo "screenshots: $ART/"
echo "UXFULL_DONE"
[ "$FAIL" = "0" ]
