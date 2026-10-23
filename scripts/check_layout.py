#!/usr/bin/env python3
"""check_layout.py — top-down vehicle layout (docs/layout/teryx.layout.json, edited in tools/layout.html).

Usage: check_layout.py [layout.json] [--json]   (default docs/layout/teryx.layout.json)

Model (same maths and wording as the page's live check, tools/layout.template.html checkLayout()):
  * Model units are inches. Every vehicle dimension sits in ONE config (dims, each labelled). One tape
    measurement of a reference dimension (scale.ref + scale.measured_in) gives k = measured / model;
    every length and distance is multiplied by k. Until then lengths are ESTIMATED (k = 1).
  * Blocks are rectangles tied to docs/PARTS.yaml ids; a block with 'in' sits inside its parent box.
    Terminals are fractions of the block (0..1). A run is a polyline: from-terminal, pts..., to-terminal.
  * Run length = polyline length x k. Cut length per conductor = length x (1 + slack pct/100) + 2 x per-end.
ERRORS (exit 1): bad label, unknown PARTS id, unknown PINMAP wire id, unknown block/terminal, bad type,
  duplicate ids, child box with an unknown parent.
WARNINGS (rules, shown on the page): an UNSEALED HV block in the cab zone; two top-level blocks overlapping on the
  same layer; HV and LV looms closer than 2 in or sharing a
  segment; HV/LV crossing not at 90 deg (+-10); P01 over 5 ft; F-MAIN more than 7 in from the DC-DC + out; F-BATT more than 7 in from the buffer battery +;
  precharge resistor without metal_mount; string-fuse group without the marker; any MSD block; a block or run
  inside a keep-out zone; e-stop out of reach of either seat.
INFO (shown, not warnings): a sealed HV housing in the cab and HV runs through the cab (in conduit/loom between sealed
  housings) — Adam 2026-10-23 DECIDED, supersedes chief's 'no HV in the cab' pending crew comment; blocks stacked on
  different layers.
LAYERS: a block or keep-out may carry z = low | mid | high (Adam 2026-10-23: Kelly high in the dash, motor mid in front of
  the dash, axle/driveshaft/chain low). Overlap and keep-out hits between two EXPLICIT different layers are skipped (INFO
  'stacked'); no z = every layer, so it still conflicts.
"""
import json, math, pathlib, sys
import yaml

R = pathlib.Path(__file__).resolve().parent.parent
LABELS = {"SOURCED", "ASSUMED", "PROPOSED", "UNVERIFIED", "DECIDED", "OPEN"}
FUSE_MARKER = "reachable from above, tool-only cover"

def f1(x):  # identical rounding in JS: Math.floor(x*10+0.5)/10
    return "%.1f" % (math.floor(x * 10 + 0.5) / 10)

def scale_k(L):
    s = L.get("scale") or {}
    d = (L.get("dims") or {}).get(s.get("ref") or "")
    m = s.get("measured_in")
    if d and isinstance(m, (int, float)) and m > 0 and d.get("v"):
        return m / d["v"]
    return 1.0

def vehicle(dims):
    """vehicle outline, wheels, seats, bed, frame rails and the cab zone — all derived from the ONE dims config
    (model inches; y = 0 at the passenger/right side, x = 0 at the front bumper). Same as the page's vehicle()."""
    D = {k: v["v"] for k, v in dims.items()}
    Lx, Wy = D["overall_length"], D["overall_width"]; cy = Wy / 2
    fa = D["front_overhang"]; ra = fa + D["wheelbase"]; td = D["tire_dia"]
    wheels = []
    for ax, tw, tr, nm in ((fa, D["tire_w_front"], D["track_front"], "front"), (ra, D["tire_w_rear"], D["track_rear"], "rear")):
        for sd, sg in (("passenger", -1), ("driver", 1)):
            wheels.append({"name": f"{nm} {sd}", "x": ax - td / 2, "y": cy + sg * tr / 2 - tw / 2, "w": td, "h": tw})
    sw, sdp, sx, cw = D["seat_width"], D["seat_depth"], D["seat_x0"], D["console_width"]
    seats = [{"name": "passenger", "x": sx, "y": cy - cw / 2 - sw, "w": sdp, "h": sw}, {"name": "driver", "x": sx, "y": cy + cw / 2, "w": sdp, "h": sw}]
    for s in seats: s["reach_from"] = [s["x"] + s["w"] / 2, s["y"] + s["h"] / 2]
    bed = {"x": Lx - D["bed_rear_gap"] - D["bed_length"], "y": cy - D["bed_width"] / 2, "w": D["bed_length"], "h": D["bed_width"]}
    cab = {"id": "cab", "name": "cab", "kind": "cab", "x": D["dash_x"], "y": D["cab_margin"], "w": D["cab_x1"] - D["dash_x"], "h": Wy - 2 * D["cab_margin"]}
    rails = [{"x0": D["rail_x0"], "x1": D["rail_x1"], "y": cy + sg * D["rail_spacing"] / 2} for sg in (-1, 1)]
    return {"length": Lx, "width": Wy, "cy": cy, "axles": [fa, ra], "wheels": wheels, "seats": seats, "bed": bed, "cab": cab, "rails": rails}

