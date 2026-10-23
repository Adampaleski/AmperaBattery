#!/usr/bin/env python3
"""gen_teryx_layout.py — writes the STARTING docs/layout/teryx.layout.json (Build 23).

Run once to (re)seed the layout; after that the committed JSON is the source and is edited in
tools/layout.html (drag, Export). Re-running overwrites hand edits, so it refuses unless --force.
Inputs: /workspace/teryx/layout_input_mechanic.md (Mechanic), layout_constraints.md (chief, Sparky),
published Teryx 750 specs (atvriders.com / utvguide.net / owner's manual), docs/PINMAP.yaml wire ids.
"""
import json, pathlib, sys
R = pathlib.Path(__file__).resolve().parents[2]
OUT = R / "docs/layout/teryx.layout.json"
PUB = "published 2008 Teryx 750 spec (atvriders.com, utvguide.net, owner's manual) — not measured on this vehicle"
EST = "estimate from photos/proportions — not published, not measured"
MECH = "layout_input_mechanic.md"; CONS = "layout_constraints.md"

def d(v, label, src, note=""): return {"v": v, "label": label, "src": src, **({"note": note} if note else {})}
dims = {
 "overall_length": d(115.7, "ASSUMED", PUB),
 "overall_width": d(58.3, "ASSUMED", PUB),
 "wheelbase": d(75.8, "ASSUMED", PUB, "best scale reference: hub centre to hub centre, one side"),
 "front_overhang": d(18.0, "ASSUMED", EST, "front bumper to front hub centre"),
 "tire_dia": d(24.0, "SOURCED", "Adam measured the tires at 24 in (layout_input_mechanic.md)"),
 "tire_w_front": d(8.0, "ASSUMED", "stock 26x8-12 width; the fitted 24 in tires' width is not measured"),
 "tire_w_rear": d(10.0, "ASSUMED", "stock 26x10-12 width; the fitted 24 in tires' width is not measured"),
 "track_front": d(50.3, "ASSUMED", EST, "centre-to-centre; overall width minus one tire width"),
 "track_rear": d(48.3, "ASSUMED", EST, "centre-to-centre; overall width minus one tire width"),
 "dash_x": d(40.0, "ASSUMED", EST, "front bumper to dash face"),
 "cab_x1": d(80.0, "ASSUMED", EST, "rear of the cab zone = seat backs + 2 in"),
 "cab_margin": d(6.0, "ASSUMED", EST, "cab zone inset from the body sides"),
 "seat_x0": d(60.0, "ASSUMED", EST, "front edge of the seat bases"),
 "seat_depth": d(18.0, "ASSUMED", EST),
 "seat_width": d(18.0, "ASSUMED", EST),
 "console_width": d(6.3, "ASSUMED", EST, "gap between the two seats"),
 "bed_length": d(32.7, "ASSUMED", PUB, "sources disagree on L vs W orientation (44.1 x 32.7 vs 44.2 x 32.7 'LxW'); drawn 32.7 fore-aft — Adam to measure"),
 "bed_width": d(44.1, "ASSUMED", PUB, "see bed_length note"),
 "bed_rear_gap": d(1.0, "ASSUMED", EST, "bed tailgate to rear bumper"),
 "rail_spacing": d(18.3, "ASSUMED", EST, "frame-rail centre spacing"),
 "rail_x0": d(6.0, "ASSUMED", EST), "rail_x1": d(112.0, "ASSUMED", EST),
}
def z(id, name, kind, x0, y0, x1, y1, src, why, exempt=None):
    return {"id": id, "name": name, "kind": kind, "x": x0, "y": y0, "w": round(x1 - x0, 3), "h": round(y1 - y0, 3), "label": "ASSUMED", "src": src, "why": why, **({"exempt": exempt} if exempt else {})}
