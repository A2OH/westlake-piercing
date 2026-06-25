#!/bin/bash
# V3 Fix H + Fix I.§5 — rebuild libart.so with:
#   - patched entrypoint_utils-inl.h (consumer-side IMT heap-range guard)
#   - patched class_linker.cc (producer-side IMT heap-range guard at FinalizeIfTable)
# Since entrypoint_utils-inl.h is an inline header included by many .cc files,
# we rebuild every .cc that includes it (or includes interpreter_common.h which
# includes it). Plus class_linker.o for Fix I.§5.
#
# Cascading rebuild list:
#   class_linker.cc                 -> class_linker.o            (Fix I.§5 + Fix H via direct include)
#   interpreter/interpreter_common.cc  -> interpreter_common.o   (Fix H via direct include + transitive)
#   interpreter/interpreter.cc      -> interpreter.o             (transitive include)
#   interpreter/interpreter_switch_impl0.cc -> interpreter_switch_impl0.o
#   interpreter/interpreter_switch_impl1.cc -> interpreter_switch_impl1.o
#   entrypoints/quick/quick_trampoline_entrypoints.cc -> quick_trampoline_entrypoints.o
#   interpreter/mterp/nterp.cc      -> nterp.o
set -e

WORK=$HOME/libart-pathA-work
OH=$HOME/libart-32arm-cache/libart-32arm-pathA-bundle/oh
A=$HOME/libart-32arm-cache/libart-32arm-pathA-bundle/aosp
ADAPTER=$HOME/libart-32arm-cache/libart-32arm-pathA-bundle/adapter
O=$WORK/out
CACHE=$WORK/cache/art

mkdir -p "$O" "$WORK/logs"

CXX=$OH/prebuilts/clang/ohos/linux-x86_64/llvm/bin/clang++
READELF=$OH/prebuilts/clang/ohos/linux-x86_64/llvm/bin/llvm-readelf
OH_OUT=$OH/out/rk3568
SR=$OH_OUT/obj/third_party/musl/usr
ML=$SR/lib/arm-linux-ohos
BC=$ADAPTER/framework/appspawn-x/bionic_compat/include
BC_SRC=$ADAPTER/framework/appspawn-x/bionic_compat/src
BUILTINS=$OH/prebuilts/clang/ohos/linux-x86_64/llvm/lib/clang/15.0.4/lib/arm-linux-ohos/libclang_rt.builtins.a

WARN_FLAGS="-Wno-unused-parameter -Wno-format -Wno-sign-compare -Wno-missing-field-initializers -Wno-c99-designator -Wno-gnu-designator -Wno-extern-c-compat -Wno-deprecated-declarations -Wno-c++11-narrowing -Wno-error"
COMMON_BASE="--target=arm-linux-ohos -march=armv7-a --sysroot=$SR -I$SR/include/arm-linux-ohos -fPIC -O2 -D__OHOS__ -D_GNU_SOURCE -D_POSIX_SOURCE $WARN_FLAGS"
CXXF="$CXX $COMMON_BASE -include $BC/libcxx_compat.h -I$BC -std=gnu++17"

