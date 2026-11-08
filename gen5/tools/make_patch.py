#!/usr/bin/env python3
"""Writes gen5/patches/<path>.patch: pokebw2's file with each OLD text replaced by NEW, as a unified diff.

    make_patch.py POKEBW2 PATH OLD NEW [OLD NEW...]

For writing patches only; the build applies them with apply_patch.py."""
import pathlib
import subprocess
import sys
import tempfile

pokebw2, path, *pairs = sys.argv[1:]
text = (pathlib.Path(pokebw2) / path).read_text()
for old, new in zip(pairs[::2], pairs[1::2]):
    if text.count(old) != 1:
        sys.exit(f"{path}: {old!r} found {text.count(old)} times")
    text = text.replace(old, new)
with tempfile.NamedTemporaryFile("w", suffix=".c", delete=False) as out:
    out.write(text)
diff = subprocess.run(["diff", "-u", "--label", f"a/{path}", "--label", f"b/{path}", str(pathlib.Path(pokebw2) / path),
                       out.name], capture_output=True, text=True).stdout
patch = pathlib.Path(__file__).resolve().parent.parent / "patches" / f"{path}.patch"
patch.parent.mkdir(parents=True, exist_ok=True)
patch.write_text(diff)
print(patch)
