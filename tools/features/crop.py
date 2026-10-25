"""Crops and scales (nearest) a region of an RGBA PNG made by tpf_png.py: crop.py in.png out.png x y w h scale"""
import struct, sys, zlib
def load(p):
    d = open(p, 'rb').read(); i = 8; idat = b''
    while i < len(d):
        n = struct.unpack_from('>I', d, i)[0]; t = d[i + 4:i + 8]; c = d[i + 8:i + 8 + n]
        if t == b'IHDR': w, h = struct.unpack_from('>II', c)
        if t == b'IDAT': idat += c
        i += 12 + n
    raw = zlib.decompress(idat); rows = []
    prev = bytearray(w * 4)
    for y in range(h):
        f = raw[y * (w * 4 + 1)]; line = bytearray(raw[y * (w * 4 + 1) + 1:(y + 1) * (w * 4 + 1)])
        for x in range(w * 4):
            a = line[x - 4] if x >= 4 else 0; b = prev[x]; c = prev[x - 4] if x >= 4 else 0
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(line); prev = line
    return w, h, rows
def save(p, w, h, rows):
    def chunk(t, c): return struct.pack('>I', len(c)) + t + c + struct.pack('>I', zlib.crc32(t + c))
    data = b''.join(b'\0' + bytes(r) for r in rows)
    open(p, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(data)) + chunk(b'IEND', b''))
if __name__ == '__main__':
    w, h, rows = load(sys.argv[1]); x, y, cw, ch, s = map(int, sys.argv[3:8])
    out = []
    for yy in range(ch * s):
        r = rows[y + yy // s]; line = bytearray()
        for xx in range(cw * s):
            px = r[(x + xx // s) * 4:(x + xx // s) * 4 + 4]; px = bytearray(px); px[3] = 255; line += px
        out.append(line)
    save(sys.argv[2], cw * s, ch * s, out)
