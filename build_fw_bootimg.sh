#!/bin/bash
D2O=/home/dspfac/art-latest/build/bin/dex2oat   # x86 host, ARM64 codegen
FW=/home/dspfac/bridge-build-arm64/fwjars
ART=/home/dspfac/art-latest
OUT=/home/dspfac/bridge-build-arm64/fwbootimg; mkdir -p $OUT
# bootclasspath in EXACT kBootClasspath order
JARS="core-oj core-libart core-icu4j okhttp bouncycastle apache-xml adapter-mainline-stubs framework adapter-runtime-bcp oh-adapter-framework"
DEXARGS=""; for j in $JARS; do DEXARGS="$DEXARGS --dex-file=$FW/$j.jar"; done
echo "[FWBOOT] building arm64 framework boot image ($(echo $JARS|wc -w) jars)..."
ANDROID_ROOT=$ART timeout 1200 $D2O $DEXARGS \
  --oat-file=$OUT/boot.oat --image=$OUT/boot.art \
  --instruction-set=arm64 --compiler-filter=verify --base=0x70000000 \
  --inline-max-code-units=0 --android-root=$ART --runtime-arg -Xverify:none --runtime-arg -Xmx256m -j1 2>$OUT/d2o.err
RC=$?; echo "[FWBOOT] dex2oat rc=$RC"
if [ $RC -eq 0 ]; then echo "[FWBOOT] OK: $(ls -la $OUT/boot.art|awk '{print $5}') bytes"; ls -la $OUT/boot*.art 2>/dev/null | sed 's|.*/||'; else echo "[FWBOOT] FAIL — cause:"; grep -aE 'Class mismatch|Check failed| F [0-9].*cc:|Fatal|error:|cannot|not found|Verify|abort|Signal|Unable' $OUT/d2o.err | grep -aviE 'Dumpable|held mutex' | tail -12 | cut -c1-140; fi
echo "FWBOOT_DONE"
