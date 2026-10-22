"""Shared loader for docs/PINMAP.yaml (single source of truth for pins/wires)."""
import pathlib, json, subprocess, re
ROOT = pathlib.Path(__file__).resolve().parent.parent
YAML = ROOT / "docs/PINMAP.yaml"
STEPS = {1: "12 V + grounds", 2: "Teensys", 3: "CAN buses (transceivers + vehicle bus)", 4: "BICMs",
         5: "ESP32 gateway", 6: "Safety I/O, throttle clamp, LEM, park, Kelly low voltage",
         7: "HIGH VOLTAGE — LATER, Sparky present, string fuses OUT", 8: "Body / aux buttons — LATER"}

def load():
    import yaml
    return yaml.safe_load(YAML.read_text())

def wiring_pages():
    """PAGES from tools/wiring_data.js via node (lines as page|a|b|label)."""
    js = (ROOT / "tools/wiring_data.js").read_text()
    code = "var window={};" + js + ";console.log(JSON.stringify(window.PAGES))"
    out = subprocess.run(["node", "-e", code], capture_output=True, text=True, check=True).stdout
    return json.loads(out)

def dia_index(d):
    idx = {}
    for w in d["wires"]:
        for k in w.get("dia") or []:
            idx.setdefault(k, w["id"])
    return idx

def line_key(pid, w):
    return f"{pid}|{w[0]}|{w[1]}|{w[2] or ''}"
