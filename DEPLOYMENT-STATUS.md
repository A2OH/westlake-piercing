# Bridge deployment + appspawn-x/framework run — status (2026-07-08)

## ✅ appspawn-x + framework RUN on the board; framework loads the bridge in-process
Ran the staged appspawn-x (/data/local/tmp/asx/) with the framework:
```
cd /data/local/tmp/asx
ANDROID_ROOT=/system ICU_DATA=/data/local/tmp/asx \
LD_LIBRARY_PATH=/data/local/tmp/asx:/system/lib64:/system/lib64/platformsdk:/system/lib64/chipset-sdk:/system/lib64/chipset-sdk-sp \
LD_PRELOAD=/data/local/tmp/asx/liblog_shim.so setsid ./appspawn-x >asx.out 2>asx.err </dev/null &
```
Result: **Phase 1 (security+socket) OK → Phase 2 ART VM OK → framework preload OK**
("Preloaded 30 classes", "Cached 11 adapter classes", "Preload complete in ~1470ms",
"Preload completed successfully") → **Phase 4 "Ready to accept spawn requests"**, listening
on /dev/unix/socket/AppSpawnX. The VM + 24Q4 framework run on art-15 arm64.

## ✅ Bridge loads in-process (the __h fix works) — resolving deps one layer at a time
The framework's preload calls `Runtime_nativeLoad path=liboh_adapter_bridge.so`. With the
__h toolchain fix, the bridge's OHOS symbols ALL resolve. Fixed in sequence:
1. `JNI_GetCreatedJavaVMs not found` → **added libart.so to the bridge's NEEDED** (libart
   defines it; ART's nativeLoad dlopen didn't put it in scope). FIXED.
2. Now: `android::EmptyAssetsProvider::Create not found` → the bridge needs the **AOSP
   native support libs** (androidfw/base/utils/binder), currently arm32-only.

## Remaining: build the AOSP native support stack for arm64
The bridge's *_aosp.cpp (AssetManager/ApkAssets/ResXML/Parcel) are JNI wrappers around
AOSP C++ libs. Undefined by lib:
- **libandroidfw** (AssetsProvider ×10, ApkAssets, ResXMLTree/Parser ×13, ResStringPool)
- **libbase** (×15), **libutils** (String8/16 ×17), **libbinder** (sp/IBinder/RpcSession ×16)
- (for render later: **libhwui + skia + libminikin/ft2/harfbuzz** — 11 skia syms already seen)
These are arm32 in /home/dspfac/bridge-build/out/aosp_lib/ (machine ARM). Build recipe:
`bridge-build/build/build_aosp_lib.sh` (targets libbase/libutils/libandroidfw/libhwui/...).
Adapt to arm64 (OHOS clang, --target=aarch64, like the bridge) → deploy → bridge loads fully.

## Read
The whole runtime chain WORKS on the board: appspawn-x → VM → framework preload → bridge
in-process load (OHOS+VM syms resolved). The ONLY remaining gap is the AOSP native support
libs (androidfw/base/utils/binder for assets; hwui/skia for render), which need an arm64
build (recipe exists). ART TOLERATES the bridge clinit failure so appspawn-x stays up.
Next: arm64 build of the aosp_lib stack → bridge fully loads → app reg + aa start → UI.

## AOSP support-stack build (2026-07-08): SOURCE-VERSION gap found
Started adapting build_aosp_lib.sh → build_aosp_lib_arm64.sh (OHOS clang + aarch64 +
6.1 SDK sysroot; bridge-build-arm64/build_aosp_lib_arm64.sh, bld fn works). BLOCKER:
- bridge-build/aosp is HEADERS-ONLY (no .cpp for libbase/libutils/androidfw — those
  built on ECS). It's a NEWER AOSP (android-12/13: AssetManager2::SelectedValue ×10,
  EmptyAssetsProvider, separate AssetsProvider.h).
- Local aosp-android-11 HAS the .cpp sources but is OLDER (SelectedValue ×0, headers
  differ) → building libandroidfw from aosp-11 gives a DIFFERENT ABI than the bridge
  (compiled vs bridge-build/aosp headers) expects → symbols still won't match.
- aosp-art-15 / aosp-libcore-15 have no androidfw/libbase/libutils sources.

**To unblock**: targeted sync of the MATCHING AOSP (~android-13) sources for just
system/libbase + system/core/libutils + system/core/libcutils + frameworks/base/libs/
androidfw + frameworks/native/libs/binder (+ later frameworks/base/libs/hwui + skia),
then build_aosp_lib_arm64.sh compiles them (toolchain proven). Dep chain:
bionic_compat→log→base→cutils→utils→ziparchive→androidfw; binder separately.

## Net (deployment phase)
The runtime chain WORKS to the AOSP-support-lib boundary: appspawn-x→VM→framework
preload→in-process bridge load (all OHOS+VM syms resolve, libart NEEDED). The final
gap is the AOSP native support libs, which need their matching (android-13-era)
sources synced before the arm64 build (recipe + toolchain ready). That source sync is
the next prerequisite.

