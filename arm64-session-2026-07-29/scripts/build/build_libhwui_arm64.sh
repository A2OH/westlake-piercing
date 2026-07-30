#!/bin/bash
# build_libhwui_arm64.sh — libhwui.so for aarch64 OHOS 6.1.
# arm64 port of build/inner/compile_libhwui.sh (same 5-phase pipeline, same AOSP
# sources + aosp_patches, retargeted to aarch64-linux-ohos against the
# ohos-sdk-6.1 native sysroot and device-pulled OHOS libs in out/board_libs).
#
# Phases: 1 = hwui core+JNI  2a = adapter shim .o  2b = liboh_hwui_shim.so
#         2c = liboh_skia_rtti_shim.so  3 = link libhwui.so
# Usage: bash build_libhwui_arm64.sh [--phases=1,2a,2b,2c,3] [--keep-obj]
set -o pipefail

OH=/home/dspfac/openharmony
AOSP=/home/dspfac/aosp-android-11
A15=/home/dspfac/aosp-15
ADAPTER=/home/dspfac/bridge-build
B64=/home/dspfac/bridge-build-arm64
NDK=/home/dspfac/ohos-sdk-6.1/linux/native
SR=$NDK/sysroot
ML=$SR/usr/lib/aarch64-linux-ohos

# NOTE(arm64): hwui comes from aosp-15 (android-11 lacks Gainmap/Mesh/MemoryPolicy/CanvasOpBuffer
# and its API drifts hard from OH Skia m133).  aosp-15 also matches the androidfw/
# libbase/libutils we already built for arm64, so the whole stack is one generation.
# ★ android-14.0.0_r1 is the EXACT baseline aosp_patches/libs/hwui/patches was authored
# against: all 49 patches apply cleanly (android-11 = 18/49, android-15 = 22/49), and it
# is the only tree that ships pipeline/skia/SkiaOpenGLPipeline.h (android-15's public
# tags omit it).  Clone: /home/dspfac/aosp-14-base (sparse, libs/hwui only).
A14=/home/dspfac/aosp-14-base
HWUI_SRC=$A14/libs/hwui
HWUI_PATCH=$ADAPTER/aosp_patches/libs/hwui
SKIA_OH=$OH/third_party/skia/m133
SKIA_COMPAT=$ADAPTER/build/skia_compat_headers
BC=$ADAPTER/framework/appspawn-x/bionic_compat/include

OUT_BUILD=$B64/hwui-build
OBJ=$OUT_BUILD/obj
SHIM_OBJ=$OUT_BUILD/shim
LOG=$OUT_BUILD/log
AOSPLIB=$B64/aosp_lib          # our arm64 builds (minikin stack, androidfw, base…)
BOARD=$B64/out/board_libs      # device-pulled OHOS libs (skia_canvaskit, EGL, …)
OUTA=$B64/out                  # liboh_hwui_shim.so / liboh_skia_rtti_shim.so land here

CXX=$OH/prebuilts/clang/ohos/linux-x86_64/llvm/bin/clang++
CC=$OH/prebuilts/clang/ohos/linux-x86_64/llvm/bin/clang
NM=$OH/prebuilts/clang/ohos/linux-x86_64/llvm/bin/llvm-nm
BUILTINS=$(find $OH/prebuilts/clang/ohos -path '*aarch64-linux-ohos*builtins.a' 2>/dev/null | head -1)

PHASES="1,2a,2b,2c,3"; KEEP_OBJ=0
for a in "$@"; do case "$a" in --phases=*) PHASES="${a#*=}";; --keep-obj) KEEP_OBJ=1;; esac; done
has_phase() { echo "$PHASES" | tr ',' '\n' | grep -qx "$1"; }
log_info() { echo; echo "=== $* ==="; }
die() { echo "FATAL: $*" >&2; exit 1; }

mkdir -p $OBJ $SHIM_OBJ $LOG $AOSPLIB $OUTA

