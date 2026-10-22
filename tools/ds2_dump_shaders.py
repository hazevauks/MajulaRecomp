#!/usr/bin/env python3
"""Dumps every Xbox 360 shader container Dark Souls II has, for Renut's translator (ds2_renderer = "renut").

sa2-nx's tools/sa2_dump_shaders.py (reNut's tools/dump_shaders.py naming and container check), for this game's two
sources:

  - the game's data: tools/extract_shaders.py has already written each container of shader/*.bnd, shader/Simple and
    font/ to out/shaders_game as {vs,ps}_<xxh3>.bin, the same naming Renut uses, so they are taken as they are;
  - the executable: Direct3D's own containers (clears, resolves) compiled into default.xex, found in its loaded image
    (out/default_image.bin, written by tools/codegen.ps1) with reNut's check.

Output: <out>/{vs,ps}_<xxh3 of the container>.bin and <out>/manifest.csv (name, type, virtual_size, physical_size,
source, asset), the layout Renut's gpu.cmake reads.

  python tools/ds2_dump_shaders.py [--image out/default_image.bin] [--game-shaders out/shaders_game]
                                   [--out out/renut_shader_dump]
"""
import argparse
import csv
import os
import re
import shutil
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from extract_shaders import xxh3_names  # noqa: E402

SHADER_MAGIC = re.compile(rb"\x10\x2a\x11[\x00\x01]")


def find_shader_headers(data):
    """reNut's check: a container's sizes, tables and its physical part's (offset, size) agree."""
    found = []
    for m in SHADER_MAGIC.finditer(data):
        o = m.start()
        if o % 4 or o + 0x24 > len(data):
            continue
        virt, phys = struct.unpack_from(">II", data, o + 4)
        ct, dt, so = struct.unpack_from(">III", data, o + 0x10)
        if not (0x24 <= virt < 0x10000 and 0 < phys < 0x100000 and o + virt + phys <= len(data)):
            continue
        if ct >= virt or dt >= virt or not (0x24 <= so < virt - 8):
            continue
        poff, psize = struct.unpack_from(">II", data, o + so)
        if poff + psize != phys:
            continue
        found.append(o)
    return found


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--image", default=os.path.join(ROOT, "out", "default_image.bin"))
    ap.add_argument("--image-base", type=lambda s: int(s, 0), default=0x82000000)
    ap.add_argument("--game-shaders", default=os.path.join(ROOT, "out", "shaders_game"))
    ap.add_argument("--out", default=os.path.join(ROOT, "out", "renut_shader_dump"))
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    manifest, seen = [], set()

    # The game's data, already one container per file.
    if not os.path.isdir(args.game_shaders):
        sys.exit(f"no {args.game_shaders}: run tools/extract_shaders.py first")
    game = sorted(n for n in os.listdir(args.game_shaders) if re.fullmatch(r"(vs|ps)_[0-9a-f]{16}\.bin", n))
    for n in game:
        data = open(os.path.join(args.game_shaders, n), "rb").read()
        if find_shader_headers(data[:max(0x24, len(data))]) != [0]:
            print(f"warning: {n} fails reNut's container check, left out")
            continue
        flags, virt, phys = struct.unpack_from(">III", data, 0)
        name = n[:-4]
        manifest.append((name, name[:2], virt, phys, "game data", n))
        if name not in seen:
            seen.add(name)
            shutil.copyfile(os.path.join(args.game_shaders, n), os.path.join(args.out, n))
    print(f"game data: {len(game)} containers")

    # The executable's own.
    if os.path.isfile(args.image):
        data = open(args.image, "rb").read()
        heads = find_shader_headers(data)
        blobs = []
        for h in heads:
            flags, virt, phys = struct.unpack_from(">III", data, h)
            blobs.append((h, flags, virt, phys, data[h:h + virt + phys]))
        hashes, temps = xxh3_names([b[4] for b in blobs], args.out)
        for (h, flags, virt, phys, blob), x in zip(blobs, hashes):
            kind = "vs" if flags & 1 else "ps"
            name = f"{kind}_{x}"
            manifest.append((name, kind, virt, phys, "default.xex", f"0x{args.image_base + h:08x}"))
            if name not in seen:
                seen.add(name)
                with open(os.path.join(args.out, name + ".bin"), "wb") as f:
                    f.write(blob)
        for t in temps + [os.path.join(args.out, 'extracting_list.txt')]:
            if os.path.exists(t):
                os.remove(t)
        print(f"default.xex: {len(heads)} containers")
    else:
        print(f"warning: no {args.image}: the executable's shaders are left out")

    manifest.sort()
    with open(os.path.join(args.out, "manifest.csv"), "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["name", "type", "virtual_size", "physical_size", "source", "asset"])
        w.writerows(manifest)
    ps = sum(1 for n in seen if n.startswith("ps_"))
    print(f"{len(seen)} unique shaders ({ps} ps, {len(seen) - ps} vs) -> {args.out}")


if __name__ == "__main__":
    main()
