---
name: chroot-dynamic-elf-ro-bind-mount-host-lib-paths
description: "Chroot binaries that are dynamic ELFs need RO bind-mount of host lib paths (/lib, /system/lib) into the chroot. Brick-safe (read-only). Avoids per-binary ldd walking. Chroot kernel returns misleading ENOENT-on-missing-interp; OH binaries link against /lib/ld-musl-arm.so.1. Stage 3 of any chroot setup must include the bind-mount before any chroot exec."
metadata:
  node_type: memory
  type: feedback
  date: 2026-05-19
  triggers:
    - chroot
    - DAYU200
    - dynamic-ELF
    - deploy-script
    - V3-W2
  blast_radius: any-chroot-based-deploy
  originSessionId: agent-70-2026-05-19
---

# Rule

**Any chroot containing dynamic ELF binaries MUST RO bind-mount host
`/lib` and `/system/lib` into the chroot at the mount stage, BEFORE any
chroot exec. Use `mount --bind` followed by `mount -o remount,ro,bind`
(the Linux idiom for "bind-mount but read-only"). Idempotent. Brick-safe
because RO.**

**Do not walk per-binary `ldd` output to copy individual `.so` files into
the chroot.** That's a moving target, brittle to [build-host] artifact drift, and
re-copies the same libs into every chroot you build.

# Why

**Concrete: 2026-05-19 chroot Phase 1a substrate validation
(commit `ad52b63d`).**

OH binaries on DAYU200 link against `/lib/ld-musl-arm.so.1` (musl
dynamic linker) and pull `libc.so` / `libm.so` / `libdl.so` / a long
tail of OH platform libs from `/system/lib`. A chroot built only with
the [build-host] bundle artifacts can't resolve any of these — `exec()` inside
the chroot returns ENOENT, but the misleading part is that the kernel
reports ENOENT on the **binary itself**, not on the missing interpreter.
You waste an hour thinking the binary copy failed.

First attempt: walk `ldd /system/bin/<target>` on the host side, build
a copy manifest, push the libs into `chroot/lib/` + `chroot/system/lib/`.
This works once but:

1. Drifts as soon as [build-host] bumps any artifact.
2. Re-copies the same ~200 MB of platform libs into every chroot you
   build.
3. Misses dlopen'd libs (no ldd entry) — surfaces at runtime as
   "could not load liboh_adapter_bridge.so" inside the JVM.
4. Tempts an agent to `chmod +w` the chroot libs to "fix" a label issue
   — which then loses the brick-safety guarantee.

**Solution: RO bind-mount host lib paths.**

```bash
# Stage 3 — mount stage. Run via hdc_shell_check (see
# feedback_hdc_shell_check_pattern.md) for control-flow safety.
mount --bind /lib              "$CHROOT/lib"
mount -o remount,ro,bind       "$CHROOT/lib"
mount --bind /system/lib       "$CHROOT/system/lib"
mount -o remount,ro,bind       "$CHROOT/system/lib"
```

Now the chroot sees host `/lib/ld-musl-arm.so.1` and the full OH
platform lib tree, **read-only**. The chroot cannot mutate host libs
even if a compromised binary tried — `EROFS` is the kernel's answer.

Per-binary ldd walking is replaced by "if the binary works outside the
chroot, it'll work inside" — which is true by construction once
`/lib` + `/system/lib` are bind-mounted.

# How to apply

## Mount sequence (Stage 3 of chroot setup)

Add to any future chroot-deploy script before the smoke test:

```bash
ensure_chroot_lib_binds() {
    local chroot="$1"
    # Idempotent: check via hdc_shell_check before re-binding.
    for path in /lib /system/lib; do
        local target="${chroot}${path}"
        if ! hdc_shell_check "mountpoint -q '$target'"; then
            hdc_shell_check "mkdir -p '$target'" \
                || die "mkdir $target failed"
            hdc_shell_check "mount --bind '$path' '$target'" \
                || die "bind-mount $path -> $target failed"
            hdc_shell_check "mount -o remount,ro,bind '$target'" \
                || die "RO-remount $target failed"
        fi
    done
}
```

## Verification

```bash
hdc shell "ls -la ${CHROOT}/lib/ld-musl-arm.so.1"
# Expect: file present, host inode matches /lib/ld-musl-arm.so.1.

hdc shell "mount | grep ${CHROOT}/lib"
# Expect: "<src> on <CHROOT>/lib type none (ro,bind)"
```

If the bind isn't RO, `EROFS` won't fire on accidental writes and you
lose the brick-safety guarantee. Always verify the `ro` flag.

## When per-binary copy IS appropriate (rare)

Phase 2 (writing to /system) does not use chroot bind-mounts — at that
point you're shipping artifacts into the actual factory image and the
ldd walk is correct because each artifact is a long-lived install
target. But Phase 2 is gated on Phase 1b/1c lands engine evidence first
([[handoff_2026-05-19]]).

## What this rule replaces

Earlier proposals to:
- Walk `ldd` per-binary and copy libs into chroot
- `cp -aL /lib/* chroot/lib/` (lose label info, lose host updates)
- Hardlink host libs into chroot (cross-fs hardlinks fail; same-fs
  hardlinks lose RO semantics)

All three are inferior to bind-mount.

# Why brick-safe

- **Read-only.** A bug inside the chroot cannot mutate host libs.
- **No host-state mutation.** No copies, no symlinks, no chmod, no
  chcon on host paths. Pure mount-table change.
- **Rollback = unmount.** `umount /data/local/tmp/v3-hbc-chroot/lib`
  reverts. Reboot also reverts (bind-mounts don't persist).
- **Cannot soft-brick.** The 2026-05-16 soft-brick was caused by
  silent-chcon on /system labels; this rule never touches /system labels.

# Co-occurring discipline

- [[feedback_hdc_shell_check_pattern]] — companion 2026-05-19 rule;
  every mount step in the Stage 3 helper above uses `hdc_shell_check`
  so a silent transport failure doesn't leave the chroot half-mounted.
- [[feedback_soft_brick_w2_2026-05-16]] — origin; the chroot-containment
  model exists because direct /system writes soft-bricked the board.
- [[project_v3_chroot_phase1a_validated]] — milestone marker where this
  rule was forged.

# Where this lives

- This memory file: lead rule + canonical Stage 3 helper.
- `scripts/v3/deploy-hbc-to-dayu200-chroot.sh` — Stage 3 RO bind-mount
  implementation (commit `ad52b63d`).
- `docs/engine/V3-CHROOT-CONTAINMENT-PROPOSAL.md` — strategic context.

# Cross-references

- `scripts/v3/deploy-hbc-to-dayu200-chroot.sh` — canonical impl
- commit `ad52b63d` — RO bind-mount Stage 3 added
- `docs/engine/V3-CHROOT-CONTAINMENT-PROPOSAL.md` — Phase 1 / Phase 2
  model
- [[feedback_hdc_shell_check_pattern]] — companion control-flow safety
- [[feedback_soft_brick_w2_2026-05-16]] — origin postmortem
- [[handoff_2026-05-19]] — current orientation
- [[project_v3_chroot_phase1a_validated]] — milestone
