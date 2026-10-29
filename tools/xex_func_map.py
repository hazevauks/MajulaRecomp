#!/usr/bin/env python3
"""Maps this port's functions (Default.xex 0.0.0.8) to their addresses in another build of the game's executable
(2026-10-29: Scholar of the First Sin's 0.0.0.22, the DLC route, docs/dlc.md).

Each function of the port's generated code (app/generated/default, DEFINE_REX_FUNC) is read from the port's image
(out/default_image.bin) as a sequence of normalised instructions: the opcode and register fields stay, the
immediates and branch displacements go (they hold addresses, which move between builds); as tools/d3d_match.py.
The other image is scanned once: at every word, the first kKey normalised instructions are looked up among the
port's functions' first kKey, and a candidate is kept when the function's whole sequence is there. A function found
at exactly one place is mapped.

  python tools/xex_func_map.py --other out/sotfs/image.bin [--image out/default_image.bin]
      [--gen app/generated/default] [--out out/sotfs/func_map.txt] [--check <addresses file or list>]

Writes "<port address> <other address> <size in instructions>" for each unique match, and prints how much of the
code maps: unique, ambiguous (the same code more than once), not found (changed, or moved inside another).
"""
import argparse
import collections
import glob
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = 0x82000000
KEY = 10          # the instructions of the lookup key
MAX_INSNS = 4000  # a function longer than this is compared on its first MAX_INSNS
MIN_INSNS = 6     # shorter functions are too common to place


def norm(x):
    op = x >> 26
    if op == 18:            # b: the link bit
        return x & 0xFC000001
    if op == 16:            # bc: BO, BI, link
        return x & 0xFFFF0001
    if op in (19, 31, 63, 59):
        return x            # X / XL / XO / A forms: registers and extended opcode only
    if op == 30:            # rld*: registers and sub-opcode (the shift and mask go)
        return x & 0xFFFF001C
    if op == 4:             # VMX
        return x & 0xFFE007FF
    return x & 0xFFFF0000   # D form: the immediate goes


def words(path):
    data = open(path, 'rb').read()
    n = len(data) // 4
    return [norm(w) for w in struct.unpack('>%dI' % n, data[:n * 4])]


def function_starts(gen):
    starts = set()
    for path in glob.glob(os.path.join(gen, '*_recomp.*.cpp')):
        for m in re.finditer(r'^DEFINE_REX_FUNC\(sub_([0-9A-F]+)\)', open(path, errors='replace').read(), re.M):
            starts.add(int(m.group(1), 16))
    return sorted(starts)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--other', required=True)
    ap.add_argument('--image', default=os.path.join(ROOT, 'out', 'default_image.bin'))
    ap.add_argument('--gen', default=os.path.join(ROOT, 'app', 'generated', 'default'))
    ap.add_argument('--out', default=None)
    ap.add_argument('--check', default=None, help='a file of addresses (hex, one a line) to report one by one')
    a = ap.parse_args()

    port = words(a.image)
    other = words(a.other)
    starts = function_starts(a.gen)
    funcs = []
    for i, s in enumerate(starts):
        end = starts[i + 1] if i + 1 < len(starts) else s + 4 * MAX_INSNS
        n = min((end - s) // 4, MAX_INSNS)
        at = (s - BASE) // 4
        if n >= MIN_INSNS and at + n <= len(port):
            funcs.append((s, at, n))
    by_key = collections.defaultdict(list)
    for s, at, n in funcs:
        by_key[tuple(port[at:at + KEY])].append((s, at, n))
    print(f'{len(starts)} functions in the port, {len(funcs)} of {MIN_INSNS}+ instructions; '
          f'{len(other):,} words in the other image', file=sys.stderr)

    found = collections.defaultdict(list)  # port address -> other addresses
    window = collections.deque(other[:KEY], maxlen=KEY)
    for i in range(len(other) - KEY):
        cands = by_key.get(tuple(window))
        if cands:
            for s, at, n in cands:
                if i + n <= len(other) and other[i:i + n] == port[at:at + n]:
                    found[s].append(BASE + 4 * i)
        window.append(other[i + KEY])

    unique = {s: v[0] for s, v in found.items() if len(v) == 1}
    ambiguous = sum(1 for v in found.values() if len(v) > 1)
    total_insns = sum(n for _, _, n in funcs)
    mapped_insns = sum(n for s, _, n in funcs if s in unique)
    print(f'unique {len(unique)} ({100.0 * len(unique) / len(funcs):.1f} % of the functions, '
          f'{100.0 * mapped_insns / total_insns:.1f} % of their instructions), ambiguous {ambiguous}, '
          f'not found {len(funcs) - len(found)}')
    shifts = collections.Counter((o - s) for s, o in unique.items())
    print('most common displacements:', ', '.join(f'{d:+#x} x{c}' for d, c in shifts.most_common(5)))
    if a.out:
        size = {s: n for s, _, n in funcs}
        with open(a.out, 'w') as f:
            for s in sorted(unique):
                f.write(f'{s:08X} {unique[s]:08X} {size[s]}\n')
        print(f'map: {a.out}')
    if a.check:
        items = open(a.check).read().split() if os.path.isfile(a.check) else a.check.split(',')
        for item in items:
            s = int(item, 16)
            v = found.get(s)
            state = f'{v[0]:08X}' if v and len(v) == 1 else (f'{len(v)} places' if v else 'not found')
            print(f'  {s:08X} -> {state}')


if __name__ == '__main__':
    main()