ART_DEFS="-DART_ARM32_SUPPRESS_LOCKFREE_ASSERT -DNDEBUG -DANDROID_HOST_MUSL -DART_STACK_OVERFLOW_GAP_arm=8192 -DART_STACK_OVERFLOW_GAP_arm64=8192 -DART_STACK_OVERFLOW_GAP_riscv64=8192 -DART_STACK_OVERFLOW_GAP_x86=8192 -DART_STACK_OVERFLOW_GAP_x86_64=8192 -DART_TARGET -DART_TARGET_LINUX -DART_BASE_ADDRESS=0x70000000 -DART_ENABLE_CODEGEN_arm -DART_DEFAULT_GC_TYPE_IS_CMS -DART_FRAME_SIZE_LIMIT=1736 -DIMT_SIZE=43"
ART_INC="-isystem $BC/libcxx_array_aosp -I$WORK/src -I$A/art/runtime/entrypoints -I$A/art -I$A/art/libdexfile -I$A/art/libartbase -I$A/art/runtime -I$A/art/runtime/interpreter -I$A/art/libartpalette/include -I$A/art/libdexfile/external/include -I$A/art/libprofile -I$A/libnativehelper/include_jni -I$A/libnativehelper/include -I$A/libnativehelper/include_platform_header_only -I$A/libnativehelper/header_only_include -I$A/libnativehelper/header_only_include -I$A/system/logging/liblog/include -I$A/system/libbase/include -I$A/system/core/include -I$A/system/core/libcutils/include -I$A/system/core/libutils/include -I$A/external/fmtlib/include -I$A/external/lz4/lib -I$A/external/zlib -I$A/system/libziparchive/include -I$A/external/vixl/src -I$A/frameworks/native/include -I$A/external/tinyxml2 -I$A/system/unwinding/libunwindstack/include -I$A/system/unwinding/libbacktrace/include -I$BC/art -I$A/external/dlmalloc -I$A/external/cpu_features/include -I$A/art/cmdline -I$A/art/libelffile -I$A/external/googletest/googletest/include -I$A/libnativehelper/include_platform_header_only -I$A/art/libnativeloader/include -I$A/art/libnativebridge/include -I$A/system/libziparchive/incfs_support/include -I$A/art/odrefresh/include -I$A/art/runtime/jit"

LNK="$CXX --target=arm-linux-ohos -B$ML -L$ML -L$ADAPTER/out/aosp_lib -shared -fPIC"

# We rebuild patched .cc from $WORK/src; for ones we did NOT pre-stage, fall
# back to the pristine AOSP path. That lets us recompile transitive consumers
# (interpreter.cc etc.) without having to copy them into our src dir.
compile_one() {
    local src_basename="$1"   # e.g. "class_linker.cc"
    local out_basename="$2"   # e.g. "class_linker.o"
    local src_rel="$3"        # e.g. "art/runtime/" (path inside $A)
    local extra_inc="$4"      # e.g. "-I$A/art/runtime/interpreter"

    local src_path
    if [ -f "$WORK/src/$src_basename" ]; then
        src_path="$WORK/src/$src_basename"
        echo "  [patched src] $src_path"
    else
        src_path="$A/$src_rel$src_basename"
        echo "  [pristine src] $src_path"
    fi

    local log="$WORK/logs/compile_${out_basename%.o}.err"
    echo "  -> $CACHE/$out_basename"
    TS_START=$(date +%s)
    $CXXF $ART_DEFS $ART_INC -fno-rtti -I$BC_SRC $extra_inc \
        -c -o "$CACHE/$out_basename" "$src_path" 2> "$log"
    local RC=$?
    TS_END=$(date +%s)
    echo "    Compile took $((TS_END-TS_START))s, rc=$RC"
    if [ $RC -ne 0 ] || [ ! -s "$CACHE/$out_basename" ]; then
        echo "  COMPILE FAIL ($out_basename)"
        head -60 "$log"
        return 1
    fi
    md5sum "$CACHE/$out_basename"
    return 0
}

echo "=== Pre-build: baseline .o md5s ==="
for o in class_linker.o interpreter_common.o interpreter.o interpreter_switch_impl0.o interpreter_switch_impl1.o quick_trampoline_entrypoints.o nterp.o; do
    md5sum "$CACHE/$o" 2>/dev/null || echo "  (no prior $o)"
done

echo ""
echo "=== Compile Fix I.§5: class_linker.cc ==="
compile_one "class_linker.cc"            "class_linker.o"                "art/runtime/"           "" || exit 1

