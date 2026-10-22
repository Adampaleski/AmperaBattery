#!/usr/bin/env python3
"""python3 scripts/test_pinmap.py — docs/PINMAP.yaml vs the firmware. Fails on ANY mismatch:
pin numbers vs include/Config.h Pins::, Config.h line cites, CAN controller/pin/bitrate map vs FlexCanCompat.h + Config.h,
Serial4 hub UART pins vs HubStatus.cpp, same-pin reuse on one board without 'shares', every drawn wiring line numbered,
every schematic edge numbered, generated docs up to date."""
import sys, re, pathlib
R = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(R / "tools"))
from pinmap_lib import load, dia_index, wiring_pages, line_key
fails = []; npass = 0
def ok(cond, msg):
    global npass
    if cond: npass += 1
    else: fails.append(msg); print("FAIL", msg)

def firmware_pins():
    """Pins:: constants from Config.h, honouring #if/#ifndef with Config.h's own #define defaults."""
    lines = (R / "include/Config.h").read_text().splitlines()
    defs = {}
    for l in lines:
        m = re.match(r"\s*#define\s+(\w+)\s+(\d+)", l)
        if m: defs.setdefault(m.group(1), int(m.group(2)))
    vals, where, stack, inpins = {}, {}, [], False
    for n, l in enumerate(lines, 1):
        if re.match(r"\s*namespace\s+Pins\b", l): inpins = True; continue
        if not inpins: continue
        if re.match(r"\s*}\s*//.*Pins|^}\s*$", l): break
        m = re.match(r"\s*#if\s+(\w+)", l)
        if m: stack.append(bool(defs.get(m.group(1), 0))); continue
        if re.match(r"\s*#ifn?def", l): stack.append(True); continue
        if re.match(r"\s*#else", l): stack[-1] = not stack[-1]; continue
        if re.match(r"\s*#endif", l): stack.pop(); continue
        if not all(stack): continue
        for c, v in re.findall(r"constexpr\s+int\s+(\w+)\s*=\s*(-?\w+)\s*;", l):
            vals[c] = int(v) if re.match(r"-?\d+$", v) else vals[v]; where[c] = n
    return vals, where

d = load()
fw, line = firmware_pins()
ok(len(fw) > 40, f"parsed {len(fw)} Pins:: constants from Config.h")
seen = set()
for b in ("A", "B"):
    for p in d["pins"][b]:
        c = p.get("const")
        if not c: continue
        seen.add(c)
        ok(c in fw, f"{b}: {c} not in Config.h Pins::")
        if c in fw:
            ok(fw[c] == p["pin"], f"{b}: {c} = pin {p['pin']} in PINMAP.yaml but {fw[c]} in Config.h:{line[c]}")
            m = re.search(r"Config\.h:(\d+)", p.get("src", ""))
            ok(m and int(m.group(1)) == line[c], f"{b}: {c} cites {p.get('src')} but is defined at Config.h:{line[c]}")
for c in fw:
    ok(c in seen, f"Config.h Pins::{c} (pin {fw[c]}) missing from PINMAP.yaml")
# same pin twice on one board must be declared with 'shares'
for b in ("A", "B"):
    by = {}
    for p in d["pins"][b]:
        if isinstance(p["pin"], int) and p["pin"] >= 0: by.setdefault(p["pin"], []).append(p)
    for pin, ps in by.items():
        if len(ps) > 1:
            ok(all(q.get("shares") for q in ps), f"{b} pin {pin} used by {[q.get('const') or q['use'] for q in ps]} without 'shares' (pin conflict)")
# CAN map
fc = (R / "include/FlexCanCompat.h").read_text()
v2 = fc[fc.index("#if CAN_LAYOUT_V2"):]
cfg = (R / "include/Config.h").read_text()
bic = int(re.search(r"kCanBitrate\s*=\s*(\d+)", cfg).group(1)); veh = int(re.search(r"kChargerCanBitrate\s*=\s*(\d+)", cfg).group(1))
want = {"CAN1": ("BICM1", 22, 23, bic), "CAN2": ("BICM2", 1, 0, bic), "CAN3": ("VEH", 31, 30, veh)}
for bus in d["buses"]:
    role, tx, rx, br = want[bus["ctrl"]]
    ok(re.search(rf"#define {role}_DEV\s+{bus['ctrl']}\b", v2) is not None, f"{bus['id']}: FlexCanCompat V2 does not map {role} to {bus['ctrl']}")
    ok((bus["tx"], bus["rx"], bus["bitrate"]) == (tx, rx, br), f"{bus['id']}: tx/rx/bitrate {bus['tx']}/{bus['rx']}/{bus['bitrate']} != firmware {tx}/{rx}/{br}")
    for b in bus["board"].split("+"):
        pins = {p["pin"] for p in d["pins"][b]}
        ok(tx in pins and rx in pins, f"{bus['id']}: pins {tx}/{rx} not listed for Teensy {b}")
