#!/usr/bin/env python3
"""Build tools/checklist.html from BACKLOG.md (open '- [ ]' items) + tools/checklist_src.json
(curated OPEN items from teryx-shop-log.md). Dedupe: a curated item's 'dup' regex drops the
matching BACKLOG item; identical normalized text is kept once. Run: python3 tools/build_checklist.py"""
import json, re, html, hashlib, pathlib, sys
R = pathlib.Path(__file__).resolve().parent.parent
SECTIONS = ["REQUIRED BEFORE FIRST DRIVE", "Shop tasks", "Buy list — needs Adam OK (never buy)",
            "Firmware / design TODO", "Bench verifications", "Decisions needed from Adam"]

def backlog():
    out, sec = [], ""
    for n, line in enumerate((R / "BACKLOG.md").read_text().splitlines(), 1):
        if line.startswith("## "): sec = line[3:].strip(); continue
        if sec.startswith("Later"): continue
        m = re.match(r"^\s*- \[ \] (.*)$", line)
        if not m: continue
        t = m.group(1).strip()
        plain = re.sub(r"[*`]", "", t)
        if "REQUIRED BEFORE FIRST DRIVE" in plain: s = 1
        elif sec.startswith("Mechanical") or plain.startswith("Service disconnect"): s = 2
        elif re.match(r"(Adam|Pick|Temperature limits|Derate hardware|\(Optional\))", plain): s = 6
        elif re.search(r"\b(bench|datasheet|measure)\b", plain, re.I) and "firmware" not in plain.lower(): s = 5
        else: s = 4
        if re.search(r"Adam/Sparky", plain): o = "Adam + Sparky"
        elif re.search(r"ask Sparky|\(Sparky\)", plain): o = "Sparky"
        elif s == 2 or "Mechanic" in sec: o = "Mechanic"
        elif s == 6: o = "Adam"
        elif re.search(r"isolation monitoring|MSD rating", plain): o = "Sparky"
        else: o = "Gilfoyle"
        out.append(dict(s=s, o=o, src=f"BACKLOG.md:{n}", t=(lambda z: z[:1].upper()+z[1:])(plain.replace("REQUIRED BEFORE FIRST DRIVE:", "").strip())))
    return out

def label():
    import os, subprocess
    return os.environ.get("BUILD_LABEL") or "dev build · " + (subprocess.run(["git","rev-parse","--short","HEAD"],cwd=R,capture_output=True,text=True).stdout.strip() or "local") + "+"

def norm(t): return re.sub(r"[^a-z0-9]+", " ", t.lower()).strip()

def main():
    cur = json.loads((R / "tools/checklist_src.json").read_text())["items"]
    dups = [re.compile(c["dup"], re.I) for c in cur if c.get("dup")]
    items, seen = [], set()
    for it in cur + [b for b in backlog() if not any(d.search(b["t"]) for d in dups)]:
        k = norm(it["t"])
        if k in seen: continue
        seen.add(k); items.append(it)
    body, counts = [], []
    for i, name in enumerate(SECTIONS, 1):
        sec = [x for x in items if x["s"] == i]
        counts.append((name, len(sec)))
        rows = []
        for x in sec:
            iid = hashlib.sha1(x["t"].encode()).hexdigest()[:10]
            rows.append(f'<li><label><input type="checkbox" data-id="{iid}"><span class="t">{html.escape(x["t"])}</span></label>'
                        f'<div class="m"><span class="own">{html.escape(x["o"])}</span><span class="src">{html.escape(x["src"])}</span></div></li>')
        cls = " req" if i == 1 else (" buy" if i == 3 else "")
        body.append(f'<section class="sec{cls}"><h2>{i}. {html.escape(name)} <span class="cnt" data-sec="{i}">0/{len(sec)}</span></h2><ul data-sec="{i}">{"".join(rows)}</ul></section>')
    tpl = (R / "tools/checklist.template.html").read_text()
    (R / "tools/checklist.html").write_text(tpl.replace("<!--BODY-->", "\n".join(body)).replace("<!--TOTAL-->", str(len(items))).replace("@@BUILD_LABEL@@", html.escape(label())))
    for n, c in counts: print(f"{c:3d}  {n}")
    print(f"{len(items):3d}  total")
main()