# ---------------------------------------------------------------- flags
WARN="-Wno-everything"
# ENABLE_STRING8_OBSOLETE_METHODS: aosp-15's String8 hides string()/isEmpty() behind this
# flag; android-14 hwui calls .string() ~29 times.  The flag only flips an access
# specifier on two inline methods — ABI-neutral, so linking aosp-15 libutils.so is fine.
# Full DEFS set ported verbatim from build/inner/compile_libhwui.sh (arm32).  These are
# load-bearing, not cosmetic: SK_BUILD_FOR_ANDROID_FRAMEWORK gates the whole
# SkAndroidFrameworkUtils / SkCanvas-android API surface inside OH Skia m133, and
# HWUI_NO_VULKAN / HWUI_OH_SURFACE / SKIA_OHOS select the OH code paths.
DEFS="-DEGL_EGLEXT_PROTOTYPES -DGL_GLEXT_PROTOTYPES"
DEFS="$DEFS -DATRACE_TAG=ATRACE_TAG_VIEW -DLOG_TAG=\"OpenGLRenderer\""
DEFS="$DEFS -D__OHOS__ -D_GNU_SOURCE -DANDROID"
DEFS="$DEFS -DHWUI_NO_VULKAN=1 -DSK_BUILD_FOR_ANDROID_FRAMEWORK=1 -DHWUI_NO_STATS=1"
DEFS="$DEFS -DHWUI_OH_SURFACE=1 -DENABLE_TEXT_ENHANCE -DSKIA_OHOS_SINGLE_OWNER"
DEFS="$DEFS -DSK_GANESH -DSKIA_OHOS -DSK_GL -DSKIA_DLL -DNDEBUG -DGPU_TEST_UTILS=1"
# arm64-only additions
DEFS="$DEFS -DENABLE_STRING8_OBSOLETE_METHODS"
# NOTE(arm64): keep bionic_compat/libcxx_compat.h for its AOSP shims, but suppress
# its hand-rolled std::__promote — the arm64 SDK libcxx already defines one and the
# two collide ("reference to '__promote' is ambiguous").
CB="--target=aarch64-linux-ohos --sysroot=$SR -fPIC -O2 -std=c++17 $WARN $DEFS"
CB="$CB -D_LIBCPP_PROMOTE_H -include $B64/libcxx_compat_arm64.h -include $SKIA_COMPAT/hwui_force_include.h -include $B64/hwui_stubinc/arm64_force_include.h -I$BC"

