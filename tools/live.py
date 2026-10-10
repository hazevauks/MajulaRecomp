#!/usr/bin/env python3
"""Drives a running PC build by hand, one step at a time: sets the live pad file (ds2_input_live) and asks for a
snapshot of the game image (ds2_snap_live). For exploring the game on a machine too slow to play it.

  python tools/live.py pad "lup, a 0.6"      hold the left stick up until the next pad command, press A for 0.6 s
  python tools/live.py pad ""                release everything
  python tools/live.py snap [name]           wait for a fresh image and write out/pc/captures/<name>.png (default snap)
  python tools/live.py pad "rright 2" snap   several commands in a row; "wait <seconds>" sleeps between them
  python tools/live.py sheet a b c           one image of the snapshots a, b, c at half size (out/pc/captures/sheet.png)

Start the game with REX_DS2_INPUT_LIVE=<folder of the exe>\\input_live.txt and REX_DS2_SNAP_LIVE=true. The folder of
the executable defaults to out/pc (DS2_EXE_DIR to change it). Buttons: see app/src/ds2_input_script.h.
"""
import os
import struct
import sys
import time
import zlib

EXE_DIR = os.environ.get('DS2_EXE_DIR', os.path.join('out', 'pc'))
CAPTURES = os.path.join(EXE_DIR, 'captures')


def write_png(path, width, height, rgba):
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)
    rows = bytearray()
    for y in range(height):
        row = rgba[y * width * 4:(y + 1) * width * 4]
        rows += b'\0' + bytes(b for i in range(0, len(row), 4) for b in row[i:i + 3])
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
                + chunk(b'IDAT', zlib.compress(bytes(rows), 6)) + chunk(b'IEND', b''))


def pad(text):
    path = os.path.join(EXE_DIR, 'input_live.txt')
    with open(path + '.tmp', 'w') as f:
        f.write(text + '\n')
    os.replace(path + '.tmp', path)
    print(f'pad: {text!r}')


def snap(name, timeout=120):
    os.makedirs(CAPTURES, exist_ok=True)
    index = os.path.join(CAPTURES, 'snap.txt')
    before = open(index).read() if os.path.exists(index) else ''
    open(os.path.join(CAPTURES, 'snap_request'), 'w').close()
    deadline = time.time() + timeout
    while time.time() < deadline:
        time.sleep(0.25)
        if os.path.exists(index) and not os.path.exists(os.path.join(CAPTURES, 'snap_request')):
            now = open(index).read()
            if now and now != before:
                width, height = (int(v) for v in now.split()[:2])
                data = open(os.path.join(CAPTURES, 'snap.rgba'), 'rb').read()
                if len(data) >= width * height * 4:
                    out = os.path.join(CAPTURES, name + '.png')
                    write_png(out, width, height, data)
                    print(f'snap: {out} ({width}x{height}, snapshot {now.split()[-1]})')
                    return
    sys.exit('snap: no image in time (is the game running with REX_DS2_SNAP_LIVE=true?)')


def read_png(path):
    """Only the PNGs this tool writes: 8-bit RGB, every row with filter 0."""
    f = open(path, 'rb').read()
    width, height = struct.unpack_from('>II', f, 16)
    pos, idat = 8, b''
    while pos < len(f):
        size, tag = struct.unpack_from('>I4s', f, pos)
        if tag == b'IDAT':
            idat += f[pos + 8:pos + 8 + size]
        pos += 12 + size
    raw = zlib.decompress(idat)
    stride = width * 3 + 1
    return width, height, [raw[y * stride + 1:(y + 1) * stride] for y in range(height)]


def sheet(names, per_row=4):
    tiles = []
    for name in names:
        width, height, rows = read_png(os.path.join(CAPTURES, name + '.png'))
        tiles.append((width // 2, height // 2,
                      [bytes(b for x in range(0, width - 1, 2) for b in rows[y][x * 3:x * 3 + 3])
                       for y in range(0, height - 1, 2)]))
    tw, th = tiles[0][0], tiles[0][1]
    count_rows = (len(tiles) + per_row - 1) // per_row
    out = bytearray(tw * per_row * th * count_rows * 4)
    for n, (w, h, rows) in enumerate(tiles):
        ox, oy = (n % per_row) * tw, (n // per_row) * th
        for y in range(min(h, th)):
            row = rows[y][:tw * 3]
            for x in range(len(row) // 3):
                p = ((oy + y) * tw * per_row + ox + x) * 4
                out[p:p + 3] = row[x * 3:x * 3 + 3]
    path = os.path.join(CAPTURES, 'sheet.png')
    write_png(path, tw * per_row, th * count_rows, bytes(out))
    print(f'sheet: {path} ({len(tiles)} snapshots, {tw}x{th} each: {" ".join(names)})')


def main():
    args = sys.argv[1:]
    if not args:
        sys.exit(__doc__)
    i = 0
    while i < len(args):
        command = args[i]
        if command == 'sheet':
            sheet(args[i + 1:])
            return
        if command == 'pad' and i + 1 < len(args):
            pad(args[i + 1])
            i += 2
        elif command == 'wait' and i + 1 < len(args):
            time.sleep(float(args[i + 1]))
            i += 2
        elif command == 'snap':
            name = 'snap'
            if i + 1 < len(args) and args[i + 1] not in ('pad', 'wait', 'snap'):
                name = args[i + 1]
                i += 1
            snap(name)
            i += 1
        else:
            sys.exit(f'unknown command {command!r}')


if __name__ == '__main__':
    main()
