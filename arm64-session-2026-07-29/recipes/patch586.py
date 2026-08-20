#!/usr/bin/env python3
"""§586 — gate 1: stop ArtInterpreterToCompiledCodeBridge force-interpreting non-native methods.

§588 found TWO kAccNative gates on the compiled-execution path; this is the first. The bridge is
entered ONLY for methods that already have JIT code (Execute gates on Jit::CanInvokeCompiledCode),
yet its non-native path never reads the entry point and unconditionally redirects to
ArtInterpreterToInterpreterBridge -- which re-enters Execute, which routes the same method back in:
    Execute -> ToCompiledCodeBridge -> ToInterpreterBridge -> Execute -> ...  (§583 frame walk)
That infinite recursion is what the StackOverflowError was reporting.

  a51938: tbnz w8, #8, 0xa51a28   ; kAccNative only -> real compiled-invoke path
                                   <-- made unconditional
  a51a28: ... loads + validates entry_point_from_quick_compiled_code_ and calls it

MUST be paired with §594 (gate 2, in ArtMethod::Invoke) -- with only this one applied, the path
tail-calls ArtMethod::Invoke, which interprets anyway (that is exactly why §586+§574 measured
*exactly* interpreter speed), and with §597 (explicit suspend checks) or compiled code takes
~7,200 SIGSEGV/s.

Written 2026-08-10 (§603e): the §594/§596/§597 recipes existed but §586 had never been scripted,
so the binary-patched JIT stack could not be reproduced from recipes alone.
"""
import struct, sys

D = 0x1000
VA = 0xa51938
TARGET = 0xa51a28
EXPECT = 0x37400788                                  # tbnz w8, #8, +0xf0
NEW = 0x14000000 | ((TARGET - VA) >> 2)              # b 0xa51a28

src, dst = sys.argv[1], sys.argv[2]
b = bytearray(open(src, 'rb').read())
off = VA - D
cur = struct.unpack_from('<I', b, off)[0]
if cur == NEW:
    print("  already applied")
elif cur != EXPECT:
    sys.exit(f"REFUSE @0x{VA:x}: {cur:08x} != expected {EXPECT:08x}")
else:
    struct.pack_into('<I', b, off, NEW)
    print(f"  §586 @0x{VA:x}: {EXPECT:08x} -> {NEW:08x}  (b 0x{TARGET:x})")
open(dst, 'wb').write(bytes(b))
print(f"  wrote {dst}")
