# BmsCore: decision logic, simulator, gates (2026-10-19)

All BMS decisions (contactors, charger request, Kelly outputs, faults) live in
`include/BmsCore.h`, a pure C++ header with no Arduino calls. The same code runs in:

| Where | File | How |
|---|---|---|
| Teensy A (dual build, 2 strings x 36S) | `src/ContactorSeq.cpp` | `g_core.step()` every loop; pins via `applyPins()` (gated) |
| PC test | `scripts/sim_bms.cpp` + `include/BmsSim.h` | `zig c++ -std=c++17 -Iinclude scripts/sim_bms.cpp` |
| Browser | `tools/bms_sim.html` | same headers -> wasm32 (`scripts/build_bms_sim_html.sh`), embedded base64, opens from file:// |

`scripts/check_bms_sim_wasm.js` runs the wasm in node and prints the same table (parity checked: identical ms on all 18 rows).

## Gates (all default OFF — enabling is a flag flip after bench tests)

| Feature | Flag(s) | Default behaviour |
|---|---|---|
| Contactor coils 11/12/21 | `BMS_CAP_CONTACTOR_DRV=0`, `TELEMETRY_ONLY=1` | state machine runs, prints `[PRINT-ONLY, coils not driven]` |
| Elcon 0x1806E5F4 TX | `BMS_CAP_CHARGE_TX=0`, `TELEMETRY_ONLY=1` | prints the would-be frame (`g`) |
| Factory balance 0x300/0x310/0x200 | `BMS_CAP_BALANCE_TX=0`, `FACTORY_BALANCE_MAP_VERIFIED=0` | prints plan + would-be bytes |
| Kelly OUT3/OUT5/OUT6 | `BMS_CAP_KELLY_LIMIT_DRV=0`, `TELEMETRY_ONLY=1` | `y` prints intended outputs |
| Current sensor A0/A1/A11 | `CURRENT_SENSE_ENABLE=0` | pins untouched; `I<amps>` simulates |
| Temperature-based charge permit | `TEMPS_VERIFIED=0` | charge refused with TEMPS-NOT-VERIFIED |

Watchdog: WDOG3 ~2 s, fed only from `loop()` (`src/Watchdog.cpp`); `setup()` drives coil pins LOW first, so a hang resets with coils off.

## Contactor state machine
Open -> Precharge (main- + precharge) -> MainClose (main+ on, precharge overlaps 100 ms) -> Closed.
Order follows Tom de Bree VoltBMSV2.ino 1192-1218 (NEG, PRE, POS, PRE off).
Precharge: bus >= 95 % of the lowest string after >= 300 ms; timeout 3 s; 2 retries 30 s apart; then **Lockout** (until reboot).
Weld check: after any open, if the bus is still > max(30 V, 50 % of lowest string) for 2 s -> WELDED, latched; no re-close for 2 s after any open.
Drive faults latch until key-off with the cause gone. CHARGE-OV latches until reboot.

## Charge limit (P setting, EEPROM)
`ceiling = 4.10 V - 0.0075 V x (100 - pct)`, pct 50..100, default 80.

| P | V/cell | 36S pack |
|---|---|---|
| 50 | 3.725 | 134.1 V |
| 80 | 3.950 | 142.2 V |
| 90 | 4.025 | 144.9 V |
| 100 | 4.100 | **147.6 V (full)** |

String layout (Adam 10/19, corrected 06:22): each string = full 24S board (M2 or M3) + only the first 12S of an M4 18S
(0x46C bottom + 0x46D middle). **36S per string, 4.10 V/cell = 147.6 V, below the 168 V Kelly**, so P100 really charges to 4.10 V.
The M4 top circuit 0x46E/0x47E (c13-18) is **empty by design**: not expected, silent is normal (never a PARTIAL/SILENT fault),
never in the balance plan (host test: 0x310[4] stays 00), shown as "empty by design" in `d` and in the simulator.
The old per-cell clamp is gone. A general pack clamp remains as a safety net: charge request <= `KELLY_MAX_V - KELLY_MARGIN_V`
(168 - 5 = 163 V); with 36S it never acts (compile-time `static_assert` in ChargeTx.cpp).
Taper: linear from `chargeMaxA` (5 A) at ceiling - 50 mV to `chargeEndA` (1 A) at the ceiling, stop at the ceiling, restart below ceiling - 50 mV
(adapted from Tom `currentlimit()` VoltBMSV2.ino 2591-2672). Any charge fault -> stop frame.

