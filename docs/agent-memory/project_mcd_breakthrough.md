---
name: McDonald's Full Progress - April 2 2026 Final
description: Both activities onCreate complete. Real setContentView runs. Last barrier is LayoutInflater include resolution for sub-views.
type: project
---

## Final State (2026-04-02)

### ACHIEVED: BaseActivity.onCreate() COMPLETES for both activities
- SplashActivity: DI injected, clearV1OrderData passes, setContentView runs → COMPLETE
- HomeDashboardActivity: created, DI injected, real setContentView(activity_base) runs → COMPLETE
- Process stable at 700MB, zero crashes

### Full Fix Chain Applied
1. ApplicationComponentManager — obfuscated fields (a,b,c) + method a() alias
2. DataSourceHelper — 7+ static fields from singleton component
3. ClickstreamDataHelper — static fields from singleton
4. ApplicationContext.a — static Context from Application
5. Crypto — passthrough shim on boot classpath
6. DeepLinkObject — null-safe shim
7. SavedStateHandleHolder — with obfuscated c() method
8. ViewModelProvider.a() — obfuscated get()
9. Lifecycle.c() — obfuscated addObserver()
10. OnBackPressedDispatcher — with obfuscated i() addCallback
11. AccessibilityManager — registered service
12. Context — getFilesDir/getCacheDir/openFileInput/openFileOutput real paths
13. getIdentifier() — reverse lookup in ResourceTable
14. NativeAllocationRegistry — nar-fix.dex overrides core-libart (GC crash fix)
15. Fragment — all final methods removed for Hilt override
16. OnContextAvailableListener — fires a(Context) for Hilt inject
17. WestlakeActivityThread — AOSP lifecycle + synchronous launch
18. WestlakeInstrumentation — real setContentView detection (skip recovery)
19. Proxy handlers — return JSONObject/List/Map/Set for non-null types

### Remaining: LayoutInflater <include> Resolution
`activity_base.xml` uses `<include>` tags for basket bar, notification bar, etc.
Our inflater handles `<include>` (calls inflate recursively + addView) but some
included layouts' views don't appear in the view tree.

`McDBaseActivity.refreshBasketLayout()` → `findViewById(R.id.basket_view)` → null
because the basket sub-layout wasn't properly included.

This is purely a LayoutInflater issue — NOT DI, NOT lifecycle, NOT config.
The fix: trace which `<include>` layouts fail and ensure their views are added.

### Key Achievement
From "stuck on splash with DataSourceModuleProvider NPE" to
"both activities onCreate complete, real layouts set, process stable"
in one session. The pattern (decompile → find null → fix via reflection/shim)
scales to any Android app's initialization.
