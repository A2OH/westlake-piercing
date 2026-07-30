#!/bin/bash
# build_minikin_stack_arm64.sh — libft2 / libicuuc / libicui18n / libharfbuzz_ng / libminikin
# for aarch64 OHOS 6.1.  arm64 port of build/inner/cross_compile_minikin_stack.sh
# (same AOSP sources + same flags; retargeted to aarch64-linux-ohos with the
# ohos-sdk-6.1 native sysroot, matching build_aosp_lib_arm64.sh conventions).
set -o pipefail

OH=$WLROOT/openharmony
AOSP=$WLROOT/aosp-android-11
NDK=$WLROOT/ohos-sdk-6.1/linux/native
SR=$NDK/sysroot
B64=$WLROOT/bridge-build-arm64
OUT=$B64/aosp_lib          # arm64 libs (libbase/liblog/libutils/... already here)
TMP=/tmp/minikin_stack64
LOG=$B64/minikin_stack64.log
BC=$WLROOT/bridge-build/framework/appspawn-x/bionic_compat/include

CXX=$OH/prebuilts/clang/ohos/linux-x86_64/llvm/bin/clang++
CC=$OH/prebuilts/clang/ohos/linux-x86_64/llvm/bin/clang
BUILTINS=$(find $OH/prebuilts/clang/ohos -path '*aarch64-linux-ohos*builtins.a' 2>/dev/null | head -1)
ML=$SR/usr/lib/aarch64-linux-ohos

ONLY=""
for arg in "$@"; do case "$arg" in --only=*) ONLY="${arg#*=}" ;; esac; done

TARGET="--target=aarch64-linux-ohos --sysroot=$SR"
COMMON="$TARGET -fPIC -O2 -fno-exceptions"
WARN="-Wno-everything"
CF="$CC $COMMON $WARN -std=c99"
CXXF="$CXX $COMMON $WARN -std=c++17"   # NOTE: arm64 SDK libcxx already has __promote/nullptr_t;
                                       # force-including bionic_compat/libcxx_compat.h collides (ambiguous __promote)
LNK="$CXX $TARGET -fuse-ld=lld -B$ML -L$ML -L$OUT -shared -fPIC -Wl,--allow-shlib-undefined"

mkdir -p "$OUT" "$TMP"; > "$LOG"

should_build() { [ -z "$ONLY" ] && return 0; echo "$ONLY" | tr ',' '\n' | grep -qx "$1"; }
banner() { echo; echo "============================================================"; echo "  $*"; echo "============================================================"; echo "===$*===" >> "$LOG"; }
compile_file() {
    local src="$1" out_o="$2" comp_cmd="$3" inc="$4"
    if $comp_cmd $inc -c "$src" -o "$out_o" 2>>"$LOG"; then return 0; fi
    echo "  FAIL $(basename $src)" >&2; return 1
}

