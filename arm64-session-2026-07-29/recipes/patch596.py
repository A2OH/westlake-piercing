#!/usr/bin/env python3
"""§596 — make the JIT emit EXPLICIT null checks instead of trapping implicit ones.

With both kAccNative gates opened (§586+§594) compiled Java code finally executes — but it runs ~17x
SLOWER than the interpreter because it takes **~7,200 SIGSEGV/s** (measured: 35,923 signo(11) in 5s).
Cause: ART's implicit null checks are a deliberate fault (`ldr wzr,[xN]` on a null ref) that a signal
handler turns into an NPE. That is cheap on Android; on OHOS the signal path is brutal — musl's
sigchain plus the DFX handler, which **unwinds the stack on every signal** (`unwind_frame` /
`walk_stackframe` dominate the profile).

`CodeGenerator::GenerateNullCheck` @0xc6bde0 picks the strategy:
    c6bde8: ldrb w8, [x8, #226]     ; compiler_options_.implicit_null_checks_
    c6be14: cmp  w8, #0
    c6be18: mov  w8, #272           ; vtable slot -> GenerateImplicitNullCheck
    c6be1c: mov  w9, #280           ; vtable slot -> GenerateExplicitNullCheck
    c6be20: csel x8, x9, x8, eq     ; flag==0 -> 280 (EXPLICIT)
Forcing the flag to 0 makes every compiled null check an explicit compare-and-branch, so no fault,
no signal, no unwind. `-Ximplicit-checks:none` would do this in stock ART, but that option is not
wired into this build (APPSPAWNX_EXPLICIT_CHECKS is absent from child_main.cpp).

⚠️Only the NULL-check strategy changes; implicit stack-overflow/suspend checks are untouched.
"""
import struct,sys
D=0x1000; VA=0xc6bde8; EXPECT=0x39438908; NEW=0x2a1f03e8      # mov w8, wzr
src,dst=sys.argv[1],sys.argv[2]
b=bytearray(open(src,'rb').read()); off=VA-D
cur=struct.unpack_from('<I',b,off)[0]
if cur==NEW: print("  already applied")
elif cur!=EXPECT: sys.exit(f"REFUSE @0x{VA:x}: {cur:08x} != {EXPECT:08x}")
else:
    struct.pack_into('<I',b,off,NEW)
    print(f"  @0x{VA:x}: ldrb w8,[x8,#226] -> mov w8,wzr ({NEW:08x})  => EXPLICIT null checks")
open(dst,'wb').write(bytes(b)); print("  wrote",dst)
