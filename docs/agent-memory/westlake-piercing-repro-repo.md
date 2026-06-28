---
name: westlake-piercing-repro-repo
description: "The canonical reproduction repo is A2OH/westlake-piercing (PUBLIC, scrubbed); westlake repo deleted; covers both stock apps (Material Catalog + noice) on the OHOS appspawn-x adapter; libart base = patched AOSP platform/art @814cc93 (24Q4/OAT230) in libart-build/; PRIVACY: email+home-paths were leaked then filter-repo scrubbed — never reintroduce"
metadata:
  node_type: memory
  type: reference
  originSessionId: 5f7b1af5-9cec-4260-888a-283e2b779990
---

Set up + cleaned 2026-06-25. The public reproduction repo for the westlake work.

**Repo:** `github.com/A2OH/westlake-piercing` (**PUBLIC**). Local clone
`$HOME/westlake-repo`, branch `main`, HEAD `fe0728d` (scrubbed — see PRIVACY).
Remote `origin` = A2OH/westlake-piercing. The old **`westlake/westlake-noice-ohos`** repo
was **DELETED** — everything lives only under A2OH now. (gh is authed as the `westlake`
personal account; I first mistakenly pushed there, user said don't, so re-homed to A2OH
and deleted westlake.)

**Covers BOTH stock apps** (Material Catalog `io.material.catalog` + noice
`com.github.ashutoshgngwr.noice`) on the OHOS `appspawn-x` adapter. Apps are STOCK (only
cosmetic smali patches). Entry-point `README.md` = ordered 6-phase repro; `BUILD-FROM-SOURCE.md`
(build either APK), `docs/REPRODUCTION-GUIDE.md` (adapter), `CATALOG-REPRODUCE.md`/`REPRODUCE.md`
(per-app), `STATUS.md`, `ARTIFACT-INVENTORY.txt` (binaries by md5+provenance).

**`libart-build/` = buildable ART ("art jvm build" provenance):** base is pristine AOSP
`platform/art @ 814cc9385f8f8eaba6f4bfd1d723160c2132c76e` ("Snap…24Q4-release", Android 15,
**OAT v230**); on-disk mirror `$HOME/aosp-art-15`. 6 patched units (class_linker.cc
W-series + Fix I; entrypoint_utils-inl.h ×2 Fix H; fault_handler.cc/_arm.cc W15; nterp_helpers.cc
Fix-J2B) + `build_libart_pathA.sh` + `BASE-MANIFEST.md`. **`A2OH/art-latest` (PF-noice) and
`A2OH/art-universal` are DIFFERENT ART patch lines — NOT this libart.** Build = incremental relink
(recompiles ~11 units, relinks all 230 baseline `.o`): needs the bundle
`$HOME/libart-32arm-cache/libart-32arm-pathA-bundle` + `$HOME/libart-pathA-work/cache/art`
(bulky → provenance-pinned, NOT committed). Output `libart.so` md5 **ba40f173**.

**★ PRIVACY — a leak happened + was cleaned (2026-06-25):** commits had been authored as
`westlake <[REDACTED-EMAIL]>` (the box's git `user.email`), and docs/scripts contained
`/home/{dspfac,[user],westlake-dev}` absolute paths. All **SCRUBBED via `git filter-repo`**:
identity → `westlake <westlake@users.noreply.github.com>`, `/home/<user>` → `$HOME`, bare names → `user`
(rules in `/tmp/wl_replace.txt`). The repo's **local** git config is now the westlake placeholder;
the **global** git config still has the real email (left intact for internal repos). **NEVER
reintroduce the real email / usernames / `/home/<user>` paths into this PUBLIC repo** — set
`user.email`/`user.name` to the placeholder before committing here. An org-wide A2OH private-info
audit was launched the same day.

**★ NETWORK QUIRK:** the Bash sandbox proxy (`198.18.x.x`) intermittently **times out** `gh` and
`git push` to github.com — once a force-push reported success ("Everything up-to-date") but had
**silently failed**, briefly exposing the email on the public repo. **Use `dangerouslyDisableSandbox: true`
for gh/git-push to github, and always verify the published tip via
`gh api repos/A2OH/westlake-piercing/commits/main --jq '.sha+" "+.commit.author.email'`** rather than
trusting the push exit message.

**Conventions:** small source/patches/docs committed; large binaries by md5+provenance only.
**No co-author trailer** on commits. Related: [[westlake-repro-repo-state]] (the OLDER A2OH/westlake
dev monorepo at $HOME/android-to-openharmony-migration — SEPARATE 31k-file repo),
[[catalog-perf-jit-aot-findings]] (libart build context), [[never-set-usb-config-none]].