## Elcon
Control 0x1806E5F4, 8 bytes: V x10 big-endian, A x10 big-endian, byte4 0 = charge / 1 = stop.
Verified against Tom's code: VoltBMSV2.ino `chargercomms()` 2870-2886 and BMWPhevBMS.ino 3524-3540 (same frame; Tom always sends byte4 = 0 and stops by sending 0 A — we also set byte4 = 1).
Status 0x18FF50E5 is **not read anywhere in Tom's code**; our decode (`include/ElconCharger.h`) is from the Elcon/TC CAN protocol sheet. Status flags are printed by `g`.

## Kelly KLS
KLS "Broadcast CAN Protocol" v1.1: only 0x0CF11E05 (rpm, motor current 0.1 A, battery V 0.1 V, error bits) and 0x0CF11F05 (throttle, controller/motor temp, status, switches), both **sent by** the controller every 50 ms at 250 k (a 500 k variant of the doc exists).
**The KLS accepts no CAN commands — the BMS cannot set a current limit over CAN.** Implemented instead (gated):
- OUT3 pin 20 LIMP -> opto/transistor onto the KLS low-speed (speed switch) input. Limits speed/throttle %, set in the Kelly tool; it is not an amp limit.
- OUT5 pin 2 PWM duty = allowed % -> RC + analog switch / digital pot on the throttle wiper (hardware TBD).
- OUT6 pin 3 regen-ok -> opto in the regen/brake-switch input (wiring TBD).
- Real faults still open the contactors. Note: KLS "motor current" is not battery current.

## Factory pack current sensor (Volt/Ampera gen1)
**What it is:** LEM **DHAB S/44**, open-loop Hall-effect, 5 V supply (15-20 mA), two ratiometric analog outputs — not CAN, not a shunt.
- ch1 (low range): +-20 A, 2.50 V offset, 100 mV/A
- ch2 (high range): -420..+230 A, 3.08 V offset, 6.15 mV/A
- outputs clamp at 0.25 / 4.75 V; load >= 10 kOhm, 1-100 nF; ~80 Hz bandwidth
Sources: LEM DHAB S/44 datasheet; SimpBMS manual (Tom de Bree) pinout; Tom VoltBMSV2.ino 53-54, 233-250, 966-1081 (dual-channel ADC read, 1750 mV offsets after his divider, switch-over at 20 A, 5 mV deadband).
The GM service part number was **not found**; identify by the LEM label.

Connector (4-pin, SimpBMS manual): 1 = +5 V (purple/red), 2 = GND (black/grey), 3 = high-range out (light blue/yellow), 4 = low-range out (light green/brown). **Confirm with a meter before powering.**

Teensy wiring (`src/CurrentSense.*`, Teensy pins are 3.3 V max):
- DHAB 4 -> 10 k -> A0 (pin 14), 18 k + 100 nF to GND
- DHAB 3 -> 10 k -> A1 (pin 15), 18 k + 100 nF to GND
- DHAB +5 V -> 10 k -> A11 (pin 25), 18 k to GND (ratiometric reference)
Firmware: 16x oversample + IIR (~0.1 s), ch1 below 18 A else ch2, supply outside 4.5-5.5 V -> invalid. `Z` = zero-cal with no current (saved to EEPROM), `I<amps>` = bench simulation.

**Finding the second sensor:** in the Volt pack it sits at the front end with the BECM/contactor box (the "front of the pack" junction area where the slave harness and current sensor meet the BECM). Look for a small black rectangular Hall sensor (~5-6 cm) with one HV cable passing through its window, a 4-pin connector, and a LEM label reading DHAB S/44. It is usually on the main HV cable leading out of the contactor/junction box; follow the 4-wire harness (colours above) from the BECM.
Primary sensing (2026-10-20) is the dual LEM A + LEM B pair (~275 A each at the 550 A Kelly peak). CAB300 / ISA IVT-S is **optional**, e.g. if a DHAB is missing or damaged.

