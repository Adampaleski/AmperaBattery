#!/usr/bin/env python3
"""check_boards.py — perf-board layouts (docs/boards/<board>.layout.json, edited in tools/perfboard.html).

Usage: check_boards.py [layout.json ...]   (default: every docs/boards/*.layout.json)

Model (same as the page's live check, tools/perfboard.template.html checkBoard()):
  * 0.1 in grid, holes "A1".."Z36" (rows letters, cols numbers). Holes are plated through, so a hole is one node.
  * A part pin sits in a hole (footprint from docs/boards/footprints.json, rot 0/90/180/270 clockwise).
  * A wire is a polyline of holes; it is soldered at EVERY vertex, so all its vertices are one node.
  * Copper connectivity = union of holes joined by wires. Nets are declared as REF.PIN member lists.
ERRORS (exit 1): part id not in docs/PARTS.yaml (parts, board_part, assembly, offboard), duplicate refdes,
  unknown footprint/rot, hole off the grid or on a mount hole, two pins in one hole, net member with an
  unknown refdes/pin, a pin in two nets, a net whose members are not all connected, a short (two nets in one
  copper island), a wire tagged with an undeclared net or touching no member of its net, an NC pin with copper.
WARNINGS: pin in no net and not NC, dangling wire end (no pin, no other wire), same-side crossing of two
  different nets, a wire running over a pin hole it is not soldered at, part not the assembly or one of its PARTS children/uses, status not one of the labels.
"""
import json, math, pathlib, re, sys
from math import gcd
import yaml

R = pathlib.Path(__file__).resolve().parent.parent
FP = json.loads((R / "docs/boards/footprints.json").read_text())
PARTS = yaml.safe_load((R / "docs/PARTS.yaml").read_text())["parts"]  # id -> part
LABELS = {"SOURCED", "ASSUMED", "PROPOSED", "UNVERIFIED", "DECIDED"}

def hole(s):
    m = re.fullmatch(r"([A-Z]+)(\d+)", str(s))
    if not m: return None
    r = 0
    for ch in m.group(1): r = r * 26 + (ord(ch) - 64)
    return (r - 1, int(m.group(2)))
def hname(rc):
    r, c = rc; s = ""; r += 1
    while r: r, k = divmod(r - 1, 26); s = chr(65 + k) + s
    return s + str(c)
def rot(dr, dc, a):
    return {0: (dr, dc), 90: (dc, -dr), 180: (-dr, -dc), 270: (-dc, dr)}[a]
def pins_of(p):
    a = hole(p["at"]); f = FP[p["fp"]]
    return {n: (a[0] + rot(dr, dc, p.get("rot", 0))[0], a[1] + rot(dr, dc, p.get("rot", 0))[1]) for n, dr, dc in f["pins"]}

