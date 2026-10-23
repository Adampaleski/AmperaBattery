"""install_steps.py — ordered Install-mode steps for docs/boards/<board>.layout.json (Build 25, t312).

Each step: {id, title, label, text, parts:[layout refs], wires:[wire ids], offboard:[refs], part_ids:[PARTS ids]}.
Parts are listed per step below. Wires are assigned automatically: a wire belongs to the first step at which every
part its vertices touch has been fitted, and never before the step that "claims" its net (e.g. the ground-bus step
claims GND); a bus run with no pin of its own goes in with the
first part on its net. Steps are PROPOSED install orders unless a step says otherwise. Called by gen_bms_master.py and
gen_coil_driver.py so a regen keeps them; checked by scripts/check_boards.py and the page's checkBoard().
"""
import json, re

def _hole(s):
    m = re.fullmatch(r"([A-Z]+)(\d+)", s); r = 0
    for ch in m.group(1): r = r * 26 + ord(ch) - 64
    return (r - 1, int(m.group(2)))

def _pins(p, FP):
    a = _hole(p["at"]); rot = p.get("rot", 0); o = {}
    for n, dr, dc in FP[p["fp"]]["pins"]:
        d = {0: (dr, dc), 90: (dc, -dr), 180: (-dr, -dc), 270: (-dc, dr)}[rot]
        o[(a[0] + d[0], a[1] + d[1])] = p["ref"]
    return o

def _members(n):
    return (n.get("members", []) if isinstance(n, dict) else n) or []

def attach(lay, spec, FP):
    """spec: list of dicts {id,title,text,parts:[refs or regex 're:...'],claim:[nets],offboard:[refs],label}."""
    refs = [p["ref"] for p in lay["parts"]]; byref = {p["ref"]: p for p in lay["parts"]}
    step_of, steps = {}, []
    for i, s in enumerate(spec):
        got = []
        for pat in s.get("parts", []):
            hits = [r for r in refs if (re.fullmatch(pat[3:], r) if pat.startswith("re:") else r == pat)]
            assert hits, f"install step {s['id']}: {pat} matches no part"
            for r in hits:
                if r not in step_of: step_of[r] = i; got.append(r)
        steps.append({"id": s["id"], "title": s["title"], "label": s.get("label", "PROPOSED"), "text": s["text"],
                      "parts": got, "wires": [], "offboard": s.get("offboard", []), "part_ids": []})
    left = [r for r in refs if r not in step_of]
    assert not left, f"parts in no install step: {left}"
    pinmap = {}
    for p in lay["parts"]: pinmap.update(_pins(p, FP))
    claim = {n: i for i, s in enumerate(spec) for n in s.get("claim", [])}
    for w in lay.get("wires", []):
        touched = {pinmap[_hole(h)] for h in w["pts"] if _hole(h) in pinmap}
        if not touched:  # a bus run with no pin of its own: it goes in with the first part on its net
            touched = {m.split(".")[0] for m in _members(lay.get("nets", {}).get(w["net"]))} & set(step_of)
            touched = {min(touched, key=step_of.get)} if touched else set()
        i = max([step_of[r] for r in touched] + [claim.get(w["net"], 0)])
        steps[i]["wires"].append(w["id"])
    for s in steps:
        ids = [byref[r]["part"] for r in s["parts"]] + [o["part"] for o in lay.get("offboard", []) if o["ref"] in s["offboard"]]
        s["part_ids"] = sorted(set(ids))
    lay["steps"] = steps
    return lay

COIL_DRIVER = [  # docs/guided_install/coil_driver.md §3 (11 steps) + §4 checks
 {"id": "prep", "title": "1 · Board prep", "parts": [], "text": "cd_board: mark the three channel columns (B+, PRE, B−) and the ground bus; keep the 4 corner holes free for M3 standoffs (guide §3.1)."},
 {"id": "fets", "title": "2 · FETs", "parts": ["Q-B+", "Q-PRE", "Q-B−"], "text": "Fit Q-B+, Q-PRE, Q-B− (IRLB8721), flat face up, pins G-D-S left to right (SOURCED BOM.md:33). Check pin order before soldering (guide §3.2)."},
 {"id": "rgate", "title": "3 · Gate resistors", "parts": ["RG-B+", "RG-PRE", "RG-B−"], "text": "100 Ω in series between each gate-input and the FET G pin, close to the pin (guide §3.3)."},
 {"id": "rpull", "title": "4 · Pull-downs", "parts": ["RP-B+", "RP-PRE", "RP-B−"], "text": "10 kΩ from G to that FET's S on the FET side of the 100 Ω — keeps the FET off while the Teensy boots (guide §3.4)."},
 {"id": "gnd", "title": "5 · Ground bus", "parts": [], "claim": ["GND"], "text": "Join the three S pins / pull-downs to the star at V22. On-board bus = DOUBLED 20 AWG solid (DECIDED Sparky 06:57 #5; the guide's 16 AWG is the off-board return, which lands in J-GND) (guide §3.5)."},
 {"id": "coil", "title": "6 · Coil outputs", "parts": ["J-B+", "J-PRE", "J-B−"], "text": "Each D pin to its coil-out terminal, 18–20 AWG solid: Q-B+ → EV200 B+ coil − (S01), Q-PRE → AEV14012 coil − (S02), Q-B− → EV200 B− coil − (S03) (guide §3.6; lead colours PROPOSED)."},
 {"id": "gate", "title": "7 · Gate inputs", "parts": ["J-SIG"], "text": "J-SIG: Teensy A:11 → B+ (S01), A:12 → PRE (S02), A:21 → B− (S03); J-SIG.1 = Teensy signal GND to the star (guide §3.7)."},
 {"id": "flyback", "title": "8 · Flyback at the coil (off-board)", "parts": [], "offboard": ["D-PRE"], "label": "DECIDED", "text": "D-PRE 1N4007 across the AEV14012 coil terminals AT THE RELAY, band (cathode) to coil +, anode to coil − (DECIDED Sparky 05:41). No diode on the EV200s (guide §3.8)."},
 {"id": "gndpt", "title": "9 · Single-point ground", "parts": ["J-GND"], "text": "J-GND.2 → TB-GND, 16 AWG+, coil return only (DECIDED room 06:11). No Teensy GND wire at TB-GND (G04 dropped) (guide §3.9)."},
 {"id": "feed", "title": "10 · Coil + feed (off-board)", "parts": [], "text": "Confirm coil + leads come from the e-stop NC blocks fed by F-COIL (E01–E03); on the bench use a current-limited 12 V supply (guide §3.10)."},
 {"id": "strain", "title": "11 · Strain relief + mounting", "parts": [], "text": "Tie every off-board lead down; no solder joint holds a wire. Standoffs, sealed box; coat only after the PASS checks (guide §3.11)."},
 {"id": "checks", "title": "Checks (12 V bench, pack disconnected)", "parts": [], "text": "Guide §4 PASS 1–8: gate-GND ≈10.1 kΩ · D-PRE orientation · fail-safe OFF · drive at 3.3 V · coil current · FET temperature · release · e-stop cut."},
]

