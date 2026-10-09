#!/usr/bin/env python3
"""Extracts every Xenos shader container of Dark Souls II from the extracted disc, for the shader package.

The game keeps its compiled shaders (the containers D3DDevice_CreateVertexShader / CreatePixelShader receive) in its
data, so they do not have to be dumped from a running game:

  shader/*.bnd            BND4 archives ("BND4", big-endian, 36-byte entries from 0x40: flags, -1, compressed size,
                          size, data offset, id, name offset); every entry is one zlib stream holding one container
  shader/Simple/*.fxo     loose containers
  font/*.fxo              loose containers

A container starts with 0x102A1100 (pixel shader) or 0x102A1101 (vertex shader). Each one is written once, named by
kind and by the SHA-1 of its bytes (vs_<16 hex>.bin / ps_<16 hex>.bin: the names tools/build_shader_library.sh
takes), with a list of where each came from.

  python tools/extract_shaders.py [--game assets/game_root] [--out out/shaders_in]

Only reads the game folder. Nothing it writes belongs in the repository (out/ is ignored).
"""
import argparse
import glob
import hashlib
import os
import struct
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAGIC = {b'\x10\x2a\x11\x00': 'ps', b'\x10\x2a\x11\x01': 'vs'}


def bnd4_entries(data, path):
    """Yields (index, bytes) for every entry of a BND4 archive."""
    if data[:4] != b'BND4':
        raise SystemExit(f'{path}: not a BND4 archive')
    count = struct.unpack_from('>I', data, 0x0C)[0]
    header, entry_size = struct.unpack_from('>Q', data, 0x10)[0], struct.unpack_from('>Q', data, 0x20)[0]
    if entry_size != 36:
        raise SystemExit(f'{path}: BND4 entries of {entry_size} bytes, this tool knows 36')
    for i in range(count):
        flags, _, packed, size, offset, _id, _name = struct.unpack_from('>IiQQIII', data, header + i * entry_size)
        raw = data[offset:offset + packed]
        # Every entry seen is a zlib stream (flags 0x03000000); an entry stored as it is would start with the magic.
        blob = raw if raw[:4] in MAGIC else zlib.decompress(raw)
        if len(blob) != size and raw[:4] not in MAGIC:
            raise SystemExit(f'{path} entry {i}: {len(blob)} bytes inflated, the entry says {size}')
        yield i, blob


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--game', default=os.path.join(ROOT, 'assets', 'game_root'))
    ap.add_argument('--out', default=os.path.join(ROOT, 'out', 'shaders_in'))
    a = ap.parse_args()

    sources = []  # (label, bytes)
    for path in sorted(glob.glob(os.path.join(a.game, 'shader', '*.bnd'))):
        data = open(path, 'rb').read()
        for i, blob in bnd4_entries(data, path):
            sources.append((f'shader/{os.path.basename(path)}#{i}', blob))
    for pattern in (os.path.join('shader', 'Simple', '*.fxo'), os.path.join('font', '*.fxo')):
        for path in sorted(glob.glob(os.path.join(a.game, pattern))):
            sources.append((os.path.relpath(path, a.game).replace('\\', '/'), open(path, 'rb').read()))

    os.makedirs(a.out, exist_ok=True)
    seen = {}       # file name -> first source
    counts = {'vs': 0, 'ps': 0}
    other = []
    lines = []
    for label, blob in sources:
        kind = MAGIC.get(blob[:4])
        if not kind:
            other.append(label)
            continue
        name = f'{kind}_{hashlib.sha1(blob).hexdigest()[:16]}.bin'
        lines.append(f'{name}\t{len(blob)}\t{label}')
        if name in seen:
            continue
        seen[name] = label
        counts[kind] += 1
        with open(os.path.join(a.out, name), 'wb') as f:
            f.write(blob)
    with open(os.path.join(a.out, 'sources.txt'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    print(f'{len(sources)} entries read; {counts["vs"]} vertex and {counts["ps"]} pixel containers written to {a.out} '
          f'({len(lines) - len(seen)} duplicates); {len(other)} entries that are not containers')
    for label in other[:20]:
        print('  not a container:', label)


if __name__ == '__main__':
    main()
