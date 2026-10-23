#!/usr/bin/env python3
"""docs/PINMAP.yaml -> docs/PINMAP.md (master pin map) + docs/CONNECTIONS.md (printable build checklist).
Run after every edit to PINMAP.yaml. Never edit the .md files by hand."""
import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pinmap_lib import load, ROOT, STEPS

def esc(x):
    return str("" if x is None else x).replace("|", "\\|").replace("\n", " ")

def table(rows, cols):
    o = ["| " + " | ".join(c[0] for c in cols) + " |", "|" + "---|" * len(cols)]
    for r in rows:
        o.append("| " + " | ".join(esc(r.get(c[1])) for c in cols) + " |")
    return "\n".join(o)

_PARTS = None
def part_name(pid):
    """Part column: name + model pulled from docs/PARTS.yaml by the explicit part: id (never by text)."""
    global _PARTS
    if _PARTS is None:
        import yaml
        _PARTS = yaml.safe_load((ROOT / "docs" / "PARTS.yaml").read_text())["parts"]
    if pid is None:
        return "— (no catalogued part)"
    out = []
    for i in (pid if isinstance(pid, list) else [pid]):
        p = _PARTS.get(i)
        out.append(f"{p['name']} — {p.get('model') or 'UNKNOWN'} [`{i}`]" if p else f"MISSING PART `{i}`")
    return "; ".join(out)

def with_part(rows):
    return [dict(r, part_name=part_name(r.get("part"))) for r in rows]

def pinmap_md(d):
    o = ["# Master pin map (GENERATED — edit docs/PINMAP.yaml, then `python3 tools/build_pinmap.py`)", "",
         "Status: SOURCED (file:line or doc) · ASSUMED · PROPOSED · UNVERIFIED · UNDEFINED (nothing chosen yet).",
         "Checked against `include/Config.h` and `include/FlexCanCompat.h` by `scripts/test_pinmap.py`.", ""]
    for k, v in d["meta"]["boards"].items():
        o.append(f"- **{k}**: {v}")
    o += ["", "## Pin conflicts and undefined pins (fix or accept before wiring)", ""]
    o += [f"{i}. {c}" for i, c in enumerate(d["conflicts"], 1)]
    for b, title in [("A", "Teensy A"), ("B", "Teensy B"), ("E", "ESP32 gateway")]:
        o += ["", f"## {title} pins", "", table(d["pins"][b], [("Pin", "pin"), ("Firmware name", "const"), ("Use", "use"), ("Wire", "wire"), ("Tier", "tier"), ("Status", "status"), ("Source", "src")])]
    o += ["", "## MCP23017 body expander bits (Config.h ExpBits::)", "", table(d.get("expander", []), [("Bit", "bits"), ("Firmware name", "const"), ("Use", "use"), ("Tier", "tier"), ("Status", "status"), ("Source", "src")])]
    o += ["", "## CAN buses", "", table(d["buses"], [("Bus", "id"), ("Board", "board"), ("Ctrl", "ctrl"), ("TX", "tx"), ("RX", "rx"), ("Bitrate", "bitrate"), ("Nodes", "nodes"), ("IDs", "ids"), ("Termination", "term"), ("Transceiver", "transceiver"), ("Status", "status"), ("Source", "src")])]
    o += ["", "## Connectors", "", table(with_part(d["connectors"]), [("ID", "id"), ("Part (docs/PARTS.yaml)", "part_name"), ("What", "what"), ("Pins", "pins"), ("Status", "status"), ("Source", "src")])]
    o += ["", "## Fuses", "", table(with_part(d["fuses"]), [("ID", "id"), ("Part (docs/PARTS.yaml)", "part_name"), ("Where", "where"), ("Rating", "rating"), ("Status", "status"), ("Source", "src")])]
    o += ["", "## Wire colour code — PROPOSED", ""] + [f"- {k}: {v}" for k, v in d["colors"].items() if k != "status"]
    o += ["", "## Wire gauge scheme — PROPOSED", ""] + [f"- {k}: {v}" for k, v in d["gauges"].items() if k != "status"]
    o += ["", "## Every wire", "", table(sorted(d["wires"], key=lambda w: (w["step"], w["id"])), [("Wire", "id"), ("Step", "step"), ("Kind", "kind"), ("From", "from"), ("To", "to"), ("Signal", "signal"), ("Voltage", "voltage"), ("Colour", "color"), ("AWG", "awg"), ("Fuse", "fuse"), ("Label", "label"), ("Tier", "tier"), ("Status", "status"), ("Source", "src")])]
    unv = [w for w in d["wires"] if w["status"] in ("UNVERIFIED", "UNDEFINED")]
    o += ["", f"## Open UNVERIFIED / UNDEFINED wires ({len(unv)})", ""] + [f"- **{w['id']}** {w['status']}: {w['from']} → {w['to']} ({w['src']})" for w in unv]
    return "\n".join(o) + "\n"

def connections_md(d):
    o = ["# Connection checklist (GENERATED from docs/PINMAP.yaml — print me)", "",
         "Build order: one wire at a time. Tick the box only after the meter check passes. Write the label on heat-shrink at BOTH ends.",
         "Nothing HV (step 7) without Sparky present and all 4 string fuses OUT. Status in brackets: anything not SOURCED needs a second look.", ""]
    for s in sorted(STEPS):
        ws = [w for w in d["wires"] if w["step"] == s and w["kind"] != "none"]
        if not ws: continue
        o += [f"## Step {s}: {STEPS[s]}", ""]
        for w in sorted(ws, key=lambda w: w["id"]):
            o.append(f"- [ ] **{w['label']}** ({w['tier']}, {w['status']}) — {w['from']} → {w['to']} · {w['signal']} · {w['awg']} AWG {w['color']} · fuse {w['fuse']}  \n  Meter: {w['check']}")
        o.append("")
    return "\n".join(o)

def main():
    d = load()
    (ROOT / "docs/PINMAP.md").write_text(pinmap_md(d))
    (ROOT / "docs/CONNECTIONS.md").write_text(connections_md(d))
    print("wrote docs/PINMAP.md, docs/CONNECTIONS.md")

if __name__ == "__main__":
    main()
