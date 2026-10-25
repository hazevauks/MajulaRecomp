"""Decodes a one-texture Xbox 360 TPF (DXT5, tiled, 16-bit swapped) to PNG: python tpf_png.py in.tpf out.png"""
import struct, sys, zlib

def tiled(x, y, width, log_bpp):
    aligned = (width + 31) & ~31
    macro = ((x >> 5) + (y >> 5) * (aligned >> 5)) << (log_bpp + 7)
    micro = ((x & 7) + ((y & 0xE) << 2)) << log_bpp
    off = macro + ((micro & ~0xF) << 1) + (micro & 0xF) + ((y & 1) << 4)
    return ((off & ~0x1FF) << 3) + ((y & 16) << 7) + ((off & 0x1C0) << 2) + ((((y & 8) >> 2) + (x >> 3)) & 3) * 64 + (off & 0x3F)

def c565(c):
    r, g, b = (c >> 11) & 31, (c >> 5) & 63, c & 31
    return [(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)]

def block(b):
    a0, a1 = b[0], b[1]
    pa = [a0, a1] + ([((6 - i) * a0 + (i + 1) * a1) // 7 for i in range(6)] if a0 > a1 else
                     [((4 - i) * a0 + (i + 1) * a1) // 5 for i in range(4)] + [0, 255])
    ab = int.from_bytes(b[2:8], 'little')
    c0, c1 = b[8] | b[9] << 8, b[10] | b[11] << 8
    p0, p1 = c565(c0), c565(c1)
    pal = [p0, p1, [(2 * p0[i] + p1[i]) // 3 for i in range(3)], [(p0[i] + 2 * p1[i]) // 3 for i in range(3)]]
    cb = int.from_bytes(b[12:16], 'little')
    return [pal[(cb >> 2 * i) & 3] + [pa[(ab >> 3 * i) & 7]] for i in range(16)]

def main():
    d = open(sys.argv[1], 'rb').read()
    off, size = struct.unpack_from('>II', d, 0x10)
    w = struct.unpack_from('>H', d, 0x1C)[0]
    h = size // w
    data = d[off:off + size]
    bw, bh = w // 4, h // 4
    img = bytearray(w * h * 4)
    for by in range(bh):
        for bx in range(bw):
            o = tiled(bx, by, bw, 4)
            raw = bytearray(data[o:o + 16])
            for i in range(0, 16, 2): raw[i], raw[i + 1] = raw[i + 1], raw[i]
            px = block(raw)
            for i in range(16):
                x, y = bx * 4 + (i & 3), by * 4 + (i >> 2)
                img[(y * w + x) * 4:(y * w + x) * 4 + 4] = bytes(px[i])
    rows = b''.join(b'\0' + bytes(img[y * w * 4:(y + 1) * w * 4]) for y in range(h))
    def chunk(t, c): return struct.pack('>I', len(c)) + t + c + struct.pack('>I', zlib.crc32(t + c))
    open(sys.argv[2], 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) +
                                  chunk(b'IDAT', zlib.compress(rows, 6)) + chunk(b'IEND', b''))
    print(w, h)

if __name__ == '__main__':
    main()
