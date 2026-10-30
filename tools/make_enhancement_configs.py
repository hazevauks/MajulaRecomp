#!/usr/bin/env python3
"""Writes the premade enhancement configurations from app/ds2_switch.toml (the default), each differing from it only
in what its name says (docs/enhancements.md):

  ds2_cpu_on.toml       the default, profiler on                   (A/B: the CPU group on)
  ds2_cpu_off.toml      ds2_cpu_enhancements = false, profiler on  (A/B: the CPU group off, non-CPU on)
  ds2_noncpu_off.toml   ds2_noncpu_enhancements = false            (the non-CPU group off, CPU on)
  ds2_all_off.toml      both masters false                         (the original paths everywhere)
  ds2_cpu_verify.toml   ds2_cpu_verify = true                      (every enhancement on, every use checked; slow)
  ds2_profile.toml      ds2_gpu_marks_categories = true     (GPU time per pass category)
  ds2_test_dedupe_off.toml        ds2_native_constants_dedupe = false   (flicker test: constants dedupe off)
  ds2_test_copies_off.toml        the 52 unguarded localized copies off, profiler on (A/B against ds2_cpu_on.toml)
  ds2_test_upload_uncached.toml   the upload buffer in uncached memory as before, profiler on (A/B)
  ds2_test_driver_paths_off.toml  the nfsmw NVK driver paths off         (flicker test: set 4 by differences etc.)
  ds2_test_deferred_off.toml      deferred Vulkan recording off (on by default since round 5), profiler on (A/B)
  ds2_test_direct_state_off.toml  the direct renderer's state records off: the ring parses PM4 as before, profiler on (A/B)
  ds2_test_worker_core2.toml      the deferred-recording worker on core 2 only (default: core 0 only), profiler on
  ds2_test_worker_core_any.toml   the deferred-recording worker on any of cores 0-2, profiler on
  ds2_test_main_priority_on.toml  the drawing game thread at 0x3A, profiler on (A/B against ds2_cpu_on.toml)
  ds2_test_sync_epochs_off.toml   round 7's dedupe sync epochs off (the old address-only reuse), profiler on (A/B: cost)
  ds2_test_direct_flush_off.toml  the direct renderer step 2 off: Direct3D's flushes write PM4 again, profiler on (A/B)
  ds2_test_gpu_round_off.toml     the GPU round's three keys off (resolve draws, blend skip, resolve aliases), profiler on
  ds2_all_on.toml                 both masters on, said explicitly (= ds2.toml: every CPU and GPU enhancement), profiler on
  ds2_test_api_off.toml           the native Direct3D layer off (draws and Clear write PM4 again), profiler on (A/B)
  ds2_test_gpu_round2_off.toml    GPU round 2's six keys off (push once, structured CF, index base, three spec bits)
  ds2_test_log_writer_off.toml    the log writer thread off: the synchronous file log as before, profiler on (A/B)
  ds2_test_render_scale_off.toml  the render resolution off: 1280x720 docked too, as before, profiler on (A/B: docked GPU)
  ds2_test_msaa2.toml             2x MSAA of the game's render targets (ds2_msaa = 2), profiler on
  ds2_test_msaa4.toml             4x MSAA of the game's render targets (ds2_msaa = 4), profiler on
  ds2_test_vsync_relaxed_off.toml the old fixed vsync (a frame that misses 60 waits for the next vblank), profiler on
  ds2_test_round13_off.toml       round 13's hand-written vertex loaders, njCalcPoint and njMultiMatrix off, profiler on
  ds2_test_vs_fetch_reorder_off.toml  vertex shaders Direct3D reordered dropped again unless the package has them, profiler on

The profiler is on in ds2.toml itself and in every configuration except ds2_cpu_verify.toml (2026-10-06, the user's
choice: every test run brings its profile; verify is slow anyway): ds2_gpu_marks_categories = true (GPU time per
pass category and the idle gap in the "GPU per Swap" lines) and switch_profile_stacks = true (the CPU sampler,
rex_profile.log). Every A/B side pays the same profiling cost. The variants below still set both keys explicitly, so a
config stays profiled even if the default changes.

  python tools/make_enhancement_configs.py [--out <folder>]     (default: out/nx/configs)
"""
import argparse
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PROFILING = {'ds2_gpu_marks_categories': 'true', 'switch_profile_stacks': 'true'}
NO_PROFILING = {'ds2_gpu_marks_categories': 'false', 'switch_profile_stacks': 'false'}
PROFILING_NOTE = ('# Profiler on: switch_profile_stacks = true makes the CPU sampler record where each thread spends its\n'
                  '# time (logs/rex/rex_profile.log); the GPU time per pass category is in\n'
                  '# the "GPU per Swap" log lines. Every config except ds2_cpu_verify profiles, so A/B runs compare\n'
                  '# fairly.\n')

