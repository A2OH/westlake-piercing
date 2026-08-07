#!/usr/bin/env python3
"""§551 — §436 SEGV guard, done properly with a code cave.

§550 tried to make room by reusing the `and x8,x23,#3 / cbnz x8,L_bail` slots (the 4-alignment test
on called_method). That was wrong: the alignment test was doing real work. With it gone, an
unaligned garbage pointer reached the load and the crash simply changed shape —
    before §550:  sig=11 code=1 addr=0x45        pc=+0xa897a4  (deref of declaring_class_ == 5)
    after  §550:  sig=11 code=2 addr=0x266489cf  pc=+0xa89794  (0x266489cf & 3 == 3, unaligned)
So BOTH guards are needed, which needs more instruction slots than exist inline -> code cave.

Cave = 488 bytes of inter-function padding at vaddr 0xfd9818, immediately after
`JNI_OnLoad_binder_with_cl`'s `ret`, inside .text (512-byte aligned). Not referenced by anything.

Layout (original instructions all preserved; only the CBZ slot is redirected):

    a89794: and  x8, x23, #0x3      unchanged  (alignment guard KEPT)
    a89798: cbnz x8, L_bail         unchanged
    a8979c: ldr  w8, [x23]          unchanged
    a897a0: b    cave               <-- was `cbz w8, L_skip`
    a897a4: ldr  w9, [x8, #64]      unchanged

    cave+0 : cbz  w8, cave+16       ; original "declaring_class == 0 -> L_skip"
    cave+4 : cmp  w8, #1, lsl #12   ; NEW plausibility test on declaring_class_
    cave+8 : b.lo cave+20           ; garbage -> bail
    cave+12: b    a897a4            ; ok -> resume inline
    cave+16: b    L_skip            ; (long branches: cbz/b.cond only reach +-1MB)
    cave+20: b    L_bail
"""
import struct, sys

SRC, DST = sys.argv[1], sys.argv[2]
D = 0x1000                                   # exec segment: file_off = vaddr - 0x1000
SITE, LDR = 0xa89794, 0xa897a4
L_SKIP, L_BAIL, CAVE = 0xa89824, 0xa89f48, 0xfd9818

def b(frm, to):            # B (imm26, +-128MB)
    return 0x14000000 | (((to - frm) >> 2) & 0x03FFFFFF)
def cbz_w(rt, frm, to):    # CBZ Wt (imm19, +-1MB)
    off = (to - frm) >> 2
    assert -(1 << 18) <= off < (1 << 18), "cbz out of range"
    return 0x34000000 | ((off & 0x7FFFF) << 5) | rt
def bcond(cond, frm, to):  # B.cond (imm19, +-1MB)
    off = (to - frm) >> 2
    assert -(1 << 18) <= off < (1 << 18), "b.cond out of range"
    return 0x54000000 | ((off & 0x7FFFF) << 5) | cond
COND_LO = 3

EXPECT = [0x924006e8, 0xb5003d88, 0xb94002e8, 0x34000428, 0xb9404109]

blob = bytearray(open(SRC, 'rb').read())
off = SITE - D
cur = [struct.unpack_from('<I', blob, off + 4*i)[0] for i in range(5)]
print("  site vaddr 0x%x (file 0x%x)" % (SITE, off))
for i, (c, e) in enumerate(zip(cur, EXPECT)):
    print("    [%d] %08x expect %08x %s" % (i, c, e, "OK" if c == e else "MISMATCH"))
if cur != EXPECT:
    sys.exit("REFUSE: site is not the pristine pre-550 sequence — start from the ORIGINAL libart")

# cave must still be all zeros
cave_off = CAVE - D
if any(blob[cave_off + i] for i in range(24)):
    sys.exit("REFUSE: cave at 0x%x is not zero padding" % CAVE)

struct.pack_into('<I', blob, off + 12, b(SITE + 12, CAVE))          # a897a0: b cave

cave = [cbz_w(8, CAVE + 0,  CAVE + 16),
        0x7140051f,
        bcond(COND_LO, CAVE + 8, CAVE + 20),
        b(CAVE + 12, LDR),
        b(CAVE + 16, L_SKIP),
        b(CAVE + 20, L_BAIL)]
for i, w in enumerate(cave):
    struct.pack_into('<I', blob, cave_off + 4*i, w)

open(DST, 'wb').write(bytes(blob))
print("  patched -> %s" % DST)
print("    a897a0: %08x  (b cave)" % b(SITE + 12, CAVE))
for i, w in enumerate(cave):
    print("    cave+%-2d: %08x" % (4*i, w))
