#!/usr/bin/env python3
"""§597 — make the JIT emit EXPLICIT SUSPEND checks. This is the real fault storm.

§596 forced explicit NULL checks and the fault rate did not move (35846 vs 35923 signo(11) per 5s),
so null checks were not the cause. The signature — `addr=0`, a FIXED pc inside the JIT code cache,
~7,200 faults/s — is an **implicit suspend check**: ART emits one on every loop back-edge, as a load
through a dedicated register pointing at a page that is mprotect()ed to request a suspend. In this
port that mechanism is not set up, so the load hits address 0 and traps EVERY iteration. On OHOS each
trap costs a musl sigchain + DFX handler round trip that unwinds the stack, so a hot loop crawls.

  arm64 InstructionCodeGeneratorARM64::GenerateSuspendCheck @0xb829c8:
      b829fc: ldrb w8, [x8, #228]   ; compiler_options_.implicit_suspend_checks_
      b82a00: cbz  w8, 0xb82b48     ; flag == 0 -> EXPLICIT suspend check
Forcing the flag to 0 emits an explicit test of the thread flags instead of a trapping load.
(implicit_null_checks_ is the neighbouring byte at #226 - see §596.)
"""
import struct,sys
D=0x1000; VA=0xb829fc; EXPECT=0x39439108; NEW=0x2a1f03e8      # mov w8, wzr
src,dst=sys.argv[1],sys.argv[2]
b=bytearray(open(src,'rb').read()); off=VA-D
cur=struct.unpack_from('<I',b,off)[0]
if cur==NEW: print("  already applied")
elif cur!=EXPECT: sys.exit(f"REFUSE @0x{VA:x}: {cur:08x} != {EXPECT:08x}")
else:
    struct.pack_into('<I',b,off,NEW)
    print(f"  @0x{VA:x}: ldrb w8,[x8,#228] -> mov w8,wzr  => EXPLICIT suspend checks")
open(dst,'wb').write(bytes(b)); print("  wrote",dst)
