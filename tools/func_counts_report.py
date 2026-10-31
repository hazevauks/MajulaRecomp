"""Read a func_counts.bin (app/src/ds2_first_exec.cpp, tools/pch_first_exec.py --counts): calls per guest function
every five seconds.

    python tools/func_counts_report.py func_counts.bin                       snapshots and their busiest functions
    python tools/func_counts_report.py func_counts.bin --stuck S [--map M]   functions called steadily from S seconds on
                                                                             that were never called before S - 10

--stuck lists what a stuck screen keeps running: the functions whose calls grow in every snapshot after S and whose
first call came after S - 10 (the state the game entered and never left), with their calls per snapshot."""
import argparse, struct

ap = argparse.ArgumentParser()
ap.add_argument('path')
ap.add_argument('--stuck', type=float)
ap.add_argument('--map')
args = ap.parse_args()

data = open(args.path, 'rb').read()
words = struct.unpack(f'<{len(data) // 4}I', data[:len(data) // 4 * 4])
snapshots, cur = [], None
for i in range(0, len(words) - 1, 2):
    a, b = words[i], words[i + 1]
    if a == 0xFFFFFFFF:
        cur = (b / 1000.0, {})
        snapshots.append(cur)
    elif cur is not None:
        cur[1][a] = b
# Cumulative totals per snapshot (a snapshot holds only the functions that changed).
totals, running = [], {}
for t, changed in snapshots:
    running.update(changed)
    totals.append((t, dict(running)))

if args.stuck is None:
    prev = {}
    for t, tot in totals:
        delta = sorted(((c - prev.get(a, 0), a) for a, c in tot.items() if c != prev.get(a, 0)), reverse=True)
        print(f'{t:8.1f} s  {len(delta):6} functions called; busiest: ' +
              ' '.join(f'sub_{a:08X}:{d}' for d, a in delta[:6]))
        prev = tot
else:
    old = {}
    if args.map:
        for line in open(args.map):
            p = line.split()
            if len(p) >= 2:
                old[int(p[1], 16)] = p[0]
    before = next((tot for t, tot in reversed(totals) if t <= args.stuck - 10), {})
    after = [(t, tot) for t, tot in totals if t >= args.stuck]
    rows = []
    for a in after[-1][1] if after else {}:
        if a in before:
            continue
        series = [tot.get(a, 0) for _, tot in after]
        deltas = [y - x for x, y in zip(series, series[1:])]
        if deltas and sum(d > 0 for d in deltas) * 2 >= len(deltas):  # most snapshots (some catch only a GPU wait)
            rows.append((sum(deltas) / len(deltas), a, deltas))
    for rate, a, deltas in sorted(rows, reverse=True):
        print(f'sub_{a:08X}  2014: {old.get(a, "-"):8}  {rate:10.1f} calls per 5 s  {deltas[:8]}')
