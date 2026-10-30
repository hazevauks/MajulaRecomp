#!/usr/bin/env python3
"""Installs Xbox 360 marketplace packages (content type 2: DLC, compatibility packs) into a content root, the way the
SDK's ContentManager::InstallContent (sdk/src/system/xam/content_manager.cpp) does at run time:

    <content root>/0000000000000000/<title>/00000002/<package file name>/...          the package's files
    <content root>/0000000000000000/<title>/Headers/00000002/<package file name>.header  its XCONTENT_AGGREGATE_DATA

The game lists marketplace content of the signed-in user and of xuid 0 (common), opens it with XamContentCreate and
reads it through its own root. Scholar of the First Sin stops before its title menu without its disc 2's
"Compatibility Pack" (2026-10-30, docs/dlc.md); on the console the content root is the `ds2` folder next to the NRO.

Usage: python tools/install_dlc.py PACKAGE... [--content-root assets/dlc_content]
"""
import argparse
import os
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from stfs_extract import Stfs  # noqa: E402

MARKETPLACE = 2
HDD = 1  # DummyDeviceId::HDD


def aggregate_data(display_name, file_name, title_id):
    """XCONTENT_AGGREGATE_DATA (0x148 bytes, big-endian fields): XCONTENT_DATA (device, type, 128 UTF-16 display
    characters, 42 file name characters, 2 of padding), then 4 of alignment, xuid, title id and 4 of alignment."""
    name = display_name.encode('utf-16-be')[:256].ljust(256, b'\0')
    raw = file_name.encode('ascii')
    if len(raw) > 42:
        sys.exit(f'{file_name}: file name longer than 42 characters')
    data = struct.pack('>II', HDD, MARKETPLACE) + name + raw.ljust(42, b'\0') + b'\0\0'
    data += b'\0' * 4 + struct.pack('>QI', 0, title_id) + b'\0' * 4
    assert len(data) == 0x148
    return data


def license_mask(path):
    # The header's licence entries (0x22C, 16 of id/bits/flags); InstallContent ORs the bits of those with flags.
    hdr = open(path, 'rb').read(0x32C)
    mask = 0
    for i in range(16):
        _, bits, flags = struct.unpack_from('>QII', hdr, 0x22C + 16 * i)
        if flags:
            mask |= bits
    return mask


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('packages', nargs='+')
    ap.add_argument('--content-root', default=os.path.join(ROOT, 'assets', 'dlc_content'))
    a = ap.parse_args()
    for path in a.packages:
        p = Stfs(path)
        if p.content_type != MARKETPLACE:
            sys.exit(f'{path}: content type {p.content_type:08X}, not marketplace content')
        name = os.path.basename(path)
        title = f'{p.title_id:08X}'
        dest = os.path.join(a.content_root, '0' * 16, title, f'{MARKETPLACE:08X}', name)
        r = subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'stfs_extract.py'), path, dest])
        if r.returncode:
            sys.exit(f'{path}: extraction failed')
        header = os.path.join(a.content_root, '0' * 16, title, 'Headers', f'{MARKETPLACE:08X}', name + '.header')
        os.makedirs(os.path.dirname(header), exist_ok=True)
        mask = license_mask(path)
        with open(header, 'wb') as f:
            f.write(aggregate_data(p.display_name, name, p.title_id))
            if mask:
                f.write(struct.pack('<I', mask))  # written by the host in its own byte order (little-endian)
        print(f'{p.display_name!r}: installed for title {title} in {dest} (licence mask {mask:08X})')


if __name__ == '__main__':
    main()
