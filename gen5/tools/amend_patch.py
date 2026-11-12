#!/usr/bin/env python3
"""Adds changes to a file's patch (gen5/patches/<path>.patch), keeping the ones it has: applies the patch to pokebw2's
file, replaces each OLD text with NEW in the result (each found once), and writes the patch of the whole difference.
For a file with no patch yet it is make_patch.py.

    amend_patch.py POKEBW2 PATH OLD NEW [OLD NEW...]"""
import pathlib
import subprocess
import sys
import tempfile

pokebw2, path, *pairs = sys.argv[1:]
original = pathlib.Path(pokebw2) / path
patch = pathlib.Path(__file__).resolve().parent.parent / "patches" / f"{path}.patch"
with tempfile.TemporaryDirectory() as work:
    patched = pathlib.Path(work) / "patched"
    if patch.exists():
        with open(patch) as diff:
            subprocess.run(["patch", "--silent", "--force", "--output", str(patched), str(original)], stdin=diff, check=True)
    else:
        patched.write_text(original.read_text())
    text = patched.read_text()
    for old, new in zip(pairs[::2], pairs[1::2]):
        if text.count(old) != 1:
            sys.exit(f"{path}: {old!r} found {text.count(old)} times")
        text = text.replace(old, new)
    patched.write_text(text)
    diff = subprocess.run(["diff", "-u", "--label", f"a/{path}", "--label", f"b/{path}", str(original), str(patched)],
                          capture_output=True, text=True).stdout
patch.parent.mkdir(parents=True, exist_ok=True)
patch.write_text(diff)
