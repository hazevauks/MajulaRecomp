#!/usr/bin/env python3
"""Turns the images of a traced frame (DS2_TRACE_DUMP=1 with banjo_trace_frame_s: trace_dump/*.raw next to the
executable, one per resolve, written by the native renderer) into PNG files and a table of their value ranges.

  python tools/trace_dump_png.py [out/pc/trace_dump]

File names say what each one is: <sequence>_<guest address>_<width>x<height>_vk<VkFormat>.raw, in the order the
frame resolved them. Formats: 37 / 43 / 44 = 8 bits per channel, 64 = A2B10G10R10 (10 bits), 97 = RGBA16F.
A PNG shows values clamped to 0..1 (16F) or the stored value (the others). The table gives, per channel, the
smallest, the mean and the largest value, and for 10-bit images also the value each channel would have if its
bits were a Xenon 7e3 float (the HDR scene the game resolves as raw bits).
"""
import glob
import math
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


def from_7e3(code):
    mantissa, exponent = code & 0x7F, code >> 7
    if exponent == 0:
        return mantissa / 128.0 * 2.0 ** -2      # denormal: m/128 * 2^(1-3)
    return (1.0 + mantissa / 128.0) * 2.0 ** (exponent - 3)


def describe(values):
    if not values:
        return '-'
    return f'{min(values):.4g}/{sum(values) / len(values):.4g}/{max(values):.4g}'


def main():
    folder = sys.argv[1] if len(sys.argv) > 1 else os.path.join('out', 'pc', 'trace_dump')
    for path in sorted(glob.glob(os.path.join(folder, '*.raw'))):
        name = os.path.basename(path)[:-4]
        _sequence, _address, size, fmt = name.split('_')
        width, height = (int(v) for v in size.split('x'))
        fmt = int(fmt[2:])
        data = open(path, 'rb').read()
        step = max(1, (width * height) // 20000)  # sample for the statistics
        rgb = bytearray(width * height * 3)
        channels = ([], [], [], [])
        extra = ''
        if fmt == 97:
            halves = struct.unpack(f'<{width * height * 4}e', data[:width * height * 8])
            for i in range(width * height):
                for c in range(3):
                    v = halves[i * 4 + c]
                    rgb[i * 3 + c] = 0 if math.isnan(v) else max(0, min(255, int(v * 255 + 0.5)))
                if i % step == 0:
                    for c in range(4):
                        channels[c].append(halves[i * 4 + c])
            nans = sum(1 for v in halves if math.isnan(v))
            extra = f' NaN {nans}' if nans else ''
        elif fmt in (64, 58):
            words = struct.unpack(f'<{width * height}I', data[:width * height * 4])
            as_7e3 = ([], [], [])
            for i, w in enumerate(words):
                # A2B10G10R10: red in the low 10 bits. (58, A2R10G10B10: blue low.)
                c10 = (w & 0x3FF, (w >> 10) & 0x3FF, (w >> 20) & 0x3FF)
                if fmt == 58:
                    c10 = c10[::-1]
                for c in range(3):
                    rgb[i * 3 + c] = c10[c] >> 2
                if i % step == 0:
                    for c in range(3):
                        channels[c].append(c10[c] / 1023.0)
                        as_7e3[c].append(from_7e3(c10[c]))
                    channels[3].append((w >> 30) / 3.0)
            extra = ' | as 7e3: ' + ' '.join(describe(v) for v in as_7e3)
        else:
            order = (2, 1, 0) if fmt == 44 else (0, 1, 2)
            for i in range(width * height):
                for c in range(3):
                    rgb[i * 3 + c] = data[i * 4 + order[c]]
                if i % step == 0:
                    for c in range(3):
                        channels[c].append(data[i * 4 + order[c]] / 255.0)
                    channels[3].append(data[i * 4 + 3] / 255.0)
        write_png(os.path.join(folder, name + '.png'), width, height, bytes(rgb))
        print(f'{name}: min/mean/max R {describe(channels[0])} G {describe(channels[1])} B {describe(channels[2])} '
              f'A {describe(channels[3])}{extra}')


if __name__ == '__main__':
    main()
