#!/usr/bin/env python3
"""Writes gen5/patches/<path>.patch: pokebw2's file with each OLD text replaced by NEW, as a unified diff.

    make_patch.py [--all] POKEBW2 PATH OLD NEW [OLD NEW...]

Each OLD must be found once, or with --all at least once (every one is replaced). An existing patch of PATH is
replaced: write all of a file's changes in one call. For writing patches only; the build applies them (apply_patch.py,
apply_header_patches.py)."""
import pathlib
import subprocess
import sys
import tempfile

args = sys.argv[1:]
replace_all = args[0] == "--all"
if replace_all:
    args = args[1:]
pokebw2, path, *pairs = args
text = (pathlib.Path(pokebw2) / path).read_text()
for old, new in zip(pairs[::2], pairs[1::2]):
    if text.count(old) == 0 or (text.count(old) > 1 and not replace_all):
        sys.exit(f"{path}: {old!r} found {text.count(old)} times")
    text = text.replace(old, new)
with tempfile.NamedTemporaryFile("w", suffix=pathlib.Path(path).suffix, delete=False) as out:
    out.write(text)
diff = subprocess.run(["diff", "-u", "--label", f"a/{path}", "--label", f"b/{path}", str(pathlib.Path(pokebw2) / path),
                       out.name], capture_output=True, text=True).stdout
patch = pathlib.Path(__file__).resolve().parent.parent / "patches" / f"{path}.patch"
patch.parent.mkdir(parents=True, exist_ok=True)
patch.write_text(diff)
print(patch)
