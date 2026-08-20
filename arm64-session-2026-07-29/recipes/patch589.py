#!/usr/bin/env python3
"""§589 — cut the per-invoke needle classifier down to only what noice actually needs.

DoCall<false>/<true> scan 14 distinct literal needles on EVERY invoke (54 `bl strstr` sites) — a
package classifier inherited from the McDonald's app. §576 already nulled 20 dead mcdonalds/newrelic
sites. §577 nulled ALL of them (strstr 28%->2.3%) but REGRESSED LAUNCH RELIABILITY, so >=1 needle
gates something the window/session/adoption path needs.

This patch nulls only the needles the audit measured as NEVER FIRING for noice, and KEEPS the four
that are plausibly load-bearing:
   KEEP  SurfaceControl      <- prime suspect for the §577 launch regression (window/session path)
   KEEP  com/google/gson     <- fires 89x
   KEEP  androidx/lifecycle  <- fires 17x
   KEEP  java/lang/reflect   <- MethodHandles fires 7x
Nulling = replace `bl strstr` with `mov x0, xzr` (x0=NULL is strstr's no-match result, so the
following `cbnz x0, handler` falls through identically). Precedent: §576/§577.
"""
import json,struct,sys

KEEP = {'SurfaceControl','com/google/gson','androidx/lifecycle','java/lang/reflect'}
MOV_X0_XZR = 0xaa1f03e0
BL_MASK, BL_OP = 0xfc000000, 0x94000000
TEXT_VA, TEXT_OFF = 0x6df000, 0x6de000          # .text: file_off = va - 0x1000

src,dst = sys.argv[1], sys.argv[2]
needles = json.load(open('live_needles.json'))
b = bytearray(open(src,'rb').read())

patched = kept = 0
for needle, sites in needles.items():
    if needle in KEEP:
        kept += len(sites); print(f"  KEEP  {needle!r} ({len(sites)} sites)"); continue
    for va in sites:
        off = TEXT_OFF + (va - TEXT_VA)
        cur = struct.unpack_from('<I', b, off)[0]
        if cur == MOV_X0_XZR:
            continue                                  # already nulled
        if (cur & BL_MASK) != BL_OP:
            print(f"  REFUSE @0x{va:x}: {cur:08x} is not a bl"); sys.exit(3)
        struct.pack_into('<I', b, off, MOV_X0_XZR)
        patched += 1
    print(f"  NULL  {needle!r} ({len(sites)} sites)")

open(dst,'wb').write(bytes(b))
print(f"\nnulled {patched} sites; kept {kept} live; wrote {dst}")
