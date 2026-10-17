#!/usr/bin/env python3
"""The post-codegen steps of the codegen flavour, in one place (docs/enhancements.md, "Build-time").

Run after EVERY codegen, by tools/codegen.ps1 and by the build itself: app/CMakeLists.txt appends this script to the
generated ds2_codegen command, because the PC build re-runs the generator on its own whenever an input changes (the
manifest, a toml, the xex, the generator binary) and that would otherwise drop the steps silently.

  - the flavour is read from app/codegen_flavor.toml (written by tools/codegen.ps1 -Flavor): "optimised" when it sets
    any codegen key, else "classic";
  - optimised: tools/pch_no_volatile.py + tools/loop_barriers.py (required together) and tools/direct_calls.py on both
    generated trees;
  - both: the localized native copies (tools/localize_native.py --list app/src/cpu/hot/localized.txt) and
    app/generated/codegen_flavor.txt (the build's name in the first "[enh]" log line).

  python tools/post_codegen.py
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
APP = os.path.join(ROOT, 'app')
GEN = os.path.join(APP, 'generated')


def run(*args):
    result = subprocess.run([sys.executable, *args], cwd=ROOT)
    if result.returncode != 0:
        sys.exit(f'post_codegen: {" ".join(os.path.basename(a) for a in args[:1])} failed ({result.returncode})')


def tool(name):
    return os.path.join(ROOT, 'tools', name)


IMAGE = os.path.join(ROOT, 'out', 'default_image.bin')
PROMOTIONS = os.path.join(APP, 'indirect_promotions.txt')


def main():
    flavor_toml = open(os.path.join(APP, 'codegen_flavor.toml'), encoding='utf-8').read()
    optimised = re.search(r'^\s*[a-z_]+\s*=\s*true', flavor_toml, re.M) is not None
    flavor = 'optimised' if optimised else 'classic'
    # Dark Souls II has a single module (Default.xex -> generated/default); a guest DLL would add its tree here.
    trees = [tree for tree in ('default',) if os.path.isdir(os.path.join(GEN, tree))]
    if optimised:
        # ds2-nx 2026-10-17: the sequence of sa2-nx 1.0.2 (its tools/post_codegen.py; docs/cpu-rounds.md there).
        if not os.path.isfile(IMAGE):
            sys.exit('post_codegen: out/default_image.bin is missing (the image dump of the code generator): run '
                     'tools/codegen.ps1 -Flavor optimised once, it writes it (REXGLUE_DUMP_IMAGE)')
        run(tool('pch_no_volatile.py'), *[os.path.join(GEN, tree, 'ds2_pch.h') for tree in trees])
        os.environ.setdefault('REXSDK_DIR', os.path.join(ROOT, 'sdk'))
        # Indirect calls tagged by site, and the sites of app/indirect_promotions.txt (a PC census) as guarded direct
        # calls. Before direct_calls.py and BEFORE loop_barriers.py (a promoted call is no longer an indirect call).
        promote = ['--promote', PROMOTIONS] if os.path.isfile(PROMOTIONS) else []
        run(tool('indirect_calls.py'), '--gen', os.path.join(GEN, 'default'), *promote)
        # REQUIRED with pch_no_volatile.py: without barriers GCC turns guest polling loops into infinite loops.
        run(tool('loop_barriers.py'), *[os.path.join(GEN, tree) for tree in trees])
        run(tool('pch_cold_traps.py'), *[os.path.join(GEN, tree, 'ds2_pch.h') for tree in trees])
        for tree in trees:
            # Undo and redo in one pass: a function hooked since the last run (a new native) must be called through
            # its hook again. --redo writes only the files that change, so the build does not recompile the others.
            run(tool('direct_calls.py'), '--redo', '--gen', os.path.join(GEN, tree))
        for tree in trees:
            # sasrt-nx's steps, after direct_calls.py (the flush-mode proofs depend on which callees are direct).
            run(tool('guest_cas.py'), '--gen', os.path.join(GEN, tree))
            run(tool('flush_mode_flow.py'), '--redo', '--gen', os.path.join(GEN, tree), '--image', IMAGE)
            run(tool('vec_byteswap.py'), '--gen', os.path.join(GEN, tree))
            run(tool('pch_neon_shuffles.py'), os.path.join(GEN, tree, 'ds2_pch.h'))
        # ro_constants before float_loads (it folds the integer form of a float load).
        run(tool('ro_constants.py'), '--gen', os.path.join(GEN, 'default'), '--image', IMAGE)
        for tree in trees:
            run(tool('float_loads.py'), '--gen', os.path.join(GEN, tree))
            run(tool('frame_traffic.py'), '--gen', os.path.join(GEN, tree))
            # The ctx registers of call-free loops as locals for the length of the loop.
            run(tool('loop_locals.py'), '--gen', os.path.join(GEN, tree))
    # The localized native copies exist only once the port has hot functions to localize (stage 9 of the guide).
    localized = os.path.join(APP, 'src', 'cpu', 'hot', 'localized.txt')
    if os.path.isfile(localized):
        run(tool('localize_native.py'), '--list', localized)
    if optimised:
        for tree in trees:
            # The ctx registers of leaf functions with loops as locals. Last: it renames registers.
            run(tool('leaf_locals.py'), '--gen', os.path.join(GEN, tree))
    # Round 7: the options added since round 6 are named too, so a console log tells the builds apart.
    extras = [name for key, name in (('non_volatile_as_local', 'non-volatile locals'), ('elide_lr_stores', 'no lr stores'),
                                     ('fp_moves_modeless', 'modeless fp moves'),
                                     ('non_argument_as_local', 'non-argument locals'),
                                     ('fp_mode_lookahead', 'fp mode lookahead'),
                                     ('fcti_inline', 'second round'))
              if re.search(r'^\s*' + key + r'\s*=\s*true', flavor_toml, re.M)]
    # Written only when it changes: the file is a CMake configure dependency (app/CMakeLists.txt), and rewriting it
    # made every build re-run CMake, rebuild the generator and run the codegen again (2026-10-07).
    text = f'{flavor} codegen' + (f' ({", ".join(extras)})' if extras else '')
    path = os.path.join(GEN, 'codegen_flavor.txt')
    try:
        old_text = open(path, encoding='utf-8').read()
    except OSError:
        old_text = None
    if old_text != text:
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            f.write(text)
    print(f'post_codegen: {flavor} codegen steps done')


if __name__ == '__main__':
    main()
