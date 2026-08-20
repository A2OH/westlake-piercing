#!/bin/bash
# §599 — AOT boot image from the CURRENT device jars (incl. WlJitBench), §318 recipe.
D2O=$WLROOT/art-latest/build/bin/dex2oat
ART=$WLROOT/art-latest
SP=${WL_SCRATCH:-/tmp/wl-scratch}
FW=$SP/fwaot
OUT=$SP/aotimg; mkdir -p $OUT
DEV=/data/local/tmp/asx/fw          # the BCP path this child actually uses
JARS="core-oj core-libart core-icu4j okhttp bouncycastle apache-xml adapter-mainline-stubs framework adapter-runtime-bcp oh-adapter-framework"
DEXARGS=""
for j in $JARS; do DEXARGS="$DEXARGS --dex-file=$FW/$j.jar --dex-location=$DEV/$j.jar"; done
echo "[AOT] filter=$1 inline=$2 start $(date +%T)"
ANDROID_ROOT=$ART timeout 3000 $D2O $DEXARGS \
  --oat-file=$OUT/boot.oat --image=$OUT/boot.art \
  --instruction-set=arm64 --compiler-filter=$1 --base=0x70000000 \
  --inline-max-code-units=$2 --android-root=$ART \
  --runtime-arg -Xverify:none --runtime-arg -Xmx256m -j4 2>$OUT/d2o.err
echo "[AOT] rc=$? $(date +%T)"
ls -la $OUT/boot.art $OUT/boot.oat 2>/dev/null | awk '{print "  "$9" "$5"B"}'
grep -aE 'Check failed|Fatal|SIGSEGV|error:|abort' $OUT/d2o.err 2>/dev/null | tail -5 | cut -c1-140
