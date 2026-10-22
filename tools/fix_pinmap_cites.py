#!/usr/bin/env python3
"""Re-point every 'Config.h:<line>' cite in docs/PINMAP.yaml at the line where that Pins:: const is defined now
(run after editing include/Config.h, then tools/build_pinmap.py). Values are NOT touched; test_pinmap.py checks them."""
import re, pathlib
R = pathlib.Path(__file__).resolve().parent.parent
src = (R / "scripts/test_pinmap.py").read_text()
ns = {"re": re, "R": R}; exec(src[src.index("def firmware_pins"):src.index("d = load()")], ns)
_, where = ns["firmware_pins"]()
p = R / "docs/PINMAP.yaml"; L = p.read_text().splitlines(); n = 0
for i, l in enumerate(L):
    m = re.search(r"const: (\w+),", l)
    if m and m.group(1) in where and re.search(r"Config\.h:\d+", l):
        new = re.sub(r"Config\.h:\d+", "Config.h:%d" % where[m.group(1)], l, count=1); n += new != l; L[i] = new
p.write_text("\n".join(L) + "\n"); print(f"fixed {n} cites")