## ✅ AOSP support stack BUILT for arm64 (2026-07-08) — android-15 sources
Synced android-15.0.0_r1 sources (targeted: system/libbase, system/core [libutils
split libutils/+libutils/binder/], system/logging, system/libziparchive, system/
incremental_delivery [incfs], frameworks/base/libs/androidfw, frameworks/native/libs/
binder; blobless sparse for the big repos). Pinned by diffing AssetsProvider.h
(identical to android-14/15). Built via build_aosp_lib_arm64.sh (OHOS clang + aarch64
+ 6.1 SDK sysroot + musl_compat for glibc globals + -DNDEBUG + gnu++20):
**liblog, libbase, libutils, libcutils, libziparchive, libandroidfw (30/30)** — ALL
build AND **load clean on the board** (dltest LOADED OK). libandroidfw exports
EmptyAssetsProvider::Create with the __h namespace = matches the bridge + board.

## Remaining: last-mile OHOS symbol→lib mapping (198 undefined)
With the 6 aosp libs + libart + 47 board libs NEEDED, the bridge still has ~198
undefined → musl dlopen SIGSEGVs (crash in dlopen_impl during the combined load).
Breakdown: ~29 skia (SkStream/SkCodec/SkData — stubbable via libbridge_compat), +
OHOS methods in libs not-yet-mapped or where the specific method isn't exported by
the obvious lib (e.g. WindowProperty::SetParentId — libwm has WindowProperty but not
that setter; MMI PointerEvent, MessageParcel, InputMethodController, AAFwk Want/
AbilitySchedulerStub, EventRunner). Next: finish the symbol→lib map (nm board libs,
add to NEEDED) + expand skia stub → bridge loads fully in-process → aa start → UI.

## Net
The AOSP support-lib blocker is SOLVED (built for arm64, load on board). The bridge
is one lib-mapping pass from loading. Everything else (appspawn-x, framework, __h ABI,
libart NEEDED) already works.

## ✅ Symbol mapping DONE — bridge links ZERO-UNDEFINED + LOADS standalone (2026-07-08)
Built a symbol→lib index by bulk-pulling the board's SDK lib dirs (platformsdk 761 +
chipset-sdk-sp 60 + chipset-sdk 47 + ndk 116) and nm-ing on host (263k syms). Mapped
all 198 undefined: **115 resolve to real board libs** (the earlier "unmapped" was a
host-analysis artifact — undefined `SYM@1.0` vs defined `SYM@@1.0`; strip the version
tag to match). Added libwmutil/libwmutil_base/libcrypto_openssl/libEGL to NEEDED (51
total). The rest = skia codecs (not on board) + OHOS typeinfo (hidden visibility) →
complete no-op stub libbridge_compat.so (skia+misc funcs as functions, _ZTI/_ZTV as
data). ★Stub MUST be compiled as C (clang -x c), not C++ — clang++ re-mangles the
_ZN.. names. Iterate the strict `--no-undefined -Wl,--no-demangle` link to get the
exact mangled unresolved set. Result: **bridge links with 0 undefined** and **dlopens
on the board (dltest LOADED OK)** after neutralizing the fragile InstallHiLogBridge
constructor (android::base::SetLogger at load, libbase state not ready). Binary:
out/liboh_adapter_bridge.so.LOADS.

## Remaining: bridge crashes in appspawn-x's ART Runtime_nativeLoad (not in dltest)
The bridge loads standalone but crashes when ART loads it in-process. It's a
constructor-in-ART-context issue (interposition with libart's bundled libbase, or a
static-init using the live JavaVM). The bridge has ~19 files with C++ global
constructors (adapter_bridge, oh_ability_manager_client, android_log_hilog_bridge,
oh_input_bridge, ...). Next: neutralize/guard the crashing constructor(s) — reproduce
by preloading libc++_shared+libart in dltest, or bisect the constructors.

## Net
Symbol mapping COMPLETE (0 undefined, loads standalone). The whole stack — ART,
appspawn-x, framework, AOSP support libs, 51 OHOS NEEDED — resolves. One
constructor-in-ART-context crash from the full in-process load → aa start → UI.

## Constructor bisection (2026-07-08): crash is a NEEDED-lib constructor
Used a bridge-level trace (a separate zz_ctor_trace.cpp with __attribute__((constructor(101)))
START + (65534) END, no existing-code changes). In appspawn-x's ART Runtime_nativeLoad:
**NEITHER trace prints** → the crash is in a NEEDED-lib constructor (which run bottom-up,
BEFORE the bridge's own constructors), not a bridge constructor (all bridge ctors run fine
in dltest — START+END+LOADED). ★Broad per-file instrumentation is unsafe (recompiling a
file can drop a method → new undefined → musl chokes); use the single separate trace file.

