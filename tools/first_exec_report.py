"""Read a first_exec.bin (app/src/ds2_first_exec.cpp): when each guest function ran for the first time.

    python tools/first_exec_report.py first_exec.bin                 new functions per second
    python tools/first_exec_report.py first_exec.bin --from S [--to S] [--map out/sotfs/func_map.txt]
                                                                     the functions first run in that window, in order

With --map (tools/xex_func_map.py's output, 2014 address -> Scholar address) each function shows its 2014 address,
or '-' for code the 2014 executable does not have."""
import argparse, collections, struct

ap = argparse.ArgumentParser()
ap.add_argument('path')
ap.add_argument('--from', dest='start', type=float)
ap.add_argument('--to', dest='end', type=float)
ap.add_argument('--map')
args = ap.parse_args()

data = open(args.path, 'rb').read()
records = [((r >> 32) & 0xFFFFFFFF, (r & 0xFFFFFFFF) / 1000.0)
           for (r,) in struct.iter_unpack('<Q', data[:len(data) // 8 * 8])]
seen, timeline = set(), []
for address, t in records:  # two threads may report one function: keep the first
    if address not in seen:
        seen.add(address)
        timeline.append((t, address))
timeline.sort()

if args.start is None:
    per_second = collections.Counter(int(t) for t, _ in timeline)
    print(f'{len(timeline)} functions, last first run at {timeline[-1][0]:.1f} s')
    for s in range(int(timeline[-1][0]) + 1):
        if per_second[s]:
            print(f'{s:5} s  {per_second[s]:6}  {"#" * min(80, per_second[s] // 10)}')
else:
    old = {}
    if args.map:
        for line in open(args.map):
            parts = line.split()
            if len(parts) >= 2:
                old[int(parts[1], 16)] = parts[0]
    end = args.end if args.end is not None else float('inf')
    for t, address in timeline:
        if args.start <= t <= end:
            print(f'{t:9.3f}  sub_{address:08X}  2014: {old.get(address, "-")}')