echo ""
echo "=== Compile Fix H consumers (entrypoint_utils-inl.h direct + transitive) ==="
compile_one "interpreter_common.cc"      "interpreter_common.o"          "art/runtime/interpreter/" "-I$A/art/runtime/interpreter" || exit 1
compile_one "interpreter.cc"             "interpreter.o"                 "art/runtime/interpreter/" "-I$A/art/runtime/interpreter" || exit 1
compile_one "interpreter_switch_impl0.cc" "interpreter_switch_impl0.o"   "art/runtime/interpreter/" "-I$A/art/runtime/interpreter" || exit 1
compile_one "interpreter_switch_impl1.cc" "interpreter_switch_impl1.o"   "art/runtime/interpreter/" "-I$A/art/runtime/interpreter" || exit 1
compile_one "quick_trampoline_entrypoints.cc" "quick_trampoline_entrypoints.o" "art/runtime/entrypoints/quick/" "" || exit 1
compile_one "nterp.cc"                   "nterp.o"                       "art/runtime/interpreter/mterp/" "-I$A/art/runtime/interpreter -I$A/art/runtime/interpreter/mterp" || exit 1

echo ""
echo "=== Compile FIX-J2B-2026-05-26: nterp_helpers.cc (CanMethodUseNterp gate) ==="
compile_one "nterp_helpers.cc"           "nterp_helpers.o"               "art/runtime/" "-I$A/art/runtime/interpreter -I$A/art/runtime/interpreter/mterp" || exit 1

echo ""
echo "=== Compile W15-PROBE: fault_handler.cc (name faulting method on unhandled SIGSEGV) ==="
compile_one "fault_handler.cc"           "fault_handler.o"               "art/runtime/" "" || exit 1

echo ""
echo "=== Compile W15-NPE-RECOVER: fault_handler_arm.cc (recover frame -> catchable NPE) ==="
compile_one "fault_handler_arm.cc"       "fault_handler_arm.o"           "art/runtime/arch/arm/" "" || exit 1

echo ""
echo "=== Compile RENDER-THREAD-HANG-PROBE: thread.cc (Thread::Init step markers) ==="
compile_one "thread.cc"                  "thread.o"                      "art/runtime/" "-I$A/art/runtime/interpreter" || exit 1

echo ""
echo "=== Link libart.so ==="
TS_START=$(date +%s)
OB=$(ls "$CACHE"/*.o | grep -v '\.qw2base$' | tr "\n" " ")
NUM_OBJ=$(ls "$CACHE"/*.o | grep -v '\.qw2base$' | wc -l)
echo "  Linking $NUM_OBJ object files"
$LNK -o "$O/libart.so" $OB -lc \
    -Wl,-Bsymbolic -lbionic_compat -llog -lbase -lcutils -lutils -lnativehelper \
    -lsigchain -ldexfile -lartbase -lartpalette -lvixl -llz4 -lziparchive \
    -lelffile -lnativebridge -lnativeloader -lprofile -ltinyxml2 -lunwindstack \
    -ldl -lpthread $BUILTINS 2> "$WORK/logs/link_libart.err"
RC=$?
TS_END=$(date +%s)
echo "  Link took $((TS_END-TS_START))s"
if [ $RC -ne 0 ]; then
    echo "  LINK FAIL (strict)"
    head -40 "$WORK/logs/link_libart.err"
    echo "  Retrying with --unresolved-symbols=ignore-all..."
    $LNK -o "$O/libart.so" $OB -lc \
        -Wl,-Bsymbolic -lbionic_compat -llog -lbase -lcutils -lutils -lnativehelper \
        -lsigchain -ldexfile -lartbase -lartpalette -lvixl -llz4 -lziparchive \
        -lelffile -lnativebridge -lnativeloader -lprofile -ltinyxml2 -lunwindstack \
        -ldl -lpthread $BUILTINS \
        -Wl,--unresolved-symbols=ignore-all 2> "$WORK/logs/link_libart_relaxed.err" || {
        echo "  LINK FAIL (relaxed too)"
        head -40 "$WORK/logs/link_libart_relaxed.err"
        exit 2
    }
fi
ls -la "$O/libart.so"
md5sum "$O/libart.so"
echo ""

echo "=== Verify JNI_CreateJavaVM export ==="
$READELF --dyn-syms "$O/libart.so" 2>/dev/null | grep JNI_CreateJavaVM | head -3
echo ""
echo "=== Verify FIX-H-RANGE / FIX-I / FIX-J2B marker strings present ==="
strings "$O/libart.so" | grep -E "FIX-H-RANGE|FIX-I|FIX-J2B" | head -10
echo ""
echo "DONE"
