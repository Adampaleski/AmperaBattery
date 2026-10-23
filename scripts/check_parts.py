#!/usr/bin/env python3
"""check_parts.py — docs/PARTS.yaml is the one parts list; every tool points at it by explicit part id.

Fails (exit 1) on a dangling id in EITHER direction:
  forward  tool -> PARTS : every part: tag in tools/wiring_data.js nodes, tools/schematic/gen.py PART,
                           docs/PINMAP.yaml connectors/fuses, tools/cutsheets.json id/also_parts
                           must be a PARTS id.
  reverse  PARTS -> tool : every diagram_ids entry (wiring:<page>/<node>, schem:<block>,
                           pinmap:connector|fuse/<id>, cutsheet:<sheet>) must exist AND carry that part;
                           every tool tag must be listed in the part's diagram_ids (else PARTS.yaml is stale:
                           rerun /workspace/teryx/tools_parts/build_parts.py);
                           pinmap_refs must be PINMAP wire/fuse/connector/bus ids;
                           parent/children and id-shaped superseded_by must be PARTS ids;
                           tools/parts.json must equal docs/PARTS.yaml.
Warnings (no fail): a tag pointing at a superseded part, PINMAP part: null (uncatalogued gap).
Reports the conflict count (parts carrying a `conflict:` field).
"""
import ast, json, pathlib, re, subprocess, sys
import yaml

R = pathlib.Path(__file__).resolve().parent.parent
errs, warns = [], []
ids = lambda v: [] if v is None else (v if isinstance(v, list) else [v])

P = yaml.safe_load((R / "docs/PARTS.yaml").read_text())["parts"]
PM = yaml.safe_load((R / "docs/PINMAP.yaml").read_text())
CS = json.loads((R / "tools/cutsheets.json").read_text())["parts"]

# ---- collect every tag: ref -> list of part ids (ref namespace identical to PARTS diagram_ids)
tags, exists = {}, set()
js = ("global.window={};eval(require('fs').readFileSync(process.argv[1],'utf8'));"
      "console.log(JSON.stringify(window.PAGES.map(p=>({id:p.id,nodes:p.nodes.map(n=>[n[0],"
      "(n.slice(7).find(o=>o&&typeof o==='object'&&!Array.isArray(o)&&'part' in o)||{part:null}).part])}))))")
for pg in json.loads(subprocess.run(["node", "-e", js, str(R / "tools/wiring_data.js")],
                                    capture_output=True, text=True, check=True).stdout):
    for nid, pt in pg["nodes"]:
        ref = f"wiring:{pg['id']}/{nid}"; exists.add(ref)
        if pt is not None: tags[ref] = ids(pt)
g = (R / "tools/schematic/gen.py").read_text()
m = re.search(r"^PART = (\{.*?^\})", g, re.S | re.M)
if not m: errs.append("gen.py: no PART dict")
else:
    for k, v in ast.literal_eval(m.group(1)).items():
        exists.add(f"schem:{k}"); tags[f"schem:{k}"] = ids(v)
    # every drawn schematic block must be in PART, or be a known non-part block (headers, rails, legend)
    SCHEM_NONPART = {"hvhdr", "lvhdr", "b12", "v5", "gnd", "pu", "load", "leg"}
    nb = re.search(r"^N = \{(.*?)^\}", g, re.S | re.M)
    pk = ast.literal_eval(m.group(1))
    for b in (re.findall(r'"(\w+)":\s*\(', nb.group(1)) if nb else []):
        if b not in pk and b not in SCHEM_NONPART:
            errs.append(f"schem:{b}: block drawn in gen.py N but missing from PART (tag it, or add to SCHEM_NONPART with a reason)")
for sec, kind in (("connectors", "connector"), ("fuses", "fuse")):
    for r in PM[sec]:
        ref = f"pinmap:{kind}/{r['id']}"; exists.add(ref)
        if "part" not in r: errs.append(f"{ref}: no part: key (use part: null for an uncatalogued item)")
        elif r["part"] is None: warns.append(f"GAP {ref}: part: null (no catalogued part)")
        else: tags[ref] = ids(r["part"])
for c in CS:
    ref = f"cutsheet:{c.get('cutsheet', c['id'])}"; exists.add(ref)
    tags[ref] = [c["id"]] + c.get("also_parts", [])

# ---- forward
superseded = {k for k, v in P.items() if v.get("superseded_by") or v.get("category") == "superseded"}
n_sup = sum(1 for v in P.values() if v.get("category") == "superseded")
nfwd = 0
for ref, pids in sorted(tags.items()):
    for pid in pids:
        nfwd += 1
        if pid not in P: errs.append(f"DANGLING forward: {ref} -> part '{pid}' not in docs/PARTS.yaml")
        else:
            if pid in superseded: warns.append(f"{ref} -> '{pid}' is superseded/dropped (status: {P[pid]['status']}; superseded_by: {P[pid]['superseded_by']})")
            if ref not in (P[pid].get("diagram_ids") or []):
                errs.append(f"STALE: {ref} tags '{pid}' but PARTS.yaml {pid}.diagram_ids lacks it — rerun build_parts.py")

# ---- reverse
pin_ids = {x["id"] for s in ("wires", "fuses", "connectors", "buses") for x in PM.get(s, [])}
nrev = 0
for pid, p in sorted(P.items()):
    for ref in p.get("diagram_ids") or []:
        nrev += 1
        if ref not in exists: errs.append(f"DANGLING reverse: {pid}.diagram_ids '{ref}' does not exist")
        elif pid not in tags.get(ref, []): errs.append(f"DANGLING reverse: {pid}.diagram_ids '{ref}' exists but does not carry part '{pid}'")
    for ref in p.get("pinmap_refs") or []:
        nrev += 1
        if ref not in pin_ids: errs.append(f"DANGLING reverse: {pid}.pinmap_refs '{ref}' not a PINMAP wire/fuse/connector/bus id")
    if p.get("parent") and p["parent"] not in P: errs.append(f"DANGLING: {pid}.parent '{p['parent']}'")
    for ch in p.get("children") or []:
        cid = ch["id"] if isinstance(ch, dict) else ch
        if cid not in P: errs.append(f"DANGLING: {pid}.children '{cid}'")
        elif P[cid].get("parent") != pid: errs.append(f"{pid}.children '{cid}' but {cid}.parent = {P[cid].get('parent')}")
    sb = p.get("superseded_by")
    if sb and re.fullmatch(r"[a-z0-9_]+", str(sb)) and sb not in P: errs.append(f"DANGLING: {pid}.superseded_by '{sb}'")

# ---- generated copies in sync
pj = json.loads((R / "tools/parts.json").read_text())
if pj["parts"] != json.loads(json.dumps(P, default=str)):
    errs.append("tools/parts.json != docs/PARTS.yaml — rerun tools/build_parts_json.py (build_wiring.py does it)")

conf = sorted(k for k, v in P.items() if v.get("conflict"))
untagged = sorted(k for k, v in P.items() if not v.get("diagram_ids") and k not in superseded)
for w in warns: print("WARN", w)
for e in errs: print("FAIL", e)
print(f"parts {len(P)} ({len(P)-n_sup} active, {n_sup} superseded) · tags {len(tags)} refs "
      f"({nfwd} part links) · reverse refs {nrev} · conflicts {len(conf)}: {', '.join(conf)} · "
      f"active parts on no diagram {len(untagged)}")
print(("FAIL" if errs else "PASS") + f" check_parts: {len(errs)} errors, {len(warns)} warnings")
sys.exit(1 if errs else 0)
