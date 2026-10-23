#!/usr/bin/env python3
"""Generator for docs/boards/bms_master.layout.json — BMS master board (PROPOSED, Gilfoyle Build 21).

Single source: every pin and net comes from docs/PINMAP.yaml (wire ids cited in each net desc) and
docs/THROTTLE_CLAMP.md (the clamp schematic PINMAP points to). This script holds:
  1. the netlist (PINMAP ids in every desc),
  2. a hand floor plan for the big parts + a greedy placer for the small passives,
  3. a 2-layer router (top = insulated solid wire, bottom = bare/insulated wire under the board),
     soldered at every vertex, with a single signal-ground STAR BAR that each ground group joins ONCE.
Rerun:  python3 tools/boards/gen_bms_master.py   then   python3 scripts/check_boards.py
"""
import heapq, json, pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parents[2]
FP = json.loads((ROOT / "docs/boards/footprints.json").read_text())

def hname(r, c):
    s = ""; r += 1
    while r: r, k = divmod(r - 1, 26); s = chr(65 + k) + s
    return s + str(c)
def rot(dr, dc, a): return {0: (dr, dc), 90: (dc, -dr), 180: (-dr, -dc), 270: (-dc, dr)}[a]

ROWS, COLS = 66, 78          # trimmed to the used area at the end
PARTS, NETS, NC = [], {}, []
def part(ref, pid, fp, r, c, rot_=0, value=None, pin_names=None, zone=None):
    p = {"ref": ref, "part": pid, "fp": fp, "at": (r, c), "rot": rot_}
    if value: p["value"] = value
    if pin_names: p["pin_names"] = pin_names
    p["zone"] = zone; PARTS.append(p); return p
def net(name, members, desc, kind="sig", group=None):
    if name in NETS: NETS[name]["members"] += members; return
    NETS[name] = {"members": list(members), "desc": desc, "kind": kind}

# ------------------------------------------------------------------ fixed floor plan (rows 0.., cols 1..)
TA = part("TA", "teensy", "TEENSY41", 4, 24, 0, "Teensy 4.1 — A (master)", zone="mcu")
TB = part("TB", "teensy", "TEENSY41", 4, 46, 0, "Teensy 4.1 — B (checker)", zone="mcu")
# power, top-left
part("J-PWR", "bm_term", "TB2_508", 1, 3, 0, "12 V in", {"1": "+12 V from F-EBOX out (P07)", "2": "GND -> TB-GND (G03, buck OUT−)"}, zone="pwr")
part("F-BUCK", "lv_fuses", "FUSE_HLD", 4, 3, 0, "3 A", zone="pwr")
part("PS1", "buck_drok", "BUCK_MOD", 7, 3, 0, "DROK 12->5 V buck", zone="pwr")
part("Z1", "zener_5v", "AXIAL600", 7, 14, 90, "1N5339B 5.6 V", zone="pwr")
part("F-BICM", "lv_fuses", "FUSE_HLD", 17, 3, 0, "1 A", zone="pwr")
# A-side CAN, left (rot 270: logic column at col 16 going up, CAN pins at col 6)
def sn_left(n, a, termrow):
    part(f"SN{n}", "sn65", "SN65_MOD", a, 16, 270, f"SN65HVD230 #{n}", zone="can")
    part(f"J-S{n}", "bm_term", "TB3_508", termrow, 2, 270, f"BICM string {n} X2",
         {"1": "5V_BICM -> X2 pin 9 (P10)", "2": "CAN_L -> X2 pin 11 (C1x, UNVERIFIED H/L)", "3": "CAN_H -> X2 pin 12 (C1x, UNVERIFIED H/L)"}, zone="can")
sn_left(2, 27, 27); sn_left(1, 37, 37)
part("SN5", "sn65", "SN65_MOD", 47, 16, 270, "SN65HVD230 #5 (vehicle bus, A)", zone="can")
# B-side CAN, right (rot 90: logic column at col 56 going down, CAN pins at col 66)
def sn_right(n, a):
    part(f"SN{n}", "sn65", "SN65_MOD", a, 56, 90, f"SN65HVD230 #{n}", zone="can")
    part(f"J-S{n}", "bm_term", "TB3_508", a - 1, 70, 90, f"BICM string {n} X2",
         {"1": "5V_BICM -> X2 pin 9 (P10)", "2": "CAN_L -> X2 pin 11 (C1x, UNVERIFIED H/L)", "3": "CAN_H -> X2 pin 12 (C1x, UNVERIFIED H/L)"}, zone="can")