Tested the interposition theory: libart EXPORTS 90 android::base + __android_log_print +
SetLogger, and my libbase.so/liblog.so also define them (with constructors) → suspected
duplicate-libbase interposition. FIX ATTEMPT (drop my libbase/liblog, resolve from libart):
did NOT fix the appspawn-x crash AND broke the standalone load (dltest has no libart) →
REVERTED. So the crashing NEEDED-lib constructor is likely one of libutils/libcutils/
libziparchive/libandroidfw (my android-15 libs) calling libart's slightly-different bundled
libbase, OR an OHOS board lib's ctor behaving differently in the non-standard appspawn-x
process. Not yet pinned.

Next: per-NEEDED-lib bisection in the ART context (drop libs one at a time / stub their
ctors), or load the bridge in an ISOLATED linker namespace (RTLD_LOCAL / android namespace)
so my libs don't interpose with libart's bundled libbase. The appspawn-x code already notes
a "musl dlopen SEGV workaround" (preloadSharedLibraries skipped) — same class of issue.

## Net (this session)
Bridge LOADS standalone (dltest LOADED OK, 0-undefined, out/liboh_adapter_bridge.so.LOADS).
appspawn-x in-process load crashes in a NEEDED-lib constructor (ART-context specific,
isolated via trace bisection). One deeper debugging pass (per-lib ctor bisection or
namespace isolation) from the in-process load → aa start → UI.

## Namespace-isolation attempt + refined diagnosis (2026-07-08)
Tried namespace isolation via STATIC-MERGE: linked the 90 aosp .o (libbase/liblog/
libutils/libcutils/libziparchive/libandroidfw) directly INTO the bridge + -Bsymbolic
(so its internal libbase binds locally, not to libart's exports) + excluded the aosp
.so from NEEDED. Bridge is now self-contained (3.14MB, out/*.MERGED), loads standalone
(dltest LOADED OK). **Did NOT fix the appspawn-x crash.**

★DEFINITIVE crash-phase bisection (file-based trace — fd 2 is redirected by ART's
logger so use open()/write() to /data/local/tmp/asx/btrace.log):
- dltest: "[TRACE] ctor START/END" written + LOADED OK.
- appspawn-x: **btrace.log EMPTY** → crash is BEFORE any bridge constructor (the
  priority-101 START ctor never runs) = during RELOCATION or a NEEDED OHOS-board-lib
  constructor. JNI_OnLoad also never reached (it's after dlopen).
- Also found + arch-guarded (skia stubbed) the JNI_OnLoad `RegisterAllSkiaCodecs()`
  call — a real latent crash (stubs return garbage) for when JNI_OnLoad IS reached.

★ROOT DIFFERENCE dltest vs appspawn-x: both load libart (bridge NEEDs it), but
appspawn-x loads libart FIRST + creates a LIVE VM before loading the bridge; dltest
loads libart fresh (no VM). So a NEEDED board-lib ctor (or a bridge relocation)
behaves differently when libart is pre-loaded with a running VM. The appspawn-x code
already notes "preloadSharedLibraries skipped (musl dlopen SEGV workaround)" = same
class. Static-merge doesn't help because the crashing ctor is an OHOS BOARD lib (libwm/
libmmi/librender/libace...), not my aosp libs (now internal).

Next: capture the real backtrace (crash-log tooling is unreliable — faultlog protected,
stale crashes; need a debugger/ptrace or faultloggerd hook), then per-board-lib bisect,
OR change WHEN board libs load (before VM creation), OR the ART LoadNativeLibrary
namespace path on OHOS.

## 🎯 RESOLVED: bridge LOADS in-process (2026-07-08)
"Get a real backtrace" → used the WORKING appspawn-x backup (out/appspawn-x-full.WORKING,
has -Xverify:none + /data/local/tmp/asx/fw classpath) instead of my session rebuilds
(which were BROKEN: src-tree main.cpp hardcodes /system/android/framework = empty boot
classpath → VM -1; and lacks the -Xverify:none fix → Runtime.initLibPaths VerifyError).
Those broke the VM BEFORE the bridge load — ALL my "board-lib ctor crash" investigations
were confounded by them.

With the WORKING appspawn-x + the MERGED bridge: **the bridge loads, framework preload
completes, appspawn-x reaches "Ready to accept spawn requests" + Entering event loop,
ALIVE.** asx.err: Runtime_nativeLoad(bridge) → moves past to Runtime_nativeLoad(libmedia_
jni) → Preload completed successfully → [B35.A] RegisterNatives OK. ZERO bridge clinit
failure (OHEnvironment loads clean), zero relocation errors. Reproducible.

Fix = MERGED bridge (90 aosp .o statically linked in + -Bsymbolic → no libbase/liblog
interposition with libart) + skia-codec-skip (JNI_OnLoad's RegisterAllSkiaCodecs hit
stubbed skia → arch-guarded off). Working combo: out/appspawn-x-full.WORKING +
out/liboh_adapter_bridge.so.INPROC_LOADS.

NEXT: aa start / spawn request to /dev/unix/socket/AppSpawnX → fork app → UI.