zones = [
 z("susp_fp", "front suspension + steering throw (passenger)", "keepout", 4, 0, 32, 13, MECH + " Avoid: suspension travel, steering throw, wheel wells", "moving parts; no harness, no box"),
 z("susp_fd", "front suspension + steering throw (driver)", "keepout", 4, 45.3, 32, 58.3, MECH + " Avoid: suspension travel, steering throw, wheel wells", "moving parts; no harness, no box"),
 z("susp_rp", "rear suspension travel + wheel well (passenger)", "keepout", 81, 0, 107, 12, MECH + " Avoid: suspension travel, wheel wells; travel 7.3 in published", "moving parts; Sparky: no HV at pinch points"),
 z("susp_rd", "rear suspension travel + wheel well (driver)", "keepout", 81, 46.3, 107, 58.3, MECH + " Avoid: suspension travel, wheel wells", "moving parts; Sparky: no HV at pinch points"),
 z("chain", "chain line + sprockets (thrown-chain reach)", "keepout", 92, 27.5, 106, 33.5, MECH + " Avoid: chain line and sprocket area", "a thrown chain cuts cable", ["motor"]),
 z("exhaust", "old exhaust area (no heat now; mud + splash)", "keepout", 107, 47, 115.7, 58.3, MECH + " Avoid: old exhaust area", "splash/mud; position of the old muffler ASSUMED"),
 z("radiator", "stock radiator (kept, Kelly loop)", "area", 3, 14, 9, 44, MECH, "coolant loop: Kelly W3-GT2 heatsink → radiator; pump at the loop's low point"),
 z("enginebay", "old engine bay (under the bed)", "area", 80, 12, 110, 46.3, MECH + " strings low + centred here", "pack, HV box, Kelly, motor area"),
 z("fueltank", "old fuel-tank area (under the seats)", "area", 60, 8, 78, 26, MECH + " 'old fuel-tank area under the seats'", "Mechanic's 2nd pack area — but it is inside the cab"),
]
def b(id, part, name, x0, y0, x1, y1, v, src, why, terms, **kw):
    o = {"id": id, "part": part, "name": name, "x": x0, "y": y0, "w": round(x1 - x0, 3), "h": round(y1 - y0, 3), "v": v, "label": "PROPOSED", "size_label": "ASSUMED", "src": src, "why": why, "terms": terms}
    o.update(kw); return o