sn_right(3, 3); sn_right(4, 13)
part("SN6", "sn65", "SN65_MOD", 23, 56, 90, "SN65HVD230 #6 (vehicle bus, B)", zone="can")
part("J-CANV", "bm_term", "TB2_508", 25, 70, 90, "vehicle CAN (C10)", {"1": "CAN-V H -> Kelly -> Elcon (C10)", "2": "CAN-V L -> Kelly -> Elcon (C10)"}, zone="can")
# throttle clamp, between the Teensys
part("U1", "oneshot", "DIP16", 11, 34, 0, "CD74HCT123E", zone="clamp")
part("U3", "digipot", "DIP8", 17, 36, 0, "MCP4131-103E/P", zone="clamp")
part("U4", "buffer", "DIP14", 24, 34, 0, "SN74AHCT125N", zone="clamp")
part("U2", "analog_switch", "DIP14", 32, 34, 0, "CD74HC4066E", zone="clamp")
# I/O expander + Teensy A link terminals, bottom-left
part("U6", "io_expander", "DIP28", 57, 4, 0, "MCP23017 (0x20)", zone="dig")
part("J-BODYB", "bm_hdr", "HDR1X8", 60, 4, 0, "body GPB0-7", {str(i + 1): f"GPB{i}" + (" BUZZER" if i == 5 else " MOTOR_FAN" if i == 6 else "") for i in range(8)}, zone="dig")
part("J-BODYA", "bm_hdr", "HDR1X8", 51, 11, 180, "body GPA0-7 (lights)", {str(i + 1): f"GPA{i}" for i in range(8)}, zone="dig")
part("J-DRV", "bm_term", "TB4_508", 64, 27, 180, "to coil_driver J-SIG", {"1": "SGND -> coil_driver J-SIG.1", "2": "S03 A:21 -> J-SIG.2", "3": "S02 A:12 -> J-SIG.3", "4": "S01 A:11 -> J-SIG.4"}, zone="dig")
part("J-ESP", "bm_term", "TB3_508", 64, 19, 180, "ESP32 link (S30/G05)", {"1": "A:17 Serial4 TX -> ESP32 GPIO16 (S30)", "2": "GND -> ESP32 GND (G05)", "3": "+5 V -> ESP32 5V (P08 rail)"}, zone="dig")
# analog, bottom-right
part("U5", "lm339", "DIP14", 56, 44, 0, "LM339 (quad comparator)", zone="ana")
part("J-PROX", "bm_term", "TB2_508", 64, 47, 180, "NACS PROX", {"1": "PROX -> NACS inlet proximity pin (S45)", "2": "NC — plug side returns to inlet PE (chassis), UNVERIFIED"}, zone="ana")
part("J-REL", "bm_term", "TB2_508", 64, 53, 180, "release detect OUT (OPEN)", {"1": "REL_A — LM339 OUT3 open collector, Teensy pin OPEN (not in PINMAP)", "2": "REL_B — LM339 OUT4 open collector, Teensy pin OPEN (not in PINMAP)"}, zone="ana")
part("J-KEL", "bm_term", "TB2_508", 64, 39, 180, "Kelly throttle (K01)", {"1": "clamped throttle -> Kelly throttle input (K01)", "2": "NC — Kelly throttle GND reference not in PINMAP (OPEN)"}, zone="ana")
part("J-THR", "bm_term", "TB4_508", 64, 71, 180, "pedal (P20/S14/S15)", {"1": "+5 V pedal supply (P20)", "2": "GND pedal return (no PINMAP wire, PROPOSED)", "3": "TRK1 pedal wiper/track 1 (S14)", "4": "TRK2 pedal track 2 (S15)"}, zone="ana")
part("J-LEMA", "bm_term", "TB4_508", 64, 63, 180, "LEM A (S05-S07)", {"1": "+5 V LEM A (S07)", "2": "GND LEM A (no PINMAP wire, PROPOSED)", "3": "CH2 -> A:15 (S06)", "4": "CH1 -> A:14 (S05)"}, zone="ana")
part("J-LEMB", "bm_term", "TB4_508", 45, 71, 90, "LEM B (S25-S27)", {"1": "+5 V LEM B (S27)", "2": "GND LEM B (no PINMAP wire, PROPOSED)", "3": "CH2 -> B:15 (S26)", "4": "CH1 -> B:14 (S25)"}, zone="ana")

# spread: everything right of the clamp moves 6 columns right so the clamp channel between the Teensys is wider
SPREAD = 6
for _p in PARTS:
    if _p["at"][1] >= 46: _p["at"] = (_p["at"][0], _p["at"][1] + SPREAD)
for _p in PARTS:
    if _p["ref"] == "U3": _p["at"] = (17, 42)
    if _p["ref"] == "U4": _p["at"] = (24, 40)

# ------------------------------------------------------------------ passives (placed by the greedy placer below)
PASS = []   # (ref, pid, fp, value, zone-region)
def pas(ref, pid, fp, value, region): PASS.append((ref, pid, fp, value, region))
RCL = (6, 36, 31, 51)       # clamp region rows r0..r1, cols c0..c1
RAN = (37, 62, 29, 75)      # analog region
RLC = (18, 50, 2, 22)       # left CAN region
RRC = (1, 31, 60, 75)       # right CAN region
RDG = (50, 62, 2, 30)       # digital bottom-left
for n, reg in ((1, RLC), (2, RLC), (5, RLC), (3, RRC), (4, RRC)):
    pas(f"RT{n}", "can_term", "AXIAL400", "120 Ω", reg)
# clamp (THROTTLE_CLAMP.md BOM refs)
pas("R1", "bm_res", "AXIAL400", "100k", RCL); pas("R4", "perfboard_kit", "AXIAL400", "10k", RCL); pas("R6", "perfboard_kit", "AXIAL400", "10k", RCL)
pas("R5", "perfboard_kit", "AXIAL400", "10k", RCL); pas("C5", "bm_cap", "CAP200", "1 µF", RCL); pas("D5", "clr_diode", "AXIAL400", "1N4148", RCL)
pas("RX1", "bm_res", "AXIAL400", "100k 1%", RCL); pas("RX2", "bm_res", "AXIAL400", "100k 1%", RCL)
pas("CX1", "bm_cap", "CAP200", "100 nF C0G/film", RCL); pas("CX2", "bm_cap", "CAP200", "100 nF C0G/film", RCL)
for u in (1, 2, 3, 4): pas(f"CD{u}", "perfboard_kit", "CAP200", "100 nF", RCL)
# analog dividers: 10k top / 18k bottom / 100 nF
DIVS = [("A14", "TA", "14", "S05"), ("A15", "TA", "15", "S06"), ("A25", "TA", "25", "S07"), ("A40", "TA", "40", "S14"),
        ("B14", "TB", "14", "S25"), ("B15", "TB", "15", "S26"), ("B25", "TB", "25", "S27"), ("B41", "TB", "41", "S15")]
for d, *_ in DIVS:
    pas(f"R{d}T", "perfboard_kit", "AXIAL400", "10k 1%", RAN); pas(f"R{d}B", "perfboard_kit", "AXIAL400", "18k 1%", RAN); pas(f"C{d}", "perfboard_kit", "CAP200", "100 nF", RAN)
# proximity / LM339
pas("RP1", "bm_res", "AXIAL400", "330 Ω", RAN); pas("RP2", "bm_res", "AXIAL400", "2.7k", RAN)
pas("RD1", "perfboard_kit", "AXIAL400", "10k", RAN); pas("RD2", "bm_res", "AXIAL400", "20k", RAN)
pas("RR1", "perfboard_kit", "AXIAL400", "10k", RAN); pas("RR2", "perfboard_kit", "AXIAL400", "10k", RAN)
pas("RL1", "perfboard_kit", "AXIAL400", "10k", RAN); pas("RL2", "bm_res", "AXIAL400", "3.9k", RAN)
pas("RUA", "perfboard_kit", "AXIAL400", "10k", RAN); pas("RUB", "perfboard_kit", "AXIAL400", "10k", RAN)
pas("CUA", "perfboard_kit", "CAP200", "100 nF", RAN); pas("CUB", "perfboard_kit", "CAP200", "100 nF", RAN)
pas("CD5", "perfboard_kit", "CAP200", "100 nF", RAN)
# expander
pas("RSDA", "bm_res", "AXIAL400", "4.7k", RDG); pas("RSCL", "bm_res", "AXIAL400", "4.7k", RDG); pas("CD6", "perfboard_kit", "CAP200", "100 nF", RDG)