def check(L, name):
    E, W = [], []
    rows, cols = L["grid"]["rows"], L["grid"]["cols"]
    inb = lambda rc: rc is not None and 0 <= rc[0] < rows and 1 <= rc[1] <= cols
    mounts = {hole(m) for m in L.get("mounts", [])}
    if L.get("status") not in LABELS: W.append(f"status {L.get('status')!r} is not one of {sorted(LABELS)}")
    for k in ("assembly", "board_part"):
        if L.get(k) and L[k] not in PARTS: E.append(f"{k} {L[k]!r} is not a PARTS id")
    kids = {L.get("assembly"), L.get("board_part")} | {c["id"] for c in (PARTS.get(L.get("assembly"), {}).get("children") or [])} | set(PARTS.get(L.get("assembly"), {}).get("uses") or [])
    pin_at, at_pin, refs = {}, {}, {}
    for p in L["parts"]:
        ref = p.get("ref")
        if ref in refs: E.append(f"duplicate refdes {ref}"); continue
        refs[ref] = p
        if p.get("part") not in PARTS: E.append(f"{ref}: part id {p.get('part')!r} is not in docs/PARTS.yaml")
        elif L.get("assembly") and p["part"] not in kids: W.append(f"{ref}: {p['part']} is not {L['assembly']} or one of its children")
        if p.get("fp") not in FP or str(p["fp"]).startswith("_"): E.append(f"{ref}: unknown footprint {p.get('fp')!r}"); continue
        if p.get("rot", 0) not in (0, 90, 180, 270): E.append(f"{ref}: rot {p.get('rot')} not 0/90/180/270"); continue
        if hole(p.get("at")) is None: E.append(f"{ref}: bad anchor hole {p.get('at')!r}"); continue
        for n, rc in pins_of(p).items():
            if not inb(rc): E.append(f"{ref}.{n} at {hname(rc)} is off the {rows}x{cols} grid"); continue
            if rc in mounts: E.append(f"{ref}.{n} sits on mount hole {hname(rc)}")
            if rc in pin_at: E.append(f"{ref}.{n} and {pin_at[rc]} share hole {hname(rc)}")
            pin_at[rc] = f"{ref}.{n}"; at_pin[f"{ref}.{n}"] = rc
    for o in L.get("offboard", []):
        if o.get("part") not in PARTS: E.append(f"offboard {o.get('ref')}: part id {o.get('part')!r} is not in docs/PARTS.yaml")
    # copper islands
    par = {}
    def f(x):
        par.setdefault(x, x)
        while par[x] != x: par[x] = par[par[x]]; x = par[x]
        return x
    def u(a, b): par[f(a)] = f(b)
    for rc in pin_at: f(rc)
    touch = {}  # hole -> wire ids
    nets = L.get("nets", {})
    for w in L.get("wires", []):
        pts = [hole(h) for h in w.get("pts", [])]
        if len(pts) < 2: E.append(f"wire {w.get('id')}: needs 2+ holes"); continue
        bad = [h for h, rc in zip(w["pts"], pts) if not inb(rc)]
        if bad: E.append(f"wire {w.get('id')}: hole(s) {bad} off the grid"); continue
        for rc in pts:
            if rc in mounts: E.append(f"wire {w.get('id')}: vertex on mount hole {hname(rc)}")
            u(rc, pts[0]); touch.setdefault(rc, set()).add(w["id"])
        if w.get("side", "top") not in ("top", "bot"): E.append(f"wire {w['id']}: side must be top|bot")
        if w.get("net") not in nets: E.append(f"wire {w['id']}: net {w.get('net')!r} is not declared")
    # nets
    pin_net, nc = {}, set(L.get("nc", []))
    island_nets = {}
    for nn, nd in nets.items():
        mem = nd.get("members", [])
        isl = set()
        for m in mem:
            if m not in at_pin:
                ref = m.rsplit(".", 1)[0]
                E.append(f"net {nn}: member {m} — " + ("unknown refdes " + ref if ref not in refs else "no such pin")); continue
            if m in pin_net: E.append(f"{m} is in nets {pin_net[m]} and {nn}")
            pin_net[m] = nn; isl.add(f(at_pin[m]))
        if len(isl) > 1:
            groups = {}
            for m in mem:
                if m in at_pin: groups.setdefault(f(at_pin[m]), []).append(m)
            E.append(f"net {nn} is NOT connected: {len(isl)} islands " + " | ".join(",".join(g) for g in groups.values()))
        for i in isl: island_nets.setdefault(i, set()).add(nn)
    for i, ns in island_nets.items():
        if len(ns) > 1: E.append(f"SHORT: nets {sorted(ns)} share one copper island")
    for w in L.get("wires", []):
        if w.get("net") in nets and len(w.get("pts", [])) >= 2 and inb(hole(w["pts"][0])):
            i = f(hole(w["pts"][0]))
            if w["net"] not in island_nets.get(i, set()): E.append(f"wire {w['id']} ({w['net']}) touches no member of net {w['net']}")
    for m in nc:
        if m not in at_pin: E.append(f"nc {m}: no such pin"); continue
        if at_pin[m] in touch: E.append(f"{m} is NC but has a wire")
        if m in pin_net: E.append(f"{m} is NC and in net {pin_net[m]}")
    for m in at_pin:
        if m not in pin_net and m not in nc: W.append(f"{m} is in no net and not marked nc")
    # dangling ends
    for w in L.get("wires", []):
        for h in (w.get("pts") or [])[:: max(1, len(w.get("pts") or [1]) - 1)]:
            rc = hole(h)
            if rc and rc not in pin_at and len(touch.get(rc, ())) < 2: W.append(f"wire {w['id']}: end {h} is dangling (no pin, no other wire)")
    # same-side crossings
    def segs(w): P = [hole(h) for h in w["pts"]]; return list(zip(P, P[1:]))
    def cross(a, b, c, d):
        o = lambda p, q, r: (q[1]-p[1])*(r[0]-p[0]) - (q[0]-p[0])*(r[1]-p[1])
        if {a, b} & {c, d}: return False
        d1, d2, d3, d4 = o(c, d, a), o(c, d, b), o(a, b, c), o(a, b, d)
        if ((d1 > 0) != (d2 > 0)) and d1 and d2 and ((d3 > 0) != (d4 > 0)) and d3 and d4: return True
        on = lambda p, q, r: min(p[0], q[0]) <= r[0] <= max(p[0], q[0]) and min(p[1], q[1]) <= r[1] <= max(p[1], q[1])
        return any(x == 0 and on(*s) for x, s in ((d1, (c, d, a)), (d2, (c, d, b)), (d3, (a, b, c)), (d4, (a, b, d))))
    WS = [w for w in L.get("wires", []) if len(w.get("pts", [])) >= 2 and all(inb(hole(h)) for h in w["pts"])]
    for i, a in enumerate(WS):
        for b in WS[i + 1:]:
            if a.get("side", "top") != b.get("side", "top") or a.get("net") == b.get("net"): continue
            if any(cross(*s, *t) for s in segs(a) for t in segs(b)):
                W.append(f"wires {a['id']} ({a['net']}) and {b['id']} ({b['net']}) cross on the {a.get('side','top')} side")
    # a wire running straight over a pin hole it is not soldered to (lead/solder collision)
    from math import gcd
    for w in WS:
        for a, b in segs(w):
            dr, dc = b[0] - a[0], b[1] - a[1]; g = gcd(abs(dr), abs(dc))
            for k in range(1, g):
                rc = (a[0] + dr // g * k, a[1] + dc // g * k)
                if rc in pin_at: W.append(f"wire {w['id']} runs over {pin_at[rc]} at {hname(rc)} without a solder point")
    # install steps (Build 25; same rules as checkBoard in tools/perfboard.html)
    if isinstance(L.get("steps"), list):
        ids, ps, ws = set(), {}, {}
        wid = {w["id"] for w in L.get("wires", [])}; ob = {o["ref"]: o for o in L.get("offboard", [])}
        for st in L["steps"]:
            sid = st.get("id")
            if not sid or sid in ids: E.append(f"install step {sid!r}: missing or duplicate id")
            ids.add(sid)
            if st.get("label") not in LABELS: W.append(f"install step {sid}: label {st.get('label')!r} is not one of {sorted(LABELS)}")
            for r in st.get("parts", []):
                if r not in refs: E.append(f"install step {sid}: unknown part {r}")
                elif r in ps: E.append(f"install step {sid}: {r} is already in step {ps[r]}")
                ps[r] = sid
            for x in st.get("wires", []):
                if x not in wid: E.append(f"install step {sid}: unknown wire {x}")
                elif x in ws: E.append(f"install step {sid}: wire {x} is already in step {ws[x]}")
                ws[x] = sid
            for r in st.get("offboard", []):
                if r not in ob: E.append(f"install step {sid}: unknown off-board item {r}")
            for i in st.get("part_ids", []):
                if i not in PARTS: E.append(f"install step {sid}: part id {i!r} is not in docs/PARTS.yaml")
            need = sorted({refs[r]["part"] for r in st.get("parts", []) if r in refs} | {ob[r]["part"] for r in st.get("offboard", []) if r in ob})
            have = sorted(set(st.get("part_ids", [])))
            if need != have: W.append(f"install step {sid}: part_ids {have} do not match its parts {need}")
        for r in refs:
            if r not in ps: W.append(f"{r} is in no install step")
        for w in L.get("wires", []):
            if w["id"] not in ws: W.append(f"wire {w['id']} is in no install step")
    return E, W

def body_cells(p):
    f = FP.get(p.get("fp")); a = hole(p.get("at"))
    if not f or a is None or p.get("rot", 0) not in (0, 90, 180, 270): return []
    x0, y0, x1, y1 = f.get("body") or [-.5, -.5, .5, .5]
    out = []
    for r in range(math.ceil(y0), math.floor(y1) + 1):
        for c in range(math.ceil(x0), math.floor(x1) + 1):
            dr, dc = rot(r, c, p.get("rot", 0)); out.append((a[0] + dr, a[1] + dc))
    return out

def wire_cells(w):
    P = [hole(h) for h in w.get("pts", [])]
    if not P or any(x is None for x in P): return []
    o = list(P)
    for a, b in zip(P, P[1:]):
        dr, dc = b[0] - a[0], b[1] - a[1]; g = gcd(abs(dr), abs(dc))
        o += [(a[0] + dr // g * k, a[1] + dc // g * k) for k in range(1, g)]
    return o

def flags(L):
    """Under-body flags: a top-side wire hole under a part body (not at one of its pins). The layout may already have
    these (the designer rejects any NEW one); they are listed, not counted as warnings."""
    body, out = {}, []
    for p in L["parts"]:
        if p.get("fp") not in FP or hole(p.get("at")) is None: continue
        pk = set(pins_of(p).values())
        for rc in body_cells(p):
            if rc not in pk: body[rc] = p["ref"]
    for w in L.get("wires", []):
        if w.get("side", "top") != "top": continue
        hit = {}
        for rc in wire_cells(w):
            if rc in body: hit.setdefault(body[rc], set()).add(hname(rc))
        for r in sorted(hit): out.append(f"wire {w['id']} runs under the {r} body at {','.join(sorted(hit[r]))} (existing layout; a new move can't do this)")
    return out

def main(argv):
    files = [pathlib.Path(a) for a in argv] or sorted((R / "docs/boards").glob("*.layout.json"))
    if not files: print("check_boards: no layouts"); return 1
    nerr = 0
    for fn in files:
        L = json.loads(fn.read_text())
        E, W = check(L, fn.name); F = flags(L)
        nerr += len(E)
        print(f"{fn.name}: board {L.get('board')} [{L.get('status')}] {len(L['parts'])} parts, {len(L.get('wires', []))} wires, "
              f"{len(L.get('nets', {}))} nets, {len(L.get('steps', []))} install steps — {'PASS' if not E else 'FAIL'} ({len(E)} errors, {len(W)} warnings; {len(F)} under-body flags)")
        for e in E: print("  ERROR", e)
        for w in W: print("  warn ", w)
        for x in F: print("  flag ", x)
    return 1 if nerr else 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
