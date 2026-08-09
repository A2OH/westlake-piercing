#!/usr/bin/env python3
"""§580 INSTRUMENT: log the caller (LR) of ThrowStackOverflowError to stderr.
Trampoline in the code cave @0xfd9818: save regs, write "LRX=" + raw 8-byte x30 (caller return addr)
to fd 2, restore, run the displaced first instruction, branch back. Patch ThrowStackOverflowError
@0x8574e8 first insn -> b cave. Lets us NAME the exact throw site from the headless bench. Removable."""
import struct,sys
SRC,DST=sys.argv[1],sys.argv[2]; D=0x1000
CAVE_VA=0xfd9818
THROW_VA=0x8574e8
RET_VA=0x8574ec               # 2nd insn of ThrowStackOverflowError
body=open('/tmp/claude-1000/-home-dspfac-openharmony/969523b9-c069-43df-983b-1395ea6611d2/scratchpad/cave.bin','rb').read()
assert len(body)==64, len(body)
def B(from_va,to_va):
    off=(to_va-from_va)>>2
    return 0x14000000 | (off & 0x03ffffff)
b=bytearray(open(SRC,'rb').read())
# sanity: cave must be zero; throw entry must be the known prologue
coff=CAVE_VA-D; toff=THROW_VA-D
assert b[coff:coff+len(body)+4]==b'\x00'*(len(body)+4), "cave not free"
cur=struct.unpack_from('<I',b,toff)[0]
assert cur==0xd10103ff, f"throw[0]={cur:08x} not sub sp,sp,#64"
# write cave body + final branch back to RET_VA
b[coff:coff+64]=body
struct.pack_into('<I', b, coff+64, B(CAVE_VA+64, RET_VA))
# patch throw entry -> b cave
struct.pack_into('<I', b, toff, B(THROW_VA, CAVE_VA))
open(DST,'wb').write(bytes(b))
print(f"cave@0x{CAVE_VA:x}: body(64)+b->0x{RET_VA:x}; throw@0x{THROW_VA:x} -> b 0x{CAVE_VA:x}")
print(f"  throw entry now: {struct.unpack_from('<I',b,toff)[0]:08x}")
print(f"  cave last b:     {struct.unpack_from('<I',b,coff+64)[0]:08x}")