ANA_PAS = {x[0] for x in PASS if x[4] is RAN}
CAN_PAS = {x[0] for x in PASS if x[4] is RLC or x[4] is RRC}
# ------------------------------------------------------------------ netlist (PINMAP.yaml ids in every desc)
# power
net("12V_IN", ["J-PWR.1", "F-BUCK.1"], "+12 V from F-EBOX out -> F-BUCK 3 A (P07)", "pwr")
net("12V_F", ["F-BUCK.2", "PS1.IN+"], "F-BUCK -> buck IN+ (P07)", "pwr")
net("5V", ["PS1.OUT+", "Z1.1", "F-BICM.1", "TA.VIN", "TB.VIN", "J-ESP.3", "J-LEMA.1", "J-LEMB.1", "J-THR.1",
           "U1.16", "U2.14", "U3.8", "U4.14", "U5.3", "RP1.2", "RR1.1", "RL1.1", "R5.2", "D5.2", "RX1.2", "RX2.2",
           "RA25T.1", "RB25T.1", "CD1.1", "CD2.1", "CD3.1", "CD4.1", "CD5.1"],
    "On-board 5 V bus = PINMAP TB-5V realised on the board: buck OUT+ (P08), zener 1N5339B cathode (P09), Teensy A/B VIN (P11/P12, VIN/VUSB pad CUT), F-BICM (P10), ESP32 5 V (P08), LEM A/B 5 V (S07/S27), pedal 5 V + U1..U4 VCC (P20), LM339 VCC + 330 Ω prox pull-up (S47), A:25/B:25 sense dividers (S07/S27)", "pwr")
net("5V_BICM", ["F-BICM.2", "J-S1.1", "J-S2.1", "J-S3.1", "J-S4.1"], "F-BICM 1 A -> BICM X2 pin 9 ×4 strings (P10)", "pwr")
net("A_3V3", ["TA.3V3", "SN1.3V3", "SN2.3V3", "SN5.3V3", "U6.9", "U6.18", "RSDA.2", "RSCL.2", "RUA.2", "CD6.1"],
    "Teensy A 3.3 V out: SN65 #1/#2/#5 VCC (C01/C03/C05), MCP23017 VDD + RESET (expander, PROPOSED), I2C pull-ups (PROPOSED), A:33 pull-up (S41)", "pwr")
net("B_3V3", ["TB.3V3", "SN3.3V3", "SN4.3V3", "SN6.3V3", "RUB.2"], "Teensy B 3.3 V out: SN65 #3/#4/#6 VCC (C02/C04/C06), B:33 pull-up (S44)", "pwr")
# CAN logic side
for t, n, tx, rx, wid in (("TA", 1, "22", "23", "C01"), ("TA", 2, "1", "0", "C03"), ("TA", 5, "31", "30", "C05"),
                         ("TB", 3, "22", "23", "C02"), ("TB", 4, "1", "0", "C04"), ("TB", 6, "31", "30", "C06")):
    net(f"{t[1]}{tx}_TX{n}", [f"{t}.{tx}", f"SN{n}.TX"], f"Teensy {t[1]}:{tx} CAN TX -> SN65 #{n} TXD ({wid})")
    net(f"{t[1]}{rx}_RX{n}", [f"{t}.{rx}", f"SN{n}.RX"], f"SN65 #{n} RXD -> Teensy {t[1]}:{rx} CAN RX ({wid})")
for n, wid in ((1, "C11"), (2, "C12"), (3, "C13"), (4, "C14")):
    net(f"CANH{n}", [f"SN{n}.CANH", f"RT{n}.1", f"J-S{n}.3"], f"SN65 #{n} CANH -> string {n} X2 pin 12 ({wid}, H/L UNVERIFIED); 120 Ω RT{n} at the Teensy end (CAN term)", "can")
    net(f"CANL{n}", [f"SN{n}.CANL", f"RT{n}.2", f"J-S{n}.2"], f"SN65 #{n} CANL -> string {n} X2 pin 11 ({wid}, H/L UNVERIFIED)", "can")
