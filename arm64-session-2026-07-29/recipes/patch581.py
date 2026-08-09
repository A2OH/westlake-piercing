#!/usr/bin/env python3
"""§581: neutralize the spurious interpreter-bridge stack-overflow throw.
ArtInterpreterToInterpreterBridge @0xabafb4:
    cmp x29, (stack_end + reserved); b.lo 0xabb024 (ThrowStackOverflowError)
For a COMPILED callee, ArtInterpreterToCompiledCodeBridge redirects here (force-interpret) and this
check fires spuriously (self->stack_end_ is wrong-high for the thread; SP is actually near the top).
NOP the b.lo @0xabafb8 so the bridge never throws SOE. Test on the headless bench.
⛔binary patch. Refuses unless the exact b.lo is present."""
import struct,sys
SRC,DST=sys.argv[1],sys.argv[2]; D=0x1000
VA=0xabafb8; NOP=0xd503201f
b=bytearray(open(SRC,'rb').read()); off=VA-D
cur=struct.unpack_from('<I',b,off)[0]
if cur==NOP: print("already"); open(DST,'wb').write(bytes(b)); sys.exit(0)
# b.lo = 0x54xxxxx3 (cond=0b0011=LO=CC)
if (cur & 0xFF00001F) != 0x54000003: print(f"REFUSE @0x{VA:x}: {cur:08x} not b.lo"); sys.exit(3)
struct.pack_into('<I',b,off,NOP); print(f"@0x{VA:x}: {cur:08x} (b.lo throw) -> nop")
open(DST,'wb').write(bytes(b)); print("wrote",DST)