VARIANTS = {
    'ds2_cpu_on.toml': dict(PROFILING),
    'ds2_cpu_off.toml': {'ds2_cpu_enhancements': 'false', **PROFILING},
    'ds2_noncpu_off.toml': {'ds2_noncpu_enhancements': 'false', **PROFILING},
    'ds2_all_off.toml': {'ds2_cpu_enhancements': 'false', 'ds2_noncpu_enhancements': 'false', **PROFILING},
    # Verify mode without the profiler (it checks, it does not measure).
    'ds2_cpu_verify.toml': {'ds2_cpu_verify': 'true', **NO_PROFILING},
    'ds2_profile.toml': dict(PROFILING),
    # Truck-light flicker tests (2026-10-06): the two paths that write shader constants differently from before.
    'ds2_test_dedupe_off.toml': {'ds2_native_constants_dedupe': 'false', **PROFILING},
    # A/B of the 53 unguarded localized copies (round 1's 15, round 3's 20 and round 4's 18; on by default since round
    # 4, smart sync since round 3), profiler on. The two guarded round-4 leaves keep their default.
    'ds2_test_copies_off.toml': {**{f'ds2_cpu_native_sub_{a}': 'false' for a in (
        '8213A4D8', '82139DA0', '8214E130', '8257AE20', '821C0160', '8256DB68', '82991068', '82141420',
        '8257ACC8', '82990CB0', '8298FD38', '825775A0', '82990B80', '8214D6D8', '8214B6C8', '8214B520', '8213D1F8',
        '82990528', '82862DB8', '8213D7F0', '8214ACA8', '82862938', '82864060', '82854670', '821BA890', '82863B78',
        '82864698', '82846F00', '82139930', '82864778', '821472E0', '82565758', '8213E040', '828637A8', '8258B908',
        '8257B930', '8257AF70', '8251AF90', '82141050', '821390C8', '8217EB38', '82141240', '82588F10', '8251D1A8',
        '82477AF0', '82227C98', '82573328', '82572598', '82139C90', '8217F048', '8214EAF0', '8213D5C8',)}, **PROFILING},
    # A/B of round 3's cached upload buffer: the uncached memory type, as before.
    'ds2_test_upload_uncached.toml': {'ds2_native_upload_memory': '0', **PROFILING},
    # A/B of deferred Vulkan recording (round 2; on by default since round 5): the ring thread queues the vkCmd* calls
    # and a worker records them, same commands in the same order (masseffect-nx's console: ring thread 62 -> 40 %).
    # This one records on the ring thread as before. Profiler on, so it compares with ds2_cpu_on.toml.
    'ds2_test_deferred_off.toml': {'ds2_native_deferred_recording': 'false',
                                   'ds2_deferred_native_fast': 'false',
                                   'ds2_deferred_native_update': 'false', **PROFILING},
    # A/B of the direct renderer step 1 (round 5): the ring parses every state packet as PM4, as before.
    'ds2_test_direct_state_off.toml': {'ds2_cpu_direct_state': 'false', **PROFILING},
    # Round 6, scheduling A/B: the deferred-recording worker is pinned to the process's default core (core 0) at a
    # priority above the game threads'; these let it run on core 2 only, or on any core.
    'ds2_test_worker_core2.toml': {'ds2_native_deferred_worker_core': '2', **PROFILING},
    'ds2_test_worker_core_any.toml': {'ds2_native_deferred_worker_core': '-1', **PROFILING},
    # A/B of the drawing game thread at priority 0x3A (round 2, from nfsmw-nx), profiler on.
    'ds2_test_main_priority_on.toml': {'ds2_cpu_main_thread_priority': 'true', **PROFILING},
    'ds2_test_sync_epochs_off.toml': {'ds2_native_dedupe_sync_epochs': 'false', **PROFILING},
    # A/B of the direct renderer step 2 (2026-10-07): Direct3D's state flush writes PM4 and the hook decodes it again.
    'ds2_test_direct_flush_off.toml': {'ds2_cpu_direct_flush': 'false', **PROFILING},
    # A/B of the GPU round (2026-10-07): resolves as copies on NVK's copy engine, no blend skip, every resolve copied.
    'ds2_test_gpu_round_off.toml': {'ds2_gpu_resolve_draw': 'false', 'ds2_gpu_blend_skip': 'false',
                                    'ds2_gpu_resolve_alias': 'false', **PROFILING},
    # Every CPU and GPU enhancement on (2026-10-07, the user's request): the default already is, this says it. MSAA
    # stays off as in ds2.toml (2x was in here for one console test; the user set it back the same day).
    'ds2_all_on.toml': {'ds2_cpu_enhancements': 'true', 'ds2_noncpu_enhancements': 'true', **PROFILING},
    # A/B of the native Direct3D layer (2026-10-07, docs/api-renderer.md): Direct3D's draws and Clear write PM4 again.
    'ds2_test_api_off.toml': {'ds2_api_draws': 'false', 'ds2_api_clears': 'false', **PROFILING},
    # A/B of GPU round 2 (2026-10-07): per-draw push constants, the skipTo control flow, the per-draw vertex index base
    # and the three per-vertex/pixel tests, as before (the shader package stays the same).
    'ds2_test_gpu_round2_off.toml': {'ds2_gpu_push_once': 'false', 'ds2_gpu_structured_cf': 'false',
                                     'ds2_gpu_index_base_held': 'false', 'ds2_gpu_texcoords_unswapped': 'false',
                                     'ds2_gpu_no_texture_signs': 'false', 'ds2_gpu_identity_remap': 'false',
                                     **PROFILING},
    # A/B of the log writer thread (2026-10-07, from futari-nx): the synchronous rotating file log, as before.
    'ds2_test_log_writer_off.toml': {'log_file_writer': 'false', **PROFILING},
    # Render resolution and MSAA (2026-10-07, docs/enhancements.md): docked at the game's 1280x720 as before (what
    # 1080p costs the GPU), and the two MSAA levels (MSAA works on the console: the user's test, 2026-10-07).
    'ds2_test_render_scale_off.toml': {'ds2_gpu_render_scale': 'false', **PROFILING},
    'ds2_test_msaa2.toml': {'ds2_msaa': '2', **PROFILING},
    'ds2_test_msaa4.toml': {'ds2_msaa': '4', **PROFILING},
    # Round 13 (2026-10-07): relaxed vsync off (the fall from 60 straight to 30 comes back), and the hand-written
    # vertex loaders / njCalcPoint / njMultiMatrix off (njCalcPoint and njMultiMatrix back to the original, not to their
    # older localized copies: the keys are the same).
    'ds2_test_vsync_relaxed_off.toml': {'ds2_vsync_relaxed': 'false', **PROFILING},
    'ds2_test_round13_off.toml': {'ds2_cpu_native_vertex_normals': 'false', 'ds2_cpu_native_vertex_points': 'false',
                                  'ds2_cpu_native_calc_point': 'false', 'ds2_cpu_native_multi_matrix': 'false',
                                  **PROFILING},
    # 2026-10-07: vertex shaders Direct3D loads with their fetches reordered are no longer identified by the run-time
    # proof (the Chao garden's stats window: drawn only if the package has a container repaired for that order).
    'ds2_test_vs_fetch_reorder_off.toml': {'ds2_gpu_vs_fetch_reorder': 'false', **PROFILING},
    'ds2_test_driver_paths_off.toml': {'ds2_native_set4_differences': 'false',
                                       'ds2_native_nvk_emission': 'false', 'ds2_native_nvk_cbufs': 'false',
                                       'ds2_native_nvk_dynamic': 'false', 'ds2_native_nvk_preload': 'false',
                                       **PROFILING},
}