net("CANV_H", ["SN5.CANH", "RT5.1", "SN6.CANH", "J-CANV.1"], "A<->B link CAN3 + vehicle bus: SN65 #5 (120 Ω RT5, A end only) -> #6 (no 120 Ω) -> Kelly -> Elcon (C05/C06/C10)", "can")
net("CANV_L", ["SN5.CANL", "RT5.2", "SN6.CANL", "J-CANV.2"], "A<->B link CAN3 + vehicle bus, low side (C05/C06/C10)", "can")
# throttle clamp (THROTTLE_CLAMP.md; PINMAP S10/S11/S12/S14/S15/K01/P20)
net("A35_DATA", ["TA.35", "U4.2"], "A:35 DATA 3.3 V -> U4 1A (S10)"); net("A36_CLK", ["TA.36", "U4.5"], "A:36 CLK -> U4 2A (S10)")
net("A38_CS", ["TA.38", "U4.9"], "A:38 CS -> U4 3A (S10)")
net("DP_SDI", ["U4.3", "U3.3"], "U4 1Y -> MCP4131 SDI (S10)"); net("DP_SCK", ["U4.6", "U3.2"], "U4 2Y -> MCP4131 SCK (S10)")
net("DP_CS", ["U4.8", "U3.1"], "U4 3Y -> MCP4131 CS (S10)")
net("A4_ALIVE", ["TA.4", "R6.1", "U4.12"], "A:4 A_THR_ALIVE + R6 10k pull-down -> U4 4A (S11)")
net("ALIVE_A5", ["U4.11", "U1.2"], "U4 4Y -> U1 1B (S11)")
net("B7_ALIVE", ["TB.7", "R4.1", "U1.10"], "B:7 B_THR_ALIVE + R4 10k pull-down -> U1 2B (S12)")
net("WIPER", ["U3.6", "U2.1"], "MCP4131 P0W -> U2a in (THROTTLE_CLAMP)")
net("SW_MID", ["U2.2", "U2.4"], "U2a out -> U2b in (series)")
net("KELLY_THR", ["U2.3", "R1.1", "J-KEL.1"], "U2b out + R1 100k to GND -> Kelly throttle input (K01)")
net("Q1", ["U1.13", "U2.13"], "U1 1Q -> U2a control"); net("Q2", ["U1.5", "U2.5"], "U1 2Q -> U2b control")
net("CX1", ["U1.14", "CX1.1"], "U1 1Cx"); net("RC1", ["U1.15", "CX1.2", "RX1.1"], "U1 1Rx/Cx: Cx1 100 nF + Rx1 100k to 5 V")
net("CX2", ["U1.6", "CX2.1"], "U1 2Cx"); net("RC2", ["U1.7", "CX2.2", "RX2.1"], "U1 2Rx/Cx: Cx2 100 nF + Rx2 100k to 5 V")
net("CLR", ["U1.3", "U1.11", "R5.1", "C5.1", "D5.1"], "U1 /CLR1 = /CLR2: R5 10k to 5 V, C5 1 µF to GND, D5 1N4148 (anode here, cathode 5 V)")
net("PEDAL1", ["J-THR.3", "U3.5", "RA40T.1"], "pedal track 1 raw 0-5 V -> MCP4131 P0A + A:40 divider (S14)", "ana")
net("PEDAL2", ["J-THR.4", "RB41T.1"], "pedal track 2 -> B:41 divider (S15)", "ana")
# dividers
for d, t, pin, wid in DIVS:
    if pin != "25":
        src = {"A14": "J-LEMA.4", "A15": "J-LEMA.3", "B14": "J-LEMB.4", "B15": "J-LEMB.3"}.get(d)
        if src: net(f"LEM_{d}", [src, f"R{d}T.1"], f"LEM {t[1]} DHAB {'ch1' if pin == '14' else 'ch2'} -> 10k ({wid})", "ana")
    net(f"{d}_SENSE", [f"R{d}T.2", f"R{d}B.1", f"C{d}.1", f"{t}.{pin}"], f"10k/18k + 100 nF -> Teensy {t[1]}:{pin} ({wid})" + (" — divider values not stated in PINMAP for :25, 10k/18k PROPOSED" if pin == "25" else ""), "ana")
# proximity + LM339 (S41/S44-S49; LM393 pin numbers in PINMAP mapped to LM339)
net("PROX", ["J-PROX.1", "RP1.1", "RP2.1", "RD1.1"], "NACS PROX: 330 Ω to 5 V + 2.7k to GND, -> 10k/20k divider (S45/S46)", "ana")
net("PROX_DIV", ["RD1.2", "RD2.1", "U5.5", "U5.7", "U5.9", "U5.11"], "10k/20k divided PROX -> LM339 IN1+ (5), IN2+ (7) (S46) and IN3+/IN4+ (9/11) for release detect (lm339 spec)", "ana")
net("VREF", ["RR1.2", "RR2.1", "U5.4", "U5.6"], "2.5 V reference 10k/10k -> LM339 IN1− (4), IN2− (6) (S48)", "ana")
net("REL_REF", ["RL1.2", "RL2.1", "U5.8", "U5.10"], "release-detect threshold ~1.4 V (lm339 spec): 10k/3.9k from 5 V = 1.40 V -> LM339 IN3− (8), IN4− (10) — PROPOSED, not in PINMAP", "ana")
net("PLUG_A", ["U5.2", "RUA.1", "CUA.1", "TA.33"], "LM339 OUT1 (2) + 10k pull-up to A 3.3 V + 100 nF -> A:33, LOW = plugged (S41)", "ana")
net("PLUG_B", ["U5.1", "RUB.1", "CUB.1", "TB.33"], "LM339 OUT2 (1) + 10k pull-up to B 3.3 V + 100 nF -> B:33 (S44)", "ana")
net("REL_A", ["U5.14", "J-REL.1"], "LM339 OUT3 (14) open collector -> J-REL.1; Teensy pin + pull-up OPEN (not in PINMAP)", "ana")
net("REL_B", ["U5.13", "J-REL.2"], "LM339 OUT4 (13) open collector -> J-REL.2; Teensy pin + pull-up OPEN (not in PINMAP)", "ana")
# expander (PINMAP expander block; I2C on A:18/19)
net("SDA", ["TA.18", "U6.13", "RSDA.1"], "A:18 SDA -> MCP23017 SDA (13) + 4.7k pull-up (PROPOSED)")
net("SCL", ["TA.19", "U6.12", "RSCL.1"], "A:19 SCL -> MCP23017 SCL (12) + 4.7k pull-up (PROPOSED)")
for i in range(8):
    net(f"GPA{i}", [f"U6.{21 + i}", f"J-BODYA.{i + 1}"], f"MCP23017 GPA{i} (expander bit {i}) -> body header")
    net(f"GPB{i}", [f"U6.{1 + i}", f"J-BODYB.{i + 1}"], f"MCP23017 GPB{i} (expander bit {8 + i}) -> body header")
# Teensy A link terminals
net("A17_ESP", ["TA.17", "J-ESP.1"], "A:17 Serial4 TX -> ESP32 GPIO16 (S30)")
net("A21_S03", ["TA.21", "J-DRV.2"], "A:21 -> coil_driver J-SIG.2 (S03)")
net("A12_S02", ["TA.12", "J-DRV.3"], "A:12 -> coil_driver J-SIG.3 (S02)")
net("A11_S01", ["TA.11", "J-DRV.4"], "A:11 -> coil_driver J-SIG.4 (S01)")

