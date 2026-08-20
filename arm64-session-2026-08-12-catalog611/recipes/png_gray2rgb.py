#!/usr/bin/env python3
"""§611f: transcode grayscale PNGs (color type 0/4, depth 8) to RGB/RGBA (2/6) while
preserving every other chunk byte-identical — critically aapt's npTc nine-patch chunk,
which PIL would strip. This port's image decoder returns null for grayscale PNGs
(§553/§611 evidence), so framework/appcompat *_mtrl_alpha art fails to inflate."""
import struct, zlib, sys

def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc: return a
    if pb <= pc: return b
    return c

def _unfilter(raw, w, h, bpp):
    stride = w * bpp
    out = bytearray()
    pos = 0
    prev = bytearray(stride)
    for _ in range(h):
        ft = raw[pos]; pos += 1
        line = bytearray(raw[pos:pos + stride]); pos += stride
        if ft == 1:
            for i in range(bpp, stride): line[i] = (line[i] + line[i - bpp]) & 0xFF
        elif ft == 2:
            for i in range(stride): line[i] = (line[i] + prev[i]) & 0xFF
        elif ft == 3:
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 0xFF
        elif ft == 4:
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                ul = prev[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + _paeth(left, prev[i], ul)) & 0xFF
        elif ft != 0:
            raise ValueError(f'filter {ft}')
        out += line
        prev = line
    return bytes(out)

def transcode(data):
    """Returns transcoded bytes, or None if not applicable (non-gray, 16-bit, interlaced)."""
    if data[:8] != b'\x89PNG\r\n\x1a\n': return None
    chunks = []
    pos = 8
    while pos < len(data):
        ln, typ = struct.unpack('>I4s', data[pos:pos + 8])
        chunks.append((typ, data[pos + 8:pos + 8 + ln]))
        pos += 12 + ln
    ihdr = next(c for t, c in chunks if t == b'IHDR')
    w, h, depth, ctype, comp, filt, inter = struct.unpack('>IIBBBBB', ihdr)
    if ctype not in (0, 4) or depth != 8 or inter != 0: return None
    bpp = 1 if ctype == 0 else 2
    raw = _unfilter(zlib.decompress(b''.join(c for t, c in chunks if t == b'IDAT')), w, h, bpp)
    out_rows = bytearray()
    nbpp = 3 if ctype == 0 else 4
    for y in range(h):
        out_rows.append(0)  # filter None
        row = raw[y * w * bpp:(y + 1) * w * bpp]
        if ctype == 0:
            for x in range(w):
                g = row[x]; out_rows += bytes((g, g, g))
        else:
            for x in range(w):
                g, a = row[2 * x], row[2 * x + 1]; out_rows += bytes((g, g, g, a))
    new_ihdr = struct.pack('>IIBBBBB', w, h, 8, 2 if ctype == 0 else 6, comp, filt, inter)
    new_idat = zlib.compress(bytes(out_rows), 9)
    out = bytearray(b'\x89PNG\r\n\x1a\n')
    idat_done = False
    for typ, body in chunks:
        if typ == b'IHDR': body = new_ihdr
        elif typ == b'IDAT':
            if idat_done: continue
            body = new_idat; idat_done = True
        elif typ == b'tRNS' and ctype == 0:
            g = struct.unpack('>H', body)[0]
            body = struct.pack('>HHH', g, g, g)
        out += struct.pack('>I', len(body)) + typ + body
        out += struct.pack('>I', zlib.crc32(typ + body) & 0xFFFFFFFF)
    return bytes(out)

def sweep_apk(src_path, dst_path):
    import zipfile
    src = zipfile.ZipFile(src_path)
    out = zipfile.ZipFile(dst_path, 'w')
    done = skipped = 0
    for item in src.infolist():
        data = src.read(item.filename)
        if item.filename.endswith('.png') and len(data) > 26 and data[25] in (0, 4):
            try:
                t = transcode(data)
                if t is not None:
                    data = t; done += 1
                else:
                    skipped += 1
            except Exception as e:
                print('  SKIP', item.filename, e); skipped += 1
        out.writestr(item, data, item.compress_type)
    out.close()
    print(f'{src_path}: transcoded {done}, skipped {skipped}')

if __name__ == '__main__':
    sweep_apk(sys.argv[1], sys.argv[2])
