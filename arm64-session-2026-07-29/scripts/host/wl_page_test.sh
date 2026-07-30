#!/bin/bash
# wl_page_test.sh <label> — capture the current page: title, screenshot, and every clickable widget
# with its tap centre, so each can then be exercised individually.
HDC=/mnt/c/Users/dspfa/Dev/ohos-tools/hdc.exe
SP=/tmp/claude-1000/-home-dspfac-openharmony/9adf5c05-1946-4e31-a77a-b6e8688c50b6/scratchpad
N=$1
/home/dspfac/wl_tree.sh > $SP/tree_$N.txt 2>&1
TITLE=$(grep -A1 'Toolbar id=action_bar' $SP/tree_$N.txt | grep -o '"[^"]*"' | head -1)
A=$($HDC shell "ps -A | grep -c '[a]shu'" 2>/dev/null | tr -d '\r')
echo "== $N  alive=$A  title=$TITLE"
grep -E 'c=1 ' $SP/tree_$N.txt | sed 's/^ *//' | while read -r line; do
  R=$(echo "$line" | grep -oE 'rect=\[[0-9-]+,[0-9-]+ [0-9]+x[0-9]+\]')
  X=$(echo "$R" | sed -E 's/rect=\[(-?[0-9]+),.*/\1/'); Y=$(echo "$R" | sed -E 's/.*,(-?[0-9]+) .*/\1/')
  W=$(echo "$R" | sed -E 's/.* ([0-9]+)x[0-9]+\]/\1/'); H=$(echo "$R" | sed -E 's/.*x([0-9]+)\]/\1/')
  [ -z "$W" ] && continue
  ID=$(echo "$line" | grep -oE 'id=[A-Za-z0-9_-]+' | head -1 | cut -d= -f2)
  TXT=$(echo "$line" | grep -oE '"[^"]*"' | head -1 | cut -c1-34)
  echo "   tap $((X+W/2)) $((Y+H/2))   id=$ID $TXT"
done