# ground groups — one net GND, routed as separate group trees that meet ONLY at the star bar
GGROUPS = {
    "PWR": ["J-PWR.2", "PS1.IN-", "PS1.OUT-", "Z1.2"],
    "CANA": ["SN1.GND", "SN2.GND", "SN5.GND"],
    "CANB": ["SN3.GND", "SN4.GND", "SN6.GND"],
    "TA": ["TA.GND1", "J-ESP.2", "J-DRV.1", "U6.10", "U6.15", "U6.16", "U6.17", "CD6.2"],
    "TB": ["TB.GND1"],
    "CLAMP": ["U1.1", "U1.9", "U1.8", "U2.6", "U2.12", "U2.7", "U3.4", "U3.7", "U4.1", "U4.4", "U4.7", "U4.10", "U4.13",
              "R1.2", "C5.2", "R4.2", "R6.2", "CD1.2", "CD2.2", "CD3.2", "CD4.2"],
    "ANALOG": [f"R{d}B.2" for d, *_ in DIVS] + [f"C{d}.2" for d, *_ in DIVS] +
              ["J-LEMA.2", "J-LEMB.2", "J-THR.2", "RP2.2", "RD2.2", "RR2.2", "RL2.2", "U5.12", "CUA.2", "CUB.2", "CD5.2"],
}
NETS["GND"] = {"members": [m for g in GGROUPS.values() for m in g], "kind": "gnd",
               "desc": "ONE signal ground, single STAR BAR (18 AWG, bottom side). Each ground group (power/buck, CAN A, CAN B, Teensy A + ESP + coil_driver SGND + expander, Teensy B, clamp, analog) joins the bar by ONE wire and nowhere else (S49 'single-point ground'; same rule as coil_driver's V22 star). J-PWR.2 is the board's only ground lead to TB-GND (G03). Teensy A signal ground leaves on J-DRV.1 -> coil_driver J-SIG.1 (DECIDED room 2026-10-23 06:11, G04: no Teensy GND wire at TB-GND)."}

# ------------------------------------------------------------------ geometry helpers
MOUNTS = [(0, 1), (0, COLS), (ROWS - 1, 1), (ROWS - 1, COLS)]
def pin_cells(p):
    a = p["at"]; return {n: (a[0] + rot(dr, dc, p["rot"])[0], a[1] + rot(dr, dc, p["rot"])[1]) for n, dr, dc in FP[p["fp"]]["pins"]}
def body_cells(p):
    """holes under the part body (top side blocked there unless the footprint stands on headers)"""
    f = FP[p["fp"]]; x0, y0, x1, y1 = f.get("body", [-.5, -.5, .5, .5]); a = p["at"]; out = set()
    import math
    for dr in range(math.ceil(y0), math.floor(y1) + 1):
        for dc in range(math.ceil(x0), math.floor(x1) + 1):
            rr, cc = rot(dr, dc, p["rot"]); out.add((a[0] + rr, a[1] + cc))
    return out
ONHDR = {"TEENSY41", "SN65_MOD", "HDR1X8"}     # stand on headers: top wires may pass under
def inb(rc): return 0 <= rc[0] < ROWS and 1 <= rc[1] <= COLS
def margin_cells(p, m=1):
    s = set()
    for (r, c) in body_cells(p) | set(pin_cells(p).values()):
        for dr in range(-m, m + 1):
            for dc in range(-m, m + 1): s.add((r + dr, c + dc))
    return s

# ------------------------------------------------------------------ greedy placer for passives
member_of = {}
for nn, nd in NETS.items():
    for m in nd["members"]: member_of[m] = nn
GROUP_OF = {m: g for g, ms in GGROUPS.items() for m in ms}
def place_passives():
    occ = set()
    for p in PARTS: occ |= margin_cells(p, 1)   # keep a routing channel round every fixed part
    keep = set()
    for p in PARTS: keep |= set(pin_cells(p).values())
    def pinpos():
        d = {}
        for p in PARTS:
            for n, rc in pin_cells(p).items(): d[f"{p['ref']}.{n}"] = rc
        return d
    for ref, pid, fp, value, (r0, r1, c0, c1) in PASS:
        P = pinpos(); best = None
        for rt in (0, 90):
            for r in range(r0, r1 + 1):
                for c in range(c0, c1 + 1):
                    p = {"ref": ref, "part": pid, "fp": fp, "at": (r, c), "rot": rt}
                    pc = pin_cells(p); cells = set(pc.values()) | body_cells(p)
                    if not all(r0 <= x[0] <= r1 and c0 <= x[1] <= c1 for x in cells): continue
                    if cells & occ: continue
                    # keep one free hole around every pin of the new part and existing pins (routing room)
                    halo = {(x[0] + a, x[1] + b) for x in pc.values() for a in (-1, 0, 1) for b in (-1, 0, 1)}
                    if halo & keep - set(): continue
                    score = 0
                    for n, rc in pc.items():
                        m = f"{ref}.{n}"; nn = member_of.get(m)
                        if nn is None: continue
                        if nn == "GND": others = [P[o] for o in GGROUPS[GROUP_OF[m]] if o in P]; w = 0.6
                        elif NETS[nn]["kind"] == "pwr": others = [P[o] for o in NETS[nn]["members"] if o in P and o != m]; w = 0.3
                        else: others = [P[o] for o in NETS[nn]["members"] if o in P and o != m]; w = 1.0
                        if others: score += w * min(abs(o[0] - rc[0]) + abs(o[1] - rc[1]) for o in others)
                    if best is None or score < best[0]: best = (score, p)
        if best is None: sys.exit(f"placer: no room for {ref} in {(r0, r1, c0, c1)}")
        p = best[1]; p["value"] = value; p["zone"] = "pas"; PARTS.append(p)
        occ |= body_cells(p) | set(pin_cells(p).values())
        keep |= set(pin_cells(p).values())
        # one-hole gap around the new body so wires can pass between parts
        occ |= {(x[0] + a, x[1] + b) for x in body_cells(p) | set(pin_cells(p).values()) for a in (-1, 0, 1) for b in (-1, 0, 1) if (a == 0 or b == 0)}
place_passives()

def ascii_map():
    g = [["." for _ in range(COLS + 1)] for _ in range(ROWS)]
    for p in PARTS:
        for (r, c) in body_cells(p):
            if inb((r, c)): g[r][c] = "#"
    for p in PARTS:
        for (r, c) in pin_cells(p).values():
            if inb((r, c)): g[r][c] = "o"
    return "\n".join(f"{r:2d} " + "".join(row[1:]) for r, row in enumerate(g))

# ------------------------------------------------------------------ 2-layer router
TOP, BOT = 0, 1
DIRS = [(-1, 0), (0, 1), (1, 0), (0, -1)]
PINPOS, PIN_AT = {}, {}
for p in PARTS:
    for n, rc in pin_cells(p).items(): PINPOS[f"{p['ref']}.{n}"] = rc; PIN_AT[rc] = f"{p['ref']}.{n}"
TOPBLOCK = set()
for p in PARTS:
    if p["fp"] not in ONHDR: TOPBLOCK |= body_cells(p) - set(pin_cells(p).values())
