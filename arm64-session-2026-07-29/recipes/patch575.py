#!/usr/bin/env python3
"""§575 AUDIT: disable redundant per-invoke PFCut hooks -> fall through to the right-layer fix.
Each hook is called as `bl hook; tbnz w0,#0,handled` — returning w0=0 (false) means "not handled,
do the normal invoke". Patch entry to `mov w0,wzr; ret`.
  proxy<0/1> : the interface/proxy dispatch repair. Root cause fixed at the right layer by §551
               (FindVirtualMethodForInterface) and bypassed by §440 (dex surgery) — likely redundant.
  systemTime : intercepts System.currentTimeMillis. §534 fixed the underlying clock source — likely
               redundant (falls through to the real, now-correct, native call).
⛔binary patch (precedent §571/§534/§551). Refuses unless the known prologue matches.
"""
import struct, sys
SRC,DST=sys.argv[1],sys.argv[2]; D=0x1000
MOV=0x2a1f03e0; RET=0xd65f03c0
T={0xa5db7c:("proxy<0>",0xd10583ff),0xa7d23c:("proxy<1>",0xd10583ff),0xa62080:("systemTime",0xd100c3ff)}
b=bytearray(open(SRC,'rb').read())
for va,(nm,exp) in sorted(T.items()):
    off=va-D; cur=struct.unpack_from('<I',b,off)[0]
    if cur==MOV: print("  %s already"%nm); continue
    if cur!=exp: print("  REFUSE %s @0x%x: %08x != %08x"%(nm,va,cur,exp)); sys.exit(3)
    struct.pack_into('<I',b,off,MOV); struct.pack_into('<I',b,off+4,RET)
    print("  %-12s @0x%x: %08x -> mov w0,wzr; ret"%(nm,va,cur))
open(DST,'wb').write(bytes(b)); print("wrote",DST)
