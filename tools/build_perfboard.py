#!/usr/bin/env python3
"""tools/perfboard.html = perfboard.template.html with /*DATA*/ <- docs/boards/*.layout.json (window.BOARDS),
docs/boards/footprints.json (window.FOOTPRINTS) and tools/parts.js (window.PARTS); @@BUILD_LABEL@@ / @@TOPNAV@@ stamped.
The layout JSON in docs/boards/ is the committed source; the page keeps edits in localStorage until Export JSON."""
import json, os, pathlib, subprocess, sys
R = pathlib.Path(__file__).resolve().parent
ROOT = R.parent
sys.path.insert(0, str(R))
lbl = os.environ.get("BUILD_LABEL") or "dev build · " + subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=ROOT, capture_output=True, text=True).stdout.strip() + "+"
if not (R / "parts.js").exists(): __import__("build_parts_json").build()
boards = {}
for f in sorted((ROOT / "docs/boards").glob("*.layout.json")):
    L = json.loads(f.read_text())
    if L.get("board") + ".layout.json" != f.name: raise SystemExit(f"{f.name}: board id {L.get('board')!r} must match the file name")
    boards[L["board"]] = L
fp = json.loads((ROOT / "docs/boards/footprints.json").read_text())
data = ("window.BOARDS=" + json.dumps(boards, ensure_ascii=False) + ";\nwindow.FOOTPRINTS=" + json.dumps(fp, ensure_ascii=False) + ";\n"
        + (R / "parts.js").read_text())
t = (R / "perfboard.template.html").read_text()
(R / "perfboard.html").write_text(t.replace("/*DATA*/", data, 1).replace("@@BUILD_LABEL@@", lbl).replace("@@TOPNAV@@", __import__("topnav").topnav("perfboard", lbl)))
print(f"tools/perfboard.html: {len(boards)} board(s) {sorted(boards)} · {lbl}")