# routing nets: GND groups become pseudo nets "GND#<group>"
RNETS = {}
for nn, nd in NETS.items():
    if nn == "GND":
        for g, ms in GGROUPS.items(): RNETS[f"GND#{g}"] = ms
    else: RNETS[nn] = nd["members"]
PIN_NET = {PINPOS[m]: rn for rn, ms in RNETS.items() for m in ms}
NCPINS = {rc for m, rc in PINPOS.items() if not any(m in ms for ms in RNETS.values())}

STAR_ROW, STAR_C0 = 52, 22          # star bar: bottom side, row STAR_ROW, cols STAR_C0 .. STAR_C0+2*(groups)-2
STAR = [(STAR_ROW, STAR_C0 + 2 * i) for i in range(len(GGROUPS))]

import itertools
_SEQ = itertools.count()
INF = float("inf")
BAR = [(STAR_ROW, c) for c in range(STAR[0][1], STAR[-1][1] + 1)]
HARD_BOT = set(BAR)

class Board:
    """negotiated-congestion (PathFinder) board: cells may be over-used while negotiating."""
    def __init__(s):
        s.occ = [dict(), dict()]      # layer -> cell -> {rnet}
        s.sold = {}                   # cell -> {rnet} solder points made by wires (vertices)
        s.paths = {}                  # rnet -> [path [(cell, layer), ...]]
        s.star_of = {}                # GND group -> star cell
        s.hist = {}                   # cell -> history cost
        s.pres = 0.5
    def others(s, rc, L, rn):
        o = s.occ[L].get(rc)
        return 0 if not o else len(o - {rn})
    def osold(s, rc, rn):
        o = s.sold.get(rc)
        return 0 if not o else len(o - {rn})
    def pen(s, rc, k):
        return 0 if k == 0 else k * s.pres * (1 + s.hist.get(rc, 0)) * 4
    def hard(s, rc, L, rn):
        if not inb(rc) or rc in MOUNTS: return True
        if L == TOP and rc in TOPBLOCK and rc not in PIN_NET: return True
        if L == BOT and rc in HARD_BOT: return True
        if rc in STAR: return True
        pn = PIN_NET.get(rc)
        if pn is not None and pn != rn: return True
        if rc in NCPINS: return True
        return False
    def add_path(s, rn, path):
        s.paths.setdefault(rn, []).append(path)
        for i, (rc, L) in enumerate(path):
            s.occ[L].setdefault(rc, set()).add(rn)
        for i, (rc, L) in enumerate(path):
            end = i in (0, len(path) - 1)
            bend = 0 < i < len(path) - 1 and (path[i - 1][1] != L or path[i + 1][1] != L or
                    (path[i + 1][0][0] - path[i - 1][0][0]) * (path[i + 1][0][1] - path[i - 1][0][1]) != 0)
            if end or bend: s.sold.setdefault(rc, set()).add(rn)
    def remove(s, rn):
        for path in s.paths.pop(rn, []):
            for rc, L in path:
                o = s.occ[L].get(rc)
                if o: o.discard(rn)
                o = s.sold.get(rc)
                if o: o.discard(rn)
        if rn.startswith("GND#"): s.star_of.pop(rn[4:], None)
    def conflicts(s):
        """cells where two different rnets clash; returns {cell: {rnets}}"""
        bad = {}
        cells = set(s.occ[0]) | set(s.occ[1]) | set(s.sold)
        for rc in cells:
            t = s.occ[0].get(rc, set()); b = s.occ[1].get(rc, set()); v = s.sold.get(rc, set())
            nets = set()
            if len(t) > 1: nets |= t
            if len(b) > 1: nets |= b
            if v and (t | b | v) - v: nets |= (t | b | v)
            elif len(v) > 1: nets |= v
            if rc in PIN_NET and (t | b | v) - {PIN_NET[rc]}: nets |= (t | b | v)
            if rc in STAR and nets:
                # a star hole is shared by the bar and the ONE group that lands there (that is the point of it)
                own = {"GND#" + g for g, c in s.star_of.items() if c == rc}
                if len(own) <= 1 and nets <= {"GND#STAR"} | own: nets = set()
            if nets: bad[rc] = nets
        return bad

def route_conn(B, rn, sources, targets, star=False):
    """multi-source Dijkstra with congestion cost; sources = {(cell, layer)}; targets = set of cells."""
    dist, prev, pq = {}, {}, []
    for (rc, L) in sources:
        st = (rc, L, -1); dist[st] = 0; prev[st] = None; heapq.heappush(pq, (0, next(_SEQ), rc, L, -1))
    while pq:
        d, _, rc, L, dr = heapq.heappop(pq)
        if dist.get((rc, L, dr), INF) < d: continue
        if rc in targets and dr != -1:
            path = []; st = (rc, L, dr)
            while st is not None: path.append((st[0], st[1])); st = prev[st]
            return path[::-1]
        oL = 1 - L
        # layer change = solder point here
        if dr != -2 and not (star and rc in targets) and not B.hard(rc, oL, rn) and rc not in STAR:
            nd = d + 6 + B.pen(rc, B.others(rc, oL, rn) + B.others(rc, L, rn) * (dr >= 0))
            ns = (rc, oL, -2)
            if nd < dist.get(ns, INF): dist[ns] = nd; prev[ns] = (rc, L, dr); heapq.heappush(pq, (nd, next(_SEQ), rc, oL, -2))
        for k, (a, b) in enumerate(DIRS):
            if dr >= 0 and k == (dr + 2) % 4: continue
            n = (rc[0] + a, rc[1] + b)
            turn = dr >= 0 and k != dr
            extra = 0
            if turn: extra += 2 + B.pen(rc, B.others(rc, oL, rn))       # bend = vertex: hole must be free on the far side too
            if n in targets:
                if star and L != TOP: continue
                if star and B.others(n, TOP, rn): continue
                if (not star) and L == TOP and n in TOPBLOCK and n not in PIN_NET: continue
            else:
                if B.hard(n, L, rn): continue
                if B.occ[L].get(n) and rn in B.occ[L][n]: continue
                extra += B.pen(n, B.others(n, L, rn) + B.osold(n, rn))
            nd = d + 1 + extra + (0.3 if L == TOP else 0) + 0.2 * B.hist.get(n, 0)
            ns = (n, L, k)
            if nd < dist.get(ns, INF): dist[ns] = nd; prev[ns] = (rc, L, dr); heapq.heappush(pq, (nd, next(_SEQ), n, L, k))
    return None

