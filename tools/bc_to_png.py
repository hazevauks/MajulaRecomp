#!/usr/bin/env python3
"""Decodes a texture dump of the native renderer (DS2_DUMP_TEXTURE=<guest base>: out/pc/texture_<base>_<w>x<h>_f<format>_
l<levels>.bin, the bytes as uploaded: untiled, host byte order) to PNG files, to see what the game's own texture holds,
independently of how a draw used it.

  python tools/bc_to_png.py out/pc/texture_1426D000_1024x256_f20_l1.bin

Formats: 18 (DXT1 / BC1), 19 (DXT2/3 / BC2), 20 (DXT4/5 / BC3). Writes <name>_rgb.png (colour) and <name>_a.png (alpha as
grey), base level only.
"""
import os
import struct
import sys
import zlib


def write_png(path, width, height, rgb):
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)
    stride = width * 3
    rows = b''.join(b'\0' + rgb[y * stride:(y + 1) * stride] for y in range(height))
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
                + chunk(b'IDAT', zlib.compress(rows, 6)) + chunk(b'IEND', b''))


def rgb565(c):
    r, g, b = (c >> 11) & 31, (c >> 5) & 63, c & 31
    return ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))


def main():
    path = sys.argv[1]
    name = os.path.basename(path)[:-4]
    parts = name.split('_')
    width, height = (int(v) for v in parts[2].split('x'))
    fmt = int(parts[3][1:])
    data = open(path, 'rb').read()
    block = 8 if fmt == 18 else 16
    rgb = bytearray(width * height * 3)
    alpha = bytearray(width * height * 3)
    for by in range(height // 4):
        for bx in range(width // 4):
            o = (by * (width // 4) + bx) * block
            a = [255] * 16
            if fmt == 20:
                a0, a1 = data[o], data[o + 1]
                bits = int.from_bytes(data[o + 2:o + 8], 'little')
                table = [a0, a1] + ([(a0 * (7 - i) + a1 * i) // 7 for i in range(1, 7)] if a0 > a1 else
                                    [(a0 * (5 - i) + a1 * i) // 5 for i in range(1, 5)] + [0, 255])
                a = [table[(bits >> (3 * i)) & 7] for i in range(16)]
                o += 8
            elif fmt == 19:
                bits = int.from_bytes(data[o:o + 8], 'little')
                a = [((bits >> (4 * i)) & 15) * 17 for i in range(16)]
                o += 8
            c0, c1, idx = struct.unpack_from('<HHI', data, o)
            p0, p1 = rgb565(c0), rgb565(c1)
            if fmt != 18 or c0 > c1:
                pal = [p0, p1, tuple((2 * p0[i] + p1[i]) // 3 for i in range(3)),
                       tuple((p0[i] + 2 * p1[i]) // 3 for i in range(3))]
            else:
                pal = [p0, p1, tuple((p0[i] + p1[i]) // 2 for i in range(3)), (0, 0, 0)]
            for i in range(16):
                x, y = bx * 4 + (i & 3), by * 4 + (i >> 2)
                p = (y * width + x) * 3
                rgb[p:p + 3] = bytes(pal[(idx >> (2 * i)) & 3])
                alpha[p:p + 3] = bytes((a[i],) * 3)
    out = os.path.join(os.path.dirname(path), name)
    write_png(out + '_rgb.png', width, height, bytes(rgb))
    write_png(out + '_a.png', width, height, bytes(alpha))
    print(out + '_rgb.png', out + '_a.png')


if __name__ == '__main__':
    main()