blocks = [
 b("bms_box", "bms_master", "BMS master box (Teensy + boards)", 44, 25, 52, 33, "LV", CONS + " / bms_master", "in the cab, sealed, rubber-mounted",
   {"esp": [0, .2], "estop": [0, .45], "key": [0, .75], "ped": [.3, 1], "lem": [1, .15], "can1": [1, .25], "can2": [1, .35], "cd": [1, .45], "kel": [1, .55], "vcan": [1, .65], "prox": [1, .75], "tbgnd": [1, .85], "pwr": [1, .95]},
   flags={"sealed": True, "rubber_mount": True}),
 b("esp32", "esp32", "ESP32 dash/telemetry", 40.5, 19, 43.5, 23, "LV", CONS, "high near the dash, under the plastic", {"in": [1, .5]}),
 b("estop", "estop_head", "E-stop (dash)", 40.5, 27.6, 43, 30.6, "LV", MECH + " / " + CONS, "on the dash, reachable from both seats", {"in": [1, .3], "blk3": [1, .5], "out": [1, .7]}),
 b("key", "key_switch", "Key switch", 40.5, 33, 42.5, 35, "LV", "PINMAP S42/S43", "stock key position (dash)", {"out": [1, .5]}),
 b("pedal", "pedal", "Throttle pedal", 41, 41, 45, 45, "LV", "PINMAP S14/S15", "driver footwell", {"out": [.5, 0]}),
 b("lvf", "lv_fuses", "LV fuse panel + 12 V bus (F-KEL, F-COIL, F-EBOX)", 73.5, 27, 78.5, 31.5, "LV", "PINMAP P02-P05", "console between the seats; LV side", {"in": [1, .9], "kel": [1, .2], "coil": [0, .3], "ebox": [0, .7]}),
 b("coil_drv", "coil_driver", "Coil-driver board", 77.5, 17, 80.5, 21.5, "LV", CONS + " (Sparky)", "just outside the HV box", {"gate": [0, .5], "coil": [1, .5], "gnd": [0, .9]}),
 b("hv_box", "busbar", "HV box (bus bar, clear tool-only cover, aluminium backplate)", 81, 13, 91, 27, "HV", MECH + " / " + CONS, "beside the pack; tool-only cover; bonded to chassis",
   {}, contains=["contactor_main", "precharge_relay", "fuse_a6t200", "lem", "precharge_res"], flags={"bonded": True, "tool_only_cover": True}),
 b("hv_fuses", "fuse_a6t200", "String fuses F-S1..S4 (A6T200) = service disconnect", 82, 13.6, 90, 16.6, "HV", "DECIDED 06:11 (" + CONS + ")", "no MSD; pull the 4 fuses with the insulated wrench",
   {"s1": [1, .25], "s2": [1, .75], "s3": [0, .25], "s4": [0, .75]}, **{"in": "hv_box", "marker": "reachable from above, tool-only cover"}),
 b("hv_lem", "lem", "LEM DHAB S/124 (B− path)", 82, 17.5, 85.5, 20.5, "HV", MECH, "inside the HV box on the B− path", {"sig": [0, .5]}, **{"in": "hv_box"}),
 b("hv_pre", "precharge_res", "Precharge resistor 50 Ω 100 W", 86.5, 17.5, 90, 20, "HV", MECH + " (aluminium backplate)", "Sparky: metal mount with airflow", {}, **{"in": "hv_box", "flags": {"metal_mount": True}}),
 b("hv_cont", "contactor_main", "EV200 B+/B− + AEV14012 precharge relay", 82, 21.5, 90, 26, "HV", MECH, "inside the HV box", {"coil": [0, .5], "out": [.6, 1], "dcdc": [.3125, 1], "chg": [.8125, 1]},
   **{"in": "hv_box", "contains": ["precharge_relay"]}),
 b("S1", "pack", "String 1 (36S, first drive)", 92, 12.5, 112, 19, "HV", MECH + " (strings low + centred, old engine bay / under the bed)", "beside the HV box", {"bus": [0, .5], "x2": [.1, 0]}),
 b("S2", "pack", "String 2 (36S, first drive)", 92, 19.5, 112, 26, "HV", MECH, "beside the HV box", {"bus": [0, .5], "x2": [.1, 1]}),
 b("S3", "pack", "String 3 (36S, 4p end state)", 61, 9, 77, 16, "HV", MECH + " ('old fuel-tank area under the seats')", "Mechanic's 2nd pack area — inside the cab zone", {"bus": [1, .5], "x2": [.5, 1]}, later="4p end state"),
 b("S4", "pack", "String 4 (36S, 4p end state)", 61, 42, 77, 49, "HV", MECH + " ('old fuel-tank area under the seats')", "Mechanic's 2nd pack area — inside the cab zone", {"bus": [1, .2], "x2": [.5, 0]}, later="4p end state"),
 b("motor", "motor", "Zero 75-7R motor", 104, 27, 113, 35, "HV", MECH + " (chain to final-drive input)", "on the chain line", {"ph": [0, .95]}),
 b("kelly", "controller", "Kelly KLS8080 + W3-GT2 heatsink", 93, 35, 102, 42, "HV", MECH + " / " + CONS, "near the motor and the HV box; coolant ports up; phase leads away from LEMs + CAN",
   {"bp": [.2, 0], "ph": [1, .3], "pwr": [0, .35], "sig": [0, .5], "can": [0, .8]}, contains=["kelly_heatsink"]),
 b("dcdc", "dcdc", "DC-DC (HV → 12 V)", 81, 36, 88, 41.5, "HV", MECH + " / " + CONS, "near the HV box and the buffer battery", {"hv": [.5, 0], "out": [1, .5]}),
 b("lv_batt", "lv_battery", "12 V buffer battery", 81, 42, 88, 45.5, "LV", MECH, "next to the DC-DC; F-MAIN on its + post", {"pos": [1, .15]}),
 b("fmain", "fuse_main", "F-MAIN 150 A", 89, 41, 90.5, 43, "LV", "Sparky: F-MAIN ≤ 7 in from battery +", "on the battery + lead", {"in": [0, .5], "out": [1, .5]}),
 b("charger", "charger", "Elcon charger", 105, 37, 113, 45, "HV", MECH, "rear, near the inlet", {"dc": [0, .3], "ac": [1, .575], "can": [0, .8]}, later="charging (LATER)"),
 b("inlet", "ac_inlet", "NACS AC inlet (rear panel, high)", 114.5, 40, 115.7, 44, "HV", MECH + " (side or rear panel, high)", "out of splash", {"ac": [0, .4], "prox": [0, .95]}, later="LATER"),
]
def r(id, name, wires, ty, gauge, n, fr, to, pts, src="PINMAP", **kw):
    o = {"id": id, "name": name, "wires": wires, "type": ty, "gauge": gauge, "n": n, "from": fr, "to": to, "pts": [list(p) for p in pts], "label": "PROPOSED", "src": src}
    o.update(kw); return o
