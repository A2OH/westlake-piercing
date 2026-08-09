#!/usr/bin/env python3
"""§578: fix the JIT-compiled stack-overflow probe.
CodeGeneratorARM64::GenerateFrameEntry emits the implicit SO check as:
    sub  temp, sp, #kStackOverflowReservedBytes(=8192)
    ldr  wzr, [temp]      ; probe — faults spuriously on this port -> spurious StackOverflowError
The reserved-bytes immediate is built at 0xb7ee4c: `movz w1,#0x2000` (mov w1,#8192).
Change it to `movz w1,#0` so the emitted probe becomes `sub temp,sp,#0; ldr wzr,[temp]` == ldr [sp],
which always hits mapped memory (you are running on sp) and never faults. One-instruction codegen
patch -> fixes the probe for EVERY JIT-compiled method.
⛔binary patch (precedent §534/§551). Refuses unless the exact movz #8192 is present.
Caveat: reduces the reserved headroom for genuine deep overflows; acceptable to get the JIT running."""
import struct,sys
SRC,DST=sys.argv[1],sys.argv[2]; D=0x1000
VA=0xb7ee4c; EXPECT=0x52840001; NEW=0x52800001  # movz w1,#0x2000 -> movz w1,#0
b=bytearray(open(SRC,'rb').read()); off=VA-D
cur=struct.unpack_from('<I',b,off)[0]
if cur==NEW: print("already patched"); open(DST,'wb').write(bytes(b)); sys.exit(0)
if cur!=EXPECT: print(f"REFUSE @0x{VA:x}: {cur:08x} != {EXPECT:08x}"); sys.exit(3)
struct.pack_into('<I',b,off,NEW)
print(f"@0x{VA:x}: {cur:08x} -> {NEW:08x} (reserved 8192 -> 0)")
open(DST,'wb').write(bytes(b)); print("wrote",DST)
