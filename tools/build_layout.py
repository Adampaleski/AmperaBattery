#!/usr/bin/env python3
"""tools/layout.html = layout.template.html with /*DATA*/ <- docs/layout/*.layout.json (window.LAYOUTS),
docs/PINMAP.yaml wire summary (window.PM_WIRES) and tools/parts.js (window.PARTS); @@BUILD_LABEL@@ / @@TOPNAV@@ stamped.
The layout JSON in docs/layout/ is the committed source; the page keeps edits in localStorage until Export JSON."""
import json, os, pathlib, subprocess, sys
import yaml
R = pathlib.Path(__file__).resolve().parent
ROOT = R.parent
sys.path.insert(0, str(R))
lbl = os.environ.get("BUILD_LABEL") or "dev build · " + subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=ROOT, capture_output=True, text=True).stdout.strip() + "+"
if not (R / "parts.js").exists(): __import__("build_parts_json").build()
lays = {}
for f in sorted((ROOT / "docs/layout").glob("*.layout.json")):
    L = json.loads(f.read_text())
    if L.get("layout") + ".layout.json" != f.name: raise SystemExit(f"{f.name}: layout id {L.get('layout')!r} must match the file name")
    lays[L["layout"]] = L
if "teryx" not in lays: raise SystemExit("docs/layout/teryx.layout.json missing (seed it with tools/layout/gen_teryx_layout.py)")
pm = {w["id"]: {k: w.get(k) for k in ("from", "to", "awg", "kind", "tier", "status", "signal")} for w in yaml.safe_load((ROOT / "docs/PINMAP.yaml").read_text())["wires"]}
data = ("window.LAYOUTS=" + json.dumps(lays, ensure_ascii=False) + ";\nwindow.PM_WIRES=" + json.dumps(pm, ensure_ascii=False) + ";\n" + (R / "parts.js").read_text())
t = (R / "layout.template.html").read_text()
(R / "layout.html").write_text(t.replace("/*DATA*/", data, 1).replace("@@BUILD_LABEL@@", lbl).replace("@@TOPNAV@@", __import__("topnav").topnav("layout", lbl)))
print(f"tools/layout.html: {len(lays)} layout(s) {sorted(lays)} · {len(pm)} PINMAP wires · {lbl}")
