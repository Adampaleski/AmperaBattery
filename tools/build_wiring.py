#!/usr/bin/env python3
"""tools/wiring.html = wiring.template.html with /*DATA*/ <- wiring_data.js and @@BUILD_LABEL@@ stamped."""
import os, subprocess, pathlib
R = pathlib.Path(__file__).resolve().parent
lbl = os.environ.get("BUILD_LABEL") or "dev build · " + subprocess.run(["git","rev-parse","--short","HEAD"],cwd=R,capture_output=True,text=True).stdout.strip() + "+"
t = (R/"wiring.template.html").read_text()
import json
cs = json.loads((R/"cutsheets.json").read_text()); ins = json.loads((R/"install_src.json").read_text())
extra = "\nwindow.CUTSHEETS=" + json.dumps(cs["parts"], ensure_ascii=False) + ";\nwindow.INSTALL=" + json.dumps(ins["pages"], ensure_ascii=False) + ";\n"
(R/"wiring.html").write_text(t.replace("/*DATA*/", (R/"wiring_data.js").read_text() + extra, 1).replace("@@BUILD_LABEL@@", lbl))
