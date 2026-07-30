// ============================================================================
// input/Input.h — adapter shadow of AOSP frameworks/native/include/input/Input.h
//
// 2026-05-18 (Plan A): adapter-private header.  Defines minimum MotionEvent /
// KeyEvent / PointerCoords / PointerProperties surface that AOSP-style JNI
// bindings (android_view_MotionEvent_aosp.cpp) need, with class layout
// completely under adapter control.
//
// WHY NOT include AOSP <input/Input.h>: it transitively pulls
// <ui/Transform.h>, <utils/BitSet.h>, <android/os/IInputConstants.h>,
// <utils/Timers.h>.  Those headers define class layouts (ui::Transform with
// 3 std::array<float,9> + ui::Rotation enum, BitSet64 with helper methods,
// etc.) that conflict with the OH device-side equivalents and produce ABI
// drift (SIGBUS in libicu_jni.so:ScopedCharArrayRO observed 2026-05-18).
//
// PHASE 1 SCOPE: HelloWorld single-tap.  multi-touch / pointer pool /
// historical samples / transform composition are stubbed but the API surface
// matches AOSP MotionEvent.cpp's call sites so Phase 2 can grow without
// changing the JNI binding.
//
// PHASE 2 EXTENSIONS:
//   - add real history samples (mSampleEventTimes / mSamplePointerCoords)
//   - implement real Transform matrix composition (replace simple xOffset/
//     yOffset with std::array<float,9> stored matrix)
//   - implement real Parcel read/write (writeToParcel / readFromParcel are
//     no-ops now)
// ============================================================================
#pragma once

#include <android/input.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

namespace android {

typedef int64_t nsecs_t;

inline constexpr int32_t HISTORY_CURRENT = -0x80000000;
inline constexpr std::array<uint8_t, 32> INVALID_HMAC = {{0}};

enum class InputEventType : int32_t {
    KEY = 1,
    MOTION = 2,
    FOCUS = 3,
    CAPTURE = 4,
    DRAG = 5,
    TOUCH_MODE = 6,
};

enum class ToolType : int32_t {
    UNKNOWN = AMOTION_EVENT_TOOL_TYPE_UNKNOWN,
    FINGER = AMOTION_EVENT_TOOL_TYPE_FINGER,
    STYLUS = AMOTION_EVENT_TOOL_TYPE_STYLUS,
    MOUSE = AMOTION_EVENT_TOOL_TYPE_MOUSE,
    ERASER = AMOTION_EVENT_TOOL_TYPE_ERASER,
    PALM = AMOTION_EVENT_TOOL_TYPE_PALM,
};

enum class MotionClassification : uint8_t {
    NONE = 0,
    AMBIGUOUS_GESTURE = 1,
    DEEP_PRESS = 2,
    TWO_FINGER_SWIPE = 3,
    MULTI_FINGER_SWIPE = 4,
    PINCH = 5,
};

// PointerCoords — adapter layout.  Uses a fixed-size axis-value array indexed
// by AMOTION_EVENT_AXIS_* (X=0, Y=1, PRESSURE=2, SIZE=3, TOUCH_MAJOR=4,
// TOUCH_MINOR=5, TOOL_MAJOR=6, TOOL_MINOR=7, ORIENTATION=8).  Axes above 31
// are not tracked in Phase 1.
struct PointerCoords {
    uint64_t bits;            // bit i set iff values[i] is meaningful
    float values[32];
    bool isResampled;

    void clear();
    void setAxisValue(int32_t axis, float value);
    float getAxisValue(int32_t axis) const;
};

struct PointerProperties {
    int32_t id;
    ToolType toolType;

    void clear();
    bool operator==(const PointerProperties& other) const {
        return id == other.id && toolType == other.toolType;
    }
};

class InputEvent {
public:
    virtual ~InputEvent() = default;
    virtual InputEventType getType() const = 0;

    int32_t getId() const { return mId; }
    int32_t getDeviceId() const { return mDeviceId; }
    int32_t getSource() const { return mSource; }
    void setSource(int32_t source) { mSource = source; }
    int32_t getDisplayId() const { return mDisplayId; }
    void setDisplayId(int32_t displayId) { mDisplayId = displayId; }

    // Monotonic id allocator (per-process).  Phase 1: simple atomic counter.
    static int32_t nextId();

protected:
    int32_t mId;
    int32_t mDeviceId;
    int32_t mSource;
    int32_t mDisplayId;
    std::array<uint8_t, 32> mHmac;
};

class KeyEvent : public InputEvent {
public:
    virtual ~KeyEvent() override = default;
    virtual InputEventType getType() const override { return InputEventType::KEY; }

    int32_t getAction() const { return mAction; }
    int32_t getKeyCode() const { return mKeyCode; }

    // Phase 1: KeyEvent is declared but not used by HelloWorld JNI;
    // declaration kept so InputEvent dispatch table can reference it.
private:
    int32_t mAction = 0;
    int32_t mKeyCode = 0;
};

class MotionEvent : public InputEvent {
public:
    virtual ~MotionEvent() override = default;
    virtual InputEventType getType() const override { return InputEventType::MOTION; }

