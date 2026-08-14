import sys, struct
def resolve(path, idx=1865):
    d = open(path,'rb').read()
    if d[:4] != b'dex\n': return path + ': not a dex'
    def u4(o): return struct.unpack('<I', d[o:o+4])[0]
    def u2(o): return struct.unpack('<H', d[o:o+2])[0]
    str_off = u4(0x3C); type_off = u4(0x44); proto_off = u4(0x4C)
    msz, moff = u4(0x58), u4(0x5C)
    if idx >= msz: return f'{path}: only {msz} methods'
    def rs(i):
        o = u4(str_off + i*4); p = o
        while d[p] & 0x80: p += 1
        p += 1
        return d[p:d.index(0,p)].decode('utf-8','replace')
    def tn(t): return rs(u4(type_off + t*4))
    mo = moff + idx*8
    cls, proto, name = u2(mo), u2(mo+2), u4(mo+4)
    po = proto_off + proto*12
    return f'{path}\n  -> {tn(cls)} {rs(name)} shorty={rs(u4(po))}'
for p in sys.argv[1:]:
    try: print(resolve(p))
    except Exception as e: print(p, 'ERR', e)
