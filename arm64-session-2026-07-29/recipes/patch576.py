#!/usr/bin/env python3
"""§576: null the McDonald's-app dead needle scans in DoCall<false>/<true>.
This libart's PFCUT machinery was built for the McDonald's app; DoCall runs 37 strstr per invoke
including 'mcdonalds'/'Lcom/mcdonalds/...'/'newrelic' needles. noice has ZERO such classes (verified:
0 in the app dex), so those strstr ALWAYS return null. Replace each `bl strstr` with `mov x0,xzr`
(x0 = null = exactly strstr's no-match result); the following `cbnz x0,handler` then falls through
identically. 20 sites (10 per DoCall template). Pure dead-work removal — semantically identical for
this app. ⛔binary patch. Refuses any site that isn't currently a BL."""
import os
import json,struct,sys
SRC,DST=sys.argv[1],sys.argv[2]; D=0x1000
MOV_X0_0=0xaa1f03e0
sites=json.load(open(os.environ.get('WL_SCRATCH','/tmp/wl-scratch')+'/mcd_sites.json'))
b=bytearray(open(SRC,'rb').read()); n=0
for va in sites:
    off=va-D; ins=struct.unpack_from('<I',b,off)[0]
    if ins==MOV_X0_0: continue
    if (ins>>26)!=0x25: print(f"REFUSE @0x{va:x}: not BL ({ins:08x})"); sys.exit(3)
    struct.pack_into('<I',b,off,MOV_X0_0); n+=1
open(DST,'wb').write(bytes(b)); print(f"nulled {n} strstr sites -> mov x0,xzr; wrote {DST}")
