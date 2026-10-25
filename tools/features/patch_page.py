"""Prototype (PC test only, nothing shipped): In-game_01's page with the bar of seven medallions, as the game's
tiled, 16-bit swapped DXT5 bytes. Writes tex/ingame01_orig.bin and tex/ingame01_patched.bin (the page's 1 MB each)
and tex/ingame01_patched.png (to look at).

New page: the icons at x 336..474, y 761..853 move to (672, 928); the bar's strip becomes, in rows 770..844,
[0..230 as it was][230..284 the chalice][284..338 the coiled sword][338..392 the gear][392..400 the end][400..446 as
the old 292..338], so its UV grows from x 337.6 to 445.6.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from crop import load, save  # noqa: E402
from tpf_png import tiled, c565, block  # noqa: E402

here = os.path.dirname(os.path.abspath(__file__))
P = lambda *a: os.path.join(here, *a)

d = open(P('menu', '40.febnd', 'In-game_01.tpf'), 'rb').read()
off, size = struct.unpack_from('>II', d, 0x10)
orig = bytearray(d[off:off + size])
W = H = 1024
BW = W // 4

_, _, page = load(P('tex', 'ingame01.png'))
_, _, cup = load(P('tex', 'bar6.png'))        # bar coordinates: page x, page y - 770
_, _, port = load(P('tex', 'bar6_port.png'))
new = [bytearray(r) for r in page]

def px(rows, x, y):
    return rows[y][x * 4:x * 4 + 4]

def put(x, y, p):
    new[y][x * 4:x * 4 + 4] = bytes(p)

# 1. the icons to (672, 928), their old place cleared
for y in range(761, 853):
    for x in range(336, 474):
        put(672 + (x - 336), 928 + (y - 761), px(page, x, y))
for y in range(761, 853):
    for x in range(336, 474):
        put(x, y, b'\0\0\0\0')
# 2. the strip, rows 770..844
for y in range(770, 844):
    for x in range(230, 446):
        put(x, y, b'\0\0\0\0')
    for x in range(230, 284):
        put(x, y, px(cup, x + 54, y - 770))
        put(x + 54, y, px(port, x + 54, y - 770))
    for x in range(230, 292):
        put(x + 108, y, px(page, x, y))
    for x in range(292, 338):
        put(x + 108, y, px(page, x, y))

# --- DXT5 encode the changed blocks
def enc_alpha(a):
    lo, hi = min(a), max(a)
    if lo == hi:
        return bytes([hi, lo]) + b'\0' * 6 if hi else bytes([0, 0]) + b'\0' * 6
    pal = [hi, lo] + [((6 - i) * hi + (i + 1) * lo) // 7 for i in range(6)]
    bits = 0
    for i, v in enumerate(a):
        k = min(range(8), key=lambda j: abs(pal[j] - v))
        bits |= k << (3 * i)
    return bytes([hi, lo]) + bits.to_bytes(6, 'little')

def to565(c):
    return (min(31, round(c[0] * 31 / 255)) << 11) | (min(63, round(c[1] * 63 / 255)) << 5) | min(31, round(c[2] * 31 / 255))

def enc_color(cs, mask):
    pts = [c for c, m in zip(cs, mask) if m] or cs
    mn = [min(p[i] for p in pts) for i in range(3)]
    mx = [max(p[i] for p in pts) for i in range(3)]
    c0, c1 = to565(mx), to565(mn)
    if c0 < c1: c0, c1 = c1, c0
    if c0 == c1:
        return struct.pack('<HHI', c0, c1, 0)
    p0, p1 = c565(c0), c565(c1)
    pal = [p0, p1, [(2 * p0[i] + p1[i]) // 3 for i in range(3)], [(p0[i] + 2 * p1[i]) // 3 for i in range(3)]]
    bits = 0
    for i, c in enumerate(cs):
        k = min(range(4), key=lambda j: sum((pal[j][t] - c[t]) ** 2 for t in range(3)))
        bits |= k << (2 * i)
    return struct.pack('<HHI', c0, c1, bits)

out = bytearray(orig)
changed = 0
for by in range(H // 4):
    for bx in range(BW):
        if all(new[by * 4 + j][bx * 16:bx * 16 + 16] == page[by * 4 + j][bx * 16:bx * 16 + 16] for j in range(4)):
            continue
        texels = [new[by * 4 + (i >> 2)][(bx * 4 + (i & 3)) * 4:(bx * 4 + (i & 3)) * 4 + 4] for i in range(16)]
        a = [t[3] for t in texels]
        blk = bytearray(enc_alpha(a) + enc_color([list(t[:3]) for t in texels], [v > 8 for v in a]))
        for i in range(0, 16, 2): blk[i], blk[i + 1] = blk[i + 1], blk[i]
        o = tiled(bx, by, BW, 4)
        out[o:o + 16] = blk
        changed += 1
open(P('tex', 'ingame01_orig.bin'), 'wb').write(orig)
open(P('tex', 'ingame01_patched.bin'), 'wb').write(out)
# what the game will see: decode the patched page
check = [bytearray(W * 4) for _ in range(H)]
for by in range(H // 4):
    for bx in range(BW):
        o = tiled(bx, by, BW, 4)
        raw = bytearray(out[o:o + 16])
        for i in range(0, 16, 2): raw[i], raw[i + 1] = raw[i + 1], raw[i]
        for i, p in enumerate(block(raw)):
            x, y = bx * 4 + (i & 3), by * 4 + (i >> 2)
            check[y][x * 4:x * 4 + 4] = bytes(p)
save(P('tex', 'ingame01_patched.png'), W, H, check)
print('blocks changed', changed)
