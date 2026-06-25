# Autonomous-demo SELinux change — Enforcing → Permissive

For the hands-off catalog demo (power on → tap icon → navigate, no laptop), the
board must boot **Permissive**. By default the OHOS base image boots
**Enforcing**, and the catalog runs in the restricted `normal_hap` domain, which
is denied the adapter-specific access it needs:

```
avc: denied { getattr } for path="/system/android"  scontext=...normal_hap  tcontext=...system_file
avc: denied { search } for name="misc"  ... /data/misc
avc: denied { search } for name="tmp"   ... /data/local/tmp
avc: denied { dac_override } ...
```

→ a tap on the launcher icon does nothing.

## The change (persistent, low-risk, reversible)

Edit `/system/etc/selinux/config`, change `SELINUX=enforcing` → `SELINUX=permissive`:

```bash
mount -o remount,rw /
cp /system/etc/selinux/config /data/local/tmp/selinux_config.bak     # back up first
sed -i 's/^SELINUX=enforcing/SELINUX=permissive/' /system/etc/selinux/config
grep '^SELINUX=' /system/etc/selinux/config                          # confirm: SELINUX=permissive
```

OHOS honors it on the next boot: the board comes up `enforce=0` on its own (no
`setenforce` needed). The `ondemand` appspawn-x then auto-spawns when the catalog
icon is tapped — no manual `start_asx.sh` bring-up.

Verify after reboot:
```bash
getenforce        # Permissive   (or: cat /sys/fs/selinux/enforce  -> 0)
```

## Revert
Restore the backup and reboot:
```bash
mount -o remount,rw /
cp /data/local/tmp/selinux_config.bak /system/etc/selinux/config
reboot
```

> This is the only SELinux change the demo needs. It is deliberately a config
> edit (not per-domain policy surgery) — low risk, trivially reversible. Production
> use would instead author the missing allow rules for the `normal_hap`→adapter
> access; Permissive is the pragmatic demo enabler.
