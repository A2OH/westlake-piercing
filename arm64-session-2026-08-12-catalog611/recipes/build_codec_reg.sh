#!/bin/bash
# §611g: compile WlSkiaCodecRegister.cpp with build_libhwui_arm64.sh's exact flags,
# drop it into the obj/ dir, then run the script's own phase-3 relink.
set -o pipefail
B64=${B64:?set B64 to the bridge build tree (contains build_libhwui_arm64.sh)}
SCRIPT=$B64/build_libhwui_arm64.sh
SRC=${SRC:-$(dirname "$0")/../../arm64-session-2026-07-29/bridge-full-src/framework/hwui-shim/jni/WlSkiaCodecRegister.cpp}

# Source the variable prelude of the build script (everything before PHASE1_SRCS).
PRELUDE_END=$(grep -n '^PHASE1_SRCS=' "$SCRIPT" | cut -d: -f1)
source <(sed -n "1,$((PRELUDE_END-1))p" "$SCRIPT")

echo "CXX=$CXX"
echo "OBJ=$OBJ"
test -n "$CXX" && test -n "$OBJ" || { echo "FATAL: prelude vars missing"; exit 1; }

$CXX $CB $INC -c "$SRC" -o "$OBJ/WlSkiaCodecRegister.o" 2>/tmp/codecreg.err
if [ $? -ne 0 ]; then echo "COMPILE FAILED:"; head -20 /tmp/codecreg.err; exit 1; fi
echo "compiled WlSkiaCodecRegister.o"
bash "$SCRIPT" --phases=3 2>&1 | tail -8
