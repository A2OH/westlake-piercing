// ============================================================================
// android_input_InputEventLabels_aosp.cpp
//
// 2026-05-18 (Plan A): adapter-private stub for AOSP
// frameworks/native/libs/input/InputEventLabels.cpp.
//
// All 8 InputEventLookup APIs return empty/null.  HelloWorld single-tap does
// not exercise label lookup (it is used by getevent / dumpsys / debug paths).
// Replace with real tables when expanding to keyboard / event-injection
// support; the header signature (include/input/InputEventLabels.h) stays
// unchanged.
//
// WHY ADAPTER-PRIVATE: see header file comment.  Compiling AOSP's
// InputEventLabels.cpp directly introduced ABI mismatch via transitive
// <input/Input.h> → <ui/Transform.h> → SIGBUS in libicu_jni.so.
// ============================================================================

#include "input/InputEventLabels.h"

namespace android {

std::optional<int> InputEventLookup::lookupValueByLabel(
        const std::unordered_map<std::string, int>& /*map*/, const char* /*literal*/) {
    return std::nullopt;
}

const char* InputEventLookup::lookupLabelByValue(
        const std::vector<InputEventLabel>& /*vec*/, int /*value*/) {
    return nullptr;
}

std::optional<int> InputEventLookup::getKeyCodeByLabel(const char* /*label*/) {
    return std::nullopt;
}

const char* InputEventLookup::getLabelByKeyCode(int32_t /*keyCode*/) {
    return nullptr;
}

std::optional<int> InputEventLookup::getKeyFlagByLabel(const char* /*label*/) {
    return std::nullopt;
}

std::optional<int> InputEventLookup::getAxisByLabel(const char* /*label*/) {
    return std::nullopt;
}

const char* InputEventLookup::getAxisLabel(int32_t /*axisId*/) {
    return nullptr;
}

std::optional<int> InputEventLookup::getLedByLabel(const char* /*label*/) {
    return std::nullopt;
}

EvdevEventLabel InputEventLookup::getLinuxEvdevLabel(
        int32_t /*type*/, int32_t /*code*/, int32_t /*value*/) {
    return EvdevEventLabel{};
}

}  // namespace android
