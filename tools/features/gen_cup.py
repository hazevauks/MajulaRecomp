"""Draft of the sixth tab (trophies), take 3: a chalice with two handles in relief on a sixth medallion of the game's
bar (In-game_01.tpf). The relief's grain is the game's own: the high frequencies of the head medallion's stone.

  python -I gen_cup.py   (reads tex/ingame01.png, writes tex/bar6_x4.png, tex/medal6_x8.png, tex/medal6.png)
"""
import math
import random
import sys

sys.path.insert(0, '.')
from crop import load, save  # noqa: E402

W, H, ROWS = load('tex/ingame01.png')
X0, X1, Y0, Y1 = 0, 292, 770, 844
STEP = 54
CX5, CY = 257, 811
CX6 = CX5 + STEP
R_IN = 20.5
SS = 4
SCALE = 1.45                # shape units to pixels
SHIFT = -1.3                # shape units, moves the cup down

def lum(x, y):
    r = ROWS[y]
    return (r[x * 4] + r[x * 4 + 1] + r[x * 4 + 2]) / 3

bw = X1 - X0 + STEP
bar = [[list(ROWS[y][x * 4:x * 4 + 4]) if x < X1 else [0, 0, 0, 0] for x in range(bw)] for y in range(Y0, Y1)]
for y in range(Y0, Y1):
    for x in range(CX5 - 27, X1):
        bar[y - Y0][x - X0 + STEP] = list(ROWS[y][x * 4:x * 4 + 4])

# --- grain: high-pass of the head medallion's stone (x 140..158, y 796..816), mirrored to tile
GX, GY, GW, GH = 140, 796, 18, 20
patch = [[lum(GX + i, GY + j) for i in range(GW)] for j in range(GH)]
def blur(p):
    out = [[0.0] * GW for _ in range(GH)]
    for j in range(GH):
        for i in range(GW):
            s = n = 0
            for jj in range(max(0, j - 2), min(GH, j + 3)):
                for ii in range(max(0, i - 2), min(GW, i + 3)):
                    s += p[jj][ii]; n += 1
            out[j][i] = s / n
    return out
pb = blur(patch)
detail = [[max(-22.0, min(22.0, patch[j][i] - pb[j][i])) for i in range(GW)] for j in range(GH)]
def grain(x, y):  # x, y in medallion pixels (float)
    i, j = int(math.floor(x)), int(math.floor(y))
    i %= 2 * GW; j %= 2 * GH
    if i >= GW: i = 2 * GW - 1 - i
    if j >= GH: j = 2 * GH - 1 - j
    return detail[j][i]

random.seed(11)
def lattice(n):
    return [[random.random() for _ in range(n + 2)] for _ in range(n + 2)]
OCT = [(lattice(6), 6, 0.45), (lattice(12), 12, 0.25), (lattice(24), 24, 0.18), (lattice(48), 48, 0.12)]
def noise(u, v):
    s = 0.0
    for g, n, a in OCT:
        x, y = (u % 1.0) * n, (v % 1.0) * n
        i, j = int(x), int(y)
        fx, fy = x - i, y - j
        fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
        top = g[j][i] * (1 - fx) + g[j][i + 1] * fx
        bot = g[j + 1][i] * (1 - fx) + g[j + 1][i + 1] * fx
        s += a * (top * (1 - fy) + bot * fy)
    return s


# --- the chalice (shape units, y down): bowl, rim, handles, stem, knot, foot
RIM_Y, BOWL_RX, BOWL_RY = -12.0, 9.4, 11.0