def tree_sources(B, rn, pins):
    S = set()
    for rc in pins: S.add((rc, TOP)); S.add((rc, BOT))
    for path in B.paths.get(rn, []):
        for rc, L in path:
            if rc in STAR: continue
            if B.others(rc, 1 - L, rn) == 0 and B.osold(rc, rn) == 0:
                S.add((rc, L))
                if not B.hard(rc, 1 - L, rn): S.add((rc, 1 - L))
    return S

def route_net(B, rn):
    ms = [PINPOS[m] for m in RNETS[rn]]
    if len(ms) >= 2:
        cr = sum(r for r, _ in ms) / len(ms); cc = sum(c for _, c in ms) / len(ms)
        start = min(ms, key=lambda x: abs(x[0] - cr) + abs(x[1] - cc))
        done = {start}; todo = set(ms) - done
        while todo:
            path = route_conn(B, rn, tree_sources(B, rn, done), todo)
            if path is None: return False
            B.add_path(rn, path); done.add(path[-1][0]); todo.discard(path[-1][0])
    if rn.startswith("GND#"):
        g = rn[4:]
        free = {rc for rc in STAR if rc not in B.star_of.values()}
        path = route_conn(B, rn, tree_sources(B, rn, set(ms)), free, star=True)
        if path is None: return False
        B.add_path(rn, path); B.star_of[g] = path[-1][0]
    return True

def length(ms):
    rs = [PINPOS[m][0] for m in ms]; cs = [PINPOS[m][1] for m in ms]
    return (max(rs) - min(rs)) + (max(cs) - min(cs))

def route_all(order, iters=100):
    B = Board()
    B.paths["GND#STAR"] = [[(rc, BOT) for rc in BAR]]
    for rc in BAR: B.occ[BOT].setdefault(rc, set()).add("GND#STAR")
    for rc in STAR: B.sold.setdefault(rc, set()).add("GND#STAR")
    todo = list(order); best, stall = INF, 0
    for it in range(iters):
        failed = []
        for rn in todo:
            B.remove(rn)
            if not route_net(B, rn): failed.append(rn); B.remove(rn)
        bad = B.conflicts()
        for rc in bad: B.hist[rc] = B.hist.get(rc, 0) + 1
        clash = set().union(*bad.values()) if bad else set()
        clash.discard("GND#STAR")
        print(f"  pass {it + 1}: {len(bad)} clash cells, {len(clash)} nets, {len(failed)} failed {failed[:6]}", file=sys.stderr)
        if not bad and not failed: return B
        B.pres = min(B.pres * 1.5, 200)
        score = len(bad) + 10 * len(failed)
        if score < best: best, stall = score, 0
        else: stall += 1
        if stall >= 6: todo, stall = list(order), 0      # stuck: rip up and re-negotiate every net
        else: todo = failed + [n for n in order if n in clash and n not in failed]
    return B, sorted(set(failed) | clash)

def final_net(rn): return "GND" if rn.startswith("GND#") else rn
def to_wires(B):
    """paths -> polylines per side; vertex at ends, bends, layer changes and every same-net solder point it passes."""
    sold_final = {}
    for rc, rns in B.sold.items(): sold_final[rc] = {final_net(x) for x in rns}
    for rc, rn in PIN_NET.items(): sold_final.setdefault(rc, set()).add(final_net(rn))
    wires = []
    for rn, paths in B.paths.items():
        fn = final_net(rn)
        for path in paths:
            segs, cur = [], [path[0]]
            for x in path[1:]:
                if x[1] != cur[-1][1]: segs.append(cur); cur = [x]
                else: cur.append(x)
            segs.append(cur)
            for sg in segs:
                if len(sg) < 2: continue
                cells = [rc for rc, _ in sg]; L = sg[0][1]; keep = []
                for i, rc in enumerate(cells):
                    if i in (0, len(cells) - 1): keep.append(rc); continue
                    a, b = cells[i - 1], cells[i + 1]
                    if (b[0] - a[0]) * (b[1] - a[1]) != 0 or fn in sold_final.get(rc, ()): keep.append(rc)
                wires.append((fn, rn, "top" if L == TOP else "bot", keep))
    return wires


