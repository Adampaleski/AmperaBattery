# BmsCore: decision logic, simulator, gates (2026-10-19)

All BMS decisions (contactors, charger request, Kelly outputs, faults) live in
`include/BmsCore.h`, a pure C++ header with no Arduino calls. The same code runs in:

| Where | File | How |
|---|---|---|
| Teensy A (dual build) | `src/ContactorSeq.cpp` | `g_core.step()` every loop; pins via `applyPins()` (gated) |
| PC test | `scripts/sim_bms.cpp` + `include/BmsSim.h` | `zig c++ -std=c++17 -Iinclude scripts/sim_bms.cpp` |
| Browser | `tools/bms_sim.html` | same headers -> wasm32 (`scripts/build_bms_sim_html.sh`), embedded base64, opens from file:// |

`scripts/check_bms_sim_wasm.js` runs the wasm in node and prints the same table (parity checked: identical ms on all 13 rows).

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

| P | V/cell | 42S pack |
|---|---|---|
| 50 | 3.725 | 156.5 V |
| 80 | 3.950 | 165.9 V |
| 90 | 4.025 | 169.1 V -> **clamped 4.000 / 168.0 V** |
| 100 | 4.100 | 172.2 V -> **clamped 4.000 / 168.0 V** |

The clamp is `KELLY_MAX_V / STRING_EXPECT_S` (168 / 42). **42S x 4.10 V = 172.2 V exceeds a 168 V Kelly**: verify the controller label/max voltage before going above P86.
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
Recommend a CAB300 / Isabellenhütte IVT-S **only if** the DHAB is missing or damaged — the DHAB covers -420..+230 A, which is enough for the Kelly.

## Temperatures
Tom decodes BICM temps from 0x7Ex bytes 6-7 (big-endian) x -0.0324 + 150 degC (VoltBMSV2 BMSModule.cpp 67-68); his resting capture gives 16.0-17.3 degC.
Implemented as `BmsCore::bicmTempC`, but **unverified on our boards**, so `TEMPS_VERIFIED=0` keeps charging refused until checked with a thermometer.

## Parts list (shop)
| Item | Qty | Why |
|---|---|---|
| Factory LEM DHAB S/44 (already owned) | 1 (+1 spare) | pack current |
| 10 k + 18 k 1 % resistors, 100 nF X7R | 3 sets | DHAB dividers/filters |
| 5 V regulator (e.g. 7805 / small buck, >= 100 mA) | 1 | DHAB supply if not on the board |
| PC817 opto or 2N7000 MOSFET + 1 k | 3 | Kelly low-speed / regen inputs |
| Digital pot (MCP4131 10 k) or analog switch (CD4066) | 1 | throttle derate (optional) |
| CAN transceivers (SN65HVD230) | as needed | CAN1/2/3 if not on the carrier |
| 10 k NTC | 0-2 | only if BICM temps prove unusable |
| CAB300 / IVT-S | 0 | fallback only |