def cup(x, y):
    """(height, outer signed distance) of the chalice at (x, y)."""
    h, d = 0.0, 1e9
    ax = abs(x)
    # bowl: the lower half of an ellipse below the rim, rounded across (a cylinder's light)
    if y >= RIM_Y:
        e = math.hypot(x / BOWL_RX, (y - RIM_Y) / BOWL_RY)
        db = (e - 1.0) * min(BOWL_RX, BOWL_RY)
    else:
        db = max(RIM_Y - y, ax - BOWL_RX)
    d = min(d, db)
    if db < 0:
        half = BOWL_RX * math.sqrt(max(0.0, 1 - ((y - RIM_Y) / BOWL_RY) ** 2))
        across = ax / max(half, 0.3)
        h = max(h, 1.6 + 2.6 * math.sqrt(max(0.0, 1 - across * across)) + 0.6 * (1 - e))
    # rim: a lip (flat ellipse) with the dark opening inside
    er = math.hypot(x / (BOWL_RX + 0.9), (y - RIM_Y) / 2.3)
    if er < 1.0:
        d = min(d, (er - 1.0) * 2.0)
        eo = math.hypot(x / (BOWL_RX - 1.4), (y - RIM_Y + 0.2) / 1.25)
        if eo < 1.0:
            h = max(h, 2.2 + 0.4 * eo)            # the opening, sunk
        else:
            h = max(h, 4.6 + 0.6 * (1 - er))      # the lip
    # handles: half rings on the sides
    for s in (-1, 1):
        hx, hy = s * (BOWL_RX + 0.4), -6.2
        r = math.hypot(x - hx, y - hy)
        if (x - hx) * s > -1.2:
            dr = abs(r - 3.4) - 1.05
            d = min(d, dr)
            if dr < 0:
                t = (r - 3.4) / 1.05
                h = max(h, 1.2 + 1.4 * math.sqrt(max(0.0, 1 - t * t)))
    # stem, knot, foot
    if -0.5 <= y <= 7.5:
        hw = 1.2 + 0.35 * max(0.0, (y - 3.0) / 4.5)
        ds = ax - hw
        d = min(d, ds)
        if ds < 0: h = max(h, 1.2 + 1.3 * math.sqrt(max(0.0, 1 - (ax / hw) ** 2)))
    ek = math.hypot(x / 2.7, (y - 2.6) / 1.25)
    if ek < 1.0:
        d = min(d, (ek - 1.0) * 1.25)
        h = max(h, 1.6 + 1.6 * math.sqrt(1 - ek * ek))
    if 7.0 <= y <= 9.6:  # the foot's slope
        hw = 2.0 + 4.6 * (y - 7.0) / 2.6
        df = max(ax - hw, max(7.0 - y, y - 9.6))
        d = min(d, df)
        if df < 0: h = max(h, 1.3 + 0.9 * min(1.0, -df / 0.8))
    if 9.4 <= y <= 11.6:  # the plinth
        df = max(ax - 7.4, max(9.4 - y, y - 11.6))
        d = min(d, df)
        if df < 0: h = max(h, 1.1 + 0.8 * min(1.0, -df / 0.7))
    return h, d


N = int(2 * R_IN + 3) * SS
off = (2 * R_IN + 3) / 2
hmap = [[0.0] * N for _ in range(N)]
dmap = [[0.0] * N for _ in range(N)]
for j in range(N):
    for i in range(N):
        x = ((i + 0.5) / SS - off) / SCALE
        y = ((j + 0.5) / SS - off) / SCALE + SHIFT
        h, d = cup(x, y)
        hmap[j][i], dmap[j][i] = h, d * SCALE

L = (-0.55, -0.65, 0.52)
ln = math.sqrt(sum(c * c for c in L)); L = tuple(c / ln for c in L)
HV = (L[0], L[1], L[2] + 1.0)
hl = math.sqrt(sum(c * c for c in HV)); HV = tuple(c / hl for c in HV)
shade = [[None] * N for _ in range(N)]
for j in range(N):
    for i in range(N):
        hgt = hmap[j][i]
        if hgt <= 0 and dmap[j][i] > 3.0 * SCALE:
            continue
        gx = (hmap[j][min(N - 1, i + 1)] - hmap[j][max(0, i - 1)]) * SS / 2 / SCALE
        gy = (hmap[min(N - 1, j + 1)][i] - hmap[max(0, j - 1)][i]) * SS / 2 / SCALE
        nx, ny, nz = -gx, -gy, 1.0
        nl = math.sqrt(nx * nx + ny * ny + nz * nz)
        nx, ny, nz = nx / nl, ny / nl, nz / nl
        lam = max(0.0, nx * L[0] + ny * L[1] + nz * L[2])
        spec = max(0.0, nx * HV[0] + ny * HV[1] + nz * HV[2]) ** 14
        if hgt > 0:
            px_, py_ = i / SS, j / SS
            val = 62 + 125 * lam ** 1.2 + 55 * spec   # the sword's range: ~130 facing, ~210 lit edges, ~80 shade
            val *= 0.92 + 0.16 * noise(px_ / 44 + 0.1, py_ / 44 + 0.1)
            val += 0.9 * grain(px_, py_)
            val *= min(1.0, 0.75 + 0.25 * min(1.0, -dmap[j][i] / 0.6))  # a shadowed edge
            sx_ = ((i + 0.5) / SS - off) / SCALE; sy_ = ((j + 0.5) / SS - off) / SCALE + SHIFT
            eo = math.hypot(sx_ / (BOWL_RX - 1.4), (sy_ - RIM_Y + 0.2) / 1.25)
            if eo < 1.0: val *= 0.35 + 0.35 * eo   # the cup's mouth, in shadow
            val = max(10.0, min(232.0, val))
            shade[j][i] = (val, val * 0.99, val * 0.955, 1.0)
        else:  # a thin dark outline, and the shadow the relief casts down and to the right (as the sword's)
            a = max(0.0, 1.0 - dmap[j][i] / 0.5) * 0.7
            si, sj = i - int(1.6 * SS), j - int(1.6 * SS)
            if 0 <= si < N and 0 <= sj < N and hmap[sj][si] > 0: a = max(a, 0.78)
            if a > 0: shade[j][i] = (12.0, 11.0, 10.0, a)


