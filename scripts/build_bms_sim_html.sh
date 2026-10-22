#!/usr/bin/env bash
# Build tools/bms_sim.html: compile sim/wasm_api.cpp (BmsCore + BmsSim) to wasm32
# with zig c++, base64-embed it into sim/bms_sim.template.html. No server needed.
#   ZIG="python -m ziglang" scripts/build_bms_sim_html.sh   (pip install ziglang)
set -euo pipefail
cd "$(dirname "$0")/.."
ZIG=${ZIG:-"/workspace/tmp/zigvenv/bin/python -m ziglang"}
OUT=${OUT:-/tmp/bms_sim.wasm}
$ZIG c++ -std=c++17 -target wasm32-wasi -Os -s -fno-exceptions -fno-rtti -mexec-model=reactor \
    -Iinclude -Wall -Wextra -Wl,--export-dynamic sim/wasm_api.cpp -o "$OUT"
python3 - "$OUT" <<'PY'
import base64, sys
b64 = base64.b64encode(open(sys.argv[1], 'rb').read()).decode()
t = open('sim/bms_sim.template.html').read()
t = t.replace('@@TOPNAV@@', '@@TOPNAV_SIM@@')
import subprocess
rev = subprocess.run(['git','rev-parse','--short','HEAD'],capture_output=True,text=True).stdout.strip() or 'local'
import os
sys.path.insert(0,'tools'); from topnav import topnav
LBL = os.environ.get('BUILD_LABEL') or ('dev build · ' + rev + '+')
dj = (open('sim/controls.js').read() + '\n' + open('sim/dash.js').read()).replace('@@BUILD@@', LBL)
cy = open('sim/drive_cycles.js').read()
vj = open('sim/vehicle.js').read()
open('tools/bms_sim.html', 'w').write(t.replace('@@WASM_B64@@', b64).replace('@@DASH_JS@@', dj).replace('@@CYCLES_JS@@', cy).replace('@@VEHICLE_JS@@', vj).replace('@@BUILD_LABEL@@', LBL).replace('@@TOPNAV_SIM@@', topnav('sim', LBL)))
open('tools/dash.html', 'w').write(open('sim/dash.template.html').read().replace('@@TOPNAV@@', topnav('dash', LBL)).replace('@@VEHICLE_JS@@', vj).replace('@@DASH_JS@@', dj).replace('@@BUILD_LABEL@@', LBL))
print('tools/dash.html', len(dj) // 1024, 'KB dash.js')
print('tools/bms_sim.html', len(b64) // 1024, 'KB wasm (base64)')
PY