def blocks_by_id(L): return {b["id"]: b for b in L.get("blocks", [])}

def term_xy(L, ref):
    bid, _, t = ref.partition(".")
    b = blocks_by_id(L).get(bid)
    if not b or t not in (b.get("terms") or {}): return None
    fx, fy = b["terms"][t]
    return (b["x"] + fx * b["w"], b["y"] + fy * b["h"])

def run_pts(L, r):
    a, b = term_xy(L, r["from"]), term_xy(L, r["to"])
    if a is None or b is None: return None
    return [a] + [tuple(p) for p in r.get("pts", [])] + [b]

def plen(P): return sum(math.hypot(P[i + 1][0] - P[i][0], P[i + 1][1] - P[i][1]) for i in range(len(P) - 1))

def lengths(L):
    """[{id,type,gauge,n,len,cut,total}] (inches, scaled by k)."""
    k = scale_k(L); sl = L.get("slack") or {}
    pct = (sl.get("pct") or {}).get("v", 0); pe = (sl.get("per_end_in") or {}).get("v", 0)
    out = []
    for r in L.get("runs", []):
        P = run_pts(L, r)
        if P is None: continue
        ln = plen(P) * k; cut = ln * (1 + pct / 100) + 2 * pe
        out.append({"id": r["id"], "type": r["type"], "gauge": r.get("gauge", ""), "n": r.get("n", 1), "len": ln, "cut": cut, "total": cut * r.get("n", 1)})
    return out

# ---- geometry
def seg_rect(a, b, x0, y0, x1, y1):
    """Liang-Barsky: does segment a-b touch the closed rect? (same as the page's segBox)"""
    t0, t1 = 0.0, 1.0; dx, dy = b[0] - a[0], b[1] - a[1]
    for p, q in ((-dx, a[0] - x0), (dx, x1 - a[0]), (-dy, a[1] - y0), (dy, y1 - a[1])):
        if p == 0:
            if q < 0: return False
        else:
            t = q / p
            if p < 0:
                if t > t1: return False
                t0 = max(t0, t)
            else:
                if t < t0: return False
                t1 = min(t1, t)
    return True

def rect_overlap(a, b):  # open overlap (touching edges do not count)
    return a["x"] < b["x"] + b["w"] and b["x"] < a["x"] + a["w"] and a["y"] < b["y"] + b["h"] and b["y"] < a["y"] + a["h"]

def seg_x(a, b, c, d):
    """proper intersection point of a-b and c-d (None if parallel or apart)"""
    r = (b[0] - a[0], b[1] - a[1]); s = (d[0] - c[0], d[1] - c[1]); den = r[0] * s[1] - r[1] * s[0]
    if abs(den) < 1e-9: return None
    t = ((c[0] - a[0]) * s[1] - (c[1] - a[1]) * s[0]) / den; u = ((c[0] - a[0]) * r[1] - (c[1] - a[1]) * r[0]) / den
    if -1e-9 <= t <= 1 + 1e-9 and -1e-9 <= u <= 1 + 1e-9: return (a[0] + t * r[0], a[1] + t * r[1])
    return None

def pt_seg(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]; l2 = dx * dx + dy * dy
    t = 0 if l2 == 0 else max(0, min(1, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / l2))
    q = (a[0] + t * dx, a[1] + t * dy)
    return math.hypot(p[0] - q[0], p[1] - q[1]), q

