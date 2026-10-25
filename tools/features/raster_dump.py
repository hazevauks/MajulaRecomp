#!/usr/bin/env python3
"""Rasterises what the trophy screen or the unlock notice drew (ds2_trophy_dump, app/src/ds2_trophies.cpp): their ImGui
draw lists and the textures the game's art made, over an optional background picture. The PC's software Vulkan driver
shows no window, so this is how the screen is looked at on the PC. Pure Python, slow (a minute or two).

  python tools/features/raster_dump.py <dump.bin> <out.png> [background.png]

Textures the dump does not carry (the trophies' own icons, ImGui's font) are drawn grey.
"""
import math
import struct
import sys
import zlib


def read_png_rgb(path):
    d = open(path, 'rb').read()
    w, h, _, ctype = struct.unpack_from('>IIBB', d, 16)
    bpp = 4 if ctype == 6 else 3
    i, idat = 8, b''
    while i < len(d):
        n = struct.unpack_from('>I', d, i)[0]
        if d[i + 4:i + 8] == b'IDAT':
            idat += d[i + 8:i + 8 + n]
        i += 12 + n
    raw = zlib.decompress(idat)
    stride = w * bpp
    prev = bytearray(stride)
    rows = []
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append([[line[x * bpp], line[x * bpp + 1], line[x * bpp + 2]] for x in range(w)])
        prev = line
    return w, h, rows


def write_png(path, w, h, px):
    def chunk(t, c):
        return struct.pack('>I', len(c)) + t + c + struct.pack('>I', zlib.crc32(t + c))
    data = b''.join(b'\0' + bytes(max(0, min(255, int(v))) for p in row for v in p) for row in px)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) +
                           chunk(b'IDAT', zlib.compress(data, 6)) + chunk(b'IEND', b''))


def main():
    d = open(sys.argv[1], 'rb').read()
    assert d[:4] == b'DS2D'
    o = 4
    dw, dh = struct.unpack_from('<ff', d, o); o += 8
    n_lists = struct.unpack_from('<I', d, o)[0]; o += 4
    lists = []
    for _ in range(n_lists):
        nv = struct.unpack_from('<I', d, o)[0]; o += 4
        verts = [struct.unpack_from('<ffffI', d, o + 20 * k) for k in range(nv)]; o += 20 * nv
        ni = struct.unpack_from('<I', d, o)[0]; o += 4
        idx = struct.unpack_from('<%dI' % ni, d, o); o += 4 * ni
        nc = struct.unpack_from('<I', d, o)[0]; o += 4
        cmds = []
        for _ in range(nc):
            cx0, cy0, cx1, cy1 = struct.unpack_from('<ffff', d, o); o += 16
            tex = struct.unpack_from('<Q', d, o)[0]; o += 8
            io, ec, vo = struct.unpack_from('<III', d, o); o += 12
            cmds.append((cx0, cy0, cx1, cy1, tex, io, ec, vo))
        lists.append((verts, idx, cmds))
    textures = {}
    nt = struct.unpack_from('<I', d, o)[0]; o += 4
    for _ in range(nt):
        tid = struct.unpack_from('<Q', d, o)[0]; o += 8
        tw, th = struct.unpack_from('<II', d, o); o += 8
        textures[tid] = (tw, th, d[o:o + tw * th * 4]); o += tw * th * 4
    W, H = int(dw), int(dh)
    if len(sys.argv) > 3:
        bw, bh, bg = read_png_rgb(sys.argv[3])
        px = [[list(bg[min(bh - 1, y * bh // H)][min(bw - 1, x * bw // W)]) for x in range(W)] for y in range(H)]
    else:
        px = [[[40, 40, 44] for _ in range(W)] for _ in range(H)]

    def sample(tex, u, v):
        tw, th, rgba = tex
        x = u * tw - 0.5
        y = v * th - 0.5
        x0, y0 = int(math.floor(x)), int(math.floor(y))
        fx, fy = x - x0, y - y0
        out = [0.0, 0.0, 0.0, 0.0]
        for yy, wy in ((y0, 1 - fy), (y0 + 1, fy)):
            yy = min(max(yy, 0), th - 1)
            for xx, wx in ((x0, 1 - fx), (x0 + 1, fx)):
                xx = min(max(xx, 0), tw - 1)
                k = (yy * tw + xx) * 4
                w = wx * wy
                for c in range(4):
                    out[c] += rgba[k + c] * w
        return out

    for verts, idx, cmds in lists:
        for cx0, cy0, cx1, cy1, tid, io, ec, vo in cmds:
            tex = textures.get(tid)
            for t in range(io, io + ec, 3):
                a, b, c = (verts[vo + idx[t + k]] for k in range(3))
                x0 = max(int(math.floor(min(a[0], b[0], c[0]))), int(cx0), 0)
                x1 = min(int(math.ceil(max(a[0], b[0], c[0]))), int(math.ceil(cx1)), W)
                y0 = max(int(math.floor(min(a[1], b[1], c[1]))), int(cy0), 0)
                y1 = min(int(math.ceil(max(a[1], b[1], c[1]))), int(math.ceil(cy1)), H)
                den = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
                if abs(den) < 1e-9:
                    continue
                for y in range(y0, y1):
                    py = y + 0.5
                    for x in range(x0, x1):
                        pxx = x + 0.5
                        l1 = ((b[1] - c[1]) * (pxx - c[0]) + (c[0] - b[0]) * (py - c[1])) / den
                        l2 = ((c[1] - a[1]) * (pxx - c[0]) + (a[0] - c[0]) * (py - c[1])) / den
                        l3 = 1 - l1 - l2
                        if l1 < -1e-6 or l2 < -1e-6 or l3 < -1e-6:
                            continue
                        # ImGui's quads share colour per vertex; take the nearest vertex's colour by weight.
                        col = a[4] if l1 >= max(l2, l3) else b[4] if l2 >= l3 else c[4]
                        cr, cg, cb, ca = col & 255, (col >> 8) & 255, (col >> 16) & 255, (col >> 24) & 255
                        if tex:
                            u = l1 * a[2] + l2 * b[2] + l3 * c[2]
                            v = l1 * a[3] + l2 * b[3] + l3 * c[3]
                            s = sample(tex, u, v)
                            r, g, bl, al = s[0] * cr / 255, s[1] * cg / 255, s[2] * cb / 255, s[3] * ca / 255
                        else:
                            r, g, bl, al = cr * 0.6, cg * 0.6, cb * 0.6, ca
                        k = al / 255
                        p = px[y][x]
                        p[0] += (r - p[0]) * k
                        p[1] += (g - p[1]) * k
                        p[2] += (bl - p[2]) * k
    write_png(sys.argv[2], W, H, px)
    print(W, H, sum(len(l[2]) for l in lists), 'commands', len(textures), 'textures')


if __name__ == '__main__':
    main()
