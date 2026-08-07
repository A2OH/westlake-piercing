#!/usr/bin/env python3
"""§564 — short-circuit libart's dead REALM predicates.

libart has 157 strstr call sites. 53 of them live in four Realm-specific helpers that DoCall invokes
on interpreted calls:

    25  PFCutTrySetRealmNativeBoundaryResult
    16  PFCutRealmNativeLooksLikeDataAccess
     6  PFCutTryRealmNativeState<true>
     6  PFCutTryRealmNativeState<false>

They are leftovers from an unrelated app port. noice contains **no Realm at all** — verified four
ways: 0 Lio/realm classes in the dex, 0 "realm" strings in the APK, no realm .so in the app dir, and
the helpers never log at runtime. So they can only ever return "no", after doing 47 strstr calls to
find that out, on every invoke.

Each returns bool and the callers test it with `tbnz w0, #0`, so returning 0 is exactly the
"not a Realm native" answer they would compute anyway — just without the scanning.

Patch: overwrite each entry with
    mov w0, wzr      ; 0x2a1f03e0   -> false
    ret              ; 0xd65f03c0

⛔BINARY PATCH, never a libart rebuild (forbidden). Precedent §534, §551.
Refuses unless the existing first instruction still looks like a real prologue, so a shifted symbol
cannot be silently clobbered.
"""
import struct, sys

SRC, DST = sys.argv[1], sys.argv[2]
D = 0x1000                       # exec segment: file_off = vaddr - 0x1000

TARGETS = {
    0xa66054: "PFCutTryRealmNativeState<false>",
    0xa828f4: "PFCutTryRealmNativeState<true>",
    0xa6b210: "PFCutRealmNativeLooksLikeDataAccess",
    0xa6b378: "PFCutTrySetRealmNativeBoundaryResult",
}
MOV_W0_0 = 0x2a1f03e0
RET      = 0xd65f03c0

blob = bytearray(open(SRC, 'rb').read())
for va, name in sorted(TARGETS.items()):
    off = va - D
    cur = struct.unpack_from('<I', blob, off)[0]
    # A real aarch64 prologue starts with stp/sub sp/str or similar; it is never already `mov w0,wzr`.
    if cur == MOV_W0_0:
        print("  %s @0x%x already patched" % (name, va)); continue
    top = cur >> 22
    looks_prologue = (cur & 0xFFC00000) in (0xA9800000, 0xA9000000, 0xD1000000, 0xF8000000,
                                            0xA9BC0000, 0xD5000000) or (cur >> 24) in (0xA9, 0xD1, 0xF8, 0xD5, 0x2A, 0x52, 0x39, 0xB9, 0xAA, 0x94, 0x14)
    if not looks_prologue:
        print("  REFUSE: %s @0x%x first insn %08x does not look like a prologue" % (name, va, cur))
        sys.exit(3)
    struct.pack_into('<I', blob, off + 0, MOV_W0_0)
    struct.pack_into('<I', blob, off + 4, RET)
    print("  %-38s @0x%x: %08x -> mov w0,wzr; ret" % (name, va, cur))

open(DST, 'wb').write(bytes(blob))
print("  wrote", DST)