def seg_seg(a, b, c, d):
    """min distance + the midpoint of the closest pair (segments that do not properly cross)"""
    best = None
    for p, s0, s1 in ((a, c, d), (b, c, d), (c, a, b), (d, a, b)):
        dd, q = pt_seg(p, s0, s1)
        if best is None or dd < best[0]: best = (dd, ((p[0] + q[0]) / 2, (p[1] + q[1]) / 2))
    return best

def angle(a, b, c, d):
    u = math.atan2(b[1] - a[1], b[0] - a[0]); v = math.atan2(d[1] - c[1], d[0] - c[0])
    x = abs(math.degrees(u - v)) % 180
    return min(x, 180 - x)

def in_rect(p, b, m=0.0): return b["x"] - m <= p[0] <= b["x"] + b["w"] + m and b["y"] - m <= p[1] <= b["y"] + b["h"] + m

ZT = {"low": 0, "mid": 1, "high": 2}

def zdiff(a, b):  # both on an explicit, different layer
    return a.get("z") in ZT and b.get("z") in ZT and a["z"] != b["z"]

def check(L, parts, wires):
    E, W, I = [], [], []
    k = scale_k(L); RU = L.get("rules") or {}
    rv = lambda n, d: (RU.get(n) or {}).get("v", d)
    gap, tol, p01max, fmax, reach = rv("hv_lv_gap_in", 2), rv("cross_tol_deg", 10), rv("p01_max_in", 60), rv("fmain_max_in", 7), rv("reach_in", 30)
    fbmax = rv("fbatt_max_in", 7)
    marker = rv("fuse_marker", FUSE_MARKER)
    if L.get("status") not in LABELS: W.append(f"status {json.dumps(L.get('status'))} is not one of the labels")
    for n, d in (L.get("dims") or {}).items():
        if d.get("label") not in LABELS: E.append(f"dim {n}: label {d.get('label')!r} is not one of the labels")
    B = blocks_by_id(L); seen = set()
    for b in L.get("blocks", []):
        if b["id"] in seen: E.append(f"duplicate block id {b['id']}")
        seen.add(b["id"])
        for p in [b.get("part")] + list(b.get("contains") or []):
            if p not in parts: E.append(f"block {b['id']}: part id '{p}' is not in docs/PARTS.yaml")
        if b.get("label") not in LABELS: E.append(f"block {b['id']}: label {b.get('label')!r} is not one of the labels")
        if b.get("v") not in ("HV", "LV"): E.append(f"block {b['id']}: v must be HV or LV")
        if b.get("in") and b["in"] not in B: E.append(f"block {b['id']}: parent '{b['in']}' is not a block")
        if "z" in b and b["z"] not in ZT: E.append(f"block {b['id']}: z {b['z']!r} must be low, mid or high")
        for f in ("z_label", "sealed_label", "guard_label"):
            if f in b and b[f] not in LABELS: E.append(f"block {b['id']}: {f} {b[f]!r} is not one of the labels")
    for z in L.get("zones", []):
        if z.get("label") not in LABELS: E.append(f"zone {z['id']}: label {z.get('label')!r} is not one of the labels")
        if "z" in z and z["z"] not in ZT: E.append(f"zone {z['id']}: z {z['z']!r} must be low, mid or high")
    runs = L.get("runs", []); RP = {}; rs = set()
    for r in runs:
        if r["id"] in rs: E.append(f"duplicate run id {r['id']}")
        rs.add(r["id"])
        if r.get("type") not in ("HV", "LV"): E.append(f"run {r['id']}: type must be HV or LV")
        if r.get("label") not in LABELS: E.append(f"run {r['id']}: label {r.get('label')!r} is not one of the labels")
        for w in r.get("wires") or []:
            if w not in wires: E.append(f"run {r['id']}: wire id '{w}' is not in docs/PINMAP.yaml")
        if not r.get("wires") and not r.get("no_wire_id"): E.append(f"run {r['id']}: no PINMAP wire ids (set no_wire_id with the reason)")
        for e in ("from", "to"):
            if term_xy(L, r.get(e, "")) is None: E.append(f"run {r['id']}: {e} '{r.get(e)}' is not a block terminal")
        P = run_pts(L, r)
        if P: RP[r["id"]] = P
    # ---- rules
    V = vehicle(L["dims"]); cab = [V["cab"]]
    keep = [z for z in L.get("zones", []) if z.get("kind") == "keepout"]
    lat = lambda o: f" ({o['later']})" if o.get("later") else ""
    sealed = lambda b: bool(b.get("sealed") or (b.get("in") and (B.get(b["in"]) or {}).get("sealed")))
    for b in L.get("blocks", []):
        if b.get("v") == "HV" and not b.get("in"):
            for z in cab:
                if rect_overlap(b, z):
                    if sealed(b): I.append(f"HV in cab (sealed housing): block {b['id']} ({b['name']}) is inside the {z['name']} zone{lat(b)}")
                    else: W.append(f"HV in cab: block {b['id']} ({b['name']}) is inside the {z['name']} zone and is not a sealed housing{lat(b)}")
        if b.get("part") == "msd" or "msd" in (b.get("contains") or []): W.append(f"MSD: block {b['id']} — there is no MSD in this build (DECIDED 2026-10-23: pull the 4 A6T200 string fuses)")
        if b.get("part") == "precharge_res" and not (b.get("flags") or {}).get("metal_mount"): W.append(f"precharge resistor {b['id']} has no metal-mount flag (Sparky: metal mount with airflow)")
        if b.get("part") == "fuse_a6t200" and b.get("marker") != marker: W.append(f"string fuses {b['id']} must carry the marker '{marker}'")
        if not b.get("in"):
            for z in keep:
                if b["id"] not in (z.get("exempt") or []) and rect_overlap(b, z):
                    if zdiff(b, z):
                        I.append(f"stacked: block {b['id']} ({b['z']}) is over {z['name']} ({z['z']}) — different layers" + (f"; {b['guard']}" if b.get("guard") else "") + lat(b))
                        if z.get("guard_rule") and b.get("v") == "HV" and not b.get("guard"): W.append(f"guard: HV block {b['id']} is stacked over {z['name']} with no guard / clearance note (crew: ≥ 2 in vertical clearance or a guard){lat(b)}")
                    else: W.append(f"keep-out: block {b['id']} is inside {z['name']}{lat(b)}")
    top = [b for b in L.get("blocks", []) if not b.get("in")]
    for i, a in enumerate(top):
        for c in top[i + 1:]:
            if rect_overlap(a, c):
                if zdiff(a, c): I.append(f"stacked: {a['id']} ({a['z']}) and {c['id']} ({c['z']}) overlap on different layers{lat(a) or lat(c)}")
                else: W.append(f"overlap: blocks {a['id']} and {c['id']} overlap on the same layer{lat(a) or lat(c)}")
    for r in runs:
        P = RP.get(r["id"])
        if not P: continue
        segs = list(zip(P, P[1:]))
        if r["type"] == "HV":
            for z in cab:
                if any(seg_rect(a, b, z["x"], z["y"], z["x"] + z["w"], z["y"] + z["h"]) for a, b in segs): I.append(f"HV in cab: run {r['id']} passes through the {z['name']} zone — in conduit/loom between sealed housings{lat(r)}")
        for z in keep:
            if r["id"] not in (z.get("exempt") or []) and any(seg_rect(a, b, z["x"], z["y"], z["x"] + z["w"], z["y"] + z["h"]) for a, b in segs): W.append(f"keep-out: run {r['id']} passes through {z['name']}{lat(r)}")
    encl = [b for b in L.get("blocks", [])]
    inside = lambda p: any(in_rect(p, b, 0.5) for b in encl)
    HV = [r for r in runs if r.get("type") == "HV" and r["id"] in RP]; LV = [r for r in runs if r.get("type") == "LV" and r["id"] in RP]
    for h in HV:
        for l in LV:
            worst_x, worst_d = None, None
            for a, b in zip(RP[h["id"]], RP[h["id"]][1:]):
                for c, d in zip(RP[l["id"]], RP[l["id"]][1:]):
                    x = seg_x(a, b, c, d)
                    if x is not None:
                        if inside(x): continue
                        an = angle(a, b, c, d)
                        if abs(90 - an) > tol and (worst_x is None or an < worst_x): worst_x = an
                        continue
                    dd, mid = seg_seg(a, b, c, d)
                    if dd * k < gap and not inside(mid) and (worst_d is None or dd < worst_d): worst_d = dd
            if worst_x is not None: W.append(f"HV/LV crossing: {h['id']} and {l['id']} cross at {f1(worst_x)}° (want 90° ± {f1(tol)}°){lat(h) or lat(l)}")
            if worst_d is not None:
                W.append(f"HV/LV loom: {h['id']} and {l['id']} share a segment{lat(h) or lat(l)}" if worst_d * k < 0.05 else f"HV/LV loom: {h['id']} and {l['id']} run {f1(worst_d * k)} in apart (want ≥ {f1(gap)} in){lat(h) or lat(l)}")
    for x in lengths(L):
        if "P01" in (next(r for r in runs if r["id"] == x["id"]).get("wires") or []) and x["len"] > p01max:
            W.append(f"P01: run {x['id']} is {f1(x['len'])} in (over {f1(p01max)} in / 5 ft)")
    blk = lambda part: [b for b in L.get("blocks", []) if b.get("part") == part]
    fm, fb, bt, dc = blk("fuse_main"), blk("fuse_batt"), blk("lv_battery"), blk("dcdc")
    def near(fs, srcs, term, lim, tag, what):  # Sparky 07:34: F-MAIN at the DC-DC output, F-BATT <= 7 in from battery +
        for f in fs:
            for b in srcs:
                p, q = term_xy(L, f["id"] + ".in"), term_xy(L, b["id"] + "." + term)
                if p and q:
                    d = math.hypot(p[0] - q[0], p[1] - q[1]) * k
                    if d > lim: W.append(f"{tag}: {f['id']} is {f1(d)} in from the {what} (want ≤ {f1(lim)} in)")
    near(fm, dc, "out", fmax, "F-MAIN", "DC-DC + out")
    near(fb, bt, "pos", fbmax, "F-BATT", "buffer battery +")
    es = [b for b in L.get("blocks", []) if b.get("part") == "estop_head"]
    for e in es:
        c = (e["x"] + e["w"] / 2, e["y"] + e["h"] / 2)
        for s in V["seats"]:
            d = math.hypot(c[0] - s["reach_from"][0], c[1] - s["reach_from"][1]) * k
            if d > reach: W.append(f"e-stop {e['id']} is {f1(d)} in from the {s['name']} seat reach point (want ≤ {f1(reach)} in, ASSUMED)")
    if not fm: W.append("F-MAIN: no fuse_main block placed")
    if bt and not fb: W.append("F-BATT: no fuse_batt block placed next to the buffer battery")
    if not es: W.append("e-stop: no estop_head block placed")
    return E, W, I

