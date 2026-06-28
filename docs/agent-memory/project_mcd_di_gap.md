---
name: McDonald's DI Injection - Almost Working
description: Hilt DI injection chain connected - OnContextAvailable fires, componentManager works, proxy has 26 interfaces including DataSourceModuleProvider. One field still null.
type: project
---

## Status as of 2026-04-01 (session end)

### BREAKTHROUGH: Full Hilt DI injection chain is now connected!
1. `Hilt_SplashActivity._initHiltInternal()` → registers `OnContextAvailableListener`
2. Our `ComponentActivity.onCreate()` fires the callback (using obfuscated method name `a()`)
3. `Hilt_SplashActivity$1.a(Context)` → calls `inject()`
4. `inject()` → `componentManager().generatedComponent()` → our universal proxy
5. Universal proxy implements **26 interfaces** including:
   - `SplashActivity_GeneratedInjector`
   - `DataSourceModuleProvider`
   - `McDMarketApplication_GeneratedInjector`
   - All singleton component interfaces (transitively collected)
6. `injectSplashActivity(activity)` → `fillNullInterfaceFields(activity)` fills 4 null interface fields

### Remaining issue:
`BaseActivity.clearV1OrderData()` calls `DataSourceModuleProvider.v()` on null.
Even though fillNullInterfaceFields fills 4 fields, the DataSourceModuleProvider access path
produces null. This might be:
- A field with obfuscated type that doesn't match the interface check
- Accessed via `EntryPoints.get(getApplication(), DataSourceModuleProvider.class)` (boot classloader can't find it)
- Or from a delegate/helper object that wasn't initialized

### Key files changed:
- `WestlakeActivityThread.java` — AOSP-style activity lifecycle manager (replaces MiniActivityManager for Hilt apps)
- `WestlakeInstrumentation.java` — Instrumentation with AppComponentFactory support
- `AppComponentFactory.java` — activity/application instantiation (Hilt hook point)
- `ActivityComponentManager.java` — universal proxy with 26 interfaces + `a()` obfuscated alias
- `ComponentActivity.java` — fires OnContextAvailableListener in onCreate (with obfuscated `a()` dispatch)
- `OnContextAvailableListener.java` — added `default void a(Context)` obfuscated alias

### Next step to try:
The inject method `injectSplashActivity(this)` on our proxy calls `fillNullInterfaceFields`.
But DataSourceModuleProvider might be accessed via `EntryPoints.get(getApplication(), DSP.class)`.
Since boot classloader can't find DSP, EntryPoints returns null.
Fix: make EntryPoints use the APPLICATION's classloader (not the calling class's boot classloader)
to find and create proxy interfaces.
