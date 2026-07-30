#!/bin/bash
# wl_click_all.sh <tabX> <LABEL> [maxwidgets] — click EVERY clickable widget on a page.
# For each: snapshot tree signature, tap centre, re-dump, report changed/unchanged, then BACK.
HDC=/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe
TAB=$1; LABEL=$2; MAX=${3:-40}
LOG=$($HDC shell "ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr | head -1" 2>/dev/null | tr -d '\r')

dump() {  # prints the widget lines emitted since line $1
  $HDC shell "tail -n +$1 $LOG | grep -a 'OH_InputBridge: VT'" 2>/dev/null | tr -d '\r' | sed -E 's/.*VT +//'
}
lines() { $HDC shell "wc -l < $LOG" 2>/dev/null | tr -d '\r '; }
alive() { $HDC shell "ps -A | grep -ac ashu" 2>/dev/null | tr -d '\r '; }

$HDC shell "echo '$TAB 1829' > /data/local/tmp/noice_tap" >/dev/null 2>&1; sleep 7
N=$(lines); $HDC shell "echo 'v' > /data/local/tmp/noice_tap" >/dev/null 2>&1; sleep 7
mapfile -t W < <(dump $N | grep -a 'c=1' | grep -vE 'id=(library|presets|sleep_timer|alarms|account) ' | head -$MAX)
echo "===== $LABEL : ${#W[@]} clickable widgets ====="
i=0
for w in "${W[@]}"; do
  i=$((i+1))
  rect=$(echo "$w" | grep -oE 'rect=\[[0-9]+,[0-9]+ [0-9]+x[0-9]+\]')
  id=$(echo "$w"  | grep -oE 'id=[^ ]+' | head -1)
  txt=$(echo "$w" | grep -oE '"[^"]*"' | head -1)
  en=$(echo "$w"  | grep -oE 'en=[01]')
  xy=$(echo "$rect" | sed -E 's/rect=\[([0-9]+),([0-9]+) ([0-9]+)x([0-9]+)\]/\1 \2 \3 \4/')
  set -- $xy; cx=$(( $1 + $3/2 )); cy=$(( $2 + $4/2 ))
  [ "$cy" -ge 1735 ] && { printf "%2d %-26s %-6s SKIP (overlaps bottom nav)\n" $i "$id" "$en"; continue; }
  B=$(lines); $HDC shell "echo '$cx $cy' > /data/local/tmp/noice_tap" >/dev/null 2>&1; sleep 5
  M=$(lines); $HDC shell "echo 'v' > /data/local/tmp/noice_tap" >/dev/null 2>&1; sleep 6
  after=$(dump $M | md5sum | cut -c1-8)
  A=$(alive)
  printf "%2d %-26s %-6s @(%4d,%4d) %-18s after=%s alive=%s\n" $i "$id" "$en" $cx $cy "$txt" "$after" "$A"
  [ "$A" != "1" ] && { echo "   !! child died — stopping"; break; }
  $HDC shell "echo 'back' > /data/local/tmp/noice_tap" >/dev/null 2>&1; sleep 4
done
