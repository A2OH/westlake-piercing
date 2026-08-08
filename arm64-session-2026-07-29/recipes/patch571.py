#!/usr/bin/env python3
"""§571 — stop libart SWALLOWING uncaught exceptions (incl. StackOverflowError).

Three westlake helpers in the interpreter turn an uncaught exception into a silent no-op:

    0xa5d914  PFCutTryKotlinCoroutineExceptionNoop
    0xa5ee50  PFCutTryThreadGroupUncaughtExceptionNoop<false>
    0xa7e510  PFCutTryThreadGroupUncaughtExceptionNoop<true>

Each returns bool = "I handled it, skip the real invoke". Returning TRUE is what makes a
coroutine's StackOverflowError vanish: the thread dies silently, ART's post-overflow bookkeeping
never runs the way a real handler would leave it, and the very next stack check on ANY thread trips
instantly — which is why the main thread then dies with a trace only 5 frames deep. The real crash
is invisible; only the secondary one is reported.

Patch: return false at entry, so DoCall performs the REAL uncaughtException call.

    mov w0, wzr   ; 0x2a1f03e0  -> false ("not handled")
    ret           ; 0xd65f03c0

Same shape as the shipped §564 patch. ⛔BINARY PATCH — never a libart rebuild (blocked/forbidden).
Precedent: §534, §551, §564.

Refuses unless the first instruction still looks like the expected prologue, so a shifted symbol
cannot be silently clobbered.
"""
import struct, sys

SRC, DST = sys.argv[1], sys.argv[2]
D = 0x1000                      # .text: file_off = vaddr - 0x1000

TARGETS = {
    0xa5d914: "PFCutTryKotlinCoroutineExceptionNoop",
    0xa5ee50: "PFCutTryThreadGroupUncaughtExceptionNoop<false>",
    0xa7e510: "PFCutTryThreadGroupUncaughtExceptionNoop<true>",
}
EXPECT_FIRST = 0xd101c3ff       # sub sp, sp, #112  — verified in all three
MOV_W0_0 = 0x2a1f03e0
RET      = 0xd65f03c0

blob = bytearray(open(SRC, 'rb').read())
for va, name in sorted(TARGETS.items()):
    off = va - D
    cur = struct.unpack_from('<I', blob, off)[0]
    if cur == MOV_W0_0:
        print("  %-46s @0x%x already patched" % (name, va)); continue
    if cur != EXPECT_FIRST:
        print("  REFUSE: %s @0x%x first insn %08x != expected %08x" % (name, va, cur, EXPECT_FIRST))
        sys.exit(3)
    struct.pack_into('<I', blob, off + 0, MOV_W0_0)
    struct.pack_into('<I', blob, off + 4, RET)
    print("  %-46s @0x%x: %08x -> mov w0,wzr; ret" % (name, va, cur))

open(DST, 'wb').write(bytes(blob))
print("  wrote", DST)
