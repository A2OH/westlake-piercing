#!/usr/bin/env python3
"""Extract the literal needle string behind every `bl strstr` in DoCall<false>/<true>.

⚠️THE TRAP: `.text` maps file_off = vaddr - 0x1000, but `.rodata` maps IDENTITY in this .so.
Using the .text rule on a .rodata pointer silently yields a *different, plausible-looking* string
(4096 bytes off) — which is how an earlier pass "found" needles like 'CumulativeLoggerLock'.
Always resolve vaddr->offset from the section headers, as below.

usage: extract_needles.py <libart.so> [start_va] [end_va]   (defaults cover both DoCall templates)
"""
import os,re,subprocess,sys,json
LIB=sys.argv[1]
START=sys.argv[2] if len(sys.argv)>2 else '0xa88584'
END  =sys.argv[3] if len(sys.argv)>3 else '0xa8c400'
SDK=os.environ.get('WLROOT', os.path.expanduser('~'))+'/ohos-sdk-6.1/linux/native/llvm/bin'
secs=[]
for m in re.finditer(r'\[\s*\d+\]\s+(\S+)\s+\S+\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)',
                     subprocess.run([f'{SDK}/llvm-readelf','-S',LIB],capture_output=True,text=True).stdout):
    a,o,s=int(m.group(2),16),int(m.group(3),16),int(m.group(4),16)
    if a: secs.append((a,o,s))
b=open(LIB,'rb').read()
def cstr(va):
    for a,o,s in secs:
        if a<=va<a+s:
            off=o+(va-a); e=b.find(b'\0',off)
            try: return b[off:e].decode('utf-8')
            except: return repr(b[off:e])
d=subprocess.run([f'{SDK}/llvm-objdump','-d',f'--start-address={START}',f'--stop-address={END}',LIB],
                 capture_output=True,text=True).stdout
pa=re.compile(r'^\s*([0-9a-f]+):\s+\S+\s+adrp\s+x1,\s*0x([0-9a-f]+)')
pd=re.compile(r'^\s*([0-9a-f]+):\s+\S+\s+add\s+x1,\s*x1,\s*#(\d+)')
pb=re.compile(r'^\s*([0-9a-f]+):\s+\S+\s+bl\s+0x\S+ <strstr@plt>')
page=imm=None; g={}
for line in d.splitlines():
    m=pa.match(line)
    if m: page=int(m.group(2),16); imm=None; continue
    m=pd.match(line)
    if m: imm=int(m.group(2)); continue
    m=pb.match(line)
    if m:
        s=cstr(page+imm) if page is not None and imm is not None else None
        g.setdefault(s,[]).append(int(m.group(1),16)); page=imm=None
for s,ss in sorted(g.items(), key=lambda kv:-len(kv[1])): print(f"{len(ss)}x {s!r}  {[hex(x) for x in ss]}")
json.dump(g,open('live_needles.json','w'),indent=1)
