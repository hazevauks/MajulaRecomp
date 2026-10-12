#!/usr/bin/env python3
"""Where the busiest threads of a running PC build are: samples their instruction pointer and the return addresses on
their stacks, and names them with llvm-symbolizer and out/pc/ds2.pdb. For a session that stopped making progress
(a guest loop that never ends, a lock nobody releases) on a machine with no debugger installed.

  python tools/win/sample_threads.py [--exe ds2.exe] [--threads 4] [--samples 3] [--depth 10]

Each sample suspends one thread for a moment (SuspendThread, GetThreadContext, a read of the top of its stack) and
resumes it. The "stack" is every value in the first 4 KB above the stack pointer that points into the executable's
image: return addresses, plus the odd stale one. The guest function names (sub_XXXXXXXX) are the useful part.
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import os
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
k32 = ctypes.WinDLL('kernel32', use_last_error=True)
psapi = ctypes.WinDLL('psapi', use_last_error=True)
k32.OpenProcess.restype = wt.HANDLE
k32.OpenThread.restype = wt.HANDLE
k32.CreateToolhelp32Snapshot.restype = wt.HANDLE


class THREADENTRY32(ctypes.Structure):
    _fields_ = [('dwSize', wt.DWORD), ('cntUsage', wt.DWORD), ('th32ThreadID', wt.DWORD),
                ('th32OwnerProcessID', wt.DWORD), ('tpBasePri', wt.LONG), ('tpDeltaPri', wt.LONG), ('dwFlags', wt.DWORD)]


class MODULEINFO(ctypes.Structure):
    _fields_ = [('lpBaseOfDll', ctypes.c_void_p), ('SizeOfImage', wt.DWORD), ('EntryPoint', ctypes.c_void_p)]


def pid_of(name):
    out = subprocess.run(['tasklist', '/FI', f'IMAGENAME eq {name}', '/FO', 'CSV', '/NH'], capture_output=True,
                         text=True).stdout
    for line in out.splitlines():
        parts = [p.strip('"') for p in line.split('","')]
        if len(parts) > 1 and parts[0].strip('"').lower() == name.lower():
            return int(parts[1])
    sys.exit(f'{name} is not running')


def threads_of(pid):
    snap = k32.CreateToolhelp32Snapshot(4, 0)
    entry = THREADENTRY32()
    entry.dwSize = ctypes.sizeof(entry)
    ids = []
    ok = k32.Thread32First(wt.HANDLE(snap), ctypes.byref(entry))
    while ok:
        if entry.th32OwnerProcessID == pid:
            ids.append(entry.th32ThreadID)
        ok = k32.Thread32Next(wt.HANDLE(snap), ctypes.byref(entry))
    k32.CloseHandle(wt.HANDLE(snap))
    return ids


def cpu_time(handle):
    times = [wt.FILETIME() for _ in range(4)]
    if not k32.GetThreadTimes(wt.HANDLE(handle), *[ctypes.byref(t) for t in times]):
        return 0
    return sum((t.dwHighDateTime << 32 | t.dwLowDateTime) for t in times[2:]) / 1e7


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', default='ds2.exe')
    ap.add_argument('--threads', type=int, default=4)
    ap.add_argument('--samples', type=int, default=3)
    ap.add_argument('--depth', type=int, default=10)
    ap.add_argument('--all-modules', action='store_true',
                    help='list stack values that point into any module, not only into the executable')
    a = ap.parse_args()
    pid = pid_of(a.exe)
    process = k32.OpenProcess(0x0410, False, pid)  # QUERY_INFORMATION | VM_READ
    if not process:
        sys.exit(f'OpenProcess failed ({ctypes.get_last_error()})')
    module = wt.HMODULE()
    needed = wt.DWORD()
    psapi.EnumProcessModules(wt.HANDLE(process), ctypes.byref(module), ctypes.sizeof(module), ctypes.byref(needed))
    info = MODULEINFO()
    psapi.GetModuleInformation(wt.HANDLE(process), module, ctypes.byref(info), ctypes.sizeof(info))
    base, size = info.lpBaseOfDll, info.SizeOfImage
    # Every module, to name an address outside the executable (the Vulkan driver, ntdll, ...).
    array = (wt.HMODULE * 512)()
    psapi.EnumProcessModules(wt.HANDLE(process), array, ctypes.sizeof(array), ctypes.byref(needed))
    modules = []
    name = ctypes.create_unicode_buffer(260)
    for handle in array[:needed.value // ctypes.sizeof(wt.HMODULE)]:
        other = MODULEINFO()
        if psapi.GetModuleInformation(wt.HANDLE(process), wt.HMODULE(handle), ctypes.byref(other), ctypes.sizeof(other)):
            psapi.GetModuleBaseNameW(wt.HANDLE(process), wt.HMODULE(handle), name, 260)
            modules.append((other.lpBaseOfDll or 0, other.SizeOfImage, name.value))

    def outside(address):
        for start, length, module_name in modules:
            if start <= address < start + length:
                return f'{module_name}+{address - start:#x}'
        return 'code outside every module (run-time compiled)'
    handles = {t: k32.OpenThread(0x004A, False, t) for t in threads_of(pid)}  # SUSPEND_RESUME | GET_CONTEXT | QUERY
    before = {t: cpu_time(h) for t, h in handles.items() if h}
    time.sleep(1.0)
    busy = sorted(((cpu_time(handles[t]) - c, t) for t, c in before.items()), reverse=True)[:a.threads]
    context = ctypes.create_string_buffer(1232 + 16)
    aligned = (ctypes.addressof(context) + 15) & ~15
    stack = ctypes.create_string_buffer(4096)
    read = ctypes.c_size_t()
    wanted = []
    for used, thread in busy:
        for sample in range(a.samples):
            handle = wt.HANDLE(handles[thread])
            if k32.SuspendThread(handle) == 0xFFFFFFFF:
                continue
            ctypes.memset(aligned, 0, 1232)
            ctypes.c_uint32.from_address(aligned + 0x30).value = 0x10000B  # CONTEXT_AMD64 | CONTROL | INTEGER | FP
            frames = []
            if k32.GetThreadContext(handle, ctypes.c_void_p(aligned)):
                rip = ctypes.c_uint64.from_address(aligned + 0xF8).value
                rsp = ctypes.c_uint64.from_address(aligned + 0x98).value
                frames.append(rip)
                if k32.ReadProcessMemory(wt.HANDLE(process), ctypes.c_void_p(rsp), stack, 4096, ctypes.byref(read)):
                    for i in range(0, read.value - 7, 8):
                        value = int.from_bytes(stack.raw[i:i + 8], 'little')
                        if len(frames) < a.depth and (base <= value < base + size or (
                                a.all_modules and any(m <= value < m + n for m, n, _ in modules))):
                            frames.append(value)
            k32.ResumeThread(handle)
            wanted.append((thread, used, sample, frames))
            time.sleep(0.15)
    exe = os.path.join(ROOT, 'out', 'pc', a.exe)
    symbolizer = os.path.join(ROOT, '_tools', 'llvm', 'bin', 'llvm-symbolizer.exe')
    addresses = sorted({f for _, _, _, frames in wanted for f in frames if base <= f < base + size})
    names = {}
    if addresses and os.path.exists(symbolizer):
        out = subprocess.run([symbolizer, '-e', exe, '--relative-address', '--functions=short', '--output-style=LLVM',
                              '--no-inlines'] + [hex(x - base) for x in addresses], capture_output=True, text=True).stdout
        blocks = [b for b in out.split('\n\n') if b.strip()]
        for address, block in zip(addresses, blocks):
            names[address] = block.strip().split('\n')[0]
    for thread, used, sample, frames in wanted:
        if sample == 0:
            print(f'thread {thread}: {used * 100:.0f} % of a core')
        text = [names.get(f, outside(f) if not base <= f < base + size else hex(f - base)) for f in frames]
        print(f'  sample {sample}: ' + ' < '.join(text))


if __name__ == '__main__':
    main()