BMS_MASTER = [  # PROPOSED order (Build 25 request): power+buck, star ground, Teensy sockets, CAN, dividers, LM339, terminals, checks
 {"id": "power", "title": "1 · Power + buck", "parts": ["J-PWR", "F-BUCK", "PS1", "Z1", "F-BICM"], "claim": ["12V_IN", "12V_F", "5V", "5V_BICM"],
  "text": "J-PWR, F-BUCK, DROK buck PS1, Z1 5 V clamp, F-BICM; the 5 V and 5V_BICM rails. Set/verify the buck at 5.0 V BEFORE anything else is fitted (PROPOSED)."},
 {"id": "star", "title": "2 · Star ground", "parts": [], "claim": ["GND"],
  "text": "Star bar / star hole and the GND bus wires that need no later part. Every GND wire lands at the star only (PROPOSED)."},
 {"id": "teensy", "title": "3 · Teensy sockets", "parts": ["TA", "TB"],
  "text": "Female header sockets for Teensy A and Teensy B (do NOT solder the Teensys); the 3V3 rails and Teensy-to-Teensy links (PROPOSED)."},
 {"id": "can", "title": "4 · CAN", "parts": ["re:SN\\d", "re:J-S\\d", "J-CANV", "re:RT\\d"],
  "text": "SN65 transceivers SN1–SN6, CAN terminals J-S1–J-S4 and J-CANV, terminators RT1–RT5 (PROPOSED)."},
 {"id": "div", "title": "5 · Dividers", "parts": ["re:R[AB]\\d+[TB]", "re:C[AB]\\d+", "RP1", "RP2", "RD1", "RD2", "re:CD\\d"],
  "text": "Analog sense dividers RA/RB (T over B) with their caps CA/CB, prox/pedal dividers RP/RD and decoupling CD1–CD6 (PROPOSED)."},
 {"id": "clamp", "title": "6 · Throttle clamp + expander", "parts": ["U1", "U2", "U3", "U4", "U6", "R1", "R4", "R5", "R6", "C5", "D5", "RX1", "RX2", "CX1", "CX2", "RSDA", "RSCL", "RRST"],
  "text": "Throttle-clamp ICs U1–U4 and the I/O expander U6 with their passives (THROTTLE_CLAMP.md). Step added between dividers and LM339 so every part has a step (PROPOSED)."},
 {"id": "lm339", "title": "7 · LM339", "parts": ["U5", "RR1", "RR2", "RL1", "RL2", "RUA", "RUB", "CUA", "CUB", "RSA", "RSB", "RRA", "RRB", "CRA", "CRB"],
  "text": "U5 LM339 release comparator and its reference/latch/pull-up network (PROPOSED)."},
 {"id": "term", "title": "8 · Terminals", "parts": ["J-BODYA", "J-BODYB", "J-DRV", "J-ESP", "J-PROX", "J-KEL", "J-THR", "J-LEMA", "J-LEMB"], "offboard": ["ESP32"],
  "text": "Body headers, J-DRV, J-ESP (ESP32 is OFF-BOARD, TELEMETRY_ONLY 1), J-PROX, J-KEL, J-THR, J-LEMA/B (DHAB pin C GND to its own star hole, G09/G10) (PROPOSED)."},
 {"id": "checks", "title": "9 · Checks", "parts": [],
  "text": "Unpowered: no 5V/3V3-to-GND short, star continuity. Powered with NO Teensys: 5.0 V at the sockets. Then Teensys in, BMS_CAP 0 / TELEMETRY_ONLY 1 firmware, CAN termination ≈60 Ω per bus (PROPOSED; values UNVERIFIED until Sparky signs off)."},
]

if __name__ == "__main__":
    import pathlib, sys
    R = pathlib.Path(__file__).resolve().parents[2]
    FP = json.loads((R / "docs/boards/footprints.json").read_text())
    for b, spec in (("coil_driver", COIL_DRIVER), ("bms_master", BMS_MASTER)):
        fn = R / f"docs/boards/{b}.layout.json"; L = json.loads(fn.read_text())
        attach(L, spec, FP); fn.write_text(json.dumps(L, indent=1, ensure_ascii=False) + "\n")
        print(b, [(s["id"], len(s["parts"]), len(s["wires"])) for s in L["steps"]], file=sys.stderr)
