#!/usr/bin/env python3
"""§579: raise the westlake invoke-depth SOE guard cap in EnterInterpreterFromInvoke.
@0xaaf4ec: `cmp w8,#50` (0x7100c91f) guards a thread-local invoke_depth counter; if depth>=50 it
throws StackOverflowError. Raise the cap 50 -> 4095 (`cmp w8,#4095` = 0x713ffd1f). Test whether this
westlake re-entrancy cap is what spuriously kills JIT-driven execution. ⛔binary patch; refuses unless
the exact cmp#50 is present."""
import struct,sys
SRC,DST=sys.argv[1],sys.argv[2]; D=0x1000
VA=0xaaf4ec; EXPECT=0x7100c91f; NEW=0x713ffd1f
b=bytearray(open(SRC,'rb').read()); off=VA-D
cur=struct.unpack_from('<I',b,off)[0]
if cur==NEW: print("already"); open(DST,'wb').write(bytes(b)); sys.exit(0)
if cur!=EXPECT: print(f"REFUSE @0x{VA:x}: {cur:08x}!={EXPECT:08x}"); sys.exit(3)
struct.pack_into('<I',b,off,NEW); print(f"@0x{VA:x}: cmp w8,#50 -> cmp w8,#4095")
open(DST,'wb').write(bytes(b)); print("wrote",DST)
