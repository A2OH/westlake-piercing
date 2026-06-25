# Catalog-specific BCP-jar adapter classes (ohaf / arb)

These are **adapter** classes (in the BootCLASSPATH jars), not app code. They are
the catalog-specific additions on top of the shared framework-smali patches in the
parent directory. Each change to a BCP jar requires a **boot-image regen** (see
`CATALOG-REPRODUCE.md` §boot-regen). They are largely universal (any adapter app
that uses the IME or shared-element transitions benefits).

> The shared `framework.jar` null-guards (ConnectivityManager in
> `handleBindApplication`, ContentResolver `registerContentObserver`) are in the
> **parent** `framework-smali-patches/` — the catalog uses the SAME
> `android_content_ContentResolver.smali` as noice (its Search-focus crash is the
> same `registerContentObserver` → null `IContentService` NPE; see the
> `:smfix_reg_ret` label).

## `OhImeBridge.smali` (+ `OhImeBridge$1.smali`) — the IME bridge class
Lives in **`oh-adapter-framework.jar` (ohaf, `4690cae1`)`**. The Java half of the
Android-IMM → OHOS-`InputMethodController` bridge:
- `show()` / `hide()` → the native `nativeShowKeyboard` / `nativeHideKeyboard`
  (registered by `bridge-src/input_method_bridge.cpp`).
- `nativeOn{InsertText,DeleteBefore,DeleteAfter,EnterAction}` post to the UI-thread
  Handler and commit into the focused Android `InputConnection` via reflection
  (`WindowManagerGlobal.mRoots → ViewRootImpl.mView → findFocus →
  onCreateInputConnection → commitText/...`).

The deployed ohaf also has 3 forwarding edits in `InputMethodManagerAdapter`
(`showSoftInput`/`hideSoftInput` → `OhImeBridge`;
`startInputOrWindowGainedFocus` → `OhImeBridge.show` when `editorInfo.inputType!=0`,
still returns `NO_IME`) — those are small in-place edits to the existing class, not
a standalone file.

**Status:** keyboard APPEARS + persists on a plain Text Field; SearchView flashes
then is torn down (the WMS-focus wall). Text-entry via synthetic input is
unconfirmed (the synthetic-input→InputConnection wall). See `CATALOG-REPRODUCE.md`
§H and §10.

## `TransitionOptionsHolder.{java,smali}` — shared-element ActivityOptions carry
Added to **`adapter-runtime-bcp.jar` (arb)**. A static `ConcurrentHashMap`:
`stashFromIntent(Intent, Bundle)` keys the `ActivityOptions` Bundle by the intent's
component className; `resolve(Intent)` pops it + `ActivityOptions.fromBundle()`
(reflection, @hide). This carries the shared-element scene-transition options from
the source activity to the destination's `EnterTransitionCoordinator` (an
in-process handoff — the catalog is ONE process, uid 16371).

The stash site is in **`ActivityTaskManagerAdapter`** (IActivityTaskManager, JNI
tag `OH_ATMJNI`) — **NOT** `ActivityManagerAdapter`: the catalog's
`startActivity` routes through the *Task* manager, which is why an earlier patch to
`ActivityManagerAdapter` never fired. The resolve site is in `AppSchedulerBridge`'s
`LaunchActivityItem` delivery. Both edits are small in-place additions to existing
classes; only the new `TransitionOptionsHolder` class is shipped here as a full
file (`.java` source + the `.smali` it compiles to).

**Status:** L1 (options carried A→B, proven via a file-log) + L2 (transition
engages, app survives, demo opens) work; the visible morph is delivered by the
durationScale APK patch (`catalog-smali-patches/`). The deployed demo-ready arb is
`c026e80c`; the L1 options-carry lineage is `fda6948c`. See `CATALOG-REPRODUCE.md`
§F and the `catalog-2nd-level-canvascontext-wall` memory for the full L1→L2→L3 arc.
