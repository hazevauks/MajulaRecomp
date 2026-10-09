#!/usr/bin/env python3
"""Says where a Windows crash dump of the PC build stopped: the exception, the module of the faulting address, and
the modules the faulting thread's stack points into. No debugger needed; it reads the minidump streams itself.

  python tools/minidump_summary.py                  the newest %LOCALAPPDATA%/CrashDumps/ds2.exe.*.dmp
  python tools/minidump_summary.py <file.dmp> ...

An address "not in a module" on a thread whose stack only names vulkan_lvp.dll is code the software Vulkan driver
(lavapipe) compiled at run time. An address in ds2.exe can be symbolized with llvm-symbolizer and out/pc/ds2.pdb.
"""
import glob
import os
import struct
import sys


def summarize(path):
    f = open(path, 'rb').read()
    _sig, _ver, count, directory = struct.unpack_from('<IIII', f, 0)
    streams = {}
    for i in range(count):
        kind, size, offset = struct.unpack_from('<III', f, directory + i * 12)
        streams[kind] = (offset, size)
    modules = []
    offset, _ = streams[4]  # ModuleListStream
    for i in range(struct.unpack_from('<I', f, offset)[0]):
        base, size, _chk, _ts, name_rva = struct.unpack_from('<QIIII', f, offset + 4 + i * 108)
        length = struct.unpack_from('<I', f, name_rva)[0]
        modules.append((base, size, os.path.basename(f[name_rva + 4:name_rva + 4 + length].decode('utf-16le'))))

    def where(address):
        for base, size, name in modules:
            if base <= address < base + size:
                return f'{name}+0x{address - base:X}'
        return None

    offset, _ = streams[6]  # ExceptionStream
    thread, _, code, _flags, _record, address, parameters, _ = struct.unpack_from('<IIIIQQII', f, offset)
    info = struct.unpack_from('<15Q', f, offset + 8 + 32)
    access = ''
    if code == 0xC0000005 and parameters >= 2:
        access = f'; {"write" if info[0] == 1 else "execute" if info[0] == 8 else "read"} of 0x{info[1]:X}'
    print(f'{os.path.basename(path)}: exception {code:08X} on thread {thread} at 0x{address:016X} '
          f'({where(address) or "not in a module"}){access}')
    offset, _ = streams[3]  # ThreadListStream
    for i in range(struct.unpack_from('<I', f, offset)[0]):
        tid, _s, _c, _p, _teb, _start, size, rva, _cs, _cr = struct.unpack_from('<IIIIQQIIII', f, offset + 4 + i * 48)
        if tid != thread:
            continue
        runs = []
        for j in range(0, size - 8, 8):
            hit = where(struct.unpack_from('<Q', f, rva + j)[0])
            if not hit:
                continue
            name = hit.split('+')[0]
            if runs and runs[-1][0] == name:
                runs[-1][2] += 1
            else:
                runs.append([name, hit, 1])
        print('  stack, top first: ' + ' | '.join(f'{n} x{c} (first {h})' for n, h, c in runs[:12]))


def main():
    paths = sys.argv[1:]
    if not paths:
        found = sorted(glob.glob(os.path.expandvars(r'%LOCALAPPDATA%/CrashDumps/ds2.exe.*.dmp')), key=os.path.getmtime)
        if not found:
            sys.exit('no ds2.exe dump in %LOCALAPPDATA%/CrashDumps')
        paths = found[-1:]
    for path in paths:
        summarize(path)


if __name__ == '__main__':
    main()
