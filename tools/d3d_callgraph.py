#!/usr/bin/env python3
"""The call graph of the generated code, for finding Dark Souls II's Direct3D functions (the Renut census, docs/deko3d.md).

Reads app/generated/default/ds2_recomp.*.cpp once and writes out/d3d_callgraph.json: for every function, its size in
guest instructions, the functions it calls directly and the kernel imports it calls (VdSwap, KeQueryPerformance...),
and who calls it. Then answers queries:

  python tools/d3d_callgraph.py --build                 # (re)make out/d3d_callgraph.json
  python tools/d3d_callgraph.py --import VdSwap         # the functions calling an import, with their callers
  python tools/d3d_callgraph.py --func 829EB228         # one function: size, callees, callers
  python tools/d3d_callgraph.py --range 829E01C8 82A001C8 --imports   # every function of a range that calls imports
"""
import argparse
import glob
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GEN = os.path.join(ROOT, 'app', 'generated', 'default')
OUT = os.path.join(ROOT, 'out', 'd3d_callgraph.json')
DEF = re.compile(r'^DEFINE_REX_FUNC\(sub_([0-9A-F]{8})\)')
CALL = re.compile(r'(?:__imp__)?sub_([0-9A-F]{8})\(ctx, base\)')
IMP = re.compile(r'__imp__([A-Z][A-Za-z0-9_]+)\(ctx, base\)')
INSN = re.compile(r'^\t// [a-z]')


def build():
    graph = {}
    for path in sorted(glob.glob(os.path.join(GEN, 'ds2_recomp.*.cpp'))):
        cur = None
        for line in open(path, encoding='utf-8', errors='replace'):
            m = DEF.match(line)
            if m:
                cur = {'size': 0, 'calls': set(), 'imports': set()}
                graph[m.group(1)] = cur
                continue
            if cur is None:
                continue
            if INSN.match(line):
                cur['size'] += 1
            for c in CALL.findall(line):
                cur['calls'].add(c)
            for i in IMP.findall(line):
                if not i.startswith('sub_'):
                    cur['imports'].add(i)
    callers = {}
    for f, d in graph.items():
        for c in d['calls']:
            callers.setdefault(c, set()).add(f)
    data = {f: {'size': d['size'], 'calls': sorted(d['calls'] - {f}), 'imports': sorted(d['imports']),
                'callers': sorted(callers.get(f, ()))} for f, d in graph.items()}
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    json.dump(data, open(OUT, 'w'), separators=(',', ':'))
    print(f'{len(data)} functions -> {OUT}')
    return data


def show(data, f, indent=''):
    d = data.get(f)
    if not d:
        print(f'{indent}{f}: not a function start')
        return
    print(f'{indent}sub_{f}: {d["size"]} insns; imports {",".join(d["imports"]) or "-"}')
    print(f'{indent}  calls   ({len(d["calls"])}): {" ".join(d["calls"][:40])}')
    print(f'{indent}  callers ({len(d["callers"])}): {" ".join(d["callers"][:40])}')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--build', action='store_true')
    ap.add_argument('--import', dest='imp')
    ap.add_argument('--func', nargs='*')
    ap.add_argument('--range', nargs=2)
    ap.add_argument('--imports', action='store_true')
    a = ap.parse_args()
    data = build() if a.build or not os.path.isfile(OUT) else json.load(open(OUT))
    if a.imp:
        for f, d in sorted(data.items()):
            if a.imp in d['imports']:
                show(data, f)
    for f in a.func or []:
        show(data, f.upper().replace('SUB_', ''))
    if a.range:
        lo, hi = int(a.range[0], 16), int(a.range[1], 16)
        for f, d in sorted(data.items()):
            if lo <= int(f, 16) < hi and (not a.imports or d['imports']):
                print(f'sub_{f} {d["size"]:5d} imp={",".join(d["imports"]) or "-"} callers={len(d["callers"])} '
                      f'calls={len(d["calls"])}')


if __name__ == '__main__':
    main()