AWG = {"pwr": "20 AWG (power)", "gnd": "20 AWG (ground)", "can": "22 AWG (signal, CAN pair)", "ana": "22 AWG (signal)", "sig": "22 AWG (signal)"}
def write_layout(W):
    """trim to parts + wires + a 2-hole margin (same rule as the page's Auto-size), mounts at the corners, write JSON."""
    used = set()
    for p in PARTS: used |= body_cells(p) | set(pin_cells(p).values())
    for _, _, _, pts in W: used |= set(pts)
    r0 = min(r for r, _ in used); r1 = max(r for r, _ in used); c0 = min(c for _, c in used); c1 = max(c for _, c in used)
    M = 2; dr = M - r0; dc = M + 1 - c0
    rows = r1 - r0 + 1 + 2 * M; cols = c1 - c0 + 1 + 2 * M
    sh = lambda rc: hname(rc[0] + dr, rc[1] + dc)
    parts = []
    for p in PARTS:
        q = {"ref": p["ref"], "part": p["part"], "fp": p["fp"], "at": sh(p["at"]), "rot": p["rot"]}
        if p.get("value"): q["value"] = p["value"]
        if p.get("pin_names"): q["pin_names"] = p["pin_names"]
        parts.append(q)
    wires = []
    for i, (fn, rn, side, pts) in enumerate(sorted(W, key=lambda w: (w[0], w[1], w[2], w[3])), 1):
        if rn == "GND#STAR": awg = "18 AWG (star bar)"
        else: awg = AWG[NETS[fn]["kind"]]
        wires.append({"id": f"w{i}", "net": fn, "side": side, "awg": awg, "pts": [sh(x) for x in pts]})
    nets = {nn: {"members": nd["members"], "desc": nd["desc"]} for nn, nd in NETS.items()}
    allm = {m for nd in NETS.values() for m in nd["members"]}
    nc = [m for m in PINPOS if m not in allm]
    # zone separation report: min Manhattan hole distance analog part pins -> CAN/buck part pins
    ana = [PINPOS[f"{p['ref']}.{n}"] for p in PARTS if p.get("zone") == "ana" or (p.get("zone") == "pas" and p["ref"] in ANA_PAS) for n, *_ in FP[p["fp"]]["pins"]]
    noisy = [PINPOS[f"{p['ref']}.{n}"] for p in PARTS if p.get("zone") in ("can", "pwr") or p["ref"] in CAN_PAS for n, *_ in FP[p["fp"]]["pins"]]
    dmin = min(abs(a[0] - b[0]) + abs(a[1] - b[1]) for a in ana for b in noisy)
    mid = STAR[len(STAR) // 2]
    star_cells = ", ".join(sh(x) for x in STAR)
    lay = {
        "schema": "teryx-perfboard/1", "board": "bms_master",
        "title": "BMS master board — Teensy A + B, 6× SN65HVD230, 5 V buck, LEM dividers, throttle clamp, LM339, MCP23017",
        "status": "PROPOSED", "assembly": "bms_master", "board_part": "bm_board",
        "grid": {"pitch_mm": 2.54, "rows": rows, "cols": cols},
        "mounts": [hname(0, 1), hname(0, cols), hname(rows - 1, 1), hname(rows - 1, cols)],
        "star": sh(mid),
        "notes": [
            "PROPOSED (Gilfoyle 2026-10-23, Build 21). Generated by tools/boards/gen_bms_master.py from docs/PINMAP.yaml + docs/THROTTLE_CLAMP.md; every net desc cites its PINMAP wire id. Edit in tools/perfboard.html, Export JSON, commit here.",
            f"Board {rows}×{cols} holes = {rows * 2.54:.1f} × {cols * 2.54:.1f} mm ({rows / 10:.1f} × {cols / 10:.1f} in) incl. a 2-hole margin; M3 mounts at the four corners.",
            f"GROUND: ONE signal-ground star BAR (18 AWG bare, bottom side) {sh(STAR[0])}–{sh(STAR[-1])}; star holes {star_cells}. Each ground group (power/buck, CAN A, CAN B, Teensy A, Teensy B, clamp, analog) lands on ONE star hole and touches no other group — same single-point rule as coil_driver's V22 star (PINMAP S49). J-PWR.2 is the only lead to TB-GND (G03, buck OUT−). Teensy A signal ground leaves only on J-DRV.1 -> coil_driver J-SIG.1 (DECIDED room 06:11; G04 dropped).",
            f"Zones: CAN + buck on the left/right/top edges; analog (LEM/pedal dividers, LM339, PROX) bottom-right, kept ≥{dmin} holes ({dmin * 2.54:.0f} mm) from any SN65 module, CAN terminator, fuse or the buck (see test_perfboard).",
            "Teensy A/B: VIN/VUSB pad CUT on both (P11/P12) — they run from the board 5 V bus. SN65 module pin order (3V3/GND/TX/RX | CANH/CANL) UNVERIFIED — check your module silkscreen and remove any on-module 120 Ω except RT-equivalents (OPEN).",
            "Top = insulated solid-core jumpers (may pass under the Teensy/SN65 headers); bottom = wire on the solder side. Soldered at every vertex. Wire gauges: 20 AWG power/ground, 22 AWG signal, 18 AWG star bar.",
            "Release detect (LM339 comparators 3/4, ~1.40 V threshold from the lm339 PARTS spec) outputs go to J-REL only: the Teensy pins are OPEN (not in PINMAP).",
            "BMS_CAP 0 / TELEMETRY_ONLY 1 — this board does not change any limit; firmware is not flashed (no flashing).",
        ],
        "source": ["docs/PINMAP.yaml (C01–C14, P07–P12, P20, S05–S07, S10–S15, S25–S27, S30, S41–S49, K01, G03–G07, expander block)",
                   "docs/THROTTLE_CLAMP.md (U1–U4, R1/R4–R6, C5, D5, RX/CX)",
                   "Room decisions 2026-10-23 06:11 CT (Sparky + chief): low side only, G04 dropped, single star",
                   "PJRC Teensy 4.1 pinout card (card11a_rev4)", "docs/PARTS.yaml specs (lm339 release threshold, mcp23017, sn65)"],
        "parts": parts, "wires": wires, "nets": nets, "nc": nc,
        "offboard": [{"ref": "ESP32", "part": "esp32", "value": "ESP32 DevKit", "near": "J-ESP",
                      "short": "ESP32 is OFF-BOARD: J-ESP 1 = A:17 TX, 2 = GND, 3 = +5 V",
                      "text": "ESP32 telemetry module is NOT on this board. J-ESP.1 = Teensy A:17 Serial4 TX -> ESP32 GPIO16 RX (S30, TX-only link); J-ESP.2 = GND (G05, from the Teensy A ground group); J-ESP.3 = +5 V board bus (P08). TELEMETRY_ONLY 1."}],
    }
    out = ROOT / "docs/boards/bms_master.layout.json"
    out.write_text(json.dumps(lay, indent=1, ensure_ascii=False) + "\n")
    print(f"wrote {out.relative_to(ROOT)}: {rows}x{cols} holes, {len(parts)} parts, {len(wires)} wires, {len(nets)} nets, {len(nc)} nc; analog<->CAN/buck min {dmin} holes", file=sys.stderr)

if __name__ == "__main__":
    if "--map" in sys.argv: print(ascii_map()); sys.exit()
    sig = [n for n in RNETS if not n.startswith("GND#") and NETS[n]["kind"] != "pwr"]
    pwr = [n for n in RNETS if not n.startswith("GND#") and NETS[n]["kind"] == "pwr"]
    gnd = [n for n in RNETS if n.startswith("GND#")]
    order = sorted(sig, key=lambda n: length(RNETS[n])) + sorted(pwr, key=lambda n: length(RNETS[n])) + sorted(gnd, key=lambda n: length(RNETS[n]))
    import pickle
    cache = pathlib.Path("/tmp/bm_route_B.pkl")
    res = pickle.loads(cache.read_bytes()) if "--cached" in sys.argv and cache.exists() else route_all(order)
    if not isinstance(res, tuple): cache.write_bytes(pickle.dumps(res))
    if isinstance(res, tuple): B, failed = res; print("UNROUTED:", failed); sys.exit(1)
    B = res
    W = to_wires(B)
    print(len(W), "wires", file=sys.stderr)
    write_layout(W)
