#!/usr/bin/env python3
# Generate one `dump`-backend translation unit per registered example, from the
# (MAIN_FILE, MAIN_CLASS) pairs in cmake/avendish.examples.cmake and the dump
# prototype. Used by the backend_matrix CI workflow to compile every example
# through the SDK-free dump binding. Usage: gen_dump_tus.py <out-dir>
import os
import re
import sys

root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
out = sys.argv[1] if len(sys.argv) > 1 else "/tmp/dump"
os.makedirs(out, exist_ok=True)

proto = open(os.path.join(root, "include/avnd/binding/dump/prototype.cpp.in")).read()
reg = open(os.path.join(root, "cmake/avendish.examples.cmake")).read()
pairs = list(zip(re.findall(r"MAIN_FILE\s+(\S+)", reg),
                 re.findall(r"MAIN_CLASS\s+(\S+)", reg)))

names = set()
for f, c in pairs:
    n = re.sub(r"[^A-Za-z0-9]", "_", c)
    names.add(n)
    tu = proto.replace("@AVND_MAIN_FILE@", f).replace("@AVND_MAIN_CLASS@", c)
    open(os.path.join(out, n + ".cpp"), "w").write(tu)

print(f"generated {len(names)} dump TUs into {out}")
