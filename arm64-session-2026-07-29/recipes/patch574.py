#!/usr/bin/env python3
"""§574 TEST: no-op Jit::MaybeDoOnStackReplacement — the interpreter's OSR hook.
Returns bool ("did OSR, don't continue interpreting"). Returning FALSE = "no OSR, keep
interpreting" = the safe answer. Patch entry to `mov w0,wzr; ret`. Keeps sampling (methods still
compile) but disables OSR, the classic interpreter<->compiled recursion path. Precedent §571.
"""
import struct, sys
SRC,DST=sys.argv[1],sys.argv[2]; D=0x1000; VA=0x9602d8
MOV=0x2a1f03e0; RET=0xd65f03c0
b=bytearray(open(SRC,'rb').read()); off=VA-D
cur=struct.unpack_from('<I',b,off)[0]
if cur==MOV: print("already"); open(DST,'wb').write(bytes(b)); sys.exit(0)
struct.pack_into('<I',b,off,MOV); struct.pack_into('<I',b,off+4,RET)
print("OSR @0x%x: %08x -> mov w0,wzr; ret"%(VA,cur)); open(DST,'wb').write(bytes(b)); print("wrote",DST)
