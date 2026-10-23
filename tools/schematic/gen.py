"""Teryx EV system schematic (overview). Wire numbers come from docs/PINMAP.yaml (schem: keys);
every edge must have one (scripts/test_pinmap.py checks). Drawing standard: orthogonal wires, HOP arc where two
unrelated wires cross, FILLED DOT where 2+ wires leave the same terminal. Outputs docs/schematic/teryx-system.{drawio,png,pdf}. Run: python3 tools/schematic/gen.py
Service disconnect = pull the 4 A6T200 string fuses (no Volt MSD)."""
import html, pathlib, sys, yaml, matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Arc
ROOT = pathlib.Path(__file__).resolve().parent.parent.parent   # repo root (moved here from /workspace/teryx/schematic, Build 18)
HERE = ROOT / "docs/schematic"   # outputs: docs/schematic/teryx-system.{drawio,png,pdf}
HERE.mkdir(parents=True, exist_ok=True)
PM = yaml.safe_load((ROOT / "docs/PINMAP.yaml").read_text())
WIRE = {s: w for w in PM["wires"] for s in (w.get("schem") or [])}
# id: (label, x, y, w, h, red=unconfirmed; 2 = LATER reserved, grey dashed box)
N = {
"hvhdr": ("HIGH VOLTAGE — isolated, B− never to chassis (LATER, Sparky)", 20, 10, 660, 30, 0),
"s1": ("String 1 (A, CAN1)", 20, 50, 150, 40, 0), "s2": ("String 2 (A, CAN2)", 185, 50, 150, 40, 0),
"s3": ("String 3 (B, CAN1)", 350, 50, 150, 40, 0), "s4": ("String 4 (B, CAN2)", 515, 50, 150, 40, 0),
"f": ("4× Mersen A6T200 200 A Class T on insulated standoffs\nSERVICE DISCONNECT = pull all 4 fuses (no MSD)", 20, 130, 645, 45, 0),
"fdi": ("HV fuse DC-DC in 15–20 A\n≥250 V DC (confirm)", 20, 215, 190, 45, 1),
"bus": ("Common B+ bus", 250, 220, 200, 35, 0),
"evp": ("EV200 B+", 250, 300, 200, 40, 0),
"pre": ("Precharge relay AEV14012\n(salvaged Volt · confirm label P/N)", 490, 290, 190, 40, 1),
"pres": ("50 Ω 100 W resistor", 490, 350, 190, 35, 0),
"kel": ("Kelly KLS8080 NPS\n168 V 550 A sin/cos", 250, 420, 200, 50, 0), "mot": ("Zero 75-7R (used)\nsin/cos · spin DONE", 490, 425, 190, 40, 0),
"evn": ("EV200 B−", 250, 540, 200, 40, 0),
"lem": ("LEM DHAB A (S1+S2) / B (S3+S4)\non string B− leads", 20, 535, 200, 50, 0),
"dcdc": ("Elcon TDC-JH-144-12 DC-DC\n(isolation barrier)", 20, 650, 220, 50, 1),
"elc": ("Elcon charger", 280, 650, 170, 45, 0), "fel": ("F-ELC Mersen A6T60 60 A\n6 AWG · holder TBD", 490, 650, 190, 45, 0),
"inlet": ("NACS inlet (AC)", 280, 740, 170, 40, 1),
"lvhdr": ("LOW VOLTAGE — 12 V− bonded to chassis", 760, 10, 900, 30, 0),
"b12": ("12 V bus bar", 760, 55, 180, 35, 0), "fmain": ("F-MAIN 12 V (size TBD)", 1000, 55, 200, 35, 1),
"fkel": ("F-KEL Kelly PWR", 760, 120, 150, 32, 1), "fdrv": ("F-COIL coil feed", 925, 120, 150, 32, 0),
"fesp": ("F-EBOX electronics", 1090, 120, 160, 32, 1), "ftab": ("F-TAB tablet", 1265, 120, 140, 32, 1),
"faux": ("F-AUX aux loads", 1420, 120, 150, 32, 0),
"fbx": ("F-BUCK 3 A mini blade", 1090, 180, 160, 32, 0),
"conv": ("5 V buck (set 5.00 V)", 1090, 235, 180, 40, 0), "z": ("Zener 1N5339B", 1320, 240, 150, 35, 0),
"v5": ("5 V terminal block", 1090, 320, 180, 32, 0), "gnd": ("GND terminal block (single point)", 1400, 320, 250, 32, 0),
"ta": ("Teensy A (dual_a) — master\nCAN1 22/23 S1 · CAN2 0/1 S2 · CAN3 30/31 veh\nSerial4 TX17 → ESP32 · pins 11/12/21 coils", 760, 400, 300, 75, 0),
"tb": ("Teensy B (dual_b) — watchdog\nCAN1 S3 · CAN2 S4 · CAN3 veh (stub)\ne-stop input pin UNDEFINED", 1100, 400, 280, 75, 1),
"esp": ("ESP32 gateway (read-only)\nGPIO16 RX · GPIO17 UNUSED", 1430, 400, 230, 50, 0),
"tab": ("iPad (Wi-Fi AP)", 1460, 500, 200, 35, 0),
"x1": ("SN65 #1", 760, 540, 90, 35, 0), "x2": ("SN65 #2", 870, 540, 90, 35, 0),
"x5": ("SN65 #5 +120 Ω", 980, 540, 110, 35, 0),
"x3": ("SN65 #3", 1110, 540, 90, 35, 1), "x4": ("SN65 #4", 1220, 540, 90, 35, 1),
"x6": ("SN65 #6 (stub)", 1330, 590, 110, 35, 1),
"bo": ("BICM X2 harness ×4: 9=5V 10=GND 11=CAN_L 12=CAN_H\n(CAN H/L unconfirmed)", 760, 640, 420, 45, 1),
"pu": ("5139 pull-up", 1210, 700, 130, 32, 0),
"drv": ("Coil-driver board: 3× IRLB8721\n10k pull-downs · D-PRE at AEV coil", 1470, 600, 190, 40, 0),
"clamp": ("Throttle clamp (MCP4131 + 4066 + 74HC123)", 1380, 680, 280, 40, 0),
"ped": ("Pedal (Hall)", 1560, 750, 100, 35, 1),
"pk": ("Park switch 3-pole", 1380, 750, 160, 35, 0),
"low": ("Low-cap relay → Kelly ECO", 1380, 810, 200, 35, 0),
"es1": ("E-stop NC block 1 (11/12)\nEV200 coils", 760, 760, 180, 40, 0),
"es2": ("E-stop NC block 2 (11/12)\nprecharge + KSI", 960, 760, 180, 40, 1),
"es3": ("E-stop NC block 3 (11/12)\n→ Teensy B (pin UNDEFINED)", 1160, 760, 190, 40, 1),
"rly": ("Aux relay 30/87/85/86", 1380, 880, 170, 35, 0), "auxb": ("NO aux button", 1570, 880, 100, 35, 0),
"load": ("Horn / lights", 1380, 950, 170, 35, 0),
"hvs": ("A:24 RESERVED spare (room 2026-10-23)\nPrecharge/weld = Kelly CAN B+ vs BICM sum\nAMC1311EVM only if bench gate fails", 470, 730, 230, 55, 2),
"lfan": ("LATER — reserved: MOTOR_FAN\nMCP23017 bit 14 → IRLB8721 + 1N4007 → fan relay", 760, 195, 250, 40, 2),
"lbuz": ("LATER — reserved: BUZZER\nMCP23017 bit 13 → 12 V buzzer", 760, 250, 250, 40, 2),
"leg": ("Red = unconfirmed / UNDEFINED. Grey dashed = LATER, reserved (no logic). Labels = wire numbers from docs/PINMAP.yaml", 760, 960, 620, 30, 1),
}
# Explicit part ids (docs/PARTS.yaml) per block: str or list. Blocks not listed carry no catalogued part
# (headers, legend, rails, terminal blocks, pull-ups). scripts/check_parts.py checks both directions.
PART = {
 "s1": ["bicm_total", "pack"], "s2": ["bicm_total", "pack"], "s3": ["bicm_total", "pack"], "s4": ["bicm_total", "pack"],
 "f": "fuse_a6t200", "fdi": "dcdc_fuse", "bus": "busbar", "evp": "contactor_main", "evn": "contactor_main",
 "pre": "precharge_relay", "pres": "precharge_res", "kel": "controller", "mot": "motor", "lem": "lem", "dcdc": "dcdc",
 "elc": "charger", "fel": "charger_fuse", "inlet": "ac_inlet", "fmain": "fuse_main",
 "fkel": "lv_fuses", "fdrv": "lv_fuses", "fesp": "lv_fuses", "ftab": "lv_fuses", "faux": "lv_fuses", "fbx": "lv_fuses",
 "conv": "buck_drok", "z": "zener_5v", "ta": "teensy", "tb": "teensy", "esp": "esp32", "tab": "tablet",
 "x1": "sn65", "x2": "sn65", "x3": "sn65", "x4": "sn65", "x5": "sn65", "x6": "sn65", "bo": "bicm_total",
 "drv": ["coil_driver", "cd_flyback"], "clamp": ["digipot", "analog_switch", "oneshot", "buffer"], "ped": "pedal",
 "pk": "park_switch", "low": "eco_relay", "es1": "estop_blocks", "es2": "estop_blocks", "es3": "estop_blocks",
 "rly": "aux_relay", "auxb": "aux_button", "hvs": "hv_sense", "lfan": "motor_fan", "lbuz": "io_expander",
}
assert not set(PART) - set(N), set(PART) - set(N)
E = [("s1","f"),("s2","f"),("s3","f"),("s4","f"),("f","bus"),("bus","evp"),("evp","kel"),("pre","evp"),("pres","pre"),
("kel","mot"),("kel","evn"),("evn","lem"),("lem","s1"),("bus","fdi"),("fdi","dcdc"),("inlet","elc"),("elc","fel"),("fel","bus"),
("dcdc","fmain"),("fmain","b12"),("b12","fkel"),("fkel","kel"),("b12","fdrv"),("fdrv","es1"),("fdrv","es2"),
("es1","evp"),("es1","evn"),("es2","pre"),("es2","kel"),("es3","tb"),("gnd","es3"),
("b12","fesp"),("fesp","fbx"),("fbx","conv"),("conv","v5"),("z","v5"),("v5","bo"),("v5","ta"),("v5","tb"),("v5","esp"),
("gnd","ta"),("gnd","tb"),("gnd","bo"),("ta","x1"),("ta","x2"),("ta","x5"),("tb","x3"),("tb","x4"),("tb","x6"),
("x1","bo"),("x2","bo"),("x3","bo"),("x4","bo"),("x5","x6"),("x6","kel"),("pu","bo"),
("ta","esp"),("gnd","esp"),("esp","tab"),("b12","ftab"),("ta","drv"),("drv","evp"),("drv","pre"),("drv","evn"),
("lem","ta"),("lem","tb"),("ped","clamp"),("ta","clamp"),("tb","clamp"),("clamp","kel"),("pk","ta"),("pk","tb"),
("ta","low"),("low","kel"),("b12","faux"),("faux","rly"),("auxb","rly"),("rly","load")]
missing = [f"{a}|{b}" for a, b in E if f"{a}|{b}" not in WIRE]
if missing: sys.exit("no wire number in PINMAP.yaml for: " + ", ".join(missing))
red = lambda a, b: WIRE[f"{a}|{b}"]["status"] in ("UNVERIFIED", "UNDEFINED")

