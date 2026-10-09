#!/usr/bin/env python3
"""Turns a recording of the game image (ds2_video_s: captures/video_<s>s.rgba + .txt) into PNG files, with no
dependency but Python: single frames, or one contact sheet of frames taken at regular steps.

  python tools/frames_to_png.py out/pc/captures/video_5s.rgba                 contact sheet, up to 16 frames
  python tools/frames_to_png.py out/pc/captures/video_5s.rgba --sheet 30      contact sheet, up to 30 frames
  python tools/frames_to_png.py out/pc/captures/video_5s.rgba --frame 12      frame 12 alone, at its full size

The .txt beside the recording gives "width height fps frames". The output goes next to the recording
(<name>_sheet.png, <name>_f<N>.png). tools/make_video.sh makes an MP4 of the same recording when ffmpeg is there.
"""
import argparse
import math
import os
import struct
import zlib


def write_png(path, width, height, rgb):
    """rgb: bytes, 3 per pixel, rows top to bottom."""
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)
    stride = width * 3
    rows = b''.join(b'\0' + rgb[y * stride:(y + 1) * stride] for y in range(height))
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
                + chunk(b'IDAT', zlib.compress(rows, 6)) + chunk(b'IEND', b''))


def read_frame(f, index, width, height):
    f.seek(index * width * height * 4)
    data = f.read(width * height * 4)
    if len(data) < width * height * 4:
        raise SystemExit(f'frame {index} is not in the recording')
    rgb = bytearray(width * height * 3)
    rgb[0::3], rgb[1::3], rgb[2::3] = data[0::4], data[1::4], data[2::4]
    return bytes(rgb)


def shrink(rgb, width, height, factor):
    """Every factor-th pixel of every factor-th row."""
    w, h = width // factor, height // factor
    out = bytearray(w * h * 3)
    for y in range(h):
        row = rgb[y * factor * width * 3:(y * factor + 1) * width * 3]
        for c in range(3):
            out[y * w * 3 + c:(y + 1) * w * 3:3] = row[c::3 * factor][:w]
    return bytes(out), w, h


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('recording', help='the .rgba file')
    ap.add_argument('--frame', type=int, help='write this frame alone (0-based) instead of the contact sheet')
    ap.add_argument('--sheet', type=int, default=16, help='frames in the contact sheet (default 16)')
    ap.add_argument('--tile-width', type=int, default=320, help='width of each tile of the sheet (default 320)')
    a = ap.parse_args()

    base = os.path.splitext(a.recording)[0]
    width, height, _fps, count = open(base + '.txt').read().split()
    width, height, count = int(width), int(height), int(count)
    with open(a.recording, 'rb') as f:
        if a.frame is not None:
            out = f'{base}_f{a.frame}.png'
            write_png(out, width, height, read_frame(f, a.frame, width, height))
            print(f'{out}: frame {a.frame} of {count}, {width}x{height}')
            return
        n = min(a.sheet, count)
        picks = [round(i * (count - 1) / max(1, n - 1)) for i in range(n)]
        factor = max(1, width // a.tile_width)
        tiles = []
        for i in picks:
            tile, tw, th = shrink(read_frame(f, i, width, height), width, height, factor)
            tiles.append(tile)
        cols = min(n, 4)
        rows = math.ceil(n / cols)
        sheet = bytearray(cols * tw * rows * th * 3)
        for k, tile in enumerate(tiles):
            cx, cy = k % cols, k // cols
            for y in range(th):
                at = ((cy * th + y) * cols * tw + cx * tw) * 3
                sheet[at:at + tw * 3] = tile[y * tw * 3:(y + 1) * tw * 3]
        out = base + '_sheet.png'
        write_png(out, cols * tw, rows * th, bytes(sheet))
        print(f'{out}: frames {picks} of {count}, tiles of {tw}x{th}, {cols} per row')


if __name__ == '__main__':
    main()
