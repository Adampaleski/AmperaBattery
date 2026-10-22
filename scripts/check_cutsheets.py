#!/usr/bin/env python3
"""Verify every cut-sheet link in tools/cutsheets.json returns HTTP 200 (url, mirror, page) and, for PDFs,
that the part keyword appears in the first pages. Network check: run before publishing a cut-sheet change."""
import json, pathlib, subprocess, sys, tempfile, re
R = pathlib.Path(__file__).resolve().parent.parent
UA = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 Chrome/124 Safari/537.36"
bad = 0
for c in json.loads((R / "tools/cutsheets.json").read_text())["parts"]:
    if c.get("missing"): print("MISSING", c["id"], "-", c["missing"][:80]); continue
    for k in ("url", "mirror", "page"):
        u = c.get(k)
        if not u: continue
        f = tempfile.mktemp()
        code = subprocess.run(["curl", "-sL", "--max-time", "60", "--retry", "2", "-A", UA, "-o", f, "-w", "%{http_code}", u], capture_output=True, text=True).stdout
        body = open(f, "rb").read() if pathlib.Path(f).exists() else b""
        txt = subprocess.run(["pdftotext", "-l", "3", f, "-"], capture_output=True, text=True).stdout if body[:4] == b"%PDF" else body.decode("utf8", "ignore")
        hit = bool(re.search(c.get("kw") or c["match"], txt, re.I))
        ok = code == "200" and hit
        blocked = code == "403" and k == "url" and c.get("mirror")
        print(("PASS" if ok else "BLOCKED(mirror used)" if blocked else "FAIL"), c["id"], k, code, "match" if hit else "no-match", u[:90])
        if not ok and not blocked and k != "page": bad += 1
sys.exit(1 if bad else 0)
