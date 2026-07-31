#!/bin/bash
# §485 — stop the sound-metadata read from being dispatched to Room's wedged TransactionExecutor.
#
# The generated DAO calls CoroutinesRoom.execute(db, inTransaction=TRUE, ...). Probed live on device,
# that database's TransactionExecutor has a non-null ACTIVE task and a non-empty queue while its
# delegate executor is healthy — i.e. an earlier withTransaction block never completed and everything
# behind it starves. The read itself needs no transaction, so flip the flag and let it run on the
# query executor, which is proven to work.
#
# Applied to the DEPLOYED apk so the §440 proxy surgery is preserved. Backup: noice.apk.pre485
set -eo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; . "$HERE/env.sh"
board_online || exit 1
OUT="${OUT:-/tmp/noice-room}"; rm -rf "$OUT"; mkdir -p "$OUT"
recv "$ASX/noice.apk" "$OUT/orig.apk" || exit 1
( cd "$OUT" && unzip -o -q orig.apk classes.dex )

javac -nowarn -cp "$DEXLIB_CP" -d "$OUT" "$HERE/../tools/PatchInvoke.java" "$HERE/../tools/Disasm.java"
echo "--- before ---"
java -cp "$DEXLIB_CP:$OUT" Disasm "$OUT/classes.dex" "Ly2/v;" b | sed -n '15,18p'

# v1 holds the bind index (1) and is then reused as the inTransaction flag; it is dead after the
# binds, so setting it to 0 immediately before the call is safe and needs no extra register.
# Find the CoroutinesRoom.execute call site rather than hardcoding an index — earlier tracing may
# have shifted it.
IDX=$(java -cp "$DEXLIB_CP:$OUT" Disasm "$OUT/classes.dex" "Ly2/v;" b \
      | grep -n 'Landroidx/room/a;->c(' | head -1 | sed 's/^.*\[\([0-9]*\)\].*/\1/')
echo "CoroutinesRoom.execute is at index $IDX"
# v2 holds the Callable (built at the NEW_INSTANCE a few instructions earlier).
java -Xmx4g -cp "$DEXLIB_CP:$OUT" PatchInvoke "$OUT/classes.dex" "$OUT/patched.dex" \
  "Ly2/v;:b:$IDX:2"

echo "--- after ---"
java -cp "$DEXLIB_CP:$OUT" Disasm "$OUT/patched.dex" "Ly2/v;" b | sed -n '15,19p'

python3 - "$OUT/orig.apk" "$OUT/patched.dex" "$OUT/noice.apk" <<'PY'
import sys, zipfile
base, dex, out = sys.argv[1:4]
zin = zipfile.ZipFile(base); zout = zipfile.ZipFile(out, 'w')
for i in zin.infolist():
    data = open(dex,'rb').read() if i.filename == 'classes.dex' else zin.read(i.filename)
    zi = zipfile.ZipInfo(i.filename, date_time=i.date_time)
    zi.compress_type = i.compress_type; zi.external_attr = i.external_attr
    zi.create_system = i.create_system
    zout.writestr(zi, data)
zout.close(); zin.close()
PY
"$ZIPALIGN" -f -p 4 "$OUT/noice.apk" "$OUT/noice-aligned.apk"
"$HDC" shell "[ -f $ASX/noice.apk.pre485 ] || cp $ASX/noice.apk $ASX/noice.apk.pre485"
push "$OUT/noice-aligned.apk" "$ASX/noice.apk"
echo "§485 applied"
