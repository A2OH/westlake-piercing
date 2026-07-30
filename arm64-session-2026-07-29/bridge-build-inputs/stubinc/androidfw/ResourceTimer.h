#ifndef RESOURCETIMER_STUB_H
#define RESOURCETIMER_STUB_H
// WESTLAKE (2026-07-11): no-op ResourceTimer stub (avoids linking the real timer impl),
// but with the real aosp-15 Counter enum values + a scoped-timer ctor so
// android_util_AssetManager_aosp.cpp (ResourceTimer _timer(Counter::GetResourceValue), etc.)
// compiles. Without these the AssetManager JNI module failed to build → the stub
// register_android_content_AssetManager returned garbage → AssetManager.nativeCreate()
// UnsatisfiedLinkError killed ActivityThread.
namespace android {
class ResourceTimer {
public:
  enum class Counter {
    GetResourceValue,
    RetrieveAttributes,
    LastCounter = RetrieveAttributes,
  };
  explicit ResourceTimer(Counter) {}
  ~ResourceTimer() {}
  static void enable() {}
};
}  // namespace android
#endif