# ============================================================ 1. libft2
if should_build libft2; then
    banner "1. libft2 (FreeType)"
    FT="$AOSP/external/freetype"; FTD="$TMP/ft2"; mkdir -p "$FTD"
    FT_SRCS=(src/autofit/autofit.c src/base/ftbase.c src/base/ftbbox.c src/base/ftbitmap.c
        src/base/ftdebug.c src/base/ftfstype.c src/base/ftgasp.c src/base/ftglyph.c
        src/base/ftinit.c src/base/ftmm.c src/base/ftstroke.c src/base/fttype1.c
        src/base/ftsystem.c src/cid/type1cid.c src/cff/cff.c src/gzip/ftgzip.c
        src/psaux/psaux.c src/pshinter/pshinter.c src/psnames/psnames.c src/raster/raster.c
        src/sfnt/sfnt.c src/smooth/smooth.c src/truetype/truetype.c src/type1/type1.c
        src/bdf/bdf.c src/sdf/sdf.c src/svg/svg.c)
    FT_CFLAGS=(-DFT2_BUILD_LIBRARY -DDARWIN_NO_CARBON -DFT_CONFIG_OPTION_USE_ZLIB -I$FT/include -I$FT)
    ok=0; fl=0
    for s in "${FT_SRCS[@]}"; do
        [ -f "$FT/$s" ] || { echo "  MISSING $s"; continue; }
        compile_file "$FT/$s" "$FTD/$(basename $s .c).o" "$CF" "${FT_CFLAGS[*]}" && ok=$((ok+1)) || fl=$((fl+1))
    done
    echo "libft2: $ok ok / $fl fail"
    if [ $fl -eq 0 ] && [ $ok -gt 0 ]; then
        $LNK -Wl,-soname=libft2.so -o "$OUT/libft2.so" $FTD/*.o $BUILTINS 2>>"$LOG" \
            && echo "  LINKED: $OUT/libft2.so ($(stat -c%s $OUT/libft2.so) B)" || echo "  LINK FAIL — see $LOG" >&2
    fi
fi

# ============================================================ 2. libicuuc / libicui18n
if should_build libicuuc || should_build libicui18n; then
    banner "2. libicuuc + libicui18n (ICU)"
    ICU="$AOSP/external/icu/icu4c/source"; ICD="$TMP/icu"; mkdir -p "$ICD/common" "$ICD/i18n"
    ICU_COMPAT_HDR="$TMP/icu_std_compat.h"
    cat > "$ICU_COMPAT_HDR" << 'EOF'
#ifdef __cplusplus
#include <stdlib.h>
#include <math.h>
namespace std { using ::div; using ::div_t; using ::labs; using ::llabs; }
#endif
EOF
    ICU_CFLAGS=(-DU_COMMON_IMPLEMENTATION -DU_ATTRIBUTE_DEPRECATED= -DU_HAVE_STD_ATOMICS=1
                -DU_HAVE_STRTOD_L=0 "-include" "$ICU_COMPAT_HDR" -I$ICU/common -I$ICU/i18n)
    for dir in common i18n; do
        ok=0; fl=0
        for s in $ICU/$dir/*.cpp; do
            [ -f "$s" ] || continue
            local_cflags=("${ICU_CFLAGS[@]}")
            if [ "$dir" = "i18n" ]; then local_cflags[0]="-DU_I18N_IMPLEMENTATION"; local_cflags+=("-DU_LIB_SUFFIX_C_NAME="); fi
            compile_file "$s" "$ICD/$dir/$(basename $s .cpp).o" "$CXXF" "${local_cflags[*]}" && ok=$((ok+1)) || fl=$((fl+1))
        done
        echo "libicu$dir: $ok ok / $fl fail"
        if [ $fl -eq 0 ] && [ $ok -gt 0 ]; then
            libname="libicuuc.so"; extra_link=""
            [ "$dir" = "i18n" ] && { libname="libicui18n.so"; extra_link="-licuuc"; }
            if [ "$dir" = "common" ]; then
                compile_file "$ICU/stubdata/stubdata.cpp" "$ICD/common/stubdata.o" "$CXXF" "-I$ICU/common"
            fi
            $LNK -Wl,-soname=$libname -o "$OUT/$libname" $ICD/$dir/*.o $extra_link $BUILTINS 2>>"$LOG" \
                && echo "  LINKED: $OUT/$libname ($(stat -c%s $OUT/$libname) B)" || echo "  LINK FAIL $libname — see $LOG" >&2
        fi
    done
fi

# ============================================================ 3. libharfbuzz_ng
if should_build libharfbuzz_ng; then
    banner "3. libharfbuzz_ng (font shaping)"
    HB="$AOSP/external/harfbuzz_ng"; HBD="$TMP/harfbuzz_ng"; mkdir -p "$HBD"
    HB_CFLAGS=(-DHB_NO_MT -DHAVE_FREETYPE -DHAVE_ICU -DHAVE_OT -DHB_NO_UNICODE_FUNCS
               -DHB_NO_FALLBACK_SHAPE -I$HB/src -I$AOSP/external/freetype/include
               -I$AOSP/external/icu/icu4c/source/common)
    if [ -f "$HB/src/harfbuzz.cc" ]; then
        if compile_file "$HB/src/harfbuzz.cc" "$HBD/harfbuzz.o" "$CXXF" "${HB_CFLAGS[*]}"; then
            $LNK -Wl,-soname=libharfbuzz_ng.so -o "$OUT/libharfbuzz_ng.so" $HBD/*.o -lft2 -licuuc $BUILTINS 2>>"$LOG" \
                && echo "  LINKED: $OUT/libharfbuzz_ng.so ($(stat -c%s $OUT/libharfbuzz_ng.so) B)" || echo "  LINK FAIL — see $LOG" >&2
        fi
    else echo "  ERROR: $HB/src/harfbuzz.cc not found"; fi
fi

# ============================================================ 4. libminikin
if should_build libminikin; then
    banner "4. libminikin (Android text layout)"
    # ★ minikin MUST match the hwui generation (android-14) — android-11's minikin::Font
    # has no typeface() and hwui/MinikinSkia.cpp et al. fail against it.
    MK14=$WLROOT/aosp-14-minikin
    MK="$MK14/libs/minikin"; MKD="$TMP/minikin"; mkdir -p "$MKD"
    MK_SRCS=(BidiUtils.cpp BoundsCache.cpp CmapCoverage.cpp Emoji.cpp Font.cpp FontCollection.cpp
        FontFamily.cpp FontFeatureUtils.cpp FontFileParser.cpp FontUtils.cpp GraphemeBreak.cpp
        GreedyLineBreaker.cpp Hyphenator.cpp HyphenatorMap.cpp Layout.cpp LayoutCore.cpp
        LayoutUtils.cpp LineBreaker.cpp LineBreakerUtil.cpp Locale.cpp LocaleListCache.cpp
        MeasuredText.cpp Measurement.cpp MinikinFontFactory.cpp MinikinInternal.cpp
        OptimalLineBreaker.cpp SparseBitSet.cpp SystemFonts.cpp WordBreaker.cpp)
    # NOTE(arm64): aosp-android-11 has no system/libbase|core|logging synced, so the
    # support headers come from aosp-15 (same tree our arm64 libbase/liblog/libutils
    # were built from).  $BC supplies gtest/gtest_prod.h which minikin/Layout.h needs.
    A15=$WLROOT/aosp-15
    MK_CFLAGS=(-I$MK14/include -I$MK14/libs/minikin
        -I$AOSP/external/harfbuzz_ng/src -I$AOSP/external/icu/icu4c/source/common
        -I$AOSP/external/icu/icu4c/source/i18n -I$BC
        -I$A15/system/libbase/include -I$A15/system/logging/liblog/include
        -I$A15/system/core/libutils/include -I$A15/system/core/libutils/binder/include
        -I$A15/system/core/libcutils/include -I$A15/system/core/include)
    ok=0; fl=0
    for s in "${MK_SRCS[@]}"; do
        [ -f "$MK/$s" ] || { echo "  MISSING $s"; continue; }
        compile_file "$MK/$s" "$MKD/$(basename $s .cpp).o" "$CXXF" "${MK_CFLAGS[*]}" && ok=$((ok+1)) || fl=$((fl+1))
    done
    echo "libminikin: $ok ok / $fl fail"
    if [ $fl -eq 0 ] && [ $ok -gt 0 ]; then
        $LNK -Wl,-soname=libminikin.so -o "$OUT/libminikin.so" $MKD/*.o \
            -lharfbuzz_ng -lft2 -licuuc -licui18n -lbase -llog $BUILTINS 2>>"$LOG" \
            && echo "  LINKED: $OUT/libminikin.so ($(stat -c%s $OUT/libminikin.so) B)" || echo "  LINK FAIL — see $LOG" >&2
    fi
fi

echo; echo "=== minikin stack (arm64) done ==="; ls -la $OUT/libft2.so $OUT/libicuuc.so $OUT/libicui18n.so $OUT/libharfbuzz_ng.so $OUT/libminikin.so 2>/dev/null