INC="-I$HWUI_SRC -I$HWUI_SRC/.. -I$HWUI_SRC/jni"
INC="$INC -I$A15/frameworks/base/libs/androidfw/include -I$A15/system/incremental_delivery/incfs/util/include -I$A15/external/fmtlib/include"
# minikin from aosp-15 (matches the aosp-15 hwui that consumes it; android-11's
# lacks FontFeature.h and its Layout/Font APIs have drifted).
MK14=/home/dspfac/aosp-14-minikin
INC="$INC -I$MK14/include -I$AOSP/external/harfbuzz_ng/src"
INC="$INC -I$SKIA_COMPAT -I$SKIA_OH -I$SKIA_OH/include -I$SKIA_OH/include/core -I$SKIA_OH/include/private"
INC="$INC -I$SKIA_OH/src/core -I$SKIA_OH/src/gpu -I$SKIA_OH/src/image -I$SKIA_OH/src/utils -I$SKIA_OH/src/shaders -I$SKIA_OH/src/codec"
INC="$INC -I$A15/system/libbase/include -I$A15/system/core/include -I$A15/system/core/libcutils/include"
INC="$INC -I$A15/system/core/libutils/include -I$A15/system/core/libutils/binder/include -I$A15/system/logging/liblog/include"
INC="$INC -I$AOSP/system/core/libsystem/include"
INC="$INC -I$AOSP/frameworks/native/include -I$AOSP/frameworks/native/libs/gui/include"
INC="$INC -I$AOSP/frameworks/native/libs/nativewindow/include -I$AOSP/frameworks/native/libs/nativebase/include"
INC="$INC -I$AOSP/frameworks/native/libs/ui/include -I$AOSP/frameworks/native/libs/ui/include_types"
INC="$INC -I$AOSP/frameworks/native/libs/arect/include -I$AOSP/frameworks/native/libs/math/include"
# nativehelper: use the NEWER headers-only tree (bridge-build/aosp), because
# skia_compat_headers/nativehelper/JNIPlatformHelp.h hard-includes it by absolute
# path — mixing in android-11's copy redefines jniCreateFileDescriptor et al.
NH15=$ADAPTER/aosp/libnativehelper
INC="$INC -I$NH15/include_jni -I$NH15/include -I$NH15/include_platform -I$NH15/include_platform_header_only"
# …then android-11's copy LAST, purely for the lowercase legacy aliases
# (nativehelper/scoped_local_ref.h, scoped_primitive_array.h) that the newer tree
# renamed.  Kept after NH15 so JNIHelp.h/JNIPlatformHelp.h still come from NH15.
INC="$INC -I$AOSP/libnativehelper/include -I$AOSP/libnativehelper/header_only_include"
INC="$INC -I$OH/third_party/EGL/api -I$OH/third_party/openGLES/api"
INC="$INC -I$OH/foundation/graphic/graphic_surface/interfaces/inner_api/surface"
INC="$INC -I$OH/foundation/graphic/graphic_surface/interfaces/inner_api/common"
INC="$INC -I$OH/foundation/graphic/graphic_surface/interfaces/inner_api/buffer_handle"
INC="$INC -I$OH/base/hiviewdfx/hilog/interfaces/native/innerkits/include"
INC="$INC -I$OH/commonlibrary/c_utils/base/include"
INC="$INC -I$AOSP/external/freetype/include -I$ADAPTER/framework/surface/jni"
INC="$INC -I$AOSP/external/icu/icu4c/source/common"
# arm64 extras: our own stub headers (SkBRDAllocator.h / statslog.h — see
# hwui_stubinc/) plus m133's private/base for SkTemplates.h, and aosp-15's
# frameworks/native/include so androidfw's ResourceTypes.h finds the newer
# ACONFIGURATION_* enums it references (must precede the aosp-11 copy).
# hwui_stubinc first; aosp-15 frameworks/native LAST so skia_compat_headers' android/
# stubs (surface_control.h etc.) still win — the real aosp-15 ones pull in ARect and a
# whole NDK surface that OH has no equivalent for.  aosp-15 is still on the path so
# androidfw's ResourceTypes.h can find android/configuration.h (ACONFIGURATION_* enums).
INC="-I$B64/hwui_stubinc $INC -I$A15/frameworks/native/libs/gui/include -I$A15/frameworks/native/include -I$SKIA_OH/include/private/base"