# --- the background: the sword medallion's own stone, the sword painted out
SX, SY = 41, 811
def page(x, y):
    return [float(v) for v in ROWS[y][x * 4:x * 4 + 3]]
work = {(x, y): page(x, y) for x in range(SX - 27, SX + 28) for y in range(SY - 27, SY + 28)}
def lerp(a, b, t):
    return [a[c] + (b[c] - a[c]) * t for c in range(3)]
# the guard: across rows 815..824, mirrored from above and below and blended
GT, GB = 814, 825
for y in range(GT + 1, GB):
    t = (y - GT) / (GB - GT)
    for x in range(24, 59):
        work[(x, y)] = lerp(work[(x, 2 * GT - y)], work[(x, 2 * GB - y)], t)
# the blade and grip: across columns 36..46, mirrored from the left and right and blended
BL, BR = 35, 47
for y in range(SY - 25, SY + 25):
    t0 = None
    for x in range(BL + 1, BR):
        t = (x - BL) / (BR - BL)
        work[(x, y)] = lerp(work[(2 * BL - x, y)], work[(2 * BR - x, y)], t)
BG = {(dx, dy): work[(SX + dx, SY + dy)] for dx in range(-24, 25) for dy in range(-24, 25)}


def put(x, y, rgb, a):
    p = bar[y - Y0][x - X0]
    for c in range(3):
        p[c] = max(0, min(255, int(round(p[c] * (1 - a) + rgb[c] * a))))

for y in range(CY - 24, CY + 25):
    for x in range(CX6 - 24, CX6 + 25):
        dx, dy = x - CX6, y - CY
        r = math.hypot(dx, dy)
        if r > R_IN + 1.0:
            continue
        a = 1.0 if r <= R_IN else (R_IN + 1.0 - r)
        put(x, y, BG[(dx, dy)], a)
        i0 = int((dx + off) * SS); j0 = int((dy + off) * SS)
        acc = [0.0, 0.0, 0.0]; aa = 0.0
        for jj in range(j0, j0 + SS):
            for ii in range(i0, i0 + SS):
                if 0 <= ii < N and 0 <= jj < N and shade[jj][ii]:
                    s = shade[jj][ii]
                    for c in range(3): acc[c] += s[c] * s[3]
                    aa += s[3]
        if aa > 0:
            put(x, y, [acc[c] / aa for c in range(3)], aa / (SS * SS))


def scaled(img, x, y, w, h, s):
    out = []
    for yy in range(h * s):
        line = bytearray()
        for xx in range(w * s):
            p = img[y + yy // s][x + xx // s]
            a = p[3] / 255
            line += bytes([int(p[0] * a + 13 * (1 - a)), int(p[1] * a + 12 * (1 - a)), int(p[2] * a + 11 * (1 - a)), 255])
        out.append(line)
    return out

save('tex/bar6.png', bw, Y1 - Y0, [bytearray(b''.join(bytes(p) for p in row)) for row in bar])
save('tex/bar6_x4.png', bw * 4, (Y1 - Y0) * 4, scaled(bar, 0, 0, bw, Y1 - Y0, 4))
mx = CX6 - 28 - X0
save('tex/medal6_x8.png', 56 * 8, 60 * 8, scaled(bar, mx, CY - 30 - Y0, 56, 60, 8))
save('tex/medal6.png', 56, 60, [bytearray(b''.join(bytes(p) for p in bar[CY - 30 - Y0 + y][mx:mx + 56])) for y in range(60)])
print('ok')