runs = [
 # ---- HV
 r("S1-BUS", "String 1 B+/B− → fuse F-S1 / LEM", ["H01", "H11"], "HV", "1/0 AWG", 2, "S1.bus", "hv_fuses.s1", [(91, 15.75), (91, 14.35)], note="B+ and B− tied side by side (Sparky)"),
 r("S2-BUS", "String 2 B+/B− → fuse F-S2 / LEM", ["H01", "H11"], "HV", "1/0 AWG", 2, "S2.bus", "hv_fuses.s2", [(90.6, 22.75), (90.6, 15.85)]),
 r("S3-BUS", "String 3 B+/B− → fuse F-S3", ["H01", "H11"], "HV", "1/0 AWG", 2, "S3.bus", "hv_fuses.s3", [(81.6, 12.5), (81.6, 14.35)], later="4p end state"),
 r("S4-BUS", "String 4 B+/B− → fuse F-S4", ["H01", "H11"], "HV", "1/0 AWG", 2, "S4.bus", "hv_fuses.s4", [(80.9, 43.4), (80.9, 15.85)], later="4p end state"),
 r("MAIN", "EV200 → Kelly B+ and Kelly B− → EV200 B−", ["H07", "H10"], "HV", "3/0 AWG", 2, "hv_cont.out", "kelly.bp", [(86.8, 34), (94.8, 34)], note="tied side by side; west of the chain line"),
 r("PHASE", "Kelly U/V/W → motor", ["H09"], "HV", "per Kelly lugs (ASSUMED 3/0)", 3, "kelly.ph", "motor.ph", [(103, 37.1), (103, 34.6)], note="short; away from LEMs and CAN (Sparky)"),
 r("H12", "HV bus → F-DCDC → DC-DC", ["H12"], "HV", "14 AWG HV", 2, "hv_cont.dcdc", "dcdc.hv", []),
 r("H13", "Elcon DC → F-ELC → pack bus", ["H13"], "HV", "6 AWG", 2, "charger.dc", "hv_cont.chg", [(104.2, 39.4), (104.2, 34.6), (88.5, 34.6)], later="charging (LATER)"),
 r("H14", "NACS AC → Elcon", ["H14"], "HV", "per charger (ASSUMED 12 AWG)", 3, "inlet.ac", "charger.ac", [], later="LATER"),
 # ---- LV
 r("P01", "DC-DC +12 → F-MAIN", ["P01"], "LV", "2 AWG (PARTS fuse_main; PINMAP P01 awg still TBD)", 1, "dcdc.out", "fmain.in", [(88.6, 38.75), (88.6, 42)]),
 r("P02", "F-MAIN → 12 V bus / fuse panel", ["P02"], "LV", "2 AWG (as P01, ASSUMED)", 1, "fmain.out", "lvf.in", [(91.2, 42), (91.2, 31.05)]),
 r("P03", "F-KEL → Kelly PWR", ["P03"], "LV", "16 AWG", 1, "lvf.kel", "kelly.pwr", [(91.2, 27.9), (91.2, 37.45)]),
 r("E01", "bus → F-COIL → e-stop", ["P04", "E01"], "LV", "18 AWG", 1, "lvf.coil", "estop.in", []),
 r("E02", "e-stop → EV200 coils + / precharge coil +", ["E02", "E03"], "LV", "18 AWG", 3, "estop.out", "hv_cont.coil", [(81.6, 29.7), (81.6, 23.75)]),
 r("E03-KSI", "e-stop → Kelly KSI (pin 7)", ["E03"], "LV", "18 AWG", 1, "estop.out", "kelly.sig", [(91.2, 29.7), (91.2, 38.5)]),
 r("E09", "e-stop block 3 → Teensy B:9", ["E09"], "LV", "22 AWG", 2, "estop.blk3", "bms_box.estop", []),
 r("GATE", "Teensy gates → coil-driver board", ["S01", "S02", "S03"], "LV", "22 AWG", 4, "bms_box.cd", "coil_drv.gate", [(76, 28.6), (76, 19.25)], note="3 gates + signal ground"),
 r("COIL", "coil-driver drains → EV200 B+/B−, precharge coil −", ["S01", "S02", "S03"], "LV", "18 AWG", 3, "coil_drv.coil", "hv_cont.coil", [(81.4, 19.25), (81.4, 23.75)]),
 r("COIL-RET", "coil-driver return → TB-GND (star)", [], "LV", "16 AWG", 1, "coil_drv.gnd", "bms_box.tbgnd", [(76.5, 21.05), (76.5, 31.8)], no_wire_id="OPEN: no PINMAP wire id for the coil return to TB-GND"),
 r("LEM", "LEM A/B → Teensy", ["S05", "S06", "S07", "S25", "S26", "S27"], "LV", "22 AWG shielded (ASSUMED)", 8, "bms_box.lem", "hv_lem.sig", [(81.7, 26.2), (81.7, 19)]),
 r("CAN-S1", "SN65 → string 1 X2 (CAN, 5 V, GND)", ["C11", "P10", "G07"], "LV", "22 AWG (CAN twisted pair)", 4, "bms_box.can1", "S1.x2", [(79.7, 27), (79.7, 12.2), (94, 12.2)]),
 r("CAN-S2", "SN65 → string 2 X2 (CAN, 5 V, GND)", ["C12", "P10", "G07"], "LV", "22 AWG (CAN twisted pair)", 4, "bms_box.can2", "S2.x2", [(53, 27.35), (94, 27.35)]),
 r("CAN-S3", "SN65 → string 3 X2", ["C13", "P10", "G07"], "LV", "22 AWG (CAN twisted pair)", 4, "bms_box.can1", "S3.x2", [(69, 27)], later="4p end state"),
 r("CAN-S4", "SN65 → string 4 X2", ["C14", "P10", "G07"], "LV", "22 AWG (CAN twisted pair)", 4, "bms_box.can2", "S4.x2", [(69, 27.8)], later="4p end state"),
 r("CANV-K", "CAN-V: Teensy → Kelly", ["C10"], "LV", "22 AWG twisted pair", 2, "bms_box.vcan", "kelly.can", [(91.2, 30.2), (91.2, 40.6)]),
 r("CANV-E", "CAN-V: Kelly → Elcon", ["C10"], "LV", "22 AWG twisted pair", 2, "kelly.can", "charger.can", [(92.4, 40.6), (92.4, 43.4)], later="charging (LATER)"),
 r("KELLY", "throttle + ECO → Kelly", ["K01", "K03"], "LV", "22 AWG (K01 shielded)", 3, "bms_box.kel", "kelly.sig", [(91.2, 29.4), (91.2, 38.5)]),
 r("PROX", "NACS proximity → Teensy", ["S45"], "LV", "22 AWG", 2, "inlet.prox", "bms_box.prox", [(114, 43.8), (114, 45.9), (91.2, 45.9), (91.2, 31)], later="LATER"),
 r("PEDAL", "pedal → Teensy", ["S14", "S15", "P20"], "LV", "22 AWG", 4, "pedal.out", "bms_box.ped", [(43, 37), (46.4, 37)]),
 r("KEY", "key switch → opto", ["S42", "S43"], "LV", "22 AWG", 2, "key.out", "bms_box.key", [(43.3, 34), (43.3, 31)]),
 r("ESP", "Teensy ↔ ESP32", ["S30", "G05"], "LV", "22 AWG", 2, "bms_box.esp", "esp32.in", [(43.8, 26.6), (43.8, 21)]),
 r("P05", "F-EBOX → BMS box 12 V", ["P05"], "LV", "16 AWG", 1, "lvf.ebox", "bms_box.pwr", [(55, 30.15), (55, 32.6)]),
]
L = {
 "layout": "teryx", "title": "Teryx 750 — top-down EV layout", "status": "PROPOSED",
 "units": "model inches; every length ESTIMATED until scaled to one tape measurement",
 "axes": "x = 0 at the front bumper → rear; y = 0 at the passenger (right) side → driver (left) side (US left-hand drive, ASSUMED)",
 "dims": dims,
 "scale": {"ref": "wheelbase", "measured_in": None, "label": "OPEN", "note": "Adam tapes the wheelbase (hub centre to hub centre, one side) and enters it; everything rescales"},
 "slack": {"pct": d(10, "PROPOSED", "routing slack on every run"), "per_end_in": d(3, "PROPOSED", "per end for lugs/strip/drip loop")},
 "rules": {"hv_lv_gap_in": d(2, "DECIDED", CONS + " (Sparky)"), "cross_tol_deg": d(10, "PROPOSED", "'cross at 90°' with about 10° allowance"),
           "p01_max_in": d(60, "DECIDED", CONS + " P01 < 5 ft"), "fmain_max_in": d(7, "DECIDED", CONS + " F-MAIN ≤ 7 in"),
           "reach_in": d(30, "ASSUMED", "seated arm reach from the seat centre"), "fuse_marker": d("reachable from above, tool-only cover", "DECIDED", CONS + " 06:11")},
 "zones": zones, "blocks": blocks, "runs": runs,
 "notes": [
  "Every position is PROPOSED; block sizes ASSUMED. Vehicle dims are ASSUMED (published spec) except tire_dia (SOURCED, measured).",
  "Inside-HV-box links (H05 fuses→bus, H06 bus→EV200, H08 precharge) are bus bar / short leads and not drawn as runs.",
  "Not drawn: G01/G02/G03 ground straps (to the nearest frame point), A01-A05 actuators (LATER), S08 (not fitted), P06-P08 aux.",
  "Low points / fording depth cannot be shown top-down: keep every box above the old engine bay floor (ground clearance 11.2 in published).",
  "No MSD in this build (DECIDED 06:11): the 4 A6T200 string fuses are the service disconnect.",
 ],
}
if __name__ == "__main__":
    if OUT.exists() and "--force" not in sys.argv: sys.exit(f"{OUT} exists; it is the source now (edit it in tools/layout.html). Use --force to reseed.")
    OUT.write_text(json.dumps(L, indent=1, ensure_ascii=False) + "\n"); print("wrote", OUT.relative_to(R))
