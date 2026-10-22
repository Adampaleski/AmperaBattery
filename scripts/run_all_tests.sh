#!/usr/bin/env bash
# Runs every host test. Exit != 0 if any fails.
cd "$(dirname "$0")/.."
CXX="/workspace/tmp/zigvenv/bin/python -m ziglang c++"
fail=0
run(){ echo "== $1"; if eval "$2" >/tmp/rt_$1.log 2>&1; then tail -2 /tmp/rt_$1.log; else echo "FAILED $1"; tail -15 /tmp/rt_$1.log; fail=1; fi; }
for t in bench_bleed drive_profile estop_latch precharge_gate factory_balance fault_log kelly_link lem_health limp park thresholds throttle_cross; do
  run $t "$CXX -std=c++17 -O1 -Iinclude scripts/test_$t.cpp -o /tmp/rt_$t && /tmp/rt_$t"
done
run soc_run "$CXX -std=c++17 -DDUAL_STRING=1 -DTEENSY_ROLE=0 -Iscripts/hoststub -Iinclude -Isrc scripts/test_soc_run.cpp -o /tmp/rt_soc && /tmp/rt_soc"
run controls "node scripts/test_controls.js"
run wasm "scripts/build_bms_sim_html.sh >/dev/null && node scripts/check_bms_sim_wasm.js /tmp/bms_sim.wasm > /tmp/rt_wasm_out.log 2>&1; ! grep -q '^FAIL' /tmp/rt_wasm_out.log && grep -c '^PASS' /tmp/rt_wasm_out.log"
[ -f scripts/test_pinmap.py ] && run pinmap "python3 scripts/test_pinmap.py"
run review_fixes "python3 scripts/test_review_fixes.py"
if [ -z "$FAST" ]; then
  run wiring_crossings "python3 scripts/test_wiring_crossings.py"
  run dash_ui "python3 scripts/test_dash_ui.py"
fi
exit $fail
