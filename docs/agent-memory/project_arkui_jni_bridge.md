---
name: ArkUI JNI Bridge
description: Java-to-ArkUI bridge via JNI - creates ArkUI NG components from Java, runs headless layout, reads geometry
type: project
---

## Overview

A JNI shared library (`libarkui_jni_bridge.so`) that bridges Java code to ArkUI NG's headless component engine. Java can create ArkUI buttons, set sizes, run layout, and read back computed frame rectangles — all without a display.

**Why:** Enables Java apps (potentially running on Dalvik) to use ArkUI's native layout engine on OpenHarmony.

**How to apply:** When working on Java-ArkUI integration or Dalvik-on-OHOS, this bridge provides the native component layer.

## Architecture

```
Java (ArkUIBridge.java)
  ↓ JNI
arkui_jni_bridge.cpp (handle table, 9 native methods)
  ↓
ArkUI NG Engine (ButtonModelNG, ViewStackProcessor, MockPipelineContext)
  ↓
Headless layout (measure + place, no GPU)
```

## Key Files

- `arkui_test_standalone/jni/arkui_jni_bridge.cpp` — C++ JNI implementation
- `arkui_test_standalone/java/com/ohos/arkui/ArkUIBridge.java` — Java API
- `arkui_test_standalone/java/com/ohos/arkui/ArkUIDemo.java` — Working demo
- `arkui_test_standalone/CMakeLists.txt` — `arkui_jni_bridge` target
- `arkui_test_standalone/build_jni.sh` — Build + run script

## Build Gotchas

1. GoogleTest/GMock must be compiled with `-fPIC` (static libs linked into shared .so)
2. `arkui_jni_bridge.cpp` must include ALL STL headers before `#define private public` hack (std::any, std::sstream break otherwise)
3. Use system JDK, not conda JDK (conda's libstdc++ lacks GLIBCXX_3.4.30)
4. JNI headers from `$HOME/miniconda3/include/jni.h` (or system JDK)

## Verified Working

- Pipeline init/teardown
- Button creation (NORMAL, CAPSULE, CIRCLE types)
- Size setting + layout flush
- Frame rect readback
- Property queries (tag, label, type)
- Child count
- Node destruction
