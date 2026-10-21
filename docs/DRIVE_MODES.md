# Drive modes = pedal-map profiles (Adam approved 2026-10-21) — CREW REVIEW before any flash

Source of truth: `include/DriveProfile.h` (firmware, wasm sim, tests); `sim/vehicle.js` mirrors it (parity-checked).
Every value is **PROPOSED** and adjustable (firmware: `MP <key> expo|cur|ramp|top|regen <v>`, RAM only; sim: Settings → Drive modes).

| Mode | Curve (expo) | Current (pedal cap) | Ramp A/s | Top mph | Regen % |
|---|---|---|---|---|---|
| High H | 0 linear | 100 % | 2000 | 49 (= Kelly 6000 RPM cap) | 100 |
| Medium M | 0.3 | 70 % | 1000 | 36 | 80 |
| Low L | 0.5 | 45 % | 600 | 22 | 60 |
| Snow | 0.8 soft | 40 % | 150 slow | 25 | 25 gentle |
| Mud | 0 (strongest the trim allows) | 90 % | 2500 fast | 30 | 20 light |
| Sand | 0 | 100 % | 3000 fast | 35 | 15 light |
| Kid | 0.7 | 20 % | 60 very soft | 10 | 30 |
| Reverse (sim) | 0.3 | 25 % | 500 | 9 | 30 |

## Firmware: reduce-only throttle trim (`src/ThrottleTrim.*`, `THROTTLE_TRIM_ENABLE=0`)
- MCP4131 as a divider in the throttle wiper: Kelly sees pedal × wiper/128, so the trim can only **lower** the pedal. Wiper rounds down.
- 1 kHz IntervalTimer: pedal ADC on pin 24 (A10, PROPOSED, shares HV_SENSE: compile error if HV_ANALOG_ENABLE=1), 5 ms one-pole filter, curve y=(1−e)x+e·x³, × cap, × BMS allowance (kellyPct), top-speed taper over the last 2 mph (speed from Kelly RPM 0x0CF11E05 × 0.008161 mph/RPM), ramp-limited rise, instant release, clamp ≤ filtered and ≤ raw pedal.
- Output 0 (wiper 0): contactors not Closed, any drive fault, Kid without a valid speed, trim disabled. `begin()` is the first line of `setup()`, so after a watchdog reset the wiper is written 0 before anything else.
- Kid: can't be entered until a PIN is set (`MPIN 0 <1000-9999>`, change: `MPIN <old> <new>`, not while in Kid); leaving needs `M <key> <pin>`.
- EEPROM (chief 2026-10-21, `include/DriveStore.h`): profiles + current mode + Kid PIN hash at 1310–1462, CRC-16; boot restores the last mode (Kid stays Kid). Blank/corrupt/out-of-bounds → PROPOSED defaults, High, no PIN. The PIN is stored only as a salted, iterated FNV-1a hash and never printed (`d` shows `kidPin=set/none`). A 4-digit PIN can still be brute-forced from the EEPROM bytes (10^4 values).
- ON HOLD (Adam to decide: chief proposes pedal direct to Kelly, no digipot): hardware default-off clamp + alive one-shot, and moving the pedal input off pin 24. See KELLY_HPD.md for HPD.
- `d` prints mode, pedal, output, wiper, max ISR period and compute µs.

## Limits / UNVERIFIED (crew)
- **MCP4131 powers up at mid-scale (50 % pass-through)** until `begin()` runs, and holds its last wiper if the Teensy hangs before the watchdog bites (~2 s). Needs a hardware default-off (e.g. pull the wiper path through a relay/FET enabled by the Teensy) — not designed yet.
- Wiper topology (pot across pedal signal / ground) and the Kelly throttle input range are UNVERIFIED on the harness.
- Bit-banged on LIGHT_DATA/LIGHT_CLK with own CS: future 595 light code must not shift while the ISR writes (`g_busLock`).
- "Strong low end" cannot be added by a reduce-only trim; Mud/Sand are linear. Real boost would be a Kelly throttle-curve setting.
- Current limit is a pedal cap (torque request proxy), not measured amps; regen strength is sim-only (firmware does not drive Brake_AN yet).
- Top speed needs Kelly RPM (`BMS_CAP_KELLY_RX=0` today): without it, only Kid is enforced (output 0).
- Latency: host test 0.01 µs/tick; on the Teensy, analogRead ~17 µs + bit-bang ~4 µs (estimate) — measure `maxPeriodUs` < 2000 on the bench.

Tests: `scripts/test_drive_profile.cpp` (30 checks: curves, top speeds, trim ≤ pedal over 1.4 M random steps, wiper, fault → 0, ramps, Kid PIN, bounds, latency), wasm parity + sim top speed + Kid PIN in `check_bms_sim_wasm.js`, UI checks in `test_dash_ui.py`.
