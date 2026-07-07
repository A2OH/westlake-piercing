#!/bin/bash
# build_libart_so_arm64.sh — link a COMPLETE shared libart.so for arm64 from
# art-latest's object set (the one whose dalvikvm runs Java). art-universal's
# libart.so was incomplete (self_tls_ UND). Mirrors art-latest Makefile.ohos-arm64
# link-runtime EXACTLY (minus the dalvikvm.o main): all find-dirs + the specific
# stub objects (link_stubs=art_jni/quick, sve_stub, quick_entrypoints_stubs=nterp,
# metrics_stubs+real metrics via --allow-multiple-definition, fault/template/thread_cpu
# stubs, fmtlib, tinyxml2, jni_stubs). Deps: board libz (NEEDED) + libc + libc++_shared.
set -e
NDK=/home/dspfac/ohos-sdk-6.1/linux/native; CXX=$NDK/llvm/bin/clang++
BD=/home/dspfac/art-latest/build-ohos-arm64; O=/home/dspfac/bridge-build-arm64/out; LIBDIR=$O/board_libs
FIND_OBJS=$(find $BD/nativehelper $BD/runtime $BD/libdexfile $BD/libartbase $BD/libelffile $BD/libprofile $BD/compiler $BD/vixl $BD/android-base $BD/ziparchive -name '*.o')
STUB_OBJS="$BD/sigchain/sigchain.o $BD/stubs/link_stubs_arm64.o $BD/stubs/code_generator_vector_arm64_sve_stub.o $BD/stubs/fault_handler_stubs.o $BD/stubs/template_instantiations.o $BD/stubs/metrics_stubs.o $BD/stubs/thread_cpu_stub.o $BD/fmtlib/format.o $BD/tinyxml2/tinyxml2.o"
ASM_OBJS="$BD/asm_arm64/quick_entrypoints_arm64.o $BD/asm_arm64/jni_entrypoints_arm64.o $BD/asm_arm64/memcmp16_arm64.o $BD/stubs/quick_entrypoints_stubs_arm64.o"
$CXX --target=aarch64-linux-ohos --sysroot=$NDK/sysroot -shared -fPIC -stdlib=libc++ -fuse-ld=lld \
  -Wl,-Bsymbolic -Wl,-soname,libart.so -Wl,--allow-shlib-undefined -Wl,--allow-multiple-definition \
  -o $O/libart.so $FIND_OBJS $STUB_OBJS $ASM_OBJS $(find $BD/jni_stubs -name '*.o') \
  -L$LIBDIR -lz -lc -ldl -lpthread
echo "libart.so -> $O/libart.so"
