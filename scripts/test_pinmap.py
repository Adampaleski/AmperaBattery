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
# MCP23017 expander bits vs Config.h ExpBits:: (Build 18 LATER reservations; names only)
cfgt = (R / "include/Config.h").read_text()
mexp = re.search(r"namespace ExpBits \{(.*?)\}\s*//\s*namespace ExpBits", cfgt, re.S)
ok(mexp is not None, "Config.h namespace ExpBits missing")
expfw = {c: int(v) for c, v in re.findall(r"constexpr\s+int\s+(\w+)\s*=\s*(\d+)", mexp.group(1))} if mexp else {}
used = {}
for e in d.get("expander", []):
    bs = e["bits"]; rng = range(int(bs.split("-")[0]), int(bs.split("-")[1]) + 1) if isinstance(bs, str) else [bs]
    for b in rng:
        ok(0 <= b <= 15, f"expander bit {b} out of range"); ok(b not in used, f"expander bit {b} listed twice ({used.get(b)} / {e['use'][:30]})"); used[b] = e["use"][:30]
    if e.get("const"):
        ok(expfw.get(e["const"]) == bs, f"expander {e['const']} = bit {bs} in PINMAP.yaml but {expfw.get(e['const'])} in Config.h ExpBits")
        ok(e["const"] not in fw, f"{e['const']} must not also be a Teensy Pins:: constant (no free A pin)")
for c in expfw: ok(any(e.get("const") == c for e in d.get("expander", [])), f"Config.h ExpBits::{c} missing from PINMAP.yaml expander")
ok(sorted(used) == list(range(16)), "expander table must cover bits 0-15 exactly once")
ok(not any(e.get("const") == "PACK_HEAT" for e in d.get("expander", [])) and "PACK_HEAT" not in expfw, "PACK_HEAT must not be reserved in Build 18 (chief review)")
for c in ("MOTOR_FAN", "BUZZER", "PACK_HEAT"):
    for b in ("A", "B"): ok(all(p.get("const") != c for p in d["pins"][b]), f"{c} must not take a Teensy {b} pin")
# ESP32 spare GPIO: read-only role, never on flash (6-11), input-only (34-39), strapping (0/2/5/12/15) or the UART pins (1/3/16/17)
for p in d["pins"]["E"]:
    if "spare" in p["use"]:
        g = int(str(p["pin"]).replace("GPIO", ""))
        ok(p["wire"] is None and "read-only" in p["use"], f"ESP32 GPIO{g} spare must be unwired, read-only role")
        ok(g not in (0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 15, 16, 17) and g < 34, f"ESP32 GPIO{g} is not an output-safe spare")
# A:24 = reserved spare, precharge via Kelly CAN B+ (room DECIDED 2026-10-23); never a plain divider (Sparky)
a24 = [p for p in d["pins"]["A"] if p["pin"] == 24][0]; s08 = [w for w in d["wires"] if w["id"] == "S08"][0]
ok("RESERVED" in a24["use"] and "Kelly CAN B+" in a24["use"] and "never a plain" in a24["use"].lower() and a24["wire"] is None and a24["tier"] == "LATER", "A:24 must be a reserved spare (Kelly CAN B+ precharge), never a plain divider")
ok("NOT FITTED" in s08["from"] and s08["tier"] == "LATER" and not s08["dia"], "S08 must be NOT FITTED / LATER with no diagram wire")
bl = (R / "BACKLOG.md").read_text()
ok("[CRITICAL s4] Precharge + weld check via Kelly CAN B+" in bl and "[CRITICAL s4] Kelly B+ bench gate" in bl and "Isolated HV bus sense" not in bl, "BACKLOG: Kelly B+ precharge + bench gate CRITICAL, isolated HV sense entry gone")
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
# tools/schematic/gen.py edges numbered
gp = R / "tools/schematic/gen.py"   # moved into the repo (Build 18)
ok(gp.exists(), "tools/schematic/gen.py missing (moved into the repo in Build 18)")
if gp.exists():
    src = gp.read_text(); body = src.split("\nE = [", 1)[1].split("]\n", 1)[0]
    edges = re.findall(r'\("(\w+)",\s*"(\w+)"\)', body)
    ok(len(edges) > 50, f"parsed {len(edges)} tools/schematic/gen.py edges")
    sk = {s: w["id"] for w in d["wires"] for s in (w.get("schem") or [])}
    for a, b in edges: ok(f"{a}|{b}" in sk, f"tools/schematic/gen.py edge {a}->{b} has no wire number in PINMAP.yaml")
    ok('"msd"' not in src and "MSD ~" not in src, "tools/schematic/gen.py still draws the Volt MSD")
# generated docs fresh
sys.path.insert(0, str(R / "tools")); import build_pinmap
ok((R / "docs/PINMAP.md").read_text() == build_pinmap.pinmap_md(d), "docs/PINMAP.md stale: run tools/build_pinmap.py")
ok((R / "docs/CONNECTIONS.md").read_text() == build_pinmap.connections_md(d), "docs/CONNECTIONS.md stale: run tools/build_pinmap.py")
print(("PASS" if not fails else "FAIL"), f"pinmap: {npass} checks passed, {len(fails)} failed")
sys.exit(1 if fails else 0)