def route(a, b):
    A, B = N[a], N[b]; x1, y1 = A[1]+A[3]/2, A[2]+A[4]/2; x2, y2 = B[1]+B[3]/2, B[2]+B[4]/2
    if abs(x2-x1) > abs(y2-y1):
        sx = A[1]+(A[3] if x2 > x1 else 0); ex = B[1]+(0 if x2 > x1 else B[3]); m = (sx+ex)/2
        return [(sx, y1), (m, y1), (m, y2), (ex, y2)]
    sy = A[2]+(A[4] if y2 > y1 else 0); ey = B[2]+(0 if y2 > y1 else B[4]); m = (sy+ey)/2
    return [(x1, sy), (x1, m), (x2, m), (x2, ey)]
R = [(a, b, route(a, b)) for a, b in E]
# hops: horizontal segment of wire i crossing vertical segment of an unrelated wire j
hops = {i: [] for i in range(len(R))}
for i, (a, b, P) in enumerate(R):
    for j, (c, d, Q) in enumerate(R):
        if i == j or {a, b} & {c, d}: continue
        for k in range(3):
            (p, q) = P[k], P[k+1]
            if abs(p[1]-q[1]) > 0.1: continue
            for m in range(3):
                (r, s) = Q[m], Q[m+1]
                if abs(r[0]-s[0]) > 0.1: continue
                if min(p[0], q[0])+2 < r[0] < max(p[0], q[0])-2 and min(r[1], s[1])+2 < p[1] < max(r[1], s[1])-2:
                    hops[i].append((k, r[0]))
