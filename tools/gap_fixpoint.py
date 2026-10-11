#!/usr/bin/env python3
"""Repeat the structural gap scan until no newly exposed gap qualifies.

Each pass: find_gaps.py on the current generated code -> gaps of 8..1024 bytes not examined before -> scratch
codegen with them declared -> gap_classify.py checks (tier 1 + no stack/LR + no undefined-at-entry registers, and
tier 2) -> append the accepted entries to app/overrides.toml -> real codegen (tools/codegen.sh), so the next pass
sees the code the new entries exposed. Evidence goes to <scratch>/gapscan-accepted.txt. Stops when a pass accepts
nothing.

  python3 tools/gap_fixpoint.py SCRATCH_DIR [examined.txt ...]

SCRATCH_DIR  work folder (one subfolder per pass; it is not cleaned).
examined.txt files whose lines start with an address that must not be examined again (selected.txt of earlier
             runs).

Environment: REXGLUE = path of the rexglue host tool (default sdk/out/host/rexglue, see tools/build_host.sh).
Needs the game in assets/game_root and an existing generated tree (tools/codegen.sh).
"""
import bisect, glob, os, re, struct, subprocess, sys

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
app = os.path.join(root, "app")
# ds2-nx: --dry-run stops after the first pass's classification (nothing is written outside the scratch folder).
dry_run = "--dry-run" in sys.argv
sys.argv = [a for a in sys.argv if a != "--dry-run"]
if len(sys.argv) < 2:
    sys.exit(__doc__)
scratch = os.path.abspath(sys.argv[1])
# ds2-nx: the generator as tools/pointer_fixpoint.py finds it (the newest rexglue under sdk/out, Windows or not).
_found = sorted((f for f in glob.glob(os.path.join(root, "sdk", "out", "*", "rexglue*"))
                 if os.path.basename(f) in ("rexglue", "rexglue.exe")), key=os.path.getmtime)
rexglue = os.path.abspath(os.environ.get("REXGLUE", _found[-1] if _found else
                                          os.path.join(root, "sdk", "out", "host", "rexglue")))
overrides_path = os.path.join(app, "overrides.toml")
manifest_path = os.path.join(app, "ds2_manifest.toml")
gen = os.path.join(app, "generated", "default")
evidence_path = os.path.join(scratch, "gapscan-accepted.txt")
tools = os.path.join(root, "tools")

def ns_code_range():
    # Dark Souls II's .text (the image's PE section table; tools/pointer_scan.py uses the same default).
    return 0x82240000, 0x83402474


ns = {"__name__": "gc"}
exec(compile(open(os.path.join(tools, "gap_classify.py")).read(), "gc", "exec"), ns)
examined = set()
for p in sys.argv[2:]:
    examined |= {l.split()[0] for l in open(p)}
os.makedirs(scratch, exist_ok=True)

