"""Extracts an NDS ROM into a data folder exactly as libntr's first start does (fireflower/nds-extract.cpp: header,
binaries, overlay tables and overlays, banner, FNT/FAT, RSA signature, the file system), without opening the game's
window: for setting up desktop data folders from a script (docs/thor/dev/desktop-windows.md).
    python nds_extract.py ROM.nds DATA_FOLDER
Files already there are kept."""
import os
import struct
import sys


def write(path, data):
    if not os.path.exists(path):
        with open(path, 'wb') as f:
            f.write(data)


def main(rom_path, out):
    rom = open(rom_path, 'rb').read()
    u = lambda i: struct.unpack_from('<I', rom, i * 4)[0]
    arm9_off, arm9_size, arm7_off, arm7_size = u(8), u(11), u(12), u(15)
    fnt_off, fnt_size, fat_off, fat_size = u(16), u(17), u(18), u(19)
    ovt9_off, ovt9_size, ovt7_off, ovt7_size = u(20), u(21), u(22), u(23)
    icon_off, rsa_off = u(26), u(32)
    for off, size in ((arm9_off, arm9_size), (arm7_off, arm7_size), (ovt9_off, ovt9_size), (ovt7_off, ovt7_size),
                      (fnt_off, fnt_size), (fat_off, fat_size)):
        if off + size > len(rom):
            sys.exit('the ROM is truncated')
    if icon_off and icon_off + 0x23C0 > len(rom):
        icon_off = 0
    for d in (out, os.path.join(out, 'overlay9'), os.path.join(out, 'overlay7')):
        os.makedirs(d, exist_ok=True)
    write(os.path.join(out, 'header.bin'), rom[:0x4000])
    write(os.path.join(out, 'arm9.bin'), rom[arm9_off:arm9_off + arm9_size])
    write(os.path.join(out, 'arm7.bin'), rom[arm7_off:arm7_off + arm7_size])
    write(os.path.join(out, 'arm9ovt.bin'), rom[ovt9_off:ovt9_off + ovt9_size])
    write(os.path.join(out, 'arm7ovt.bin'), rom[ovt7_off:ovt7_off + ovt7_size])
    icon_size = 0
    if icon_off:
        icon_size = {1: 0x840, 2: 0x940, 3: 0xA40, 0x103: 0x23C0}.get(struct.unpack_from('<H', rom, icon_off)[0], 0x840)
    write(os.path.join(out, 'banner.bin'), rom[icon_off:icon_off + icon_size])
    write(os.path.join(out, 'fnt.bin'), rom[fnt_off:fnt_off + fnt_size])
    write(os.path.join(out, 'fat.bin'), rom[fat_off:fat_off + fat_size])
    if rsa_off + 136 <= len(rom):
        write(os.path.join(out, 'rsasig.bin'), rom[rsa_off:rsa_off + 136])

    def fat(fid):
        start, end = struct.unpack_from('<II', rom, fat_off + fid * 8)
        return start, end - start

    for table_off, table_size, folder, prefix in ((ovt9_off, ovt9_size, 'overlay9', 'overlay9_'),
                                                   (ovt7_off, ovt7_size, 'overlay7', 'overlay7_')):
        for i in range(table_size // 32):
            ov_id = struct.unpack_from('<I', rom, table_off + i * 32)[0]
            fid = struct.unpack_from('<H', rom, table_off + i * 32 + 24)[0]
            start, size = fat(fid)
            write(os.path.join(out, folder, f'{prefix}{ov_id}.bin'), rom[start:start + size])

    fnt = rom[fnt_off:fnt_off + fnt_size]
    count = 0

    def dump(dir_id, path):
        nonlocal count
        sub_off, first_id = struct.unpack_from('<IH', fnt, (dir_id & 0xFFF) * 8)
        pos, fid = sub_off, first_id
        while pos < len(fnt):
            n = fnt[pos]
            pos += 1
            if n == 0 or n == 0x80:
                break
            name = fnt[pos:pos + (n & 0x7F)].decode('latin-1')
            pos += n & 0x7F
            if n & 0x80:
                sub_id = struct.unpack_from('<H', fnt, pos)[0]
                pos += 2
                os.makedirs(os.path.join(path, name), exist_ok=True)
                dump(sub_id, os.path.join(path, name))
            else:
                start, size = fat(fid)
                if start + size <= len(rom):
                    write(os.path.join(path, name), rom[start:start + size])
                    count += 1
                fid += 1

    dump(0xF000, out)
    print(f'extracted {rom_path} into {out}: {count} files')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
