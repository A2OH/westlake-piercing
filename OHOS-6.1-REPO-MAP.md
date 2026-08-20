# OHOS 6.1.0.31 source repos needed to build the adapter (appspawn-x + bridge)

Board = OpenHarmony **6.1.0.31** (API 23 Release). Sync source at tag
`OpenHarmony-6.1-Release` (`repo init -u https://gitcode.com/openharmony/manifest
-b OpenHarmony-6.1-Release`). The bridge/appspawn-x need **inner-API headers** from
the source tree (the public SDK only carries NDK/ArkTS headers). Mapping below is
derived from the arm32 adapter's actual `#include`s and its 38 NEEDED `.so` libs.

## Targeted repo set (≈24 repos vs the full ~1000-repo manifest)

### Core IPC / SA / utils / logging (foundation everything depends on)
| repo path | provides (headers / libs) |
|---|---|
| `foundation/communication/ipc` | ipc_skeleton.h, iremote_object.h, iremote_broker.h, iremote_stub.h, ipc_types.h → libipc_single.z.so |
| `foundation/systemabilitymgr/samgr` | iservice_registry.h, system_ability_manager_proxy.h → libsamgr_proxy.z.so |
| `commonlibrary/c_utils` | refbase.h, utils/misc.h → libutils.z.so |
| `base/hiviewdfx/hilog` | hilog/log.h, hilog/log_c.h → libhilog.so |
| `base/startup/init` | begetutil → libbegetutil.z.so |

### Ability / bundle / app-mgr (app launch + Want)
| repo | provides |
|---|---|
| `foundation/ability/ability_runtime` | ability_manager_interface.h, ability_manager_proxy.h, ability_connect_callback_*, app_scheduler → libability_manager, libapp_manager, libmission_info, libabilitykit_native, libability_connect_callback_stub |
| `foundation/ability/ability_base` | want.h, ability_info.h, configuration → libwant.z.so, libconfiguration.z.so |
| `foundation/bundlemanager/bundle_framework` | appexecfwk_base/core (bundle parse, the .app→.apk path) → libappexecfwk_base/core.z.so |

