#!/usr/bin/env python3
"""§550 — stop the §436 SEGV: guard the declaring_class_ load in DoCall<false>.

The crash (reproduced, sig=11 code=1 addr=0x45, pc=libart.so+0xa897a4) is:

    a89774: bl   Class::FindVirtualMethodForInterface(...)   ; called_method from the IMT
    a89778: mov  x23, x0
    a89780: cbz  x0, L_bail          \\
    a89788: cbnz x8, L_bail           |  westlake already sanity-checks the RETURNED POINTER:
    a89790: b.ls L_bail               |  non-null, no bits above 48, > 65536, 4-aligned
    a89798: cbnz x8, L_bail          /
    a8979c: ldr  w8, [x23]           ; called_method->declaring_class_   <-- NOT checked
    a897a0: cbz  w8, L_skip          ; only tests == 0
    a897a4: ldr  w9, [x8, #64]       ; FAULT: x8 == 5, so addr = 5 + 0x40 = 0x45
    a897a8: tbz  w9, #18, L_skip     ; bit 18 = kAccClassIsProxy  (this is GetInterfaceMethodIfProxy)

So the IMT hands back a plausible-looking but corrupt ArtMethod whose declaring_class_ is garbage
(5), and the guard chain covers the pointer but not the field it immediately dereferences. §318
already recorded this boot image as "IMT-incompatible", which fits.

FIX: extend the existing guard style to the loaded value — if declaring_class_ is not a plausible
heap reference, take L_bail, the same path every other guard failure already takes (verified to be a
normal continuation that reloads x23 from [sp,#40], not an abort). Converts a hard crash into the
already-exercised fallback.

Room is made by dropping the 4-alignment test on x23 (a89794/a89798), which is the weakest of the
four pointer checks and is redundant once the far stronger field check exists.

    a89794: ldr  w8, [x23]        ; load declaring_class_ early
    a89798: cbz  w8, L_skip       ; preserve original "== 0 -> skip" behaviour exactly
    a8979c: cmp  w8, #1, lsl #12  ; plausible heap ref? (>= 0x1000)
    a897a0: b.lo L_bail           ; NEW: garbage -> bail instead of faulting
    a897a4: ldr  w9, [x8, #64]    ; unchanged

⛔This is a BINARY PATCH, not a libart rebuild (rebuilding is forbidden: the deployed image is not
reproducible from source). Precedent: §534's one-instruction libart patch.
"""
import shutil, struct, sys

SRC, DST = sys.argv[1], sys.argv[2]
VADDR_BASE, FILE_DELTA = 0xa89794, 0x1000        # exec segment: file_off = vaddr - 0x1000
OFF = VADDR_BASE - FILE_DELTA

L_SKIP, L_BAIL = 0xa89824, 0xa89f48

def cbz_w(rt, frm, to):                      # CBZ Wt, label
    return 0x34000000 | ((((to - frm) >> 2) & 0x7FFFF) << 5) | rt
def bcond(cond, frm, to):                    # B.cond label
    return 0x54000000 | ((((to - frm) >> 2) & 0x7FFFF) << 5) | cond
COND_LO = 3

EXPECT = [0x924006e8,   # and  x8, x23, #0x3
          0xb5003d88,   # cbnz x8, L_bail
          0xb94002e8,   # ldr  w8, [x23]
          0x34000428,   # cbz  w8, L_skip
          0xb9404109]   # ldr  w9, [x8, #64]      (kept, must still be here)

NEW = [0xb94002e8,                        # ldr  w8, [x23]
       cbz_w(8, 0xa89798, L_SKIP),        # cbz  w8, L_skip
       0x7140051f,                        # cmp  w8, #1, lsl #12   (subs wzr, w8, #0x1000)
       bcond(COND_LO, 0xa897a0, L_BAIL),  # b.lo L_bail
       0xb9404109]                        # ldr  w9, [x8, #64]     (unchanged)

blob = bytearray(open(SRC, 'rb').read())
cur = [struct.unpack_from('<I', blob, OFF + 4*i)[0] for i in range(5)]
print("  at vaddr 0x%x (file 0x%x)" % (VADDR_BASE, OFF))
for i, (c, e) in enumerate(zip(cur, EXPECT)):
    print("    [%d] found %08x expect %08x %s" % (i, c, e, "OK" if c == e else "MISMATCH"))
if cur != EXPECT:
    sys.exit("REFUSE: instructions are not what §550 was written against")

for i, w in enumerate(NEW):
    struct.pack_into('<I', blob, OFF + 4*i, w)
open(DST, 'wb').write(bytes(blob))
print("  patched ->", DST)
for i, w in enumerate(NEW):
    print("    [%d] %08x" % (i, w))
