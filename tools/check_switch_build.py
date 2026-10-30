#!/usr/bin/env python3
"""Checks a Switch build before it is packaged (from sasrt-nx's check_switch_build.py and nfsmw-nx's lessons).

  python tools/check_switch_build.py [--elf out/nx/ds2] [--nro out/nx/ds2.nro]

Checks (exit 1 if any fails):
  - _start is at address 0 (nfsmw-nx: a section-ordering file that moved the start-up code made the NRO unbootable);
  - the build names the optimised codegen flavour (the only one delivered);
  - the NRO links the nfsmw NVK driver of the SDK (its "Mesa x.y.z" string; the generic portlibs Mesa once got in);
  - branch-to-self loops in recompiled functions are only in the functions with a known guest halt (KNOWN_HALTS; a
    polling loop GCC turned into "b ." with non-volatile memory means a missing loop barrier);
  - no vector fmla/fmls in recompiled functions: the generated code has no vector fused multiply-add (VMX multiply-adds
    are a multiply and an add), so any would be contraction that leaked past -ffp-contract=off (e.g. at an LTO link).
    Scalar fmadd/fmsub/fnmadd/fnmsub come from the guest's own std::fma and are only counted.
"""
import argparse
import collections
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# Dark Souls II (2026-10-10, classic codegen, no LTO). Its image has no "b ." instruction at all; the five
# branch-to-self the first NRO has are loops of the original code that never end for an input the game does not
# produce, and that read no memory on that path: a list search that keeps comparing a null node with the wanted one
# (sub_82C187B8), a fill loop whose count is not reloaded (sub_8292C568). GCC proves them endless and emits "b .".
# Read in the generated code: sub_82C187B8 and sub_8292C568; the other three have the same shape in the disassembly
# (a compare of two unchanged registers before the branch). A branch-to-self anywhere else is a polling loop that lost
# its barrier.
# Two more of the same kind show as "b ." only in the PGO-instrumented build (2026-10-11; the plain build compiles them
# as a compare and a conditional branch): sub_8288E410, a copy of a quoted string that stops reading at 512 bytes and
# then compares the last byte with '"' forever when there was no quote (read in the generated code), and sub_82B77BB0
# (in the disassembly: a test of an unchanged register's low byte before the branch, no memory access on that path).
# And one in the instrumented build of the sa2-nx 1.0.2 rounds (2026-10-17): sub_82DD01A8, a remainder by repeated
# subtraction (subf r9,r10,r9 until r9 < r8) entered only when the divisor r10 is 0, which never changes in the loop;
# on the game's path r10 is the count it has just set to 0 and incremented, so 1 (read in the generated code).
KNOWN_HALTS = ['sub_8292C568', 'sub_82B41518', 'sub_82C187B8', 'sub_82C6F9D0', 'sub_82E15628',
               'sub_8288E410', 'sub_82B77BB0', 'sub_82DFFE60']
DEVKITA64 = next((d + '/devkitA64/bin/' for d in (os.environ.get('DS2_DEVKITPRO', ''), os.environ.get('DEVKITPRO', ''),
                                                  'C:/devkitPro', os.path.expanduser('~/Downloads/devkitPro'),
                                                  '/opt/devkitpro')
                  if d and os.path.isdir(d + '/devkitA64/bin')), 'C:/devkitPro/devkitA64/bin/')
EXE = '.exe' if os.name == 'nt' else ''


def generated_calls():
    """name -> the recompiled functions its generated C++ body calls directly (app/generated/*/ *_recomp.*.cpp)."""
    import glob
    out = {}
    head = re.compile(r'^DEFINE_REX_FUNC\((sub_[0-9A-F]{8})\) \{$', re.M)
    call = re.compile(r'\b(?:__imp__)?(sub_[0-9A-F]{8})\(ctx, base\)')
    for path in glob.glob(os.path.join(ROOT, 'app', 'generated', '*', '*_recomp.*.cpp')):
        text = open(path, encoding='utf-8', errors='replace').read()
        heads = list(head.finditer(text))
        for i, m in enumerate(heads):
            end = heads[i + 1].start() if i + 1 < len(heads) else len(text)
            out[m.group(1)] = set(call.findall(text, m.end(), end))
    return out