# junction dots: 2+ wires leaving the same terminal point
from collections import Counter
ends = Counter((round(P[0][0]), round(P[0][1]), a) for a, b, P in R) + Counter((round(P[-1][0]), round(P[-1][1]), b) for a, b, P in R)
fig, ax = plt.subplots(figsize=(24, 15)); ax.set_xlim(0, 1700); ax.set_ylim(1010, -10); ax.axis("off")
nh = 0
for i, (a, b, P) in enumerate(R):
    c = "#cc0000" if red(a, b) else "#555"; ls = "--" if red(a, b) else "-"
    for k in range(3):
        (x0, y0), (x1, y1) = P[k], P[k+1]
        hs = sorted(x for kk, x in hops[i] if kk == k)
        if not hs: ax.plot([x0, x1], [y0, y1], color=c, lw=1, ls=ls, zorder=1); continue
        d = 1 if x1 > x0 else -1; xs = x0
        for hx in (hs if d > 0 else hs[::-1]):
            ax.plot([xs, hx-6*d], [y0, y0], color=c, lw=1, ls=ls, zorder=1)
            ax.add_patch(Arc((hx, y0), 12, 12, theta1=180, theta2=360, color=c, lw=1, zorder=1)); nh += 1
            xs = hx+6*d
        ax.plot([xs, x1], [y0, y1], color=c, lw=1, ls=ls, zorder=1)
    (mx, my) = ((P[1][0]+P[2][0])/2, (P[1][1]+P[2][1])/2)
    ax.text(mx, my, WIRE[f"{a}|{b}"]["id"], fontsize=6, color=c, ha="center", va="center", bbox=dict(fc="white", ec="none", pad=0.3), zorder=3)
