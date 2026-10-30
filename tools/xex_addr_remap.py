#!/usr/bin/env python3
"""Moves the guest addresses in this port's files from one build of Default.xex to another (2026-10-29: the 2014
disc's 0.0.0.8 to Scholar of the First Sin's 0.0.0.22, docs/dlc.md), with tools/xex_func_map.py's map.

Every token that reads as an address (0x8XXXXXXX, sub_/loc_ 8XXXXXXX, or the bare eight digits) is looked up:
  code   in the port's .text: placed in the function that holds it (app/generated/default's function starts); when
         that function is in the map and the normalised instructions from the address on (up to 8, or to the
         function's end) are at the same place in the other image, it moves by the function's displacement.
         "S" when the address is a function start or a label of the generated code, "I" otherwise (an instruction:
         check it is not a constant that only looks like an address).
  data   in .rdata/.data: found through the code that loads it (lis rX,hi / addi|lwz|stw|... rY,rX,lo, both in a
         mapped function): the same two instructions at the other image's place give the new address ("D", when
         every such pair found agrees).
Tokens left as they are ("?") are listed for doing by hand.

  python tools/xex_addr_remap.py FILE... [--map out/sotfs/func_map.txt] [--other out/sotfs/image.bin]
      [--image out/default_image.bin] [--gen app/generated/default] [--write] [--report out/sotfs/remap.txt]
"""
import argparse
import bisect
import collections
import glob
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = 0x82000000
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from xex_func_map import norm, function_starts  # noqa: E402

TOKEN = re.compile(r'(?<![0-9A-Za-z])(0x|sub_|loc_)?(8[23][0-9A-Fa-f]{6})(?![0-9A-Za-z])')
WINDOW = 8
# Comments name other games' addresses too (the code came from banjo-nx and sa2-nx): a token with one of these earlier
# on its line is left alone ("K").
OTHER_GAMES = ('N&B', 'SA2', 'sa2', 'Banjo', 'banjo', 'Mass Effect', 'usf4', 'USF4', 'Marathon', 'Unleashed')


def sections(data):
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    count = struct.unpack_from('<H', data, pe + 6)[0]
    opt = struct.unpack_from('<H', data, pe + 20)[0]
    out = {}
    for i in range(count):
        name, size, va = struct.unpack_from('<8sII', data, pe + 24 + opt + 40 * i)
        out[name.rstrip(b'\0').decode()] = (BASE + va, BASE + va + size)
    return out


