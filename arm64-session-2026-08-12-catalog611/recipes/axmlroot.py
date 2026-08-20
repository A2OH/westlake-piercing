import struct, zipfile, sys
def strings(d):
    # chunk 0x0001 string pool right after 8-byte header
    assert struct.unpack('<H', d[0:2])[0] == 0x0003
    off = 8
    t, hs, sz = struct.unpack('<HHI', d[off:off+8])
    assert t == 0x0001
    n, styles, flags, soff, _ = struct.unpack('<IIIII', d[off+8:off+28])
    utf8 = bool(flags & (1<<8))
    idx = [struct.unpack('<I', d[off+28+i*4:off+32+i*4])[0] for i in range(n)]
    base = off + soff
    out = []
    for i in idx:
        p = base + i
        if utf8:
            l = d[p+1]; p += 2
            out.append(d[p:p+l].decode('utf-8','replace'))
        else:
            l = struct.unpack('<H', d[p:p+2])[0]; p += 2
            out.append(d[p:p+2*l].decode('utf-16le','replace'))
    return out, off + sz
def root_tag(d):
    ss, p = strings(d)
    while p < len(d):
        t, hs, sz = struct.unpack('<HHI', d[p:p+8])
        if t == 0x0102:  # start element
            name_idx = struct.unpack('<I', d[p+20:p+24])[0]
            return ss[name_idx]
        p += sz
    return None
if __name__ == '__main__':
    z = zipfile.ZipFile(sys.argv[1])
    prefix = sys.argv[2] if len(sys.argv) > 2 else 'res/drawable'
    for n in z.namelist():
        if n.startswith(prefix) and n.endswith('.xml'):
            try:
                tag = root_tag(z.read(n))
                if tag in ('shape','color','inset'):
                    print(len(z.read(n)), tag, n)
            except Exception: pass