PHASE1_SRCS="
renderthread/RenderThread.cpp renderthread/RenderProxy.cpp renderthread/CanvasContext.cpp
renderthread/DrawFrameTask.cpp renderthread/EglManager.cpp renderthread/Frame.cpp
renderthread/RenderTask.cpp renderthread/TimeLord.cpp renderthread/CacheManager.cpp
renderthread/RenderEffectCapabilityQuery.cpp renderthread/HintSessionWrapper.cpp
renderthread/ReliableSurface.cpp renderstate/RenderState.cpp thread/CommonPool.cpp
utils/GLUtils.cpp utils/Color.cpp utils/Blur.cpp utils/LinearAllocator.cpp utils/StringUtils.cpp
DamageAccumulator.cpp Properties.cpp MemoryPolicy.cpp LightingInfo.cpp DeviceInfo.cpp
FrameInfo.cpp FrameInfoVisualizer.cpp TreeInfo.cpp JankTracker.cpp ProfileData.cpp
ProfileDataContainer.cpp FrameMetricsReporter.cpp Matrix.cpp Interpolator.cpp
AnimationContext.cpp Animator.cpp AnimatorManager.cpp PropertyValuesAnimatorSet.cpp
PropertyValuesHolder.cpp SkiaInterpolator.cpp
"
PHASE2_SRCS="
pipeline/skia/SkiaPipeline.cpp pipeline/skia/SkiaOpenGLPipeline.cpp pipeline/skia/SkiaDisplayList.cpp
pipeline/skia/SkiaRecordingCanvas.cpp pipeline/skia/SkiaProfileRenderer.cpp pipeline/skia/RenderNodeDrawable.cpp
pipeline/skia/ReorderBarrierDrawables.cpp pipeline/skia/LayerDrawable.cpp pipeline/skia/ShaderCache.cpp
pipeline/skia/SkiaMemoryTracer.cpp pipeline/skia/ATraceMemoryDump.cpp pipeline/skia/HolePunch.cpp
pipeline/skia/StretchMask.cpp pipeline/skia/TransformCanvas.cpp pipeline/skia/GLFunctorDrawable.cpp
canvas/CanvasFrontend.cpp canvas/CanvasOpBuffer.cpp canvas/CanvasOpRasterizer.cpp
RenderNode.cpp RenderProperties.cpp RootRenderNode.cpp RecordingCanvas.cpp SkiaCanvas.cpp
CanvasTransform.cpp DeferredLayerUpdater.cpp Layer.cpp LayerUpdateQueue.cpp
AutoBackendTextureRelease.cpp HardwareBitmapUploader.cpp Readback.cpp
effects/StretchEffect.cpp effects/GainmapRenderer.cpp Gainmap.cpp Tonemapper.cpp Mesh.cpp
VectorDrawable.cpp hwui/Bitmap.cpp hwui/Canvas.cpp hwui/PaintImpl.cpp hwui/MinikinSkia.cpp
hwui/MinikinUtils.cpp hwui/Typeface.cpp hwui/BlurDrawLooper.cpp hwui/AnimatedImageDrawable.cpp
hwui/AnimatedImageThread.cpp hwui/ImageDecoder.cpp PathParser.cpp utils/VectorDrawableUtils.cpp
utils/NdkUtils.cpp WebViewFunctorManager.cpp
"

compile_files() {
    local phase_name="$1"; shift
    echo; echo "--- $phase_name ---"
    local ok=0 fl=0 fails=""
    for src in "$@"; do
        [ -z "$src" ] && continue
        local name=$(echo "$src" | tr '/' '_' | sed 's/\.cpp$//')
        local full_path="$HWUI_SRC/$src"
        [ -f "$HWUI_PATCH/$src" ] && full_path="$HWUI_PATCH/$src"   # patched stub wins
        [ -f "$full_path" ] || { echo "  $src NOT FOUND"; fl=$((fl+1)); continue; }
        if $CXX $CB $INC -c "$full_path" -o "$OBJ/$name.o" 2>"$LOG/$name.err"; then ok=$((ok+1))
        else fl=$((fl+1)); fails="$fails $src"; fi
    done
    echo "--- $phase_name: $ok OK, $fl FAILED ---"
    [ -n "$fails" ] && { for f in $fails; do n=$(echo "$f" | tr '/' '_' | sed 's/\.cpp$//'); echo "    $f: $(grep -m1 'error:' $LOG/$n.err | grep -oE 'error:.*' | cut -c1-110)"; done; }
    return 0
}

