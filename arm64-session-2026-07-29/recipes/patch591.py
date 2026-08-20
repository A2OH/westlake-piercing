#!/usr/bin/env python3
"""§591 — null ALL remaining DoCall needles (the §577 configuration, retested properly).

§577 nulled every needle (strstr 28% -> 2.3%) but was rejected for a "launch reliability
regression" measured with **no control** under a repeated-`pkill -9` harness. §589 later showed that
harness fails identically on the untouched §576 baseline (1/5 vs 1/5), so that verdict is suspect.

This is §589 + the last 14 sites (SurfaceControl, com/google/gson, androidx/lifecycle,
java/lang/reflect) — i.e. the full §577 configuration — to be validated POST-REBOOT with the app
actually exercised, rather than by counting launch attempts.

⚠️Unlike the §589 set, these four needles DO gate hooks that fire (gson 89x, lifecycle 17x,
MethodHandles 7x). If anything misbehaves, the culprit is here, and §589 is the safe fallback.
"""
import json,struct,sys
MOV_X0_XZR=0xaa1f03e0; BL_MASK,BL_OP=0xfc000000,0x94000000
TEXT_VA,TEXT_OFF=0x6df000,0x6de000
src,dst=sys.argv[1],sys.argv[2]
needles=json.load(open('live_needles.json'))
b=bytearray(open(src,'rb').read()); n=0
for needle,sites in needles.items():
    hit=0
    for va in sites:
        off=TEXT_OFF+(va-TEXT_VA); cur=struct.unpack_from('<I',b,off)[0]
        if cur==MOV_X0_XZR: continue
        if (cur&BL_MASK)!=BL_OP: print(f"  REFUSE @0x{va:x}: {cur:08x}"); sys.exit(3)
        struct.pack_into('<I',b,off,MOV_X0_XZR); n+=1; hit+=1
    if hit: print(f"  NULL  {needle!r} (+{hit} sites)")
open(dst,'wb').write(bytes(b))
print(f"\nnulled {n} further sites (total = all 54); wrote {dst}")