## Temperatures
Tom decodes BICM temps from 0x7Ex bytes 6-7 (big-endian) x -0.0324 + 150 degC (VoltBMSV2 BMSModule.cpp 67-68); his resting capture gives 16.0-17.3 degC.
Implemented as `BmsCore::bicmTempC`, but **unverified on our boards**, so `TEMPS_VERIFIED=0` keeps charging refused until checked with a thermometer.

## Parts list (shop)
| Item | Qty | Why |
|---|---|---|
| Factory LEM DHAB S/44 (already owned) | 2 (LEM A + LEM B) | pack current, primary |
| 10 k + 18 k 1 % resistors, 100 nF X7R | 3 sets | DHAB dividers/filters |
| 5 V regulator (e.g. 7805 / small buck, >= 100 mA) | 1 | DHAB supply if not on the board |
| PC817 opto or 2N7000 MOSFET + 1 k | 3 | Kelly low-speed / regen inputs |
| Digital pot (MCP4131 10 k) or analog switch (CD4066) | 1 | throttle derate (optional) |
| CAN transceivers (SN65HVD230) | as needed | CAN1/2/3 if not on the carrier |
| 10 k NTC | 0-2 | only if BICM temps prove unusable |
| CAB300 / IVT-S | 0 | optional only |
| Used TE EV200 | 2 | main contactors, pack + / − |
| Mersen A6T200 200 A Class T | 4 | string fuses — planned; size to be confirmed vs 650 A trip (Sparky) |
| Volt AEV14012 (owned) | — | charger / DC-DC / precharge branches only |

## Current trips and derates (decided 2026-10-20)
- **In code (2026-10-20):** hard trip on LEM A + LEM B, `overcurrentA = 650` for `overcurrentMs = 100`, or `instantTripA = 750` on any single sample (logged as `OC instant`). The 250 A Kelly derate is off (`maxDischargeA = 0`). Sim scenarios 7 and 8 plus 6 boundary checks in `scripts/sim_bms.cpp`. Not on hardware: `CURRENT_SENSE_ENABLE` is 0.
- **In code (2026-10-20, sim only)** — derates: LEM imbalance > 100 A for 1 s (`imbalanceA`, `imbalanceMs`, cut to `imbalancePct` = 50 %, PLACEHOLDER level); cell sag 3.2 V → 3.0 V (`sagStartV`, `sagZeroV`); heat only with verified temps. Inputs `lemSplitValid`, `lemA`, `lemB`; outputs `lemImbalance`, `lemDiffA`. **Superseded 2026-10-21:** cell sag is now relative to the pack average (`relSagStartV`/`relSagZeroV`), whole-pack sag is judged against load (`restA`, `packSagBandV`), `CELL-LOW` is a debounced hard floor under load (`hardFloorV`/`hardFloorMs`) and a debounced `lowCutV` at rest (`lowCutMs`). See docs/KELLY_CUTBACK.md.
- Kelly phase and battery limits 550 A. Two used EV200 mains. 36S strings from 24S + 18S BICM boards (no 30S).

## Charge mode (Adam 2026-10-21) — CREW REVIEW NEEDED

`Inputs.plugIn` (charger present / proximity) wakes the Core without key-on. Sequence: B−, precharge, B+
(same as drive), then `Outputs.chargerPath` = WANTED Volt AEV14012 charger-branch contactor, only in CHARGE
mode with no charge fault. `Outputs.mode` = IDLE / DRIVE / CHARGE. While plugged in: `kellyPct` 0, limp,
no regen (`kellyWhy` "charging - drive locked"). Key-on while plugged in is refused (`keyRefused`) and stays
refused after unplugging until the key is cycled off. `Inputs.chargeStop` = driver Stop on the charge screen.
Charge current: cell-max taper (unchanged), 0–45 °C window (unchanged), below `chargeColdC` 10 °C reduced to
`chargeColdFrac` 50 % of `chargeMaxA` (both PLACEHOLDER). Fault clear now needs key off AND unplugged.
Firmware: ContactorSeq never sets `plugIn` / `chargeStop` (no proximity pin chosen), so live behaviour is
unchanged; contactors and charger stay print-only (BMS_CAP_* 0, TELEMETRY_ONLY=1).
