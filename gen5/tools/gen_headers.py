#!/usr/bin/env python3
"""Generates pokebw2's constant and text ID headers, as its own build does, in one process.

    gen_headers.py POKEBW2 OUTPUT_DIR STAMP

pokebw2's build generates constants/*.h from generated/*.txt (tools/data/gen_constants.py), text/<dir>/*.h from
res/text/<dir>/*.txt (tools/text/text_ids.py) and constants/pms_words.h (tools/text/pms_words.py). This runs the same
functions, writing a header only when it changes. Needs Python 3.11, as pokebw2's tools do."""
import pathlib
import sys

pokebw2, output, stamp = (pathlib.Path(arg).resolve() for arg in sys.argv[1:])
sys.path.insert(0, str(pokebw2))
from tools.data import gen_constants  # noqa: E402
from tools.text import pms_words, text_ids  # noqa: E402


def write(path: pathlib.Path, text: str):
    if path.exists() and path.read_text() == text:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)


for source in sorted((pokebw2 / "generated").glob("*.txt")):
    write(output / "constants" / source.with_suffix(".h").name, gen_constants.generate(source))
for directory in ("system", "script"):
    for source in sorted((pokebw2 / "res" / "text" / directory).glob("*.txt")):
        write(output / "text" / directory / source.with_suffix(".h").name, text_ids.header(source))
write(output / "constants" / "pms_words.h", pms_words.header())
stamp.write_text("")
