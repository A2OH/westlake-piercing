// ============================================================================
// android_input_Input_aosp.cpp
//
// 2026-05-18 (Plan A): adapter-private implementation of MotionEvent /
// KeyEvent / PointerCoords / PointerProperties classes.
//
// Mirrors AOSP frameworks/native/libs/input/Input.cpp's surface but with
// class layout under adapter control (see include/input/Input.h header for
// rationale).  Phase 1 implements what HelloWorld single-tap needs:
//   - MotionEvent::initialize / addSample / copyFrom
//   - axis-value getters (raw vs transformed; transform is identity in
//     Phase 1, so getRawAxisValue == getAxisValue)
//   - offsetLocation / scale (simple in-place mutation)
//   - transform / applyTransform (no-op in Phase 1; Phase 2 adds real matrix
//     composition)
//
// Out of scope (Phase 2):
//   - real history-sample retrieval (samples vector currently keeps current +
//     historical but only "current" path is actively exercised)
//   - Parcel serialization (readFromParcel/writeToParcel are stubs)
//   - axis-label lookup (delegated to InputEventLookup which itself is a stub
//     in this phase)
// ============================================================================

#include "input/Input.h"

#include <atomic>
#include <cmath>
#include <cstring>

namespace android {

// ---------------- PointerCoords ----------------

#if !defined(__aarch64__)
void PointerCoords::clear() {
    bits = 0;
    for (float& v : values) v = 0.f;
    isResampled = false;
}
#endif

void PointerCoords::setAxisValue(int32_t axis, float value) {
    if (axis < 0 || axis >= 32) return;
    values[axis] = value;
    bits |= (uint64_t(1) << axis);
}

float PointerCoords::getAxisValue(int32_t axis) const {
    if (axis < 0 || axis >= 32) return 0.f;
    if (!(bits & (uint64_t(1) << axis))) return 0.f;
    return values[axis];
}

// ---------------- PointerProperties ----------------

#if !defined(__aarch64__)
void PointerProperties::clear() {
    id = -1;
    toolType = ToolType::UNKNOWN;
}
#endif

// ---------------- InputEvent ----------------

int32_t InputEvent::nextId() {
    static std::atomic<int32_t> counter{1};
    return counter.fetch_add(1, std::memory_order_relaxed);
}

// ---------------- MotionEvent ----------------

void MotionEvent::initialize(int32_t id, int32_t deviceId, int32_t source, int32_t displayId,
                             const std::array<uint8_t, 32>& hmac, int32_t action,
                             int32_t actionButton, int32_t flags, int32_t edgeFlags,
                             int32_t metaState, int32_t buttonState,
                             MotionClassification classification, float xOffset, float yOffset,
                             float xPrecision, float yPrecision, float rawXCursorPosition,
                             float rawYCursorPosition, int64_t downTime, int64_t eventTime,
                             size_t pointerCount, const PointerProperties* pointerProperties,
                             const PointerCoords* pointerCoords) {
    mId = id;
    mDeviceId = deviceId;
    mSource = source;
    mDisplayId = displayId;
    mHmac = hmac;
    mAction = action;
    mActionButton = actionButton;
    mFlags = flags;
    mEdgeFlags = edgeFlags;
    mMetaState = metaState;
    mButtonState = buttonState;
    mClassification = classification;
    mXOffset = xOffset;
    mYOffset = yOffset;
    mXPrecision = xPrecision;
    mYPrecision = yPrecision;
    mXCursorPosition = rawXCursorPosition;
    mYCursorPosition = rawYCursorPosition;
    mDownTime = downTime;

    mPointerProperties.clear();
    mPointerProperties.reserve(pointerCount);
    for (size_t i = 0; i < pointerCount; ++i) {
        mPointerProperties.push_back(pointerProperties[i]);
    }

    mSampleEventTimes.clear();
    mSamplePointerCoords.clear();
    addSample(eventTime, pointerCoords);
}

void MotionEvent::addSample(int64_t eventTime, const PointerCoords* pointerCoords) {
    mSampleEventTimes.push_back(eventTime);
    size_t n = mPointerProperties.size();
    for (size_t i = 0; i < n; ++i) {
        mSamplePointerCoords.push_back(pointerCoords[i]);
    }
}

void MotionEvent::copyFrom(const MotionEvent* other, bool keepHistory) {
    mId = other->mId;
    mDeviceId = other->mDeviceId;
    mSource = other->mSource;
    mDisplayId = other->mDisplayId;
    mHmac = other->mHmac;
    mAction = other->mAction;
    mActionButton = other->mActionButton;
    mFlags = other->mFlags;
    mEdgeFlags = other->mEdgeFlags;
    mMetaState = other->mMetaState;
    mButtonState = other->mButtonState;
    mClassification = other->mClassification;
    mXOffset = other->mXOffset;
    mYOffset = other->mYOffset;
    mXPrecision = other->mXPrecision;
    mYPrecision = other->mYPrecision;
    mXCursorPosition = other->mXCursorPosition;
    mYCursorPosition = other->mYCursorPosition;
    mDownTime = other->mDownTime;
    mPointerProperties = other->mPointerProperties;

    if (keepHistory) {
        mSampleEventTimes = other->mSampleEventTimes;
        mSamplePointerCoords = other->mSamplePointerCoords;
    } else {
        mSampleEventTimes.clear();
        mSamplePointerCoords.clear();
        size_t n = mPointerProperties.size();
        if (!other->mSampleEventTimes.empty()) {
            mSampleEventTimes.push_back(other->mSampleEventTimes.back());
            size_t base = other->mSamplePointerCoords.size() - n;
            for (size_t i = 0; i < n; ++i) {
                mSamplePointerCoords.push_back(other->mSamplePointerCoords[base + i]);
            }
        }
    }
}

int64_t MotionEvent::getEventTime() const {
    return mSampleEventTimes.empty() ? 0 : mSampleEventTimes.back();
}

int64_t MotionEvent::getHistoricalEventTime(size_t historicalIndex) const {
    if (historicalIndex >= mSampleEventTimes.size()) return 0;
    return mSampleEventTimes[historicalIndex];
}

const PointerProperties* MotionEvent::getPointerProperties(size_t pointerIndex) const {
    if (pointerIndex >= mPointerProperties.size()) return nullptr;
    return &mPointerProperties[pointerIndex];
}

int32_t MotionEvent::getPointerId(size_t pointerIndex) const {
    if (pointerIndex >= mPointerProperties.size()) return -1;
    return mPointerProperties[pointerIndex].id;
}

ToolType MotionEvent::getToolType(size_t pointerIndex) const {
    if (pointerIndex >= mPointerProperties.size()) return ToolType::UNKNOWN;
    return mPointerProperties[pointerIndex].toolType;
}

ssize_t MotionEvent::findPointerIndex(int32_t pointerId) const {
    for (size_t i = 0; i < mPointerProperties.size(); ++i) {
        if (mPointerProperties[i].id == pointerId) return static_cast<ssize_t>(i);
    }
    return -1;
}

size_t MotionEvent::getHistorySize() const {
    // History size = total samples - 1 (last sample is "current").
    return mSampleEventTimes.empty() ? 0 : mSampleEventTimes.size() - 1;
}

bool MotionEvent::isResampled(size_t pointerIndex, size_t historicalIndex) const {
    size_t n = mPointerProperties.size();
    if (n == 0) return false;
    size_t base = historicalIndex * n + pointerIndex;
    if (base >= mSamplePointerCoords.size()) return false;
    return mSamplePointerCoords[base].isResampled;
}

bool MotionEvent::isTouchEvent() const {
    return (mSource & AINPUT_SOURCE_CLASS_POINTER) != 0;
}

// Phase 1: raw == transformed (identity transform).  getXOffset/getYOffset
// are applied to X/Y axes to mirror AOSP semantics.
float MotionEvent::getRawAxisValue(int32_t axis, size_t pointerIndex) const {
    size_t historyIdx = mSampleEventTimes.empty() ? 0 : mSampleEventTimes.size() - 1;
    return getHistoricalRawAxisValue(axis, pointerIndex, historyIdx);
}

float MotionEvent::getAxisValue(int32_t axis, size_t pointerIndex) const {
    float v = getRawAxisValue(axis, pointerIndex);
    if (axis == AMOTION_EVENT_AXIS_X) v += mXOffset;
    if (axis == AMOTION_EVENT_AXIS_Y) v += mYOffset;
    return v;
}

float MotionEvent::getHistoricalRawAxisValue(int32_t axis, size_t pointerIndex,
                                             size_t historicalIndex) const {
    size_t n = mPointerProperties.size();
    if (n == 0) return 0.f;
    size_t base = historicalIndex * n + pointerIndex;
    if (base >= mSamplePointerCoords.size()) return 0.f;
    return mSamplePointerCoords[base].getAxisValue(axis);
}

float MotionEvent::getHistoricalAxisValue(int32_t axis, size_t pointerIndex,
                                          size_t historicalIndex) const {
    float v = getHistoricalRawAxisValue(axis, pointerIndex, historicalIndex);
    if (axis == AMOTION_EVENT_AXIS_X) v += mXOffset;
    if (axis == AMOTION_EVENT_AXIS_Y) v += mYOffset;
    return v;
}

const PointerCoords* MotionEvent::getRawPointerCoords(size_t pointerIndex) const {
    size_t historyIdx = mSampleEventTimes.empty() ? 0 : mSampleEventTimes.size() - 1;
    return getHistoricalRawPointerCoords(pointerIndex, historyIdx);
}

const PointerCoords* MotionEvent::getHistoricalRawPointerCoords(size_t pointerIndex,
                                                                size_t historicalIndex) const {
    size_t n = mPointerProperties.size();
    if (n == 0) return nullptr;
    size_t base = historicalIndex * n + pointerIndex;
    if (base >= mSamplePointerCoords.size()) return nullptr;
    return &mSamplePointerCoords[base];
}

void MotionEvent::offsetLocation(float xOffset, float yOffset) {
    mXOffset += xOffset;
    mYOffset += yOffset;
}

void MotionEvent::scale(float scaleFactor) {
    mXOffset *= scaleFactor;
    mYOffset *= scaleFactor;
    for (PointerCoords& pc : mSamplePointerCoords) {
        for (int axis = 0; axis < 32; ++axis) {
            if (pc.bits & (uint64_t(1) << axis)) {
                pc.values[axis] *= scaleFactor;
            }
        }
    }
}

// Phase 1: transform / applyTransform are no-ops.  HelloWorld in landscape /
// portrait switch + window transforms are not in scope.  Phase 2: real
// matrix composition.
void MotionEvent::transform(const std::array<float, 9>& /*matrix*/) {}
void MotionEvent::applyTransform(const std::array<float, 9>& /*matrix*/) {}

int32_t MotionEvent::readFromParcel(void* /*parcel*/) {
    // Phase 1: cross-process input dispatch is not used; HelloWorld receives
    // events via in-process InputEventReceiver worker socketpair, not Parcel.
    return -1;
}

int32_t MotionEvent::writeToParcel(void* /*parcel*/) const { return -1; }

const char* MotionEvent::getLabel(int32_t /*axis*/) {
    // Phase 1 stub.  See InputEventLookup::getAxisLabel for the canonical
    // table-driven impl when extended.
    return nullptr;
}

std::optional<int> MotionEvent::getAxisFromLabel(const char* /*label*/) {
    return std::nullopt;
}

}  // namespace android