for (x, y, _), n in ends.items():
    if n > 1: ax.plot([x], [y], "o", color="black", ms=4, zorder=5)
for k, (l, x, y, w, h, r) in N.items():
    c = "#888888" if r == 2 else "#cc0000" if r else "black"
    if k.endswith("hdr"): ax.text(x, y+15, l, fontsize=12, weight="bold", va="center"); continue
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=2", fc="#fff", ec=c, lw=1.5 if r == 2 else 2 if r else 1, ls="--" if r == 2 else "-", zorder=2))
    ax.text(x+w/2, y+h/2, l, fontsize=6.8, ha="center", va="center", color=c, zorder=4)
ax.axvline(720, color="#999", ls=":")
ax.set_title("Teryx EV system — 4-string dual-Teensy (CAN_LAYOUT_V2), wire numbers from docs/PINMAP.yaml (generated)", fontsize=13)
plt.savefig(HERE/"teryx-system.png", dpi=130, bbox_inches="tight"); plt.savefig(HERE/"teryx-system.pdf", bbox_inches="tight")
# drawio (orthogonal edges; draw.io draws its own jumps with jumpStyle=arc)
cells = []
for k, (l, x, y, w, h, r) in N.items():
    st = ("text;fontStyle=1;fontSize=14;" if k.endswith("hdr") else "rounded=1;whiteSpace=wrap;html=1;")+("dashed=1;strokeColor=#888888;fontColor=#666666;" if r == 2 else "strokeColor=#CC0000;fontColor=#CC0000;strokeWidth=2;" if r else "")
    pa = PART.get(k); pa = " ".join(pa) if isinstance(pa, list) else pa
    if pa:  # UserObject carries the part id(s) as a custom property (draw.io: Edit Data)
        cells.append(f'<UserObject label="{html.escape(l).replace(chr(10),"&lt;br&gt;")}" part="{pa}" id="{k}"><mxCell style="{st}" vertex="1" parent="1"><mxGeometry x="{x}" y="{y}" width="{w}" height="{h}" as="geometry"/></mxCell></UserObject>')
    else:
        cells.append(f'<mxCell id="{k}" value="{html.escape(l).replace(chr(10),"&lt;br&gt;")}" style="{st}" vertex="1" parent="1"><mxGeometry x="{x}" y="{y}" width="{w}" height="{h}" as="geometry"/></mxCell>')
for i, (a, b) in enumerate(E):
    st = "edgeStyle=orthogonalEdgeStyle;html=1;endArrow=none;jumpStyle=arc;jumpSize=10;"+("strokeColor=#CC0000;fontColor=#CC0000;dashed=1;" if red(a, b) else "")
    cells.append(f'<mxCell id="e{i}" value="{html.escape(WIRE[a+"|"+b]["id"])}" style="{st}" edge="1" parent="1" source="{a}" target="{b}"><mxGeometry relative="1" as="geometry"/></mxCell>')
(HERE/"teryx-system.drawio").write_text('<mxfile><diagram name="Teryx EV system"><mxGraphModel dx="1700" dy="1000" grid="1" pageWidth="1700" pageHeight="1100"><root><mxCell id="0"/><mxCell id="1" parent="0"/>'+"".join(cells)+'</root></mxGraphModel></diagram></mxfile>')
print(f"schematic: {len(N)} blocks, {len(E)} wires, {nh} hops, {sum(1 for v in ends.values() if v>1)} junction dots")