    // Mirror AOSP MotionEvent::initialize signature minus ui::Transform args
    // (we use plain xOffset/yOffset).  android_view_MotionEvent_aosp.cpp's
    // nativeInitialize translates AOSP's call to this signature.
    void initialize(int32_t id, int32_t deviceId, int32_t source, int32_t displayId,
                    const std::array<uint8_t, 32>& hmac, int32_t action, int32_t actionButton,
                    int32_t flags, int32_t edgeFlags, int32_t metaState, int32_t buttonState,
                    MotionClassification classification, float xOffset, float yOffset,
                    float xPrecision, float yPrecision, float rawXCursorPosition,
                    float rawYCursorPosition, int64_t downTime, int64_t eventTime,
                    size_t pointerCount, const PointerProperties* pointerProperties,
                    const PointerCoords* pointerCoords);

    void addSample(int64_t eventTime, const PointerCoords* pointerCoords);
    void copyFrom(const MotionEvent* other, bool keepHistory);

    int32_t getAction() const { return mAction; }
    void setAction(int32_t action) { mAction = action; }
    int32_t getActionButton() const { return mActionButton; }
    void setActionButton(int32_t b) { mActionButton = b; }
    int32_t getFlags() const { return mFlags; }
    void setFlags(int32_t f) { mFlags = f; }
    int32_t getEdgeFlags() const { return mEdgeFlags; }
    void setEdgeFlags(int32_t e) { mEdgeFlags = e; }
    int32_t getMetaState() const { return mMetaState; }
    void setMetaState(int32_t m) { mMetaState = m; }
    int32_t getButtonState() const { return mButtonState; }
    void setButtonState(int32_t b) { mButtonState = b; }
    MotionClassification getClassification() const { return mClassification; }

    float getXOffset() const { return mXOffset; }
    float getYOffset() const { return mYOffset; }
    float getXPrecision() const { return mXPrecision; }
    float getYPrecision() const { return mYPrecision; }
    float getXCursorPosition() const { return mXCursorPosition; }
    float getYCursorPosition() const { return mYCursorPosition; }
    void setCursorPosition(float x, float y) { mXCursorPosition = x; mYCursorPosition = y; }

    int64_t getDownTime() const { return mDownTime; }
    void setDownTime(int64_t t) { mDownTime = t; }
    int64_t getEventTime() const;
    int64_t getHistoricalEventTime(size_t historicalIndex) const;

    size_t getPointerCount() const { return mPointerProperties.size(); }
    const PointerProperties* getPointerProperties(size_t pointerIndex) const;
    int32_t getPointerId(size_t pointerIndex) const;
    ToolType getToolType(size_t pointerIndex) const;
    ssize_t findPointerIndex(int32_t pointerId) const;
    size_t getHistorySize() const;
    bool isResampled(size_t pointerIndex, size_t historicalIndex) const;

    bool isTouchEvent() const;

    float getRawAxisValue(int32_t axis, size_t pointerIndex) const;
    float getAxisValue(int32_t axis, size_t pointerIndex) const;
    float getHistoricalRawAxisValue(int32_t axis, size_t pointerIndex, size_t historicalIndex) const;
    float getHistoricalAxisValue(int32_t axis, size_t pointerIndex, size_t historicalIndex) const;

    const PointerCoords* getRawPointerCoords(size_t pointerIndex) const;
    const PointerCoords* getHistoricalRawPointerCoords(size_t pointerIndex,
                                                       size_t historicalIndex) const;

    void offsetLocation(float xOffset, float yOffset);
    void scale(float scaleFactor);
    void transform(const std::array<float, 9>& matrix);
    void applyTransform(const std::array<float, 9>& matrix);

    // Phase 1: returns -1 ("unknown rotation").  Phase 2: derive from
    // mTransform when real transform composition is wired.
    int32_t getSurfaceRotation() const { return -1; }

    // Phase 1: stub — adapter does not serialize MotionEvent through Parcel.
    // Phase 2: implement when cross-process input dispatch is added.
    int32_t readFromParcel(void* parcel);
    int32_t writeToParcel(void* parcel) const;

    static const char* getLabel(int32_t axis);
    static std::optional<int> getAxisFromLabel(const char* label);

private:
    int32_t mAction = 0;
    int32_t mActionButton = 0;
    int32_t mFlags = 0;
    int32_t mEdgeFlags = 0;
    int32_t mMetaState = 0;
    int32_t mButtonState = 0;
    MotionClassification mClassification = MotionClassification::NONE;
    float mXOffset = 0.f;
    float mYOffset = 0.f;
    float mXPrecision = 1.f;
    float mYPrecision = 1.f;
    float mXCursorPosition;  // initialized to NaN in initialize()
    float mYCursorPosition;
    int64_t mDownTime = 0;

    std::vector<PointerProperties> mPointerProperties;
    std::vector<int64_t> mSampleEventTimes;
    std::vector<PointerCoords> mSamplePointerCoords;
};

}  // namespace android