def chain_to(calls, f, targets, depth=4, seen=()):
    """The shortest-first chain of direct calls from f to one of the targets, at most `depth` calls long, or None."""
    callees = calls.get(f, ())
    hit = sorted(callees & targets)
    if hit:
        return [f, hit[0]]
    if depth > 1:
        for c in sorted(callees):
            if c not in seen:
                rest = chain_to(calls, c, targets, depth - 1, seen + (f,))
                if rest:
                    return [f] + rest
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--elf', default=os.path.join(ROOT, 'out', 'nx', 'ds2'))
    ap.add_argument('--nro', default=os.path.join(ROOT, 'out', 'nx', 'ds2.nro'))
    ap.add_argument('--halts', default=','.join(KNOWN_HALTS),
                    help='the recompiled functions with a genuine guest halt (b . in the original PowerPC code)')
    ap.add_argument('--sdk-vulkan', default=os.environ.get('NVK_SDK', os.path.join(
        ROOT, '_tools', 'mesa-switch', 'mesa-unified-install', 'opt', 'devkitpro', 'portlibs', 'switch'))
        + '/lib/libvulkan.a')
    a = ap.parse_args()
    failures = []

    nm = subprocess.run([DEVKITA64 + 'aarch64-none-elf-nm' + EXE, a.elf], capture_output=True, text=True).stdout
    start = next((int(l.split()[0], 16) for l in nm.splitlines() if l.endswith(' _start')), None)
    print(f'_start at {start:#x}' if start is not None else '_start not found')
    if start != 0:
        failures.append('_start is not at address 0')

    nro = open(a.nro, 'rb').read()
    # ds2-nx: the classic codegen is what is delivered until the optimised flavour is brought up for this game.
    print('flavour: ' + ('optimised codegen' if b'optimised codegen' in nro else 'classic codegen'))
    mesa = sorted(set(re.findall(rb'Mesa \d+\.\d+\.\d+ \(git-[0-9a-f]+\)', nro)))
    nvk = sum(1 for l in nm.splitlines() if ' nvk_' in l)
    if nvk == 0:
        # sa2-nx 1.0.3 (ported 2026-10-23): a build without the driver (REXGLUE_SWITCH_NO_NVK: Renut on deko3d,
        # tools\win\build_switch.ps1 -NoNvk) must carry none of Mesa's driver: no nvk_ symbol and no Mesa string.
        print('Vulkan driver: none linked (no nvk_ symbols); Mesa in the NRO:', [m.decode() for m in mesa])
        if mesa:
            failures.append('a build without the NVK driver carries a Mesa version string')
    else:
        sdk = sorted(set(re.findall(rb'Mesa \d+\.\d+\.\d+ \(git-[0-9a-f]+\)', open(a.sdk_vulkan, 'rb').read())))
        print('Mesa in the NRO:', [m.decode() for m in mesa], '| in the SDK:', [m.decode() for m in sdk])
        if not mesa or mesa != sdk:
            failures.append('the NRO does not carry the SDK driver\'s Mesa string')

    # One streaming pass over the disassembly: per function, b-to-self and fused multiply-adds.
    proc = subprocess.Popen([DEVKITA64 + 'aarch64-none-elf-objdump' + EXE, '-d', '--no-show-raw-insn', a.elf],
                            stdout=subprocess.PIPE, text=True, errors='replace', bufsize=1 << 20)
    func = None
    self_loops = []
    vector_fma = collections.Counter()
    scalar_fma = 0
    head = re.compile(r'^[0-9a-f]+ <([^>]+)>:$')
    ins = re.compile(r'^\s+([0-9a-f]+):\s+(\S+)\s*(.*)$')
    for line in proc.stdout:
        m = head.match(line)
        if m:
            func = m.group(1)
            continue
        m = ins.match(line)
        if not m or not func:
            continue
        addr, op, args = m.groups()
        guest = func.startswith(('sub_8', '__imp__sub_8'))
        if op == 'b' and args.split(' ')[0] == addr:
            if guest:
                self_loops.append(func)
        elif op in ('fmla', 'fmls') and guest and '.2d' in args:
            # ds2-nx: two of the guest's own scalar fmadd/fmsub (double lanes) that GCC put in one vector instruction
            # (sub_82C61958 has 24 of them in a row). VMX arithmetic is single precision (.4s lanes).
            scalar_fma += 2
        elif op in ('fmla', 'fmls') and guest:
            vector_fma[func] += 1
        elif op in ('fmadd', 'fmsub', 'fnmadd', 'fnmsub') and guest:
            scalar_fma += 1
    proc.wait()
    known = set(a.halts.split(','))
    functions = {f.replace('__imp__', '') for f in self_loops}
    unknown = sorted(functions - known)
    # Round 7: GCC may inline a known halt into a caller (non_volatile_as_local made the callees smaller: the halts of
    # sub_8290BC30 and sub_8296FA90 then appear in sub_82907490 and sub_8296FEB0), and with a higher inline limit
    # that caller into its own caller (sub_828F6008 inlines sub_82907490). A function whose generated body reaches
    # a known-halt function through at most four direct calls is accepted, and named with the chain.
    inlined = []
    if unknown:
        calls = generated_calls()
        for f in list(unknown):
            chain = chain_to(calls, f, known)
            if chain:
                inlined.append(f'{f} (inlines {" -> ".join(chain[1:])})')
                unknown.remove(f)
    print(f'branch-to-self in recompiled functions: {len(self_loops)} instructions in {len(functions)} functions, '
          f'{len(functions & known)} of them known guest halts'
          + (f'; {len(inlined)} more calling one that GCC inlined: {"; ".join(inlined)}' if inlined else ''))
    if unknown:
        failures.append('branch-to-self outside the known guest halts (a polling loop without its barrier?): '
                        + ', '.join(unknown[:20]))
    print(f'fused multiply-adds in recompiled functions: scalar {scalar_fma} (the guest\'s own), vector '
          f'{sum(vector_fma.values())} in {len(vector_fma)} functions (must be 0)')
    if vector_fma:
        failures.append('vector fmla/fmls in recompiled code (contraction leaked): '
                        + ', '.join(f'{k} x{v}' for k, v in vector_fma.most_common(10)))
    if failures:
        print('\nFAILED:\n  - ' + '\n  - '.join(failures))
        sys.exit(1)
    print('\nall checks passed')


if __name__ == '__main__':
    main()
