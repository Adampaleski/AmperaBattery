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
dj = open('sim/dash.js').read()
cy = open('sim/drive_cycles.js').read()
open('tools/bms_sim.html', 'w').write(t.replace('@@WASM_B64@@', b64).replace('@@DASH_JS@@', dj).replace('@@CYCLES_JS@@', cy))
open('tools/dash.html', 'w').write(open('sim/dash.template.html').read().replace('@@DASH_JS@@', dj))
print('tools/dash.html', len(dj) // 1024, 'KB dash.js')
print('tools/bms_sim.html', len(b64) // 1024, 'KB wasm (base64)')
PY
