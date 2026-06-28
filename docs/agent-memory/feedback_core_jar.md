---
name: Core JAR selection for Dalvik
description: Must use core-android-x86.jar (2125 classes, has VMThread) — NOT core-boot.jar or core-kitkat.jar
type: feedback
---

Use `core-android-x86.jar` as the Dalvik boot classpath JAR. NOT `core-boot.jar` or `core-kitkat.jar`.

- `core-android-x86.jar`: 1.2MB, 2125 classes, has VMThread/VMClassLoader/serialVersionUID — **WORKS**
- `core-boot.jar`: 241KB, 280 classes, missing VMThread — **CRASHES**
- `core-kitkat.jar`: 111KB, fewer classes — **CRASHES**

**Why:** Dalvik KitKat VM requires `java.lang.VMThread`, `java.lang.VMClassLoader`, and `java.lang.Class` with exactly 1 static field (`serialVersionUID`). Only `core-android-x86.jar` has all of these.

**How to apply:** Always use `-Xbootclasspath:/data/a2oh/core.jar` where `core.jar` is a copy of `dalvik-port/core-android-x86.jar`. The `app.dex` goes on both bootclasspath AND classpath: `-Xbootclasspath:core.jar:app.dex -classpath app.dex`

Also: `CLASS_SFIELD_SLOTS` check in `dalvik-kitkat/vm/oo/Class.cpp` was changed from `dvmAbort()` to `ALOGW()` warning — allows shim Class with different static field count to work.
