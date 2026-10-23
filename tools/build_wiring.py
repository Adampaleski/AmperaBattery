#!/usr/bin/env python3
import sys as _s, os as _o; _s.path.insert(0, _o.path.dirname(_o.path.abspath(__file__)))
"""tools/wiring.html = wiring.template.html with /*DATA*/ <- wiring_data.js and @@BUILD_LABEL@@ stamped."""
import os, subprocess, pathlib
R = pathlib.Path(__file__).resolve().parent
lbl = os.environ.get("BUILD_LABEL") or "dev build · " + subprocess.run(["git","rev-parse","--short","HEAD"],cwd=R,capture_output=True,text=True).stdout.strip() + "+"
t = (R/"wiring.template.html").read_text()
import json
__import__("build_parts_json").build()  # tools/parts.json + parts.js <- docs/PARTS.yaml
cs = json.loads((R/"cutsheets.json").read_text()); ins = json.loads((R/"install_src.json").read_text())
extra = "\nwindow.CUTSHEETS=" + json.dumps(cs["parts"], ensure_ascii=False) + ";\nwindow.INSTALL=" + json.dumps(ins["pages"], ensure_ascii=False) + ";\n"
# docs/PINMAP.yaml is the single source: every drawn line gets its wire number from it (build fails if one is missing)
from pinmap_lib import load as _pm_load, dia_index, wiring_pages, line_key
_pm = _pm_load(); _idx = dia_index(_pm); _byid = {w["id"]: w for w in _pm["wires"]}; _pages = wiring_pages()
_miss = [line_key(pg["id"], w) for pg in _pages for w in pg["wires"] if line_key(pg["id"], w) not in _idx]
if _miss: raise SystemExit("PINMAP.yaml has no wire number for drawn line(s): " + ", ".join(_miss))
for pg in _pages:
    for w in pg["wires"]:
        wid = _idx[line_key(pg["id"], w)]; w[2] = wid + (" " + w[2] if w[2] else "")
    if not pg.get("net"):  # pages drawn only from PINMAP.yaml
        seen = []
        for w in pg["wires"]:
            y = _byid[w[2].split(" ")[0]]
            if y["id"] in seen: continue
            seen.append(y["id"])
            pg["net"].append([y["label"] + " · " + y["from"], y["to"], y["signal"], f'{y["awg"]} AWG', y["color"], y["src"], {"SOURCED": "S", "PROPOSED": "P"}.get(y["status"], "U")])
_pm_js = "\nwindow.PAGES=" + json.dumps(_pages, ensure_ascii=False) + ";\nwindow.PINMAP=" + json.dumps({"wires": _pm["wires"], "conflicts": _pm["conflicts"]}, ensure_ascii=False) + ";\n"
(R/"wiring.html").write_text(t.replace("/*DATA*/", (R/"wiring_data.js").read_text() + "\n" + (R/"parts.js").read_text() + _pm_js + extra, 1).replace("@@BUILD_LABEL@@", lbl).replace("@@TOPNAV@@", __import__("topnav").topnav("wiring", lbl)))
