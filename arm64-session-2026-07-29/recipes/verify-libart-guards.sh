#!/bin/bash
# Verify a libart.so actually CONTAINS the guards we believe are shipped.
# Written because §551 was recorded as shipped for weeks while the running binary never had it —
# the §436 guard had been dropped by a later rebuild, and the launch "lottery" was the result.
# RUN THIS AFTER ANY LIBART REBASE.   usage: verify-libart-guards.sh <libart.so>
python3 - "$1" <<'PY'
import struct,sys
f=sys.argv[1]; b=open(f,'rb').read(); D=0x1000
SITE,CAVE=0xa89794,0xfd9818
PRISTINE=[0x924006e8,0xb5003d88,0xb94002e8,0x34000428,0xb9404109]
cur=[struct.unpack_from('<I',b,SITE-D+4*i)[0] for i in range(5)]
cave=[struct.unpack_from('<I',b,CAVE-D+4*i)[0] for i in range(6)]
print(f"{f}")
if cur==PRISTINE and all(w==0 for w in cave):
    print("  §551 §436-guard : ⛔ MISSING  -> launches will hang in a SIGSEGV storm (addr=0x45).")
    print("                     fix: python3 patch551.py <in> <out>")
    sys.exit(1)
if cur[3]==0x1415401e and cave[1]==0x7140051f:
    print("  §551 §436-guard : ✅ present (a897a0 -> cave; cave has cmp w8,#4096 / b.lo bail)")
else:
    print(f"  §551 §436-guard : ⚠️ UNRECOGNISED  site[3]={cur[3]:08x} cave[1]={cave[1]:08x}")
    sys.exit(2)
PY
