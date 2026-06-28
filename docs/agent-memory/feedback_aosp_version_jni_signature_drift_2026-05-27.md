---
name: aosp-version-jni-signature-drift-2026-05-27
description: "FEEDBACK — When porting AOSP JNI registration tables, verify each method signature against the SMALI of the framework jar actually running on the target device, NOT against AOSP HEAD or AOSP-15 reference. OHOS DAYU200 V7 runs AOSP-14-derived framework. AOSP-15 added params to several native methods (e.g., SQLiteConnection.nativeExecute went from (JJ)V to (JJZ)V with isPragmaStmt). Mismatch surfaces as RegisterNatives returning -1 silently with NoSuchMethodError on first call. Discovered 2026-05-27 during Fix J.2-G."
metadata:
  node_type: memory
  type: feedback
  originSessionId: 92c37e05-ea23-45cc-8746-3d0c3553d5f3
---

## Rule

When porting `JNINativeMethod sMethods[]` tables from AOSP into [build-host]'s adapter:

**Verify each method signature against the SMALI of the actual framework jar running on the target device, NOT against AOSP HEAD or any specific AOSP version reference.**

Wrong: pull from `/aosp-master/frameworks/base/...` or even `/aosp-15/...`
Right: `apktool d /system/android/framework/core-oj.jar` (or wherever the relevant framework class lives) and read the `.method native` declarations

## Why

OHOS DAYU200 V7 ships an AOSP-derived framework that is pinned to a specific AOSP version (currently AOSP-14-ish based on observed behavior). Native method signatures evolved across AOSP versions.

Concrete drift case discovered:
- AOSP-14 `android.database.sqlite.SQLiteConnection.nativeExecute`: `(JJ)V` (2-arg: connectionPtr, statementPtr)
- AOSP-15 added: `(JJZ)V` (3-arg, with `boolean isPragmaStmt`)

Fix J.2's original `sqlite_jni.cpp` was AOSP-15-style. Loaded against AOSP-14 framework smali → `RegisterNatives` returned -1 silently → first call to `nativeExecute` threw NoSuchMethodError.

## How to apply

Before porting any AOSP JNI registration block:

1. Find the Java class on device:
   ```bash
   $HDC shell "find /system/android/framework -name '*.jar' | xargs -I{} unzip -l {} 2>/dev/null | grep -l ClassName"
   ```

2. Extract and disassemble:
   ```bash
   $HDC pull /system/android/framework/<framework-jar>.jar /tmp/
   apktool d /tmp/<framework-jar>.jar -o /tmp/<jar-name>-smali
   grep '\.method.*native' /tmp/<jar-name>-smali/<path>/<ClassName>.smali
   ```

3. For each native method, the smali signature is authoritative. E.g.:
   ```
   .method static native nativeExecute(JJZ)V
   ```
   means `(JJZ)V` not `(JJ)V`.

4. Build `JNINativeMethod` table matching smali, NOT matching AOSP source you pulled.

## Signal in failure mode

If you mistakenly use AOSP-N signatures against AOSP-(N-1) framework:
- `RegisterNatives` returns `-1` (not -1 per method, but for the whole table on first mismatch)
- Subsequent calls throw `NoSuchMethodError: no static method "Lname/of/class;.method(args)V"`
- Log line: `JNI ERROR (app bug): jmethodID ... not valid for class ...`

If you see `RegisterNatives` -1 after a fix, FIRST check signature alignment with on-device smali before debugging anything else.

## Class of fix this applies to

Any work touching `JNINativeMethod` tables: SQLite (J.2-G), Bitmap, Surface, Binder native methods, GraphicBuffer, Display, etc. Each AOSP version may have signature deltas.

## See also

- [[v3-fix-j2-g-landed-2026-05-27]] — first encounter
- [[liboh-android-runtime-dual-path-2026-05-27]] — sister deploy trap