phase1() {
    log_info "Phase 1 — hwui core + JNI"
    [ $KEEP_OBJ -eq 1 ] || rm -f $OBJ/*.o
    compile_files "Phase 1: Core Rendering Pipeline" $PHASE1_SRCS
    compile_files "Phase 2: DisplayList + Skia Pipeline" $PHASE2_SRCS
    # out-of-tree ColorSpace.cpp ([C-2])
    echo; echo "--- Phase Native ---"
    if $CXX $CB $INC -c $AOSP/frameworks/native/libs/ui/ColorSpace.cpp -o $OBJ/ColorSpace.o 2>$LOG/ColorSpace.err; then
        echo "  ColorSpace.cpp OK"
    else echo "  ColorSpace.cpp FAILED: $(grep -m1 'error:' $LOG/ColorSpace.err | cut -c1-110)"; fi
    # JNI: recursive jni/* (excl. jni/pdf) + apex/jni_runtime.cpp ([C-12])
    echo; echo "--- Phase JNI ---"
    local ok=0 fl=0 fails=""
    local jni_list=$(find $HWUI_SRC/jni -name '*.cpp' -not -path '*/pdf/*' | sort)
    # [C-15] apex/jni_runtime.cpp INTENTIONALLY SKIPPED — register_android_graphics_classes
    # is disabled in aosp_patches AndroidRuntime.cpp.patch; the known-good arm32
    # libhwui.so never contained apex_jni_runtime.o.
    for src in $jni_list; do
        local rel=${src#$HWUI_SRC/}
        local name="jni_$(echo ${rel#jni/} | tr '/' '_' | sed 's/\.cpp$//')"
        if $CXX $CB $INC -c "$src" -o "$OBJ/$name.o" 2>"$LOG/$name.err"; then ok=$((ok+1))
        else fl=$((fl+1)); fails="$fails $rel"; fi
    done
    echo "--- Phase JNI: $ok OK, $fl FAILED ---"
    [ -n "$fails" ] && { for f in $fails; do n="jni_$(echo ${f#jni/} | tr '/' '_' | sed 's/\.cpp$//')"; echo "    $f: $(grep -m1 'error:' $LOG/$n.err | grep -oE 'error:.*' | cut -c1-110)"; done | head -20; }
    echo; echo "  hwui object files: $(ls -1 $OBJ/*.o 2>/dev/null | wc -l)"
}

phase2a() {
    log_info "Phase 2a — adapter shim .o"
    local CFLAGS_STUB="--target=aarch64-linux-ohos --sysroot=$SR -fPIC -O2 -std=c++17 $WARN
        -I$BC -I$AOSP/libnativehelper/include_jni -I$AOSP/libnativehelper/include
        -D_LIBCPP_PROMOTE_H -include $B64/libcxx_compat_arm64.h"
    for f in hwui_oh_abi_patch hwui_register_stubs; do
        echo -n "  $f.cpp ... "
        if $CXX $CFLAGS_STUB -c $HWUI_PATCH/$f.cpp -o $OBJ/$f.o 2>$LOG/$f.err; then
            echo "OK ($(stat -c%s $OBJ/$f.o) B)"
        else echo "FAIL"; head -5 $LOG/$f.err; fi
    done
}

phase2b() {
    log_info "Phase 2b — liboh_hwui_shim.so"
    local SHIM_SRC=$ADAPTER/framework/hwui-shim/jni
    local SHIM_CB="--target=aarch64-linux-ohos --sysroot=$SR -fPIC -O2 $WARN"
    local SHIM_INC="-I$OH/foundation/graphic/graphic_surface/interfaces/inner_api/surface"
    SHIM_INC="$SHIM_INC -I$OH/foundation/graphic/graphic_surface/interfaces/inner_api/common"
    SHIM_INC="$SHIM_INC -I$OH/foundation/graphic/graphic_2d/interfaces/inner_api/composer"
    SHIM_INC="$SHIM_INC -I$OH/foundation/graphic/graphic_2d/rosen/modules/2d_graphics/drawing_ndk/include"
    SHIM_INC="$SHIM_INC -I$OH/interface/sdk_c/graphic/graphic_2d/native_buffer"
    SHIM_INC="$SHIM_INC -I$AOSP/libnativehelper/include_jni"
    # oh_skia_ahb_shim.cpp includes <GLES/gl.h>; OH ships the Khronos headers here.
    SHIM_INC="$SHIM_INC -I$OH/third_party/openGLES/api -I$OH/third_party/EGL/api"
    local objs=""
    for c in oh_native_window_shim oh_choreographer_shim oh_hardwarebuffer_shim atrace_compat sync_wait_compat ashmem_compat oh_display_shim wl_gl_trace wl_looper_trace; do
        echo -n "  $c.c ... "
        if $CC $SHIM_CB $SHIM_INC -c $SHIM_SRC/$c.c -o $SHIM_OBJ/$c.o 2>$SHIM_OBJ/$c.err; then
            echo "OK"; objs="$objs $SHIM_OBJ/$c.o"
        else echo "FAIL"; head -4 $SHIM_OBJ/$c.err; fi
    done
    # NOTE(arm64): oh_sync_builtins.S is ARM32-ONLY (hand-rolled __sync_synchronize /
    # __sync_val_compare_and_swap_1 because the arm32 clang builtins lib lacked them).
    # aarch64 clang provides both natively — deliberately skipped here.
    for cc in oh_minikin_shim oh_skia_ahb_shim oh_typeface_init oh_graphicsstats_shim; do
        echo -n "  $cc.cpp ... "
        if $CXX $SHIM_CB -std=c++17 -D_LIBCPP_PROMOTE_H -include $B64/libcxx_compat_arm64.h -I$BC \
             -DSK_BUILD_FOR_ANDROID -DSK_BUILD_FOR_ANDROID_FRAMEWORK -DSK_GANESH \
             -D__ANDROID_API__=33 -DENABLE_TEXT_ENHANCE -DSKIA_OHOS -DSKIA_DLL -DSK_GL \
             -DAHARDWAREBUFFER_FORMAT_R8_UNORM=0x38 \
             -I$SKIA_OH -I$SKIA_OH/include -I$SKIA_COMPAT -I$AOSP/frameworks/minikin/include \
             $SHIM_INC -c $SHIM_SRC/$cc.cpp -o $SHIM_OBJ/$cc.o 2>$SHIM_OBJ/$cc.err; then
            echo "OK"; objs="$objs $SHIM_OBJ/$cc.o"
        else echo "FAIL"; head -4 $SHIM_OBJ/$cc.err; fi
    done
    local SKIA_INC_FULL="-I$SKIA_OH -I$SKIA_OH/include -I$SKIA_OH/src -I$SKIA_OH/modules/skcms/src -I$OH/third_party/mesa3d/include/android_stub"
    local SKIA_DEFS="-DSK_BUILD_FOR_ANDROID -DSK_BUILD_FOR_ANDROID_FRAMEWORK -D__ANDROID_API__=33 -DSK_GANESH"
    for src_rel in src/android/SkAndroidFrameworkUtils.cpp src/codec/SkAndroidCodec.cpp src/android/SkAnimatedImage.cpp src/codec/SkSampledCodec.cpp src/codec/SkAndroidCodecAdapter.cpp src/codec/SkCodec.cpp; do
        local fn=$(basename $src_rel .cpp)
        echo -n "  $fn.cpp (OH Skia m133) ... "
        if $CXX $SHIM_CB -fno-rtti -std=c++17 -D_LIBCPP_PROMOTE_H -include $B64/libcxx_compat_arm64.h -I$BC $SKIA_DEFS $SKIA_INC_FULL \
             -c $SKIA_OH/$src_rel -o $SHIM_OBJ/$fn.o 2>$SHIM_OBJ/$fn.err; then
            echo "OK"; objs="$objs $SHIM_OBJ/$fn.o"
        else echo "FAIL"; head -4 $SHIM_OBJ/$fn.err; fi
    done
    echo "  linking liboh_hwui_shim.so ..."
    $CXX --target=aarch64-linux-ohos --sysroot=$SR -fuse-ld=lld -shared -fPIC \
        -B$ML -L$ML -L$BOARD -L$AOSPLIB -L$OUTA \
        -o $OUTA/liboh_hwui_shim.so $objs \
        -lnative_window -lnative_vsync -lnative_buffer -lnative_drawing \
        -lhitrace_ndk.z -lskia_canvaskit.z -lEGL -lGLESv3 -llog -loh_adapter_bridge \
        -Wl,--allow-shlib-undefined -Wl,--unresolved-symbols=ignore-all $BUILTINS 2>$SHIM_OBJ/link.err
    [ -f $OUTA/liboh_hwui_shim.so ] && echo "  OK ($(stat -c%s $OUTA/liboh_hwui_shim.so) B)" || { echo "  FAIL"; head -20 $SHIM_OBJ/link.err; }
}

phase2c() {
    log_info "Phase 2c — liboh_skia_rtti_shim.so"
    local SRC_DIR=$ADAPTER/framework/surface/jni/skia_rtti_shim
    [ -f "$SRC_DIR/skia_rtti_shim.cpp" ] || { echo "  SKIP (source not found: $SRC_DIR)"; return 0; }
    local RTTI_CFLAGS="--target=aarch64-linux-ohos --sysroot=$SR -fPIC -Os -std=c++17 -frtti -fvisibility=hidden -fno-exceptions $WARN"
    echo -n "  skia_rtti_shim.cpp ... "
    if $CXX $RTTI_CFLAGS -c $SRC_DIR/skia_rtti_shim.cpp -o $SHIM_OBJ/skia_rtti_shim.o 2>$LOG/rtti.err; then
        echo "OK ($(stat -c%s $SHIM_OBJ/skia_rtti_shim.o) B)"
    else echo "FAIL"; head -10 $LOG/rtti.err; return 1; fi
    local VER=""
    [ -f "$SRC_DIR/skia_rtti_shim.ver" ] && VER="-Wl,--version-script=$SRC_DIR/skia_rtti_shim.ver"
    echo -n "  liboh_skia_rtti_shim.so ... "
    if $CXX --target=aarch64-linux-ohos --sysroot=$SR -fuse-ld=lld -fPIC -shared -B$ML -L$ML \
         $VER -Wl,-soname=liboh_skia_rtti_shim.so \
         -o $OUTA/liboh_skia_rtti_shim.so $SHIM_OBJ/skia_rtti_shim.o -lc++ -lc $BUILTINS 2>$LOG/rtti_link.err; then
        echo "OK ($(stat -c%s $OUTA/liboh_skia_rtti_shim.so) B)"
    else echo "FAIL"; head -10 $LOG/rtti_link.err; fi
}

phase3() {
    log_info "Phase 3 — link libhwui.so"
    local n=$(ls -1 $OBJ/*.o 2>/dev/null | wc -l)
    [ "$n" -gt 0 ] || die "no objects in $OBJ"
    echo "  linking $n objects ..."
    $CXX --target=aarch64-linux-ohos --sysroot=$SR -fuse-ld=lld -fPIC -shared \
        -B$ML -L$ML -L$OUTA -L$AOSPLIB -L$BOARD \
        -Wl,--gc-sections -Wl,--as-needed -Wl,-Bsymbolic-functions \
        -Wl,--allow-shlib-undefined -Wl,-z,lazy -Wl,--unresolved-symbols=ignore-all \
        -Wl,-soname=libhwui.so -o $OUT_BUILD/libhwui.so $OBJ/*.o \
        -loh_hwui_shim -loh_skia_rtti_shim -lskia_canvaskit.z \
        -landroidfw -lEGL -lGLESv3 \
        -lminikin -lharfbuzz_ng -lft2 -licuuc -licui18n \
        -lutils -lcutils -lbase -llog -lhilog -lnative_display_manager -lnativehelper \
        -ldl -lc++ -lm -lc -lpthread $BUILTINS 2>$LOG/link.err
    if [ -f $OUT_BUILD/libhwui.so ]; then
        echo "  ✅ libhwui.so ($(stat -c%s $OUT_BUILD/libhwui.so) B)"
        echo "  register_android_graphics_* exported: $($NM -D --defined-only $OUT_BUILD/libhwui.so 2>/dev/null | grep -c register_android_graphics)"
    else echo "  ❌ LINK FAILED"; head -25 $LOG/link.err; fi
}

# ★ phase 1 does `rm -f $OBJ/*.o`, which also removes phase 2a's objects
# (hwui_oh_abi_patch.o / hwui_register_stubs.o).  Running `--phases=1,3` therefore
# links a libhwui.so missing them — it fails to relocate at dlopen with
# SkImages::DeferredFromAHardwareBuffer undefined.  Always run 2a after 1.
if has_phase 1 && has_phase 3 && ! has_phase 2a; then
  echo "NOTE: phase 1 wipes \$OBJ — adding phase 2a so the ABI-patch objects are rebuilt"
  PHASES="$PHASES,2a"
fi
has_phase 1  && phase1
has_phase 2a && phase2a
has_phase 2b && phase2b
has_phase 2c && phase2c
has_phase 3  && phase3
echo; echo "=== build_libhwui_arm64 done ==="
