# Unified-connectivity graft (framework `f5fd86ef` → `1c334902`)

Gives adapter apps a **non-null `ConnectivityManager`** from `getSystemService(...)`, so noice's
`getSystemService(CONNECTIVITY_SERVICE) as ConnectivityManager` (Kotlin non-null cast) no longer NPEs,
while keeping every catalog-validated framework patch (catalog's metaData fix is in the bcp, not the
framework, so there is no catalog regression).

## What differs

The catalog framework (`f5fd86ef`) and the noice framework (`8524dc56`) differ in **exactly one
class**: `android/app/ContextImpl`. The noice version adds a fallback at the tail of
`getSystemService(String)` and `getSystemServiceName(Class)`:

- `name == "connectivity"`  → `new android.net.ConnectivityManager()`  (the stub class already lives in
  `adapter-mainline-stubs.jar` `41834c1f`; its `<init>()` takes no binder and `getDefaultProxy()`
  returns null — safe to construct).
- `name == "jobscheduler"`  → `new android.app.job.JobSchedulerStub()`  (added by this graft).
- `getSystemServiceName(Class)`: `ConnectivityManager.class → "connectivity"`,
  `JobScheduler.class → "jobscheduler"`.

`SystemServiceRegistry` and the `ContentResolver.registerContentObserver` guard are byte-identical in
both frameworks, so switching cannot regress them.

## Files

- `ContextImpl-grafted-methods.smali` — the two methods, verbatim from the noice framework
  `8524dc56` (the `.locals` are already correct). Drop-in replacements for the same two methods in
  the catalog `ContextImpl.smali`. (The methods are otherwise identical between the two frameworks —
  the diff is purely these connectivity/jobscheduler additions.)
- `JobSchedulerStub.smali` — `android/app/job/JobSchedulerStub` (extends the existing `JobScheduler`);
  add to the same dex as `ContextImpl` (classes.dex). Only needed if you keep the jobscheduler branch;
  noice itself only needs the connectivity branch.

## Apply

1. `apktool d -f -o fw <catalog framework.jar f5fd86ef>` (apktool 2.9.3; its bundled baksmali `Main`
   has no standalone entrypoint, so use `apktool d`/`b`, not `java -cp apktool.jar ...baksmali.Main`).
2. Replace `fw/smali/android/app/ContextImpl.smali`'s two methods with the ones here (or, since the
   class is otherwise identical, drop in the whole noice `ContextImpl.smali`), and add
   `fw/smali/android/app/job/JobSchedulerStub.smali`.
3. `apktool b -f -o framework.jar fw`  → unified framework (`1c334902`).
4. Regenerate the boot image (BCP) with libart of the `7b856a2d` branch. See `../../UNIFIED-CONFIG-REPRODUCE.md` §2.