# Serial4 hub UART (Teensy 4.1 Serial4 = RX16/TX17) and ESP32 GPIO16
hs = (R / "src/HubStatus.cpp").read_text()
ok("RX pin 16, TX pin 17" in hs and "Serial4.begin(115200)" in hs, "HubStatus.cpp no longer Serial4 RX16/TX17 @115200")
a17 = [p for p in d["pins"]["A"] if p["pin"] == 17]; e16 = [p for p in d["pins"]["E"] if p["pin"] == "GPIO16"]
ok(a17 and e16 and a17[0]["wire"] == e16[0]["wire"] == "S30", "Teensy A 17 -> ESP32 GPIO16 must be one wire (S30)")
ok(all(p["wire"] is None for p in d["pins"]["E"] if p["pin"] == "GPIO17"), "ESP32 GPIO17 must be unconnected (GATEWAY.md)")
# wires
ids = [w["id"] for w in d["wires"]]
ok(len(ids) == len(set(ids)), "duplicate wire ids")
need = ["id", "step", "kind", "from", "to", "signal", "voltage", "color", "awg", "fuse", "label", "tier", "status", "src", "check"]
for w in d["wires"]:
    ok(all(k in w and w[k] not in (None, "") for k in need), f"wire {w.get('id')} missing a field")
    ok(w["status"] in ("SOURCED", "ASSUMED", "PROPOSED", "UNVERIFIED", "UNDEFINED"), f"wire {w['id']} bad status")
    ok(w["tier"] in ("CRITICAL", "LATER"), f"wire {w['id']} bad tier")
    ok(w["label"].startswith(w["id"].split("-")[0]), f"wire {w['id']} label must start with its number")
for b in ("A", "B", "E"):
    for p in d["pins"][b]:
        ok(p.get("wire") in (None, *ids), f"{b} pin {p['pin']} references unknown wire {p.get('wire')}")
ok(not any("msd" in str(w).lower() and "no msd" not in str(w).lower() for w in d["wires"]), "a wire still references the Volt MSD")
# every drawn line has a wire number
idx = dia_index(d); pages = wiring_pages()
keys = [line_key(p["id"], w) for p in pages for w in p["wires"]]
for k in keys: ok(k in idx, f"tools/wiring_data.js line {k} has no wire number in PINMAP.yaml")
for k in idx: ok(k in keys, f"PINMAP.yaml dia {k} matches no drawn line (stale)")
# schematic/gen.py edges numbered
gp = R.parent / "schematic/gen.py"
if gp.exists():
    src = gp.read_text(); body = src.split("\nE = [", 1)[1].split("]\n", 1)[0]
    edges = re.findall(r'\("(\w+)",\s*"(\w+)"\)', body)
    ok(len(edges) > 50, f"parsed {len(edges)} schematic/gen.py edges")
    sk = {s: w["id"] for w in d["wires"] for s in (w.get("schem") or [])}
    for a, b in edges: ok(f"{a}|{b}" in sk, f"schematic/gen.py edge {a}->{b} has no wire number in PINMAP.yaml")
    ok('"msd"' not in src and "MSD ~" not in src, "schematic/gen.py still draws the Volt MSD")
# generated docs fresh
sys.path.insert(0, str(R / "tools")); import build_pinmap
ok((R / "docs/PINMAP.md").read_text() == build_pinmap.pinmap_md(d), "docs/PINMAP.md stale: run tools/build_pinmap.py")
ok((R / "docs/CONNECTIONS.md").read_text() == build_pinmap.connections_md(d), "docs/CONNECTIONS.md stale: run tools/build_pinmap.py")
print(("PASS" if not fails else "FAIL"), f"pinmap: {npass} checks passed, {len(fails)} failed")
sys.exit(1 if fails else 0)