for n in range(1, 20):
    d = os.path.join(scratch, f"pass{n}")
    os.makedirs(d, exist_ok=True)
    subprocess.run([sys.executable, os.path.join(tools, "find_gaps.py"), "--gen", gen,
                    "--report", f"{d}/h.txt", "--toml", f"{d}/c.toml"], capture_output=True)
    sel = [(m.group(1), int(m.group(2))) for l in open(f"{d}/c.toml")
           for m in [re.match(r'"(0x[0-9A-F]+)" = \{ \}\s*#\s*(\d+) bytes', l)]
           if m and 8 <= int(m.group(2)) <= 1024 and m.group(1) not in examined]
    examined |= {a for a, _ in sel}
    if not sel:
        print(f"pass {n}: no new gaps; fixpoint")
        break
    ov = open(overrides_path, encoding="utf-8").read()
    m = re.search(r"^\[functions\]\s*$", ov, re.M)
    scratch_overrides = f"{d}/overrides.toml"
    open(scratch_overrides, "w", encoding="utf-8").write(
        ov[:m.end()] + "\n" + "".join(f'"{a}" = {{ }}\n' for a, _ in sel) + ov[m.end():])
    open(f"{d}/selected.txt", "w").write("".join(f"{a} {s}\n" for a, s in sel))
    # Scratch manifest: same game, output in the pass folder, and the scratch overrides instead of the real ones.
    # ds2-nx: relative names inside the pass folder and forward slashes (a TOML string takes \\ as an escape), only
    # the entry point's module, and the flavour file the real manifest includes.
    game = os.path.join(root, "assets", "game_root").replace("\\", "/")
    flavour = os.path.join(app, "codegen_flavor.toml").replace("\\", "/")
    man = open(manifest_path).read().split("[[modules]]", 1)[0]
    man = re.sub(r'^game_root = .*$', f'game_root = "{game}"', man, flags=re.M)
    man = re.sub(r'^file_path = .*$', f'file_path = "{game}/default.xex"', man, flags=re.M)
    man = re.sub(r'^out_directory_path = .*$', 'out_directory_path = "generated"', man, flags=re.M)
    man = re.sub(r'^includes = .*$', f'includes = ["overrides.toml", "{flavour}"]', man, flags=re.M)
    open(f"{d}/manifest.toml", "w").write(man)
    r = subprocess.run([rexglue, "--force", "codegen", "manifest.toml"], cwd=d, capture_output=True, text=True,
                       errors="replace")
    open(f"{d}/codegen.log", "w", encoding="ascii", errors="replace").write(r.stdout + r.stderr)
    # A failed validation still writes the listings the classification needs (as in pointer_fixpoint.py); only a
    # manifest that was never read is fatal.
    if "Failed to load manifest" in (r.stdout + r.stderr)[-800:] or not os.path.isdir(f"{d}/generated"):
        print(f"pass {n}: scratch codegen failed (see {d}/codegen.log)")
        break
    subprocess.run([sys.executable, os.path.join(tools, "gap_classify.py"), f"{d}/generated", f"{d}/selected.txt",
                    f"{d}/codegen.log", f"{d}/acc.txt"], capture_output=True)
    subprocess.run([sys.executable, os.path.join(tools, "gap_classify.py"), "--tier2", f"{d}/generated",
                    f"{d}/selected.txt", f"{d}/codegen.log", overrides_path, f"{d}/t2.txt"], capture_output=True)
    have = set(re.findall(r'"(0x[0-9A-Fa-f]+)"\s*=', ov))
    # ds2-nx: two kinds of false gap. find_gaps.py counts one instruction per comment line, so a function with a jump
    # table in the code (the table's words have no comment) looks shorter than it is and its last bytes, the case
    # code, show up as a gap: declaring a function there would cut the switch from its cases. Such a gap holds a
    # label of the generated code (the cases are branch targets). And a table whose words read as instructions that
    # end in a terminator is data: at least half of its words are 4-aligned addresses inside the code range.
    labels = sorted({int(x, 16) for f in glob.glob(os.path.join(gen, "*_recomp.*.cpp"))
                     for x in re.findall(r"^loc_([0-9A-F]{8}):", open(f, errors="replace").read(), re.M)})
    image_path = os.path.join(root, "out", "default_image.bin")
    image = open(image_path, "rb").read() if os.path.exists(image_path) else b""
    code_lo, code_hi = ns_code_range()

    def false_gap(address, size):
        i = bisect.bisect_left(labels, address)
        if i < len(labels) and labels[i] < address + size:
            return "label"
        if image:
            words = [struct.unpack_from(">I", image, address - 0x82000000 + k)[0] for k in range(0, size, 4)]
            pointers = sum(1 for w in words if code_lo <= w < code_hi and not w & 3)
            if pointers >= max(2, len(words) // 2):
                return "table"
        return ""

    dropped = {"label": 0, "table": 0}
    accepted = []
    for l in open(f"{d}/acc.txt").readlines() + open(f"{d}/t2.txt").readlines():
        a, s, rest = l.split(" ", 2)
        ins = [i.strip() for i in rest.lstrip("| ").strip().split(" ; ")]
        ops = [i.split()[0] for i in ins]
        if (a in have or any(re.search(r"\br1\b", i) for i in ins)
                or any(o in ("mtlr", "mflr", "ld", "std", "stwu", "lfd", "stfd") for o in ops)
                or ns["reads_undefined_at_entry"](ins)):
            continue
        why = false_gap(int(a, 16), int(s))
        if why:
            dropped[why] += 1
            continue
        accepted.append(l)
        have.add(a)
    print(f"pass {n}: {len(sel)} new gaps, {len(accepted)} accepted; false gaps dropped: {dropped['label']} holding a "
          f"generated label, {dropped['table']} address tables", flush=True)
    if dry_run:
        open(f"{d}/dry-accepted.txt", "w").writelines(accepted)
        print(f"dry run: see {d}/dry-accepted.txt")
        break
    if not accepted:
        print("fixpoint")
        break
    with open(evidence_path, "a") as ev_file:
        ev_file.writelines(accepted)
    block = f"\n# Structural gap scan, fixpoint pass {n} (tools/gap_fixpoint.py).\n" + "".join(
        f'"{l.split()[0]}" = {{ }}\n' for l in accepted)
    open(overrides_path, "w", encoding="utf-8", newline="").write(ov[:m.end()] + block + ov[m.end():])
    # ds2-nx: the Windows codegen script; its last step calls plain `python`, which may be the Store alias and
    # fail there, so the post-codegen steps are run again with this interpreter and that is the result that counts.
    b = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                        os.path.join(tools, "codegen.ps1"), "-Rexglue", rexglue], capture_output=True, text=True,
                       errors="replace")
    b2 = subprocess.run([sys.executable, os.path.join(tools, "post_codegen.py")], capture_output=True, text=True,
                        errors="replace")
    if b2.returncode:
        print("codegen failed")
        print((b.stdout + b.stderr + b2.stdout + b2.stderr)[-2000:])
        break
