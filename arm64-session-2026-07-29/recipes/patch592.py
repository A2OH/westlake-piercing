#!/usr/bin/env python3
"""§592 — disable the PFCut hooks that are provably DEAD for noice.

DoCallCommon still calls 61 PFCut hooks per invoke (57 distinct). A hook returns bool; false =
"not handled, do the normal invoke", so a hook is disabled by patching its entry to
`mov w0,wzr; ret` (precedent §571/§575).

Disabled here, each with positive evidence:
  * McDonald's-app hooks (MdcLogger/JustFlipEvent/PerfAnalytics/NetworkBoundary) - noice dex has
    ZERO mcdonalds classes (established in §576).
  * NewRelic - zero newrelic classes (§576).
  * Realm - noice uses Room/SQLite, not Realm.
  * KotlinReflection, AndroidxWorkManager, AndroidxSplash, ClassNewInstance, ClassGetDeclaredField
    - the audit measured these firing 0 times.

⚠️Only .text symbols are touched; the `__emutls_v.*` symbols of the same name live in .data and
patching those would corrupt state. `_ZZ*`-mangled symbols (entities nested INSIDE a hook, e.g. a
static-local guard or lambda) are skipped too: the hook's own entry already returns, so they are
unreachable, and patching them would clobber something that is not a hook entry. Every target's first instruction is verified to be a real
function prologue first, so a shifted/aliased symbol can never be silently clobbered.
"""
import os,json,struct,sys,subprocess,re

NM=os.environ.get('WLROOT', os.path.expanduser('~'))+'/ohos-sdk-6.1/linux/native/llvm/bin/llvm-nm'
DEAD=['PFCutTryMcdLoggerNoop','PFCutTryMcdJustFlipEventNoop','PFCutTryMcdPerfAnalyticsNoop',
      'PFCutTryMcdNetworkBoundaryNoop','PFCutTryNewRelicNoop','PFCutTryRealmNativeBoundaryNoop',
      'PFCutTryKotlinReflectionFallback','PFCutTryAndroidxWorkManagerConstructorLite',
      'PFCutTryAndroidxSplashNoop','PFCutTryClassNewInstanceIntrinsic',
      'PFCutTryClassGetDeclaredFieldIntrinsic']
TEXT_VA,TEXT_OFF,TEXT_SZ=0x6df000,0x6de000,0x94182c
MOV_W0_WZR,RET=0x2a1f03e0,0xd65f03c0

src,dst=sys.argv[1],sys.argv[2]
b=bytearray(open(src,'rb').read())
syms=[]
for line in subprocess.run([NM,src],capture_output=True,text=True).stdout.splitlines():
    p=line.split()
    if len(p)<3 or p[1] not in ('t','T','W'): continue
    a=int(p[0],16)
    if not (TEXT_VA<=a<TEXT_VA+TEXT_SZ): continue          # .text only - never the emutls data copies
    if p[2].startswith('_ZZ'): continue   # nested-in-function entity, not the hook entry
    if any(d in p[2] for d in DEAD): syms.append((a,p[2]))

done=0
for a,name in sorted(set(syms)):
    off=TEXT_OFF+(a-TEXT_VA)
    cur=struct.unpack_from('<I',b,off)[0]
    if cur==MOV_W0_WZR:
        print(f"  already 0x{a:x}"); continue
    # accept only a real prologue: stp x29,x30,[sp,#..]! / sub sp,sp,#imm / str x..,[sp,#-..]!
    ok = (cur & 0xffc003e0)==0xa98003e0 or (cur & 0xff8003ff)==0xd10003ff or (cur & 0xffe003e0)==0xf8000fe0
    if not ok:
        print(f"  REFUSE 0x{a:x} ({cur:08x}) not a prologue: {name[:60]}"); sys.exit(3)
    struct.pack_into('<I',b,off,MOV_W0_WZR)
    struct.pack_into('<I',b,off+4,RET)
    print(f"  disable 0x{a:x}  {name[:76]}")
    done+=1
open(dst,'wb').write(bytes(b))
print(f"\ndisabled {done} dead hooks; wrote {dst}")
