#!/usr/bin/env python3
"""Writes a patched copy of one of pokebw2's files: apply_patch.py SOURCE PATCH OUTPUT"""
import subprocess
import sys

source, patch, output = sys.argv[1:]
with open(patch) as diff:
    subprocess.run(["patch", "--silent", "--force", "--output", output, source], stdin=diff, check=True)
