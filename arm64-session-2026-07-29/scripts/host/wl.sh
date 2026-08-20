#!/bin/bash
# Send a command to the noice tap harness and print ONLY the log bytes produced after it.
#
# ⚠️`tail -0 -f` on this board's toybox dumps the WHOLE file (cost hours once), so this records a
# byte offset before the command and reads from that offset afterwards.
#
#   usage: wl.sh '<cmd>' [settle_seconds] [grep_filter]
#     cmd = harness verb: 'v' (dump rects) | 'w' | 'r<N>' | 'x y' (tap) | 'x1 y1 x2 y2' (swipe) | 'back'
. $WLROOT/westlake-arm64/arm64-session-2026-07-29/recipes/env.sh 2>/dev/null
: "${HDC:?source recipes/env.sh first}"
CMD="$1"; SETTLE="${2:-3}"; FILT="${3:-}"

C=$("$HDC" shell "ls -t /data/service/el1/public/appspawnx/adapter_child_*.stderr 2>/dev/null | head -1" | tr -d '\r')
[ -z "$C" ] && { echo "no child log"; exit 1; }

# Liveness FIRST — a dead process fakes every stall signature at once (hard-won rule).
# ⚠️Count appspawn-x, NOT "ashu": this board's ps prints the EXE basename, and the ART child keeps
# comm=appspawn-x. Grepping for the bundle name silently matches nothing and every command then
# looks like a dead app. Healthy = 2 (daemon + child).
ALIVE=$("$HDC" shell "ps -A -o ARGS | grep -c appspawn-x" | tr -d '\r')
[ "$ALIVE" -lt 2 ] && { echo "!! APP NOT RUNNING (appspawn-x procs=$ALIVE) — not sending '$CMD'"; exit 2; }

OFF=$("$HDC" shell "wc -c < $C" | tr -d '\r ')
"$HDC" shell "echo '$CMD' > /data/local/tmp/noice_tap"
sleep "$SETTLE"
NEW=$("$HDC" shell "wc -c < $C" | tr -d '\r ')
echo "### cmd='$CMD' settle=${SETTLE}s bytes=$OFF->$NEW (+$((NEW-OFF)))"
if [ "$NEW" -gt "$OFF" ]; then
  if [ -n "$FILT" ]; then
    "$HDC" shell "tail -c +$((OFF+1)) $C | grep -aE '$FILT'" | tr -d '\r'
  else
    "$HDC" shell "tail -c +$((OFF+1)) $C" | tr -d '\r'
  fi
fi
A2=$("$HDC" shell "ps -A | grep -c ashu" | tr -d '\r')
[ "$A2" -lt 1 ] && echo "!! APP DIED during this command"
exit 0
