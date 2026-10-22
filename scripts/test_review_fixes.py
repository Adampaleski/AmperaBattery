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

# ---- #2: serial U/I fake inputs compiled out of non-bench builds ----
import subprocess, tempfile, os
cfg = (R / "include/Config.h").read_text()
ok("#define BENCH_SIM_INPUTS 0" in cfg and "requires TELEMETRY_ONLY=1" in cfg, "#2 Config.h: BENCH_SIM_INPUTS defaults 0 and #errors without TELEMETRY_ONLY=1")
for e in envs:
    blk = ini.split(f"[env:{e}]")[1].split("\n[")[0]
    if "BENCH_SIM_INPUTS=1" in blk:
        ok("bench" in e and "TELEMETRY_ONLY=1" in blk, f"#2 {e}: BENCH_SIM_INPUTS only on telemetry-only bench envs")
ok(not any("BENCH_SIM_INPUTS=1" in ini.split(f"[env:{e}]")[1].split("\n[")[0] for e in envs if e.startswith("teensy41_dual")), "#2 dual_a/dual_b: no sim inputs")
CXX = os.environ.get("CXX_PP", "/workspace/tmp/zigvenv/bin/python -m ziglang c++").split()
stub = tempfile.mkdtemp(); open(os.path.join(stub, "EEPROM.h"), "w").write("#pragma once\n")
def pp(f, flags):
    r = subprocess.run(CXX + ["-E", "-P", "-std=c++17", *flags, "-I" + stub, "-Iscripts/hoststub", "-Iinclude", "-Isrc", f], cwd=R, capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr
for name, flags, want in [("vehicle (dual_a flags)", ["-DTELEMETRY_ONLY=1", "-DDUAL_STRING=1", "-DTEENSY_ROLE=0"], False),
                          ("bench", ["-DTELEMETRY_ONLY=1", "-DBENCH_SIM_INPUTS=1"], True)]:
    rc, out = pp("src/Settings.cpp", flags)
    ok(rc == 0, f"#2 preprocess Settings.cpp {name}")
    ok(("g_simBus = (num" in out) == want, f"#2 {name}: U sim handler {'present' if want else 'compiled out'}")
    ok(("setSimAmps(num" in out) == want, f"#2 {name}: I sim handler {'present' if want else 'compiled out'}")
    rc, out = pp("src/CurrentSense.cpp", flags)
    ok(rc == 0 and ("g_simOn = on" in out) == want, f"#2 {name}: CurrentSense sim amps {'present' if want else 'compiled out'}")
rc, out = pp("src/Settings.cpp", ["-DTELEMETRY_ONLY=0", "-DBENCH_SIM_INPUTS=1"])
ok(rc != 0 and "requires TELEMETRY_ONLY=1" in out, "#2 BENCH_SIM_INPUTS with live outputs refuses to build")
print(f"review fixes: {fails} failed")
sys.exit(1 if fails else 0)
