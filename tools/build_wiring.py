#!/usr/bin/env python3
"""tools/wiring.html = wiring.template.html with /*DATA*/ <- wiring_data.js and @@BUILD_LABEL@@ stamped."""
import os, subprocess, pathlib
R = pathlib.Path(__file__).resolve().parent
lbl = os.environ.get("BUILD_LABEL") or "dev build · " + subprocess.run(["git","rev-parse","--short","HEAD"],cwd=R,capture_output=True,text=True).stdout.strip() + "+"
t = (R/"wiring.template.html").read_text()
(R/"wiring.html").write_text(t.replace("/*DATA*/", (R/"wiring_data.js").read_text(), 1).replace("@@BUILD_LABEL@@", lbl))
