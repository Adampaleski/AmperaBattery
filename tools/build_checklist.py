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
        out.append(dict(later=sec.startswith("Later"), s=s, o=o, src=f"BACKLOG.md:{n}", t=(lambda z: z[:1].upper()+z[1:])(plain.replace("REQUIRED BEFORE FIRST DRIVE:", "").strip())))
    return out

def label():
    import os, subprocess
    return os.environ.get("BUILD_LABEL") or "dev build · " + (subprocess.run(["git","rev-parse","--short","HEAD"],cwd=R,capture_output=True,text=True).stdout.strip() or "local") + "+"

CRIT = re.compile(r"e-stop|estop|park|precharge|fuse|BICM|SN65|transceiver|HV build|12 V side|insulation|isolation|Hall|pedal|cool|Kelly config|Kelly CAN|candump|temps vs|thermometer|Elcon stop|silent-string|dead-LEM|LEM|weld|ground strap|axle|pinion|chain|pack mount|cage|strings under|motor temp|Meter transceiver|ECO|Low cap|brake overrides|direction change|charge-plug|MSD|service disconnect|split-LEM|current sense sign|ISR|TDC|DC-DC|Kelly/motor|Temperature limits|placeholder numbers|interlock|12 V low", re.I)
FORCE_CRIT = re.compile(r"NACS|proximity / charger-present|Push \+ flash OK", re.I)   # judgment calls, see report
def tier(x):
    if x.get("tier"): return x["tier"]
    if FORCE_CRIT.search(x["t"]): return "CRITICAL"
    if x.get("later"): return "LATER"
    if x["s"] == 1: return "CRITICAL"
    return "CRITICAL" if CRIT.search(x["t"]) else "LATER"

def norm(t): return re.sub(r"[^a-z0-9]+", " ", t.lower()).strip()

def main():
    cur = json.loads((R / "tools/checklist_src.json").read_text())["items"]
    dups = [re.compile(c["dup"], re.I) for c in cur if c.get("dup")]
    items, seen, acct = [], set(), []
    bl = backlog()
    for it in cur + bl:
        isbl = it in bl
        if isbl and any(d.search(it["t"]) for d in dups): acct.append((it, "dup of a shop-log item")); continue
        k = norm(it["t"])
        if k in seen: acct.append((it, "identical text")); continue
        seen.add(k); it["tier"] = tier(it); items.append(it); acct.append((it, "kept"))
    # FAIL LOUDLY: every open '- [ ]' line in BACKLOG.md must be accounted for (kept or deduped with a reason)
    raw = [n for n, l in enumerate((R / "BACKLOG.md").read_text().splitlines(), 1) if re.match(r"^\s*(\d+\.\s*)?[-*]?\s*\[ \]", l)]
    epic = [n for n, l in enumerate((R / "BACKLOG.md").read_text().splitlines(), 1) if re.match(r"^\s*\d+\.\s*\[ \]", l)]
    seen_lines = {int(x["src"].split(":")[1]) for x, why in acct if x["src"].startswith("BACKLOG.md:")}
    missing = [n for n in raw if n not in seen_lines and n not in epic]
    if missing or len([1 for x, w in acct if w == "kept"]) != len(items):
        sys.exit("build_checklist: FAIL — BACKLOG open lines dropped (not parsed): " + ", ".join(map(str, missing)))
    for x, why in acct:
        if why != "kept": print(f"  dedupe: {x['src']} ({why}): {x['t'][:60]}", file=sys.stderr)
    body, counts = [], []
    for i, name in enumerate(SECTIONS, 1):
        sec = [x for x in items if x["s"] == i]
        counts.append((name, len(sec)))
        rows = []
        for x in sec:
            iid = hashlib.sha1(x["t"].encode()).hexdigest()[:10]
            rows.append(f'<li class="{x["tier"].lower()}"><label><input type="checkbox" data-id="{iid}"><span class="t">{html.escape(x["t"])}</span></label>'
                        f'<div class="m"><span class="tier">{x["tier"]}</span><span class="own">{html.escape(x["o"])}</span><span class="src">{html.escape(x["src"])}</span></div></li>')
        cls = " req" if i == 1 else (" buy" if i == 3 else "")
        body.append(f'<section class="sec{cls}"><h2>{i}. {html.escape(name)} <span class="cnt" data-sec="{i}">0/{len(sec)}</span></h2><ul data-sec="{i}">{"".join(rows)}</ul></section>')
    tpl = (R / "tools/checklist.template.html").read_text()
    (R / "tools/checklist.html").write_text(tpl.replace("<!--BODY-->", "\n".join(body)).replace("<!--TOTAL-->", str(len(items))).replace("@@BUILD_LABEL@@", html.escape(label())))
    md = ["# CRITICAL / LATER tiers (generated by tools/build_checklist.py — do not edit)", "", "CRITICAL = needed for a safe first drive. LATER = refine over time.", ""]
    for i, name in enumerate(SECTIONS, 1):
        md.append(f"## {i}. {name}")
        md += [f"- **{x['tier']}** · {x['t']} ({x['o']}; {x['src']})" for x in items if x["s"] == i]
        md.append("")
    (R / "docs/TIERS.md").write_text("\n".join(md))
    for n, c in counts: print(f"{c:3d}  {n}")
    print(f"{sum(1 for x in items if x['tier']=='CRITICAL'):3d}  CRITICAL  ({sum(1 for x in items if x['tier']=='LATER')} LATER)")
    print(f"{len(items):3d}  total")
main()
