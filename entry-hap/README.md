# Launcher-icon `entry.hap` build inputs (Material Catalog)

These are the build inputs for the minimal OHOS resource HAP that gives the
Material Catalog a real launcher icon + label (the Material Catalog logo +
"Material Catalog"). Without it the OHOS launcher shows a blank/default icon and
the raw package name.

**Why it's needed.** The launcher resolves an app's icon/label **client-side via
resourceManager** (the ability's `iconId` / `labelId`), **not** via
`bundleResource.db`. Adapter apps register an ability whose `resourcePath` is
`…/<pkg>/entry.hap`, but that file is never created — so resourceManager can't
resolve the ids. This HAP supplies them. (Per-app: repeat for any other adapter
app with its own logo + iconId/labelId.)

## Inputs (committed here)
```
entry-hap/
  id_defined.json                              forces media->iconId (0x01000005) + string->labelId (0x01000003)
  src/
    module.json                                stage-model module; EntryAbility with icon:$media:app_icon + label:$string:EntryAbility_label
    resources/base/element/string.json         app_name / EntryAbility_label = "Material Catalog"
    resources/base/media/app_icon.png          the Material Catalog logo
```

The ids come from the bundle (`bm dump -n io.material.catalog | grep -iE
'"iconId"|"labelId"'`): catalog **iconId `16777221` = 0x01000005**, **labelId
`16777219` = 0x01000003**. `id_defined.json` forces restool to assign exactly
those ids so they match what the ability references.

## Build (restool) + deploy
```bash
cd entry-hap
# restool: $HOME/openharmony/out/sdk/ohos-sdk/linux/toolchains/restool (v4.105)
restool -i src -j src/module.json -p io.material.catalog -o out \
        -r out/ResourceTable.h --defined-ids id_defined.json -f
# verify out/ResourceTable.txt shows app_icon=0x01000005, app_name=0x01000003
(cd out && zip -r ../../entry.hap module.json resources.index resources)   # module.json + resources.index + resources/ at the zip ROOT
```
Deploy + clear the launcher's frozen layout (full steps in `CATALOG-REPRODUCE.md`
§2):
```bash
cp entry.hap /data/app/el1/bundle/public/io.material.catalog/entry.hap
chown installs:installs ...; chmod 644 ...; chcon u:object_r:data_app_el1_file:s0 ...
rm /data/app/el1/100/database/com.ohos.launcher/phone_launcher/rdb/Launcher.db*
reboot   # (or restart com.ohos.launcher)
```

**Verify:** launcher shows the Material Catalog logo + "Material Catalog"
(`docs/engine/V3-CATALOG-LAUNCHER-ICON-EVIDENCE/`).

> `bm install` WIPES the bundle dir → re-deploy `entry.hap` after every install.
> Setting the ability `launchMode` to `singleton` in `module.json` also prevents
> the multiton "zombie mission" recents entries (see `CATALOG-REPRODUCE.md` §10).
> The icon shows as a raw square (the launcher doesn't round it) — pre-round the
> PNG if rounded corners are wanted.
