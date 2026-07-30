// ============================================================================
// input/InputEventLabels.h — adapter shadow of AOSP
// frameworks/native/include/input/InputEventLabels.h
//
// 2026-05-18 (Plan A): adapter-private header.  Mirrors AOSP InputEventLookup
// class interface so AOSP MotionEvent.cpp (when ported as *_aosp.cpp) can
// link against the same names.  Implementations live in
// android_input_InputEventLabels_aosp.cpp as stubs (return nullopt/nullptr) —
// HelloWorld single-tap does not exercise label-table lookup; expanding to
// keyboard support means replacing the stub .cpp with real tables but keeping
// this header unchanged.
//
// WHY NOT include AOSP's header: AOSP's <input/InputEventLabels.h>
// transitively pulls <input/Input.h> which pulls <ui/Transform.h>,
// <utils/BitSet.h>, etc.  Those headers define class layouts that conflict
// with the OH device-side equivalents and produce ABI drift (SIGBUS in
// libicu_jni.so:ScopedCharArrayRO observed 2026-05-18 with state E2).  This
// shadow keeps the surface adapter controls completely.
// ============================================================================
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace android {

struct InputEventLabel {
    const char* literal;
    int value;
};

struct EvdevEventLabel {
    std::string type;
    std::string code;
    std::string value;
};

class InputEventLookup {
public:
    static std::optional<int> lookupValueByLabel(
            const std::unordered_map<std::string, int>& map, const char* literal);
    static const char* lookupLabelByValue(
            const std::vector<InputEventLabel>& vec, int value);

    static std::optional<int> getKeyCodeByLabel(const char* label);
    static const char* getLabelByKeyCode(int32_t keyCode);

    static std::optional<int> getKeyFlagByLabel(const char* label);

    static std::optional<int> getAxisByLabel(const char* label);
    static const char* getAxisLabel(int32_t axisId);

    static std::optional<int> getLedByLabel(const char* label);

    static EvdevEventLabel getLinuxEvdevLabel(int32_t type, int32_t code, int32_t value);
};

}  // namespace android