# The Switch build has logging off (app/ds2_switch_defaults.toml): every test configuration turns the log back on.
LOGGING = {'log_level': '"info"', 'log_to_file': 'true', 'switch_profile_report': 'true'}


def apply(text, changes, name):
    changes = {**LOGGING, **changes}
    for key, value in changes.items():
        pattern = re.compile(r'^' + re.escape(key) + r'\s*=.*$', re.M)
        if pattern.search(text):
            text = pattern.sub(f'{key} = {value}', text, count=1)
        else:
            # Top-level keys go before the first [table] (ds2.toml ends with [input_p1] / [input_p2]): after it they
            # would belong to that table.
            table = re.search(r'^\[', text, re.M)
            line = f'{key} = {value}\n'
            text = text[:table.start()] + line + '\n' + text[table.start():] if table else text + '\n' + line
    header = (f'# {name}: app/ds2_switch.toml with ' + ', '.join(f'{k} = {v}' for k, v in changes.items()) +
              ' (tools/make_enhancement_configs.py). Copy it over ds2.toml next to the NRO.\n')
    if changes.get('ds2_gpu_marks_categories') == 'true':
        header += PROFILING_NOTE
    return header + text


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', default=os.path.join(ROOT, 'out', 'nx', 'configs'))
    a = ap.parse_args()
    base = open(os.path.join(ROOT, 'app', 'ds2_switch.toml'), encoding='utf-8').read()
    os.makedirs(a.out, exist_ok=True)
    for name, changes in VARIANTS.items():
        with open(os.path.join(a.out, name), 'w', encoding='utf-8', newline='\n') as f:
            f.write(apply(base, changes, name))
        print(os.path.join(a.out, name))


if __name__ == '__main__':
    main()
