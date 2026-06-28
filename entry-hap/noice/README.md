# noice launcher icon (entry.hap) + first-run wizard skip

## A. Launcher icon + label

The OHOS launcher resolves an app's icon/label client-side via resourceManager from the bundle's
`entry.hap`, which the adapter never creates → adapter apps show a blank icon + the raw package name.
Fix = build a minimal `entry.hap` whose resources.index maps noice's BMS-registered ability ids
(`iconId 0x01000005`, `labelId 0x01000003` — same as catalog; confirm with
`bm dump -n com.github.ashutoshgngwr.noice | grep -iE '"iconId"|"labelId"'`).

noice ships only an **adaptive** icon (no raster fallback), so first rasterize it:

```sh
AAPT=<android-sdk>/build-tools/<ver>/aapt \
  python3 render_noice_icon.py <noice-base.apk> out/app_icon.png
```

Then build + deploy (restool from the OHOS SDK toolchains):

```sh
mkdir -p src/resources/base/{media,element,profile}
cp out/app_icon.png src/resources/base/media/app_icon.png
# module.json (this dir) -> src/module.json ; string.json app_name="Noice" ; main_pages.json
printf '{ "string": [ { "name": "app_name", "value": "Noice" } ] }\n' > src/resources/base/element/string.json
printf '{ "src": [ "pages/Index" ] }\n' > src/resources/base/profile/main_pages.json
cp module.json src/module.json

restool -i src -j src/module.json -p com.github.ashutoshgngwr.noice \
        -o out -r out/ResourceTable.h --defined-ids id_defined.json -f
# verify: grep -iE 'app_icon|app_name' out/ResourceTable.txt  → 0x01000005 / 0x01000003

( cd out && zip -rq ../entry.hap module.json resources.index resources )
# deploy:
#   push entry.hap -> /data/app/el1/bundle/public/com.github.ashutoshgngwr.noice/entry.hap
#   chown installs:installs ; chmod 644 ; chcon u:object_r:data_app_el1_file:s0
#   rm /data/app/el1/100/database/com.ohos.launcher/phone_launcher/rdb/Launcher.db*
#   restart com.ohos.launcher  (or reboot)
```

Files here: `render_noice_icon.py`, `id_defined.json` (forces media→0x01000005, string→0x01000003),
`module.json` (stage-model template). Per-app — repeat with each app's own logo + iconId/labelId.

## B. Skip the first-run wizard (un-clickable AppIntroActivity)

On a fresh launch noice sometimes shows its onboarding wizard (`AppIntroActivity`, "欢迎/Welcome")
instead of the Sound Library — a race on the intro-flag read. That wizard is a **second-level
Activity** and hits the adapter input-routing/WMS-focus wall: injected taps (Skip / › / swipe) are
not delivered → stuck. The MainActivity (Sound Library) itself *is* tappable.

`MainActivity` decides via plain SharedPreferences (not DataStore):
`getSharedPreferences("<pkg>_preferences").getBoolean("has_user_seen_app_intro", false)`. Pre-seed it
so noice boots straight to the navigable MainActivity:

```sh
# force-stop noice first, then write:
PREF=/data/app/el2/0/base/com.github.ashutoshgngwr.noice/shared_prefs/com.github.ashutoshgngwr.noice_preferences.xml
cat > "$PREF" <<'XML'
<?xml version='1.0' encoding='utf-8' standalone='yes' ?>
<map>
    <boolean name="has_user_seen_app_intro" value="true" />
</map>
XML
chown 13731:13731 "$PREF"        # noice uid
chcon u:object_r:appdat:s0 "$PREF"
```

Persists across reboot (el2 /data). General lesson: any adapter app gated by a first-run/2nd-Activity
wizard that won't accept touch → pre-seed its "seen" flag rather than fight the input wall (the deeper
2nd-Activity input routing is the unsolved foundational wall).