### Graphics / surface / window (the UI path)
| repo | provides |
|---|---|
| `foundation/graphic/graphic_surface` | surface.h, iconsumer_surface.h, external_window.h, surface/window.h, sync_fence → libsurface.z.so, libsync_fence.z.so |
| `foundation/graphic/graphic_2d` | ui/rs_surface_node.h, render_service, libnative_drawing_ndk, EGL → librender_service_client/base.z.so, libnative_drawing_ndk.z.so |
| `foundation/window/window_manager` | display_manager.h, display_info.h, window_manager, session/container/zidl/* → libwm.z.so, libdm.z.so, libscene_session, libwindow_scene_common |

### Input (touch/key routing → ViewRootImpl)
| repo | provides |
|---|---|
| `foundation/multimodalinput/input` | input_manager.h, input_device.h, i_input_event_consumer.h → libmmi-client.z.so |

### Audio / media (noice playback — MediaCodec→OH_AudioCodec, AudioTrack→OH_AudioRenderer)
| repo | provides |
|---|---|
| `foundation/multimedia/audio_framework` | OH_AudioRenderer / ohaudio → libohaudio + audio client |
| `foundation/multimedia/av_codec` | OH_AudioCodec / MediaCodec decode |
| `foundation/multimedia/player_framework` | media session / player |

### Security (token + selinux — the spawn DAC path)
| repo | provides |
|---|---|
| `base/security/access_token` | token_setproc → libtokensetproc_shared.z.so |
| `base/security/selinux_adapter` | hap_restorecon.h → libhap_restorecon.z.so, libselinux |

### Misc app services (events / clipboard / datashare — app runtime needs)
| repo | provides |
|---|---|
| `base/notification/common_event_service` | common_event_subscriber → libcesfwk_innerkits.z.so |
| `base/miscservices/pasteboard` | pasteboard → libpasteboard.so |
| `foundation/distributeddatamgr/udmf` | udmf, zuri → libudmf.so, libzuri.z.so |
| `foundation/distributeddatamgr/data_share` | datashare_consumer → libdatashare_consumer.z.so |

### Build infra (required to compile any of the above)
`build`, `build/common`, `third_party/*` (bounded subset: musl, libcxx, EGL, zlib,
selinux, googletest), `interface/sdk_c` (NDK headers), `arkcompiler/*` if dex tooling.

## Sync strategy
Full `repo sync -c` is ~100 GB / hours. To cut it: `repo init` the 6.1 manifest,
then `repo sync -c <project ...>` for ONLY the paths above (repo supports selective
project sync), or hand-edit a trimmed manifest. Headers-only is enough for compiling;
the matching aarch64 `.so`s are already on the board (`/system/lib64/{platformsdk,
chipset-sdk,...}`) to link against at runtime.

## Reality check
This unblocks *compiling* appspawn-x + the bridge against 6.1 ABIs. The bridge source
was written for the older-OHOS arm32 board, so 6.1 API drift (window session zidl,
render_service, MMI) will need per-call fixups — that's the porting work, now possible.

## ✅ SDK acquired + map validated (2026-07-08)
- **Public SDK downloaded + extracted**: `$WLROOT/ohos-sdk-6.1/linux/native/` —
  6.1.0.31 arm64 sysroot (`sysroot/usr/lib/aarch64-linux-ohos`: crt1.o, libc.so,
  libc++, **libohaudio.so**, libace_napi/ndk) + bundled **OHOS clang 15.0.4**. This
  is the correct 6.1 arm64 build toolchain (replaces the improvised sysroot).
- **NDK public headers cover part of the UI/audio bridge WITHOUT source**:
  `native_window`, `native_image`, `native_buffer`, `native_drawing`, `native_vsync`,
  `ohaudio`, EGL/GLES2/3, multimedia. So the graphics/audio shims that already target
  the NDK boundary (oh_anativewindow_shim, oh_audiotrack_shim, ohaudio) build against
  the SDK alone. Only the DEEP inner-APIs (ability_manager, ipc_skeleton, surface
  inner, window session zidl) need a source sync.
- **manifest_tag.xml pins 500 repos at exact 6.1.0.31 revisions** — a precise
  board-matching sparse sync is possible (not a floating branch). ALL mapped repos
  confirmed present at expected paths (ability_runtime, graphic_surface,
  window_manager, multimodalinput, audio_framework, communication_ipc, ...).

### Build-toolchain env for 6.1 arm64 (use the SDK sysroot)
```
NDK=$WLROOT/ohos-sdk-6.1/linux/native
CLANG=$NDK/llvm/bin/clang++   # OHOS clang 15.0.4
SYSROOT=$NDK/sysroot
# --target=aarch64-linux-ohos --sysroot=$SYSROOT
```
### Next: sparse source sync of the ~24 inner-API repos (from the pinned manifest),
then rebuild appspawn-x + bridge for aarch64 against 6.1 headers.

## ✅ Sparse sync KICKED OFF (2026-07-08) — $WLROOT/ohos-6.1-src/
Working recipe (repo init succeeded → 502-project 6.1 manifest; selective sync of
the 21 validated inner-API repos, NO third_party source needed — the SDK sysroot
provides libc/libc++ headers):
```
cd $WLROOT/ohos-6.1-src
repo init -u https://gitcode.com/openharmony/manifest -b OpenHarmony-6.1-Release --no-repo-verify
repo sync -c -j6 --no-clone-bundle $(cat valid_projects.txt)   # 21 repos, headers
```
★Gotchas: don't use `--fail-fast` with hand-guessed paths (one bad path aborts all);
validate paths against `.repo/manifests/ohos/ohos.xml` (projects are in the INCLUDED
sub-manifest, not top-level default.xml). All 21 targets confirmed present.
Running in background; when done → build appspawn-x aarch64 vs 6.1 headers +
`$NDK/sysroot` (NDK=$WLROOT/ohos-sdk-6.1/linux/native).
