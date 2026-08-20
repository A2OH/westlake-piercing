#!/usr/bin/env python3
"""§590 — stop calling getenv on every interpreted invoke.

hiperf on §589 showed `getenv` at **2.46%** of a tab switch. Cause: westlake's interpreter
predicates read diagnostic/opt-out env vars *per invoke* — `bl getenv` sits inside DoCall<false>,
DoCall<true> and the Execute path. musl's getenv is a linear scan of the whole environment;
§557 already added an index cache in appspawn-x, and it is STILL 2.46%.

All five variables are UNSET in the child (verified against /proc/<pid>/environ and the launcher),
so every one of these calls returns NULL. Replacing `bl getenv` with `mov x0, xzr` yields the
identical value with no call — the same technique as §576/§589 do for `bl strstr`.

⚠️Only the per-invoke sites are touched. The library-loading reads (WESTLAKE_NATIVE_LIB_DIR,
WESTLAKE_LIBRARY_PATH, LD_LIBRARY_PATH) are left ALONE: they run once at load time, cost nothing,
and LD_LIBRARY_PATH in particular is genuinely set.
⚠️These vars become permanently-unset for the interpreter. To use one for debugging again, revert.
"""
import json,struct,sys

NULLABLE = {'WESTLAKE_NO_PROXYFIX','WL_BADREF','WESTLAKE_TRACE_TZ',
            'WESTLAKE_TRACE_INTERP_JNI','WESTLAKE_UNSAFE_JNI_DIAG'}
MOV_X0_XZR = 0xaa1f03e0
BL_MASK, BL_OP = 0xfc000000, 0x94000000
TEXT_VA, TEXT_OFF = 0x6df000, 0x6de000

src,dst = sys.argv[1], sys.argv[2]
sites = json.load(open('getenv_sites.json'))
b = bytearray(open(src,'rb').read())
n = 0
for var, vas in sites.items():
    if var not in NULLABLE:
        print(f"  skip  {var!r} ({len(vas)} sites — not per-invoke / genuinely used)"); continue
    for va in vas:
        off = TEXT_OFF + (va - TEXT_VA)
        cur = struct.unpack_from('<I', b, off)[0]
        if cur == MOV_X0_XZR: continue
        if (cur & BL_MASK) != BL_OP:
            print(f"  REFUSE @0x{va:x}: {cur:08x} is not a bl"); sys.exit(3)
        struct.pack_into('<I', b, off, MOV_X0_XZR); n += 1
    print(f"  NULL  {var!r} ({len(vas)} sites)")
open(dst,'wb').write(bytes(b))
print(f"\nnulled {n} per-invoke getenv calls; wrote {dst}")
