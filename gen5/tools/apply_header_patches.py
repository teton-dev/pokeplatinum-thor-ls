#!/usr/bin/env python3
"""Writes patched copies of pokebw2's headers (gen5/patches/include/...) into OUTPUT_DIR, which the build puts on the
quote path ahead of pokebw2's include/, and touches STAMP. A copy is only rewritten when it changes.

    apply_header_patches.py POKEBW2 PATCHES_DIR OUTPUT_DIR STAMP"""
import pathlib
import subprocess
import sys
import tempfile

pokebw2, patches, output, stamp = (pathlib.Path(arg) for arg in sys.argv[1:])
for patch in sorted((patches / "include").rglob("*.h.patch")):
    header = patch.relative_to(patches / "include").with_suffix("")
    with tempfile.TemporaryDirectory() as work:
        patched = pathlib.Path(work) / header.name
        with open(patch) as diff:
            subprocess.run(["patch", "--silent", "--force", "--output", str(patched), str(pokebw2 / "include" / header)],
                           stdin=diff, check=True)
        text = patched.read_text()
    target = output / header
    if not target.exists() or target.read_text() != text:
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text)
stamp.write_text("")
