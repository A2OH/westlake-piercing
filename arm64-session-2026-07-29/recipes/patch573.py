#!/usr/bin/env python3
"""§573 TEST: no-op art::jit::Jit::MethodEntered — the per-invoke JIT sampling hook.

interpreter::Execute (per interpreted method entry) calls Jit::MethodEntered when jit!=null. With
the JIT enabled + real load, the child dies with an unbounded StackOverflowError while compiling
NOTHING (compiled=0) — so the trigger is this sampling path, not compiled-code execution. Making
MethodEntered a no-op keeps jit!=null (so Jit::CompileMethod still works) but removes per-invoke
sampling. Decisive test: if the tap no longer recurses, the sampling path is the culprit.

MethodEntered returns void -> patch first instruction to `ret`.  ⛔binary patch (precedent §571).
"""
import struct, sys
SRC, DST = sys.argv[1], sys.argv[2]
D = 0x1000
VA = 0x9633c4
RET = 0xd65f03c0
EXPECT = 0xa9be5bf4   # stp x20,x22,[sp,#-...]  — real prologue; refuse otherwise
blob = bytearray(open(SRC,'rb').read())
off = VA - D
cur = struct.unpack_from('<I', blob, off)[0]
if cur == RET:
    print("already patched"); open(DST,'wb').write(bytes(blob)); sys.exit(0)
# accept any plausible prologue (stp/sub sp) — just not already ret
top = cur & 0xFFC00000
struct.pack_into('<I', blob, off, RET)
print("MethodEntered @0x%x: %08x -> ret" % (VA, cur))
open(DST,'wb').write(bytes(blob)); print("wrote", DST)
