#!/usr/bin/env python3
"""Renames every symbol a static library defines to bw2_<name>, with its references in the library, so that BW2's
code can sit in the app beside Platinum's (PlayTime_Init, SaveData_Load, SPL... are in both). The SDK and C library
calls it makes keep their names. The port's interface to Platinum, the functions named Gen5_..., keeps its own.

    prefix_symbols.py NM OBJCOPY AR INPUT.a OUTPUT.a

Meson's static libraries are thin archives (lists of objects), which objcopy can't rewrite: each object is renamed
on its own and the output is a normal archive."""
import os
import pathlib
import subprocess
import sys
import tempfile

nm, objcopy, ar, source, output = sys.argv[1:]
listing = subprocess.run([nm, "-g", "--defined-only", source], capture_output=True, text=True, check=True).stdout
symbols = sorted({line.split()[2] for line in listing.splitlines() if len(line.split()) == 3})
renames = output + ".syms"
with open(renames, "w") as out:
    for symbol in symbols:
        if not symbol.startswith("Gen5_"):
            out.write(f"{symbol} bw2_{symbol}\n")
members = subprocess.run([ar, "t", source], capture_output=True, text=True, check=True).stdout.split()
with tempfile.TemporaryDirectory(dir=os.path.dirname(os.path.abspath(output))) as work:
    objects = []
    for index, member in enumerate(members):
        # ar lists a thin archive's members as paths from the current directory
        path = pathlib.Path(member)
        renamed = os.path.join(work, f"{index:04}_{path.name}")
        subprocess.run([objcopy, f"--redefine-syms={renames}", str(path), renamed], check=True)
        objects.append(renamed)
    if os.path.exists(output):
        os.remove(output)
    subprocess.run([ar, "rcs", output, *objects], check=True)
os.remove(renames)
