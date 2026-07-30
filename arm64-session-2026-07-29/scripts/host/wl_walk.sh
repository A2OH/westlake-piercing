#!/bin/bash
# wl_walk.sh — exercise one widget: ensure the app is up, go to a tab, tap the widget,
# then report liveness / resumes / any Java exception / the resulting screen.
HDC=/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe
SP=/tmp/claude-1000/-home-dspfac-openharmony/9adf5c05-1946-4e31-a77a-b6e8688c50b6/scratchpad
NAME=$1; TABX=$2; TABY=$3; X=$4; Y=$5
alive() { $HDC shell "ps -A | grep -c '[a]shu'" 2>/dev/null | tr -d '\r'; }
if [ "$(alive)" = "0" ]; then /home/dspfac/wl_restart.sh >/dev/null 2>&1; sleep 10; fi
# note where the log ends now, so we only report NEW failures
BEFORE=$($HDC shell "C=\$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr|head -1); grep -ac 'J_invokeStaticMain_main_threw' \$C" 2>/dev/null | tr -d '\r')
[ -n "$TABX" ] && { $HDC shell "echo '$TABX $TABY' > /data/local/tmp/noice_tap" >/dev/null 2>&1; sleep 3; }
$HDC shell "echo '$X $Y' > /data/local/tmp/noice_tap" >/dev/null 2>&1
sleep 5
A=$(alive)
EXC=$($HDC shell "C=\$(ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr|head -1); grep -a 'J_invokeStaticMain_main_threw' \$C | tail -n +$((BEFORE+1)) | tail -1" 2>/dev/null | tr -d '\r' | sed 's/.*main_threw: //' | cut -c1-150)
if [ "$A" != "0" ]; then
  /home/dspfac/wl_tree.sh > $SP/tree_$NAME.txt 2>&1
  SZ=$(/home/dspfac/wl_shot.sh $NAME 2>/dev/null | head -1)
  TITLE=$(grep -A1 'Toolbar id=action_bar' $SP/tree_$NAME.txt | grep -o '"[^"]*"' | head -1)
  echo "[$NAME] alive=$A shot=$SZ title=$TITLE widgets=$(grep -c 'c=1 ' $SP/tree_$NAME.txt)"
else
  echo "[$NAME] alive=0 DIED"
fi
[ -n "$EXC" ] && echo "        exception: $EXC"; true