class Images:
    def __init__(self, image, other, gen, mapfile):
        self.port_raw = open(image, 'rb').read()
        self.other_raw = open(other, 'rb').read()
        self.port = [norm(w) for w in struct.unpack('>%dI' % (len(self.port_raw) // 4), self.port_raw[:len(self.port_raw) // 4 * 4])]
        self.other = [norm(w) for w in struct.unpack('>%dI' % (len(self.other_raw) // 4), self.other_raw[:len(self.other_raw) // 4 * 4])]
        self.text = sections(self.port_raw)['.text']
        self.port_sections = sections(self.port_raw)
        self.starts = function_starts(gen)
        self.labels = set(self.starts)
        for path in glob.glob(os.path.join(gen, '*_recomp.*.cpp')):
            for m in re.finditer(r'^loc_([0-9A-F]{8}):', open(path, errors='replace').read(), re.M):
                self.labels.add(int(m.group(1), 16))
        self.map = {}
        for line in open(mapfile):
            f = line.split()
            self.map[int(f[0], 16)] = (int(f[1], 16), int(f[2]))
        self.mapped = sorted(self.map)
        self.label_list = sorted(self.labels)
        self.data_index = None

    def word(self, raw, addr):
        at = addr - BASE
        return struct.unpack_from('>I', raw, at)[0] if 0 <= at <= len(raw) - 4 else None

    def code(self, a):
        j = bisect.bisect_right(self.starts, a) - 1
        if j < 0:
            return None
        s = self.starts[j]
        end = self.starts[j + 1] if j + 1 < len(self.starts) else self.text[1]
        cands = []
        if s in self.map:
            cands.append(self.map[s][0] - s)
        k = bisect.bisect_left(self.mapped, s)
        for i in (k - 2, k - 1, k, k + 1, k + 2):  # the neighbours' displacements, for a function the map left out
            if 0 <= i < len(self.mapped):
                cands.append(self.map[self.mapped[i]][0] - self.mapped[i])
        li = bisect.bisect_right(self.label_list, a)  # up to the next label: tiny entries (D3D's state setters) differ after it
        if li < len(self.label_list):
            end = min(end, self.label_list[li])
        # At least 4 words even past the entry: a one-instruction function (a lone blr) matches anywhere.
        n = max(4, min(WINDOW, (end - a) // 4))
        at = (a - BASE) // 4
        for d in dict.fromkeys(cands):
            o = at + d // 4
            if 0 <= o and o + n <= len(self.other) and self.other[o:o + n] == self.port[at:at + n]:
                return a + d
        # Tiny entries that moved apart from their neighbours (Direct3D's render state setters and getters, a few
        # words each, whose displacement changes inside their block): their exact words (the device offsets they
        # read and write make them unique) looked for near where the neighbours went.
        if cands and n >= 2:
            raw = self.port_raw[a - BASE:a - BASE + 4 * n]
            lo = max(0, a + min(cands) - 0x4000 - BASE)
            hi = a + max(cands) + 0x4000 - BASE
            hits = []
            at_other = self.other_raw.find(raw, lo, hi)
            while at_other >= 0 and len(hits) < 2:
                if at_other % 4 == 0:
                    hits.append(at_other)
                at_other = self.other_raw.find(raw, at_other + 1, hi)
            if len(hits) == 1:
                return BASE + hits[0]
        return None

    def build_data_index(self):
        # Every lis/D-form pair of the port's .text: data address -> [(address of the D-form instruction)]
        idx = collections.defaultdict(list)
        lo, hi = self.text
        raw = self.port_raw
        last_lis = {}
        for a in range(lo, hi, 4):
            w = struct.unpack_from('>I', raw, a - BASE)[0]
            op = w >> 26
            if op == 15 and (w >> 16) & 31 == 0:  # lis rD, imm
                last_lis[(w >> 21) & 31] = (a, (w & 0xFFFF) << 16)
                continue
            if op in (14, 32, 34, 36, 38, 40, 44, 48, 50, 52, 54, 58, 62):  # addi, lwz, lbz, stw, stb, lhz, sth, lfs, lfd, stfs, stfd, ld, std
                ra = (w >> 16) & 31
                if ra in last_lis and a - last_lis[ra][0] <= 64:
                    simm = w & 0xFFFF
                    if op in (58, 62):
                        simm &= 0xFFFC
                    if simm & 0x8000:
                        simm -= 0x10000
                    idx[(last_lis[ra][1] + simm) & 0xFFFFFFFF].append((last_lis[ra][0], a))
            if op == 18 or (op == 19 and ((w >> 1) & 0x3FF) == 16):  # b / blr: the register state ends
                last_lis.clear()
        self.data_index = idx

    def pointer_data(self, a):
        # A word of a table of code pointers (a vtable, a callback table): the nearest code pointer at or around it
        # (up to 16 words away) is translated and looked for in the other image's data; when it is there once, the
        # address moves with it, checked on the words between.
        if not hasattr(self, 'other_pointers'):
            lo, hi = sections(self.other_raw)['.text']
            idx = collections.defaultdict(list)
            for name in ('.rdata', '.data'):
                slo, shi = sections(self.other_raw)[name]
                for p in range(slo, shi - 3, 4):
                    w = struct.unpack_from('>I', self.other_raw, p - BASE)[0]
                    if lo <= w < hi:
                        idx[w].append(p)
            self.other_pointers = idx
        for k in sorted(range(-16, 17), key=abs):
            p = a + 4 * k
            w = self.word(self.port_raw, p)
            if w is None or not (self.text[0] <= w < self.text[1]):
                continue
            new = self.code(w)
            places = self.other_pointers.get(new, []) if new is not None else []
            if len(places) != 1:
                continue
            q = places[0] - 4 * k
            lo, hi = sorted((a, p))
            ok = True
            for x in range(lo, hi + 4, 4):  # the words between: pointers translate, the rest are equal
                wp, wo = self.word(self.port_raw, x), self.word(self.other_raw, x - a + q)
                if self.text[0] <= wp < self.text[1]:
                    ok = ok and self.code(wp) == wo
                else:
                    ok = ok and wp == wo
            if ok:
                return q
        return None

    def data(self, a):
        q = self.pointer_data(a)
        if q is not None:
            return q
        if self.data_index is None:
            self.build_data_index()
        seen = set()
        for lis_at, use_at in self.data_index.get(a, [])[:40]:
            new_lis = self.code(lis_at)
            new_use = self.code(use_at)
            if new_lis is None or new_use is None:
                continue
            wl = self.word(self.other_raw, new_lis)
            wu = self.word(self.other_raw, new_use)
            if wl >> 26 != 15 or wu >> 26 != (self.word(self.port_raw, use_at) >> 26):
                continue
            simm = wu & 0xFFFF
            if wu >> 26 in (58, 62):
                simm &= 0xFFFC
            if simm & 0x8000:
                simm -= 0x10000
            seen.add((((wl & 0xFFFF) << 16) + simm) & 0xFFFFFFFF)
        return seen.pop() if len(seen) == 1 else None

    def translate(self, a):
        if self.text[0] <= a < self.text[1]:
            n = self.code(a)
            if n is not None:
                return n, 'S' if a in self.labels else 'I'
            n = self.data(a)  # data kept in .text (the game's statics past its code)
            return (n, 'D') if n is not None else (None, '?')
        for name in ('.rdata', '.data'):
            lo, hi = self.port_sections.get(name, (0, 0))
            if lo <= a < hi:
                n = self.data(a)
                return (n, 'D') if n is not None else (None, '?')
        return None, '?'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('files', nargs='+')
    ap.add_argument('--map', default=os.path.join(ROOT, 'out', 'sotfs', 'func_map.txt'))
    ap.add_argument('--other', default=os.path.join(ROOT, 'out', 'sotfs', 'image.bin'))
    ap.add_argument('--image', default=os.path.join(ROOT, 'out', 'gen_2014', 'default_image.bin'))
    ap.add_argument('--gen', default=os.path.join(ROOT, 'out', 'gen_2014', 'default'))
    ap.add_argument('--write', action='store_true')
    ap.add_argument('--report', default=os.path.join(ROOT, 'out', 'sotfs', 'remap.txt'))
    a = ap.parse_args()
    im = Images(a.image, a.other, a.gen, a.map)
    cache = {}
    counts = collections.Counter()
    report = []
    for path in a.files:
        text = open(path, encoding='utf-8', newline='').read()
        notes = []

        def sub(m):
            addr = int(m.group(2), 16)
            line_start = text.rfind('\n', 0, m.start()) + 1
            before = text[line_start:m.start()]
            if any(k in before for k in OTHER_GAMES):  # another game's address, in a comment
                counts['K'] += 1
                return m.group(0)
            if addr not in cache:
                cache[addr] = im.translate(addr)
            new, kind = cache[addr]
            counts[kind] += 1
            line = text.count('\n', 0, m.start()) + 1
            notes.append(f'{kind} {path}:{line} {m.group(0)}' + (f' -> {new:08X}' if new is not None else ''))
            if new is None:
                return m.group(0)
            digits = f'{new:08X}' if m.group(2).upper() == m.group(2) else f'{new:08x}'
            return (m.group(1) or '') + digits

        out = TOKEN.sub(sub, text)
        report += notes
        if a.write and out != text:
            open(path, 'w', encoding='utf-8', newline='').write(out)
    with open(a.report, 'w') as f:
        f.write('\n'.join(sorted(report, key=lambda r: r[0])) + '\n')
    print(f'{sum(counts.values())} tokens: ' + ', '.join(f'{k} {v}' for k, v in sorted(counts.items())) +
          f' ({"written" if a.write else "dry run"}); report: {a.report}')


if __name__ == '__main__':
    main()
