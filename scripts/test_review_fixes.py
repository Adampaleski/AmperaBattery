#!/usr/bin/env python3
"""Regression checks for chief firmware review cd7027f fixes (docs: /workspace/teryx/review/firmware_review_chief.md)."""
import re, sys, pathlib
R = pathlib.Path(__file__).resolve().parent.parent
fails = 0
def ok(c, m):
    global fails
    print(("PASS " if c else "FAIL ") + m); fails += (not c)
ini = (R / "platformio.ini").read_text()
envs = re.findall(r"^\[env:(\w+)\]", ini, re.M)
ok("teensy41_bms" not in envs, "#1 teensy41_bms env deleted")
ok(not re.search(r"default_envs\s*=.*teensy41_bms", ini), "#1 teensy41_bms not a default env")
for e in envs:
    blk = ini.split(f"[env:{e}]")[1].split("\n[")[0]
    ok("ENABLE_BMS_CONTACTORS=1" not in blk or "DUAL_STRING=1" in blk, f"#1 {e}: no single-string contactor build")
print(f"review fixes: {fails} failed")
sys.exit(1 if fails else 0)
