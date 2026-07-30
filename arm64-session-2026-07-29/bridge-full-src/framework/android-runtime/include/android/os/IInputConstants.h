// ============================================================================
// IInputConstants.h — adapter stub of AOSP AIDL-generated header.
//
// 2026-05-18 (L2 / Input_Adapter_design §3.3.5, memory
// feedback_phase1_must_be_foundation):
//
// AOSP `~/aosp/frameworks/native/libs/input/android/os/IInputConstants.aidl`
// generates (via aidl --lang=cpp) a header that DROPS libbinder dependencies:
//
//     #include <binder/IBinder.h>
//     #include <binder/IInterface.h>
//     #include <binder/Status.h>
//
// libbinder cannot run on OH (OH binder kernel driver protocol is binary-
// incompatible with Android binder), so the AIDL-generated artifact's
// IInterface scaffolding is unusable.  The .aidl source itself only declares
// `const int32_t` constants — pure values, no methods.  We provide an
// adapter stub exposing the SAME constants in the SAME namespace with the
// SAME values, but without the IInterface inheritance.
//
// adapter INC_AOSP path order places this header BEFORE the aidl-gen tree,
// so this stub shadows the libbinder-dependent variant for our compile.
//
// Values mirror frameworks/native/libs/input/android/os/IInputConstants.aidl
// (verified 2026-05-18).  If AOSP rev bumps any constant, the AIDL gen step
// in build/compile_oh_android_runtime.sh will produce a diff against this
// stub — a check we should add as a future build assertion (TODO Phase 3).
// ============================================================================
#pragma once

#include <cstdint>

namespace android {
namespace os {

class IInputConstants {
public:
    enum : int32_t {
        UNMULTIPLIED_DEFAULT_DISPATCHING_TIMEOUT_MILLIS = 5000,
        INVALID_BATTERY_CAPACITY                        = -1,
        INVALID_INPUT_EVENT_ID                          = 0,
        INVALID_INPUT_DEVICE_ID                         = -2,
        POLICY_FLAG_INJECTED_FROM_ACCESSIBILITY         = 0x20000,
        INPUT_EVENT_FLAG_IS_ACCESSIBILITY_EVENT         = 0x800,
        DEFAULT_POINTER_ACCELERATION                    = 3,
    };
};

}  // namespace os
}  // namespace android
