#!/usr/bin/env python3
"""§594 — gate 2: let NON-NATIVE (Java) methods reach art_quick_invoke_stub in ArtMethod::Invoke.

§588 found TWO kAccNative gates on the compiled-execution path. §586 patched only the first (in
ArtInterpreterToCompiledCodeBridge). That path then tail-calls ArtMethod::Invoke, which hits its OWN
gate here and interprets anyway — which is exactly why §586+§574 measured *exactly* interpreter speed.
This patches the second gate, so both must be applied to actually run compiled Java code.

  876cf8: cbz  x8, 0x876e24    ; entry_point == 0 -> other path   (KEPT: guards a missing entry)
  876cfc: tbnz w9,#8, 0x876d10 ; NATIVE only -> quick stub        <-- made unconditional
  876d00..876d0c               ; (Java fallthrough -> EnterInterpreterFromInvoke)  now dead
  876d10: ... -> bl art_quick_invoke_stub @0xfda330

This restores what stock AOSP ART does: ArtMethod::Invoke dispatches through the quick stub, which
marshals by shorty and calls entry_point_from_quick_compiled_code_. For a method with no JIT code the
entry point is the to-interpreter bridge, which the stub calls correctly.
"""
import struct,sys
D=0x1000; VA=0x876cfc; EXPECT=0x374000a9; NEW=0x14000000 | ((0x876d10-VA)>>2)
src,dst=sys.argv[1],sys.argv[2]
b=bytearray(open(src,'rb').read()); off=VA-D
cur=struct.unpack_from('<I',b,off)[0]
if cur==NEW: print("  already applied")
elif cur!=EXPECT: sys.exit(f"REFUSE @0x{VA:x}: {cur:08x} != expected {EXPECT:08x}")
else:
    struct.pack_into('<I',b,off,NEW)
    print(f"  @0x{VA:x}: tbnz w9,#8,0x876d10 -> b 0x876d10 ({NEW:08x})")
open(dst,'wb').write(bytes(b)); print("  wrote",dst)
