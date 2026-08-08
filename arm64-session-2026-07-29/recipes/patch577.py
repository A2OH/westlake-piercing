#!/usr/bin/env python3
"""§577: null ALL 74 per-invoke needle strstr in DoCall<false>/<true> (remove the whole gate).
Goal: strip the McDonald's-era package classifier from the per-invoke path. Each `bl strstr` ->
`mov x0,xzr` (null = no-match; following cbnz falls through). The real hook dispatch lives in
DoCallCommon (its own strcmp chain) — this tests whether DoCall's needle scan is a removable
classifier or a gate DoCallCommon depends on. ⛔binary patch; refuses any non-BL site."""
import json,struct,sys
SRC,DST=sys.argv[1],sys.argv[2]; D=0x1000; MOV_X0_0=0xaa1f03e0
sites=json.load(open('/tmp/claude-1000/-home-dspfac-openharmony/969523b9-c069-43df-983b-1395ea6611d2/scratchpad/all_needle_sites.json'))
b=bytearray(open(SRC,'rb').read()); n=0; done=0
for va in sites:
    off=va-D; ins=struct.unpack_from('<I',b,off)[0]
    if ins==MOV_X0_0: done+=1; continue
    if (ins>>26)!=0x25: print(f"REFUSE @0x{va:x}: not BL ({ins:08x})"); sys.exit(3)
    struct.pack_into('<I',b,off,MOV_X0_0); n+=1
open(DST,'wb').write(bytes(b)); print(f"nulled {n} new (+{done} already) = {n+done}/74; wrote {DST}")