def load_refs():
    parts = yaml.safe_load((R / "docs/PARTS.yaml").read_text())["parts"]
    wires = {w["id"] for w in yaml.safe_load((R / "docs/PINMAP.yaml").read_text())["wires"]}
    return parts, wires

def totals(L):
    t = {}
    for x in lengths(L):
        a = t.setdefault(x["type"], {"len": 0.0, "cut": 0.0, "runs": 0}); a["len"] += x["len"] * x["n"]; a["cut"] += x["total"]; a["runs"] += 1
    return t

def main(argv):
    js = "--json" in argv; args = [a for a in argv if not a.startswith("--")]
    f = pathlib.Path(args[0]) if args else R / "docs/layout/teryx.layout.json"
    L = json.loads(f.read_text()); parts, wires = load_refs(); E, W, I = check(L, parts, wires)
    if js:
        print(json.dumps({"E": E, "W": W, "I": I, "k": scale_k(L), "lengths": lengths(L), "totals": totals(L)}, ensure_ascii=False)); return 1 if E else 0
    k = scale_k(L); tag = "ESTIMATED" if k == 1 else f"SCALED k={k:.4f}"
    for x in lengths(L): print(f"  {x['id']:<14} {x['type']} {x['gauge']:<22} x{x['n']}  {f1(x['len']):>6} in  cut {f1(x['cut']):>6} in/cond  [{tag}]")
    for ty, a in sorted(totals(L).items()): print(f"  total {ty}: {a['runs']} runs, {f1(a['len'])} in of conductor ({f1(a['len']/12)} ft), cut with slack {f1(a['cut'])} in ({f1(a['cut']/12)} ft) [{tag}]")
    for e in E: print("ERROR", e)
    for w in W: print("WARN ", w)
    for i in I: print("INFO ", i)
    print(f"{f.name}: layout {L.get('layout')} [{L.get('status')}] {len(L.get('blocks', []))} blocks, {len(L.get('runs', []))} runs, {len(L.get('zones', []))} zones — " + ("FAIL" if E else "PASS") + f" ({len(E)} errors, {len(W)} rule warnings, {len(I)} info)")
    return 1 if E else 0

if __name__ == "__main__": sys.exit(main(sys.argv[1:]))
