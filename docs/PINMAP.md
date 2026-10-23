# Master pin map (GENERATED — edit docs/PINMAP.yaml, then `python3 tools/build_pinmap.py`)

Status: SOURCED (file:line or doc) · ASSUMED · PROPOSED · UNVERIFIED · UNDEFINED (nothing chosen yet).
Checked against `include/Config.h` and `include/FlexCanCompat.h` by `scripts/test_pinmap.py`.

- **A**: Teensy 4.1 A, env teensy41_dual_a (TEENSY_ROLE=0) — BMS master, contactors, throttle
- **B**: Teensy 4.1 B, env teensy41_dual_b (TEENSY_ROLE=1) — watchdog, can only open/cap
- **E**: ESP32 DevKit gateway (teryx-ev-hub firmware/esp32-gateway), read-only

## Pin conflicts and undefined pins (fix or accept before wiring)

1. DECIDED (Sparky 06:57 #1): release-button detect on A:2 (OUT5 spare) and B:35 (B spare), LM339 ch3/ch4, 1k series + own 10k pull-up + 100 nF each. FIRMWARE GAP: A:2 boots as an output (pinMode LOW in BmsApp.cpp) — firmware must set it INPUT; nothing reads plug, key or release yet (later commit; no flashing now).
2. DECIDED (Sparky 06:57 #1): plugged but NOT latched (plug channel LOW, release channel HIGH) = stop the charger over CAN and open the AC contactor. Firmware logic = later commit.
3. DECIDED (Sparky 06:57 #2): SN65HVD230 modules are wired by silkscreen (pin order varies by vendor). Keep the onboard 120 Ω (usually R2 marked 121) only on the module at a physical bus end. Check powered off: CANH-CANL ≈60 Ω.
4. DECIDED (chief 06:57 #5 / Sparky #4): TB-GND = LV star (G03, G07, G08). The bms_master board star reaches TB-GND by exactly one link (J-PWR.2). LEM A, LEM B, pedal and Kelly RTN each get their own wire to the board star (G09-G12, ids PROPOSED), no daisy-chain.
5. NO FREE TEENSY A OUTPUT (Build 18 audit): edge pins 0-41 all used, 42-47 = SD card, 48-54 = bottom pads (no PSRAM fitted, UNVERIFIED, needs fine soldering). MOTOR_FAN / BUZZER reserved on MCP23017 bits 14 / 13 instead (PROPOSED). Other options for Adam: a second MCP23017 on the same Wire bus (addr 0x21), Teensy B spare outputs over the A-B link, or the bottom pads.
6. DECIDED (room: chief + Sparky + crew, 2026-10-23): A:24 HV_SENSE is a reserved spare. Precharge + weld check use Kelly CAN B+ vs the BICM sum (BACKLOG CRITICAL, bench gate UNVERIFIED). If the bench gate fails, AMC1311EVM returns on A:24 (isolated, never a plain divider).
7. PROPOSED (Sparky 2026-10-22): KEY-ON on pin 34 (IN1 spare) on A and B — no free GPIO on A; IN2-IN4 deferred. LIGHT_OE (retired) also names 34; nothing drives it. Firmware does not read key-on or plug-in yet (later commit, chief #4).
8. AUDIT 2026-10-22 (chief): NO free analog pin on A. 14/15 LEM, 16/17 Serial4 (HubStatus owns both), 18/19 I2C (MCP23017), 20/21 OUT3/OUT4, 22/23 CAN1, 24 HV_SENSE, 25 CUR_VC, 26/27 IS_4WD/IS_DIFF, 38 THR_CS (digipot), 39 POS_DIFF, 40 PEDAL_IN_A, 41 POS_4WD. The rough-regex candidates A:23/26/27/38/39 are all in use. B has unassigned analog pins in the map (18, 19, 27, 38, 39; firmware role-gating UNVERIFIED), but the same threshold is needed on both boards, so proximity uses ONE LM339 comparator (re-pinned from the LM393, DECIDED chief 06:57 #1; ch1/ch2 2.5 V plug, ch3/ch4 1.40 V release, one network) into A:33 and B:33 (PROPOSED, Adam t284 replaces the per-board pair).
9. RESOLVED (Adam 2026-10-22): plug-in detect B mirror = S44 on B:33 (free per this map and Config.h B spare list).
10. RESOLVED: IN1/IN2 were Serial4 17/16 — IN1 moved to pin 34 (free on A and B), IN2 retired (-1). A pin 16 (RX4) stays unconnected on purpose: the gateway is receive-only (TX17 -> ESP32 GPIO16).
11. RESOLVED: IN3/IN4 were I2C 18/19 — retired (-1); 18/19 are only I2C_SDA/I2C_SCL now. No other free GPIO exists on A (33 reserved charge detect).
12. INTENDED (not a conflict): A uses pins 4/40 (A_THR_ALIVE, PEDAL_IN_A), B uses 7/41 (B_THR_ALIVE, PEDAL_IN_B). Pin 7 = RAD_FAN on A, 41 = POS_4WD on A, 4 = B_FAULT_OUT on B, 40 = ST_4WD_B on B — different boards; label harness wires TA/TB.
13. RESOLVED (PROPOSED pins, free on B per this map): e-stop block 3 -> B pin 9; 12 V low alarm -> B pin 26 (A12). No firmware const yet.
14. ESP32 GPIO17: teryx-ev-hub firmware/esp32-gateway/docs/PINMAP.md says GPIO17 TX <- Teensy RX 16 (crossed pair); GATEWAY.md:18 says leave unconnected (RX-only). This map follows GATEWAY.md. Hub PINMAP.md is stale (other repo, not edited).
15. FIXED: src/HubStatus.cpp:14 and include/Config.h:37 stale CAN comments corrected to CAN_LAYOUT_V2.
16. FIXED: old wiring overview drew park on 'pin 18'; park is pin 32 on A and B.
17. RESOLVED: precharge contactor = salvaged AEV14012, 0.35 A at 12 V coil (34 Ω, 4.2 W, no economizer; ≈0.41 A at 14.0 V; coil allowed 10-16 V; SOURCED Panasonic EV relay catalog, crew 2026-10-23); the earlier coil figure was the EV200 inrush. TE EV11 dropped.
18. RESOLVED: HV gauge — string leads from each A6T200 = 1/0 (~138 A at a 550 A launch); pack-to-Kelly mains = owned 3/0.
19. RESOLVED: SN65HVD230 — 4 owned enough for the one-string bench; 2 extra on the buy list for Gate 3 (no purchase now).
20. RESOLVED: PEDAL_IN_B = B pin 41, pedal track 2 via 10k/18k; A reads track 1 on pin 40.
21. Kelly pin 3: ONE pull-down, the existing 100k (R1). Sparky's 10k NOT added — PROPOSED bench option only if pin 3 floats, after a full-voltage check through the 4066s.
22. RESOLVED: DC-DC = Elcon TDC-JH-144-12, 110 A out (SOURCED crew 2026-10-23). F-MAIN and P01/P02 gauge: Sparky sizes them (TBD).

## Teensy A pins

| Pin | Firmware name | Use | Wire | Tier | Status | Source |
|---|---|---|---|---|---|---|
| 0 |  | CAN2 RX -> SN65 #2 (string 2) | C03 |  | SOURCED | FlexCanCompat.h:44 |
| 1 |  | CAN2 TX -> SN65 #2 | C03 |  | SOURCED | FlexCanCompat.h:44 |
| 2 | OUT5 | RELEASE-BUTTON detect A (reuses the OUT5 spare): LM339 ch3 OUT3 (pin 14) via 1k series, own 10k pull-up to A 3.3 V + 100 nF. LOW = plugged and latched, HIGH = release pressed or unplugged. FIRMWARE MUST set A:2 to INPUT — it boots as an output (pinMode LOW, BmsApp.cpp); the 1k series limits the fight until then. Firmware change = later commit, no flashing now | S51 | CRITICAL | PROPOSED | Config.h:180; Sparky 2026-10-23 06:57 #1/#3 DECIDED (bms_master_answers.md) |
| 3 | OUT6 | OUT6 spare output |  |  | SOURCED | Config.h:181 |
| 4 | A_THR_ALIVE | A throttle heartbeat 1 kHz -> U4 -> U1 B1 | S11 |  | SOURCED | Config.h:256 |
| 5 | SEAT_DRV | seat heater driver PWM (OUT7) |  | LATER | PROPOSED | Config.h:210 |
| 5 | OUT7 | alias of SEAT_DRV |  |  | SOURCED | Config.h:187 |
| 6 | SEAT_PAS | seat heater passenger PWM (OUT8) |  | LATER | PROPOSED | Config.h:211 |
| 6 | OUT8 | alias of SEAT_PAS |  |  | SOURCED | Config.h:188 |
| 7 | RAD_FAN | radiator fan relay coil (OUT9) |  | LATER | PROPOSED | Config.h:212 |
| 7 | OUT9 | alias of RAD_FAN |  |  | PROPOSED | Config.h:212 |
| 8 | COOL_PUMP | coolant pump PWM (OUT10) |  | LATER | PROPOSED | Config.h:213 |
| 8 | OUT10 | alias of COOL_PUMP |  |  | PROPOSED | Config.h:213 |
| 9 | ACT4WD_IN1 | 4WD actuator H-bridge IN1 |  | LATER | PROPOSED | Config.h:214 |
| 10 | ACT4WD_IN2 | 4WD actuator H-bridge IN2 |  | LATER | PROPOSED | Config.h:215 |
| 11 | MAIN_POS | EV200 B+ coil FET gate (OUT1) | S01 |  | SOURCED | Config.h:193 |
| 11 | OUT1 | alias of MAIN_POS | S01 |  | SOURCED | Config.h:174 |
| 12 | PRECHARGE | precharge relay coil FET gate (OUT2) | S02 |  | SOURCED | Config.h:194 |
| 12 | OUT2 | alias of PRECHARGE | S02 |  | SOURCED | Config.h:175 |
| 13 | LED | on-board LED (no wire) |  |  | SOURCED | Config.h:166 |
| 14 | CUR_LO | LEM A DHAB pin 4 (ch1 ±20 A) via 10k/18k + 100 nF | S05 |  | SOURCED | Config.h:198 |
| 15 | CUR_HI | LEM A DHAB pin 3 (ch2 -420..+230 A) via 10k/18k + 100 nF | S06 |  | SOURCED | Config.h:199 |
| 16 |  | Serial4 RX4 — UNCONNECTED ON PURPOSE: the gateway is receive-only (Teensy TX17 -> ESP32 GPIO16; ESP32 GPIO17 unused). Never wire anything else here. |  |  | SOURCED | src/HubStatus.cpp:12; GATEWAY.md:5,18 |
| 17 |  | Serial4 TX4 -> ESP32 GPIO16 (hub UART 115200) | S30 |  | SOURCED | src/HubStatus.cpp:12 |
| 18 | I2C_SDA | MCP23017 SDA (body expander, LATER: footprint only; 4.7k pull-up to 3.3 V when fitted, chief 06:57 #3) |  | LATER | PROPOSED | Config.h:245 |
| 19 | I2C_SCL | MCP23017 SCL (LATER: footprint only; 4.7k pull-up to 3.3 V when fitted) |  | LATER | PROPOSED | Config.h:246 |
| 20 | OUT3 | Kelly Low-cap relay FET gate: HIGH = release cap, LOW/dead = capped | S20 |  | SOURCED | Config.h:176; docs/DRIVE_MODES.md |
| 21 | MAIN_NEG | EV200 B- coil FET gate (OUT4) | S03 |  | SOURCED | Config.h:195 |
| 21 | OUT4 | alias of MAIN_NEG | S03 |  | SOURCED | Config.h:177 |
| 22 | CAN1_TX | CAN1 TX -> SN65 #1 (string 1) | C01 |  | SOURCED | Config.h:185 |
| 23 | CAN1_RX | CAN1 RX | C01 |  | SOURCED | Config.h:186 |
| 24 | HV_SENSE | RESERVED spare (room DECIDED 2026-10-23: chief + Sparky + crew). Isolated HV_SENSE / AMC1311 DROPPED for now: precharge + weld check use Kelly CAN B+ (bus side) vs the BICM sum (pack side), see BACKLOG CRITICAL. Comes back as AMC1311EVM (~$50, Adam OK) only if the Kelly B+ bench gate fails. Never a plain 60.6:1 divider (bonds pack negative to Teensy ground/chassis). HV_ANALOG_ENABLE=0 |  | LATER | PROPOSED | Config.h:196; room 2026-10-23 |
| 25 | CUR_VC | LEM A 5 V supply sense (ratiometric) via divider | S07 |  | SOURCED | Config.h:200 |
| 26 | IS_4WD | BTS7960 #1 current sense |  | LATER | PROPOSED | Config.h:229 |
| 27 | IS_DIFF | BTS7960 #2 current sense |  | LATER | PROPOSED | Config.h:230 |
| 28 | ACTDIF_IN1 | diff-lock H-bridge IN1 |  | LATER | PROPOSED | Config.h:216 |
| 29 | ACTDIF_IN2 | diff-lock H-bridge IN2 |  | LATER | PROPOSED | Config.h:217 |
| 30 |  | CAN3 RX vehicle bus -> SN65 #5 | C05 |  | SOURCED | FlexCanCompat.h:45 |
| 31 |  | CAN3 TX vehicle bus | C05 |  | SOURCED | FlexCanCompat.h:45 |
| 32 | PARK_IN_A | park brake contact 1 (direct GPIO, 10k pull-up, LOW = set) | S16 |  | SOURCED | Config.h:260 (BODY_IO_EXPANDER=1) |
| 33 |  | CHARGE PLUG-IN detect: LM339 ch1 OUT1 (pin 2, open collector) of the ONE shared LM339 on the NACS proximity line (DECIDED re-pin chief 06:57; LM339 NOT OWNED, pending purchase approval), own 10k pull-up to A 3.3 V + 100 nF, LOW = plugged. Plugged = drive blocked on both boards (like park). Feeds BmsCore plugIn (chief #4); firmware read + 20 ms debounce = later commit | S41 | CRITICAL | PROPOSED | Config.h:254; chief 2026-10-23 06:57 #1 DECIDED (bms_master_answers.md) |
| 34 | LIGHT_OE | spare (retired 595 /OE; nothing drives it) |  |  | SOURCED | Config.h:244 |
| 34 | IN1 | KEY-ON input (12 V key via opto, 10k pull-up, LOW = key on; Sparky 2026-10-22) — reuses IN1 spare (no other free GPIO on A). Firmware read = later commit (chief #4 KSI) | S42 | CRITICAL | PROPOSED | Config.h:170 |
| -1 | IN2 | IN2 retired (was 16 = Serial4 RX): no free GPIO on A; DEFERRED until after first drive (chief 2026-10-22; not on the pre-drive CRITICAL list) |  | LATER | PROPOSED | Config.h:171 |
| -1 | IN3 | IN3 retired (was 18 = I2C SDA); DEFERRED until after first drive (chief 2026-10-22; not on the pre-drive CRITICAL list) |  | LATER | PROPOSED | Config.h:172 |
| -1 | IN4 | IN4 retired (was 19 = I2C SCL); DEFERRED until after first drive (chief 2026-10-22; not on the pre-drive CRITICAL list) |  | LATER | PROPOSED | Config.h:173 |
| 35 | LIGHT_DATA | bit-bang DATA -> U4 -> MCP4131 SDI (shared with lights) | S10 |  | PROPOSED | Config.h:234 |
| 36 | LIGHT_CLK | bit-bang CLK -> U4 -> MCP4131 SCK | S10 |  | PROPOSED | Config.h:235 |
| 37 | ACT_EN | both BTS7960 enables |  | LATER | PROPOSED | Config.h:228 |
| 38 | THR_CS | MCP4131 /CS -> U4 | S10 |  | PROPOSED | Config.h:252 |
| 39 | POS_DIFF | diff actuator pot wiper |  | LATER | PROPOSED | Config.h:219 |
| 40 | PEDAL_IN_A | throttle pedal wiper (10k/18k + 100 nF) | S14 |  | SOURCED | Config.h:222 |
| 41 | POS_4WD | 4WD actuator pot wiper |  | LATER | PROPOSED | Config.h:218 |
| -1 | LIGHT_LATCH | retired (lights on MCP23017) |  |  | SOURCED | Config.h:243 |

## Teensy B pins

| Pin | Firmware name | Use | Wire | Tier | Status | Source |
|---|---|---|---|---|---|---|
| 0 |  | CAN2 RX -> SN65 #4 (string 4) | C04 |  | SOURCED | FlexCanCompat.h:44 |
| 1 |  | CAN2 TX | C04 |  | SOURCED | FlexCanCompat.h:44 |
| 2 | OUT5 | OUT5 pinMode LOW, no wire |  |  | SOURCED | Config.h:180 |
| 3 | OUT6 | OUT6 pinMode LOW, no wire |  |  | SOURCED | Config.h:181 |
| 4 | B_FAULT_OUT | B fault out HIGH=healthy (B_FAULT_PIN_ENABLE=0, disabled) |  | LATER | UNVERIFIED | Config.h:184 |
| 5 | OUT7 | pinMode LOW, no wire |  |  | SOURCED | Config.h:187 |
| 6 | OUT8 | pinMode LOW, no wire |  |  | SOURCED | Config.h:188 |
| 7 | B_THR_ALIVE | B throttle heartbeat 1 kHz -> U1 B2 | S12 |  | SOURCED | Config.h:225 |
| 11 | OUT1 | pinMode LOW; B never drives a coil (watchdog can only open) |  |  | SOURCED | Config.h:174 |
| 12 | OUT2 | pinMode LOW, no wire |  |  | SOURCED | Config.h:175 |
| 13 | LED | on-board LED |  |  | SOURCED | Config.h:166 |
| 14 | CUR_LO | LEM B DHAB pin 4 | S25 |  | SOURCED | Config.h:198 |
| 15 | CUR_HI | LEM B DHAB pin 3 | S26 |  | SOURCED | Config.h:199 |
| 16 |  | Serial4 RX (not wired on B) |  |  | ASSUMED | HubStatus.cpp:12 |
| 17 |  | Serial4 TX (not wired on B; gateway listens to A only) |  |  | ASSUMED | HubStatus.cpp:12 |
| 20 | OUT3 | pinMode LOW, no wire |  |  | SOURCED | Config.h:176 |
| 21 | OUT4 | pinMode LOW, no wire |  |  | SOURCED | Config.h:177 |
| 22 | CAN1_TX | CAN1 TX -> SN65 #3 (string 3) | C02 |  | SOURCED | Config.h:185 |
| 23 | CAN1_RX | CAN1 RX | C02 |  | SOURCED | Config.h:186 |
| 24 | HV_SENSE | optional HV sense (no wire) |  | LATER | UNVERIFIED | Config.h:196 |
| 25 | CUR_VC | LEM B 5 V supply sense | S27 |  | SOURCED | Config.h:200 |
| 30 |  | CAN3 RX vehicle bus -> SN65 #6 (stub) | C06 |  | SOURCED | FlexCanCompat.h:45 |
| 31 |  | CAN3 TX vehicle bus | C06 |  | SOURCED | FlexCanCompat.h:45 |
| 32 | PARK_IN_B | park brake contact 2 (10k pull-up, LOW = set) | S17 |  | SOURCED | Config.h:264 |
| 40 | ST_4WD_B | OPTIONAL stock 4WD indicator (not read yet) |  | LATER | PROPOSED | Config.h:223 |
| 41 | PEDAL_IN_B | throttle pedal wiper (own 10k/18k + 100 nF) | S15 |  | SOURCED | Config.h:224 |
| 9 |  | E-STOP block 3 latch input, 10k pull-up, LOW = released/closed (pin free on B; no firmware const yet — EstopLatch.h not wired, chief #5) | E09 |  | PROPOSED | crew 2026-10-22; docs/ESTOP.md |
| 26 |  | 12 V LOW ALARM sense (A12) via 47k/10k + 100 nF (pin free on B; no firmware const yet; A has no free analog pin) | S40 |  | PROPOSED | crew 2026-10-22 |
| 33 |  | CHARGE PLUG-IN mirror: LM339 ch2 OUT2 (pin 1) of the same LM339, own 10k pull-up to B 3.3 V + 100 nF, LOW = plugged. Plugged = drive blocked on B too (like park). B:33 free per this map (Config.h spare list 33-39). Firmware read + 20 ms debounce = later commit | S44 | CRITICAL | PROPOSED | Adam 2026-10-22; chief 2026-10-23 06:57 #1 DECIDED (bms_master_answers.md) |
| 35 |  | RELEASE-BUTTON detect B: LM339 ch4 OUT4 (pin 13) via 1k series, own 10k pull-up to B 3.3 V + 100 nF. LOW = plugged and latched (~1.0 V at IN+), HIGH = release button pressed (~1.8 V) or unplugged. B:35 free on B (Config.h B spare list 33-39; LIGHT_DATA 35 is an A-only role). Firmware read + 20 ms debounce = later commit | S52 | CRITICAL | PROPOSED | Sparky 2026-10-23 06:57 #1/#3 DECIDED (bms_master_answers.md) |
| 34 | IN1 | KEY-ON mirror (own opto channel, 10k pull-up, LOW = key on) — reuses IN1 spare, pin 34 free on B. Firmware read = later commit | S43 | CRITICAL | PROPOSED | Config.h:170 |

## ESP32 gateway pins

| Pin | Firmware name | Use | Wire | Tier | Status | Source |
|---|---|---|---|---|---|---|
| GPIO16 |  | UART2 RX <- Teensy A pin 17 (3.3 V to 3.3 V, NO level shifter; TX-only gateway by design, DECIDED chief 06:57 #2) | S30 |  | SOURCED | teryx-ev-hub firmware/esp32-gateway/docs/GATEWAY.md:17 |
| GPIO4 |  | spare, read-only role (gateway never drives it today) |  | LATER | ASSUMED | teryx-ev-hub cursor/schema-sync firmware/esp32-gateway/src/main.cpp:36 (only GPIO16 used); output-safe per ESP32-WROOM strapping/flash rules |
| GPIO13 |  | spare, read-only role (gateway never drives it today) |  | LATER | ASSUMED | teryx-ev-hub cursor/schema-sync firmware/esp32-gateway/src/main.cpp:36 (only GPIO16 used); output-safe per ESP32-WROOM strapping/flash rules |
| GPIO14 |  | spare, read-only role (gateway never drives it today) — outputs PWM briefly at boot |  | LATER | ASSUMED | teryx-ev-hub cursor/schema-sync firmware/esp32-gateway/src/main.cpp:36 (only GPIO16 used); output-safe per ESP32-WROOM strapping/flash rules |
| GPIO18 |  | spare, read-only role (gateway never drives it today) |  | LATER | ASSUMED | teryx-ev-hub cursor/schema-sync firmware/esp32-gateway/src/main.cpp:36 (only GPIO16 used); output-safe per ESP32-WROOM strapping/flash rules |
| GPIO19 |  | spare, read-only role (gateway never drives it today) |  | LATER | ASSUMED | teryx-ev-hub cursor/schema-sync firmware/esp32-gateway/src/main.cpp:36 (only GPIO16 used); output-safe per ESP32-WROOM strapping/flash rules |
| GPIO21 |  | spare, read-only role (gateway never drives it today) |  | LATER | ASSUMED | teryx-ev-hub cursor/schema-sync firmware/esp32-gateway/src/main.cpp:36 (only GPIO16 used); output-safe per ESP32-WROOM strapping/flash rules |
| GPIO22 |  | spare, read-only role (gateway never drives it today) |  | LATER | ASSUMED | teryx-ev-hub cursor/schema-sync firmware/esp32-gateway/src/main.cpp:36 (only GPIO16 used); output-safe per ESP32-WROOM strapping/flash rules |
| GPIO23 |  | spare, read-only role (gateway never drives it today) |  | LATER | ASSUMED | teryx-ev-hub cursor/schema-sync firmware/esp32-gateway/src/main.cpp:36 (only GPIO16 used); output-safe per ESP32-WROOM strapping/flash rules |
| GPIO25 |  | spare, read-only role (gateway never drives it today) |  | LATER | ASSUMED | teryx-ev-hub cursor/schema-sync firmware/esp32-gateway/src/main.cpp:36 (only GPIO16 used); output-safe per ESP32-WROOM strapping/flash rules |
| GPIO26 |  | spare, read-only role (gateway never drives it today) |  | LATER | ASSUMED | teryx-ev-hub cursor/schema-sync firmware/esp32-gateway/src/main.cpp:36 (only GPIO16 used); output-safe per ESP32-WROOM strapping/flash rules |
| GPIO27 |  | spare, read-only role (gateway never drives it today) |  | LATER | ASSUMED | teryx-ev-hub cursor/schema-sync firmware/esp32-gateway/src/main.cpp:36 (only GPIO16 used); output-safe per ESP32-WROOM strapping/flash rules |
| GPIO32 |  | spare, read-only role (gateway never drives it today) |  | LATER | ASSUMED | teryx-ev-hub cursor/schema-sync firmware/esp32-gateway/src/main.cpp:36 (only GPIO16 used); output-safe per ESP32-WROOM strapping/flash rules |
| GPIO33 |  | spare, read-only role (gateway never drives it today) |  | LATER | ASSUMED | teryx-ev-hub cursor/schema-sync firmware/esp32-gateway/src/main.cpp:36 (only GPIO16 used); output-safe per ESP32-WROOM strapping/flash rules |
| GPIO17 |  | UART2 TX — UNUSED, leave unconnected (gateway is RX-only, TX pin = -1) |  |  | SOURCED | GATEWAY.md:5,18 |
| GND |  | common LV ground with Teensy A | G05 |  | SOURCED | GATEWAY.md:19 |
| 5V/VIN |  | 5 V from fused buck on VIN — power only, no logic at 5 V (DECIDED chief 06:57 #2) (never Teensy 3.3 V) | P08 |  | SOURCED | GATEWAY.md:20 |

## MCP23017 body expander bits (Config.h ExpBits::)

| Bit | Firmware name | Use | Tier | Status | Source |
|---|---|---|---|---|---|
| 0-8 |  | 9 light outputs (HEAD_LO, HEAD_HI, TAIL, BRAKE, TURN_L, TURN_R, REVERSE, BAR, WORK) via FETs | LATER | PROPOSED | docs/BODY_IO.md:31,60 |
| 9-12 |  | free (stock 4WD switch input moves here; bit not assigned yet) | LATER | UNDEFINED | docs/BODY_IO.md:60 |
| 13 | BUZZER | LATER — reserved: 12 V buzzer for the dash master alert (no logic; only if Adam counts expander bits as A outputs) | LATER | PROPOSED | BACKLOG.md BUZZER (Build 18) |
| 14 | MOTOR_FAN | LATER — reserved: 12 V motor fan relay via low-side IRLB8721 + 10k pull-down + 1N4007; future trigger Kelly CAN 0x0CF11F05 byte 2 (motor temp, UNVERIFIED) (no logic) | LATER | PROPOSED | BACKLOG.md MOTOR_FAN (Build 18) |
| 15 |  | free — PACK_HEAT NOT reserved (chief full review 2026-10-23: new HV circuit, dropped from Build 18) | LATER | SOURCED | review/full_program_review_chief.md |

## CAN buses

| Bus | Board | Ctrl | TX | RX | Bitrate | Nodes | IDs | Termination | Transceiver | Status | Source |
|---|---|---|---|---|---|---|---|---|---|---|---|
| CAN-A1 | A | CAN1 | 22 | 23 | 125000 | Teensy A + String 1 BICMs (M2 24S + M4 c1-12) | 0x460-0x47E BICM reports; 0x200/0x300/0x310 keep-alive/balance (TX only if flags) | 120 Ω at Teensy transceiver + BICM end (BICM-end built-in UNVERIFIED: meter ~60 Ω powered off) SN65HVD230 module: wire by silkscreen (pin order varies by vendor); keep the onboard 120 Ω (usually R2 marked 121) only on the module at a physical bus end, remove it elsewhere; check powered off CANH-CANL ≈60 Ω (DECIDED Sparky 06:57 #2). | SN65HVD230 #1 | SOURCED | include/FlexCanCompat.h:43; include/Config.h:9; include/StringConfig.h:11-15 |
| CAN-A2 | A | CAN2 | 1 | 0 | 125000 | Teensy A + String 2 BICMs (M3 24S + M4) | same factory IDs as string 1 (own bus) | 120 Ω both ends (as CAN-A1) SN65HVD230 module: wire by silkscreen (pin order varies by vendor); keep the onboard 120 Ω (usually R2 marked 121) only on the module at a physical bus end, remove it elsewhere; check powered off CANH-CANL ≈60 Ω (DECIDED Sparky 06:57 #2). | SN65HVD230 #2 | SOURCED | include/FlexCanCompat.h:44 |
| CAN-B1 | B | CAN1 | 22 | 23 | 125000 | Teensy B + String 3 BICMs | factory IDs | 120 Ω both ends SN65HVD230 module: wire by silkscreen (pin order varies by vendor); keep the onboard 120 Ω (usually R2 marked 121) only on the module at a physical bus end, remove it elsewhere; check powered off CANH-CANL ≈60 Ω (DECIDED Sparky 06:57 #2). | SN65HVD230 #3 | SOURCED | include/FlexCanCompat.h:43 |
| CAN-B2 | B | CAN2 | 1 | 0 | 125000 | Teensy B + String 4 BICMs | factory IDs | 120 Ω both ends SN65HVD230 module: wire by silkscreen (pin order varies by vendor); keep the onboard 120 Ω (usually R2 marked 121) only on the module at a physical bus end, remove it elsewhere; check powered off CANH-CANL ≈60 Ω (DECIDED Sparky 06:57 #2). | SN65HVD230 #4 | SOURCED | include/FlexCanCompat.h:44 |
| CAN-V | A+B | CAN3 | 31 | 30 | 250000 | Teensy A (end, 120 Ω) · Teensy B (stub, no 120 Ω) · Kelly · Elcon (far end 120 Ω, built-in UNVERIFIED) | Kelly 0x0CF11E05/0x0CF11F05 (include/KellyLink.h:4); Elcon 0x1806E5F4 / 0x18FF50E5 (Config.h:88, StringConfig.h:81); link 0x6A0-0x6A4 B->A, 0x6A8-0x6AA A->B (StringConfig.h:83-93) | 120 Ω at Teensy A and at the far end only SN65HVD230 module: wire by silkscreen (pin order varies by vendor); keep the onboard 120 Ω (usually R2 marked 121) only on the module at a physical bus end, remove it elsewhere; check powered off CANH-CANL ≈60 Ω (DECIDED Sparky 06:57 #2). | SN65HVD230 #5 (A), #6 (B) — 4 owned cover the one-string bench; 2 extra on the buy list for Gate 3 (no purchase now) | SOURCED | include/FlexCanCompat.h:45 |

## Connectors

| ID | Part (docs/PARTS.yaml) | What | Pins | Status | Source |
|---|---|---|---|---|---|
| X2-S1..S4 | BICM count (summary) — 2× M2 24S + 2× M3 24S + 4× M4 18S (22880272) [`bicm_total`] | Volt BICM harness K16 X2 (one per string) | 9 = 5 V (705 GY/RD) · 10 = GND (470 BK/VT) · 11 = CAN_L (6434 L-BU/YE) · 12 = CAN_H (6435 WH/YE) | SOURCED | tools/wiring_data.js bms page; tools/schematic/gen.py 'bo' (CAN H/L UNVERIFIED: swap if no cells) |
| KELLY-J | Motor controller — KLS8080 NPS, 168 V, 550 A (+ high-speed option, anti-slip, all add-ons) [`controller`] | Kelly KLS-8080N signal connector | 3 throttle · 7 KSI/PWR · 11 12 V out · 15 Foot_SW · 22 ECO · 2 Brake_AN · throttle RTN pin number UNVERIFIED (G12) | UNVERIFIED | KLS-8080N V2.11 / HC V2.14 manuals via docs/ESTOP.md:18, wiring_data.js CONFLICTS; Adam's harness unread |
| ESTOP | E-stop mushroom head (1 NC incl.) — XB4BS8442 (PROPOSED; alternate XB5AS8442) [`estop_head`]; E-stop extra NC blocks — ZBE102 [`estop_blocks`] | XB5AS8442 (or XB4BS8442) head + 2x ZBE102 = 3 NC blocks | each block: terminals 11 (in) / 12 (out), NC | SOURCED | docs/ESTOP.md; se.com XB5AS8442 page (1 NC in head) |
| EBOX-DT | Deutsch DT kit + crimper — Amazon B0DZ331GDL (217 pc) [`deutsch`] | Electronics box Deutsch DT harness connector | UNDEFINED — pin-out not assigned yet | UNDEFINED | tools/schematic/gen.py 'dt' |
| TB-5V | LV distribution terminal blocks — 5 V rail (TB-5V) + single-point GND (TB-GND) — UNKNOWN (type/position count not chosen; barrier or DIN terminal block PROPOSED) [`lv_tb`] | 5 V terminal block | Teensy A VIN, Teensy B VIN, transceivers? (SN65 run on 3.3 V from each Teensy), LEM 5 V, pedal 5 V, clamp chips | PROPOSED | tools/wiring_data.js power12/clamp |
| TB-GND | LV distribution terminal blocks — 5 V rail (TB-5V) + single-point GND (TB-GND) — UNKNOWN (type/position count not chosen; barrier or DIN terminal block PROPOSED) [`lv_tb`] | GND terminal block (single point) | LV STAR (DECIDED chief 06:57 #5): G03 buck return, G07 BICM GND (stays), G08 coil_driver dedicated 16 AWG return, plus EXACTLY ONE link from the bms_master board star (J-PWR.2, = G03 path; no loop). Nothing else lands here | SOURCED | tools/schematic/gen.py 'gnd' |

## Fuses

| ID | Part (docs/PARTS.yaml) | Where | Rating | Status | Source |
|---|---|---|---|---|---|
| F-MAIN | Main 12 V fuse F-MAIN (DC-DC out, P01) — 150 A MIDI/MEGA bolt-down fuse (exact P/N + holder not chosen) [`fuse_main`] | DC-DC 12 V out | TBD — Sparky sizes it (TDC-JH-144-12 = 110 A out, SOURCED crew 2026-10-23) | PROPOSED | Sparky 2026-10-22 |
| F-COIL | Mini-blade fuses + holders — Amazon B0B24YYYCF / B0C15H3F9M [`lv_fuses`] | 12 V coil feed -> e-stop blocks 1+2 | 10 A | PROPOSED | Sparky 2026-10-22 |
| F-KEL | Mini-blade fuses + holders — Amazon B0B24YYYCF / B0C15H3F9M [`lv_fuses`] | Kelly PWR logic | 2 A | PROPOSED | Sparky 2026-10-22 |
| F-EBOX | Mini-blade fuses + holders — Amazon B0B24YYYCF / B0C15H3F9M [`lv_fuses`] | electronics box 12 V | 5 A (F-BUCK 3 A after it) | PROPOSED | Sparky 2026-10-22 |
| F-BUCK | Mini-blade fuses + holders — Amazon B0B24YYYCF / B0C15H3F9M [`lv_fuses`] | 12 V -> 5 V buck | 3 A mini blade | SOURCED | gen.py fbx |
| F-BICM | Mini-blade fuses + holders — Amazon B0B24YYYCF / B0C15H3F9M [`lv_fuses`] | 5 V -> BICM 705 feed | 1 A (resize after measuring BICM draw) | UNVERIFIED | gen.py f705 |
| F-TAB | Mini-blade fuses + holders — Amazon B0B24YYYCF / B0C15H3F9M [`lv_fuses`] | tablet USB-C PD feed | UNDEFINED | UNVERIFIED | gen.py ftab |
| F-AUX | Mini-blade fuses + holders — Amazon B0B24YYYCF / B0C15H3F9M [`lv_fuses`] | each aux load (horn, lights) via relay 30 | size to load, PROPOSED 10-15 A | PROPOSED | Adam 10/22 NO aux buttons through relay/fuse |
| F-S1..S4 | String fuses (Class T 200 A) — A6T200 [`fuse_a6t200`] | string fuses on bus bar (service disconnect) | Mersen A6T200 200 A Class T | SOURCED | docs/WIRING.md:66-69 |
| F-DCDC | DC-DC branch fuse — KLKD-020 [`dcdc_fuse`] | HV in to DC-DC | 15-20 A >=250 V DC (confirm label) | UNVERIFIED | gen.py fdi |
| F-ELC | Charger branch fuse — A6T60 [`charger_fuse`] | Elcon HK-LF-144-46 out (46 A max) -> pack | Mersen A6T60, 60 A Class T, on 6 AWG; fuse holder TBD | SOURCED | crew 2026-10-23 |

## Wire colour code — PROPOSED

- +12V unswitched: RD
- +12V switched/fused feed: RD/WH
- 12V coil feed (after e-stop): RD/YE
- GND: BK
- +5V: OR
- +3V3: OR/WH
- CAN-H: YE
- CAN-L: GN
- analog signal: BU
- digital input: BU/BK
- heartbeat / enable: VT
- UART: WH
- HV+: orange HV cable, RED tape
- HV-: orange HV cable, BLACK tape

## Wire gauge scheme — PROPOSED

- signal / CAN / UART: 22 AWG (CAN = twisted pair, ~1 twist per inch)
- 5 V / 3.3 V logic power: 20 AWG
- 12 V coil feeds, relay coils, KSI: 18 AWG
- 12 V branch to Kelly PWR / electronics box: 16 AWG
- 12 V main (DC-DC out): TBD (Sparky sizes it) — DC-DC TDC-JH-144-12 = 110 A out (SOURCED crew 2026-10-23); 10 AWG ≤30 A, 8 AWG ≤40 A, 6 AWG ≤60 A (Sparky)
- HV: 1/0 AWG string leads from each A6T200 (~138 A at a 550 A launch); 3/0 owned TEMCo for pack-to-Kelly mains (crew 2026-10-22)

## Every wire

| Wire | Step | Kind | From | To | Signal | Voltage | Colour | AWG | Fuse | Label | Tier | Status | Source |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| G01 | 1 | wire | 12V bus bar:- | frame ground strap | GND | 0 V | BK | strap | none | G01 12V- CHASSIS | CRITICAL | PROPOSED | wiring_data power12 net (Mechanic) |
| G02 | 1 | wire | Kelly case | frame | GND | 0 V | BK | strap | none | G02 KELLY CASE | CRITICAL | PROPOSED | wiring_data power12 |
| G03 | 1 | wire | 5 V buck OUT- (on the bms_master board star) via J-PWR.2 | TB-GND | GND | 0 V | BK | 20 | none | G03 BUCK GND | CRITICAL | PROPOSED | gen.py gnd; chief 06:57 #5 DECIDED: the ONE link board star -> TB-GND (no loop) |
| P01 | 1 | wire | DC-DC TDC-JH-144-12:+12V out | F-MAIN:in | +12V main | 12-14 V | RD | TBD: Sparky sizes it; TDC-JH-144-12 = 110 A out (SOURCED) (10 AWG ≤30 A, 8 AWG ≤40 A, 6 AWG ≤60 A) | F-MAIN | P01 12V MAIN | CRITICAL | UNVERIFIED | gen.py; wiring_data power12 |
| P02 | 1 | wire | F-MAIN:out | 12V bus bar:+ | +12V main | 12-14 V | RD | TBD: same as P01 | F-MAIN | P02 12V BUS | CRITICAL | PROPOSED | gen.py |
| P03 | 1 | wire | 12V bus bar:+ | F-KEL:in -> Kelly PWR | +12V | 12 V | RD/WH | 16 | F-KEL | P03 KELLY PWR | CRITICAL | UNVERIFIED | gen.py fkel |
| P04 | 1 | wire | 12V bus bar:+ | F-COIL:in | +12V coil feed | 12 V | RD/WH | 18 | F-COIL | P04 COIL FEED | CRITICAL | PROPOSED | docs/ESTOP.md |
| P05 | 1 | wire | 12V bus bar:+ | F-EBOX:in (electronics box) | +12V | 12 V | RD/WH | 16 | F-EBOX | P05 EBOX 12V | CRITICAL | UNVERIFIED | gen.py fesp |
| P06 | 1 | wire | 12V bus bar:+ | F-TAB -> tablet USB-C PD | +12V | 12 V | RD/WH | 18 | F-TAB | P06 TABLET | LATER | UNVERIFIED | gen.py ftab |
| P07 | 1 | wire | F-EBOX:out | F-BUCK 3 A -> 5 V buck IN+ | +12V | 12 V | RD/WH | 18 | F-BUCK | P07 BUCK IN | CRITICAL | SOURCED | gen.py fbx |
| P08 | 1 | wire | 5 V buck OUT+ | TB-5V | +5V | 5.00 V | OR | 20 | F-BUCK | P08 5V | CRITICAL | PROPOSED | gen.py conv/v5 |
| P09 | 1 | board | Zener 1N5339B cathode | TB-5V (anode to TB-GND) | 5 V clamp | 5 V | — | lead | none | P09 ZENER | CRITICAL | SOURCED | gen.py z |
| G04 | 2 | board | Teensy A:GND | bms_master board star (signal ground) — NO wire to TB-GND (G04 to TB-GND dropped: DECIDED room Sparky + chief 2026-10-23 06:11). Teensy signal GND reaches coil_driver on J-SIG.1 in the gate cable; coil_driver J-GND -> TB-GND carries only the coil return (16 AWG+) | GND | 0 V | — | board | none | G04 TA GND STAR | CRITICAL | PROPOSED | room decision 2026-10-23 06:11 CT; docs/boards/bms_master.layout.json (Build 21) |
| G06 | 2 | board | Teensy B:GND | bms_master board star (signal ground) — same as G04 (PROPOSED Gilfoyle Build 21 for consistency with the 06:11 room decision; no separate wire to TB-GND) | GND | 0 V | — | board | none | G06 TB GND STAR | CRITICAL | PROPOSED | docs/boards/bms_master.layout.json (Build 21); consistency with G04 |
| P11 | 2 | wire | TB-5V | Teensy A:VIN (VIN/VUSB pad CUT) | +5V | 5 V | OR | 20 | F-BUCK | P11 TA VIN | CRITICAL | SOURCED | gen.py teensy; wiring_data power12 |
| P12 | 2 | wire | TB-5V | Teensy B:VIN (VIN/VUSB pad CUT) | +5V | 5 V | OR | 20 | F-BUCK | P12 TB VIN | CRITICAL | SOURCED | as P11 |
| C01 | 3 | board | Teensy A:22 TX / 23 RX + 3V3 + GND | SN65HVD230 #1 TXD/RXD/VCC/GND | CAN logic | 3.3 V | — | board | none | C01 A-CAN1 | CRITICAL | SOURCED | Config.h:174-175 |
| C02 | 3 | board | Teensy B:22/23 | SN65HVD230 #3 | CAN logic | 3.3 V | — | board | none | C02 B-CAN1 | CRITICAL | SOURCED | Config.h:174-175 |
| C03 | 3 | board | Teensy A:1 TX / 0 RX + 3V3 + GND | SN65HVD230 #2 | CAN logic | 3.3 V | — | board | none | C03 A-CAN2 | CRITICAL | SOURCED | FlexCanCompat.h:44 |
| C04 | 3 | board | Teensy B:1/0 | SN65HVD230 #4 | CAN logic | 3.3 V | — | board | none | C04 B-CAN2 | CRITICAL | SOURCED | FlexCanCompat.h:44 |
| C05 | 3 | board | Teensy A:31 TX / 30 RX | SN65HVD230 #5 + 120 Ω | CAN logic | 3.3 V | — | board | none | C05 A-CAN3 | CRITICAL | SOURCED | FlexCanCompat.h:45 |
| C06 | 3 | board | Teensy B:31/30 | SN65HVD230 #6 (no 120 Ω) | CAN logic | 3.3 V | — | board | none | C06 B-CAN3 | CRITICAL | SOURCED | FlexCanCompat.h:45 |
| C10 | 3 | wire | SN65 #5 CANH/CANL (A) | SN65 #6 CANH/CANL (B) -> Kelly -> Elcon (daisy chain) | CAN-V 250k | CAN | YE (H) / GN (L) twisted | 22 | none | C10 VEH CAN | CRITICAL | PROPOSED | FlexCanCompat.h:45; wiring_data bms |
| C11 | 4 | wire | SN65 #1 CANH/CANL | String 1 X2 pin 12 CAN_H (6435 WH/YE) / pin 11 CAN_L (6434 L-BU/YE) | CAN-A1 125k | CAN | YE/GN -> factory WH/YE, L-BU/YE | 22 | none | C11 S1 CAN | CRITICAL | UNVERIFIED | wiring_data bms net (H/L unconfirmed: swap if no cells) |
| C12 | 4 | wire | SN65 #2 CANH/CANL | String 2 X2 pins 12/11 | CAN-A2 125k | CAN | YE/GN | 22 | none | C12 S2 CAN | CRITICAL | UNVERIFIED | as C11 |
| C13 | 4 | wire | SN65 #3 CANH/CANL | String 3 X2 pins 12/11 | CAN-B1 125k | CAN | YE/GN | 22 | none | C13 S3 CAN | CRITICAL | UNVERIFIED | as C11 |
| C14 | 4 | wire | SN65 #4 CANH/CANL | String 4 X2 pins 12/11 | CAN-B2 125k | CAN | YE/GN | 22 | none | C14 S4 CAN | CRITICAL | UNVERIFIED | as C11 |
| G07 | 4 | wire | X2 pin 10 GND (470 BK/VT) x4 | TB-GND | GND | 0 V | BK/VT (factory) | factory | none | G07 BICM GND | CRITICAL | SOURCED | gen.py bo |
| G08 | 4 | wire | coil_driver J-GND.2 (5.08 mm screw terminal rated ≥10 A; on-board doubled 20 AWG solid bus to it) | TB-GND | GND (coil return: <1 A continuous, 7.6 A for 130 ms) | 0 V | BK | 16 | none | G08 COIL RET | CRITICAL | PROPOSED | chief 06:57 #5 + Sparky 06:57 #5 DECIDED (bms_master_answers.md); id G08 PROPOSED Gilfoyle Build 24 |
| P10 | 4 | wire | TB-5V | F-BICM 1 A -> BICM X2 pin 9 (705 GY/RD) x4 strings | +5V BICM | 5 V | GY/RD (factory) | factory | F-BICM | P10 BICM 5V | CRITICAL | UNVERIFIED | gen.py f705; wiring_data bms |
| S50 | 4 | wire | BICM 5139 fault loop | pull-up few kΩ -> 5 V | fault loop | 5 V | factory | factory | none | S50 5139 PULLUP | LATER | SOURCED | gen.py pu |
| G05 | 5 | wire | Teensy A:GND | ESP32:GND | GND | 0 V | BK | 22 | none | G05 TA-ESP GND | LATER | SOURCED | GATEWAY.md:19 |
| S30 | 5 | wire | Teensy A:17 (Serial4 TX) | ESP32:GPIO16 (UART2 RX) | UART 115200 8N1 | 3.3 V | WH | 22 | none | S30 TA17-ESP16 | LATER | SOURCED | GATEWAY.md:17; HubStatus.cpp:12; chief 06:57 #2 DECIDED: 3.3 V -> 3.3 V, no level shifter, TX-only by design; ESP32 5 V on VIN = power only |
| X01 | 5 | none | ESP32 Wi-Fi AP 192.168.4.1 | iPad / head unit | Wi-Fi (no wire) | — | — | — | none | X01 WIFI | LATER | SOURCED | GATEWAY.md |
| B01 | 6 | board | U1 /CLR (R5 10k -> 5 V, C5 1 µF, 1N4148) | U1 /CLR1 /CLR2 | power-on clear | 5 V | — | board | none | B01 U1 CLR | CRITICAL | PROPOSED | docs/THROTTLE_CLAMP.md |
| B02 | 6 | board | U3 P0W | U2a -> U2b (series 4066) | throttle | 0-5 V | — | board | none | B02 WIPER-4066 | CRITICAL | PROPOSED | docs/THROTTLE_CLAMP.md |
| B03 | 6 | board | U1 Q1 / Q2 | U2a ctl / U2b ctl | enables | 5 V | — | board | none | B03 Q-CTL | CRITICAL | PROPOSED | docs/THROTTLE_CLAMP.md |
| E01 | 6 | wire | F-COIL:out | E-stop block 1:11 + block 2:11 (jumper) | +12V coil feed | 12 V | RD/WH | 18 | F-COIL | E01 ESTOP FEED | CRITICAL | PROPOSED | docs/ESTOP.md:112-113 |
| E02 | 6 | wire | E-stop block 1:12 | EV200 B+ coil + AND EV200 B- coil + (high sides) | 12V coil (cut) | 12 V | RD/YE | 18 | F-COIL | E02 BLK1 MAIN COILS | CRITICAL | SOURCED | docs/ESTOP.md:112 (0.26 A break measured, OK) |
| E03 | 6 | wire | E-stop block 2:12 | precharge relay coil + AND Kelly KSI/PWR pin 7 | 12V coil + KSI (cut) | 12 V | RD/YE | 18 | F-COIL | E03 BLK2 PRE+KSI | CRITICAL | UNVERIFIED | docs/ESTOP.md:113 (KSI draw: Adam meters; Kelly pin 7 from manual) |
| E09 | 6 | wire | E-stop block 3:11 -> TB-GND; block 3:12 | Teensy B:9 (10k pull-up) — free on B per this map | e-stop sense (latch, never the cut) | 3.3 V | BU/BK | 22 | none | E09 BLK3 TB-ESTOP | CRITICAL | PROPOSED | crew 2026-10-22; docs/ESTOP.md; EstopLatch.h not wired (chief review #5) |
| G09 | 6 | wire | LEM A GND (Sparky 'pin C'; DHAB pin number UNVERIFIED vs S05/S06 numbering) | bms_master J-LEMA.2 -> own board-star hole (no daisy-chain) | GND | 0 V | BK | 22 | none | G09 LEMA GND | CRITICAL | PROPOSED | Sparky 06:57 #4 DECIDED; id PROPOSED Gilfoyle Build 24 |
| G10 | 6 | wire | LEM B GND (Sparky 'pin C'; pin number UNVERIFIED) | bms_master J-LEMB.2 -> own board-star hole | GND | 0 V | BK | 22 | none | G10 LEMB GND | CRITICAL | PROPOSED | Sparky 06:57 #4 DECIDED; id PROPOSED Gilfoyle Build 24 |
| G11 | 6 | wire | Pedal GND (pedal not chosen, S14 UNVERIFIED) | bms_master J-THR.2 -> own board-star hole | GND | 0 V | BK | 22 | none | G11 PEDAL GND | CRITICAL | PROPOSED | Sparky 06:57 #4 DECIDED; id PROPOSED Gilfoyle Build 24 |
| G12 | 6 | wire | Kelly KLS-8080N throttle RTN (KELLY-J pin number UNVERIFIED) | bms_master J-KEL.2 -> own board-star hole | GND (throttle return) | 0 V | BK | 22 | none | G12 KELLY RTN | CRITICAL | PROPOSED | Sparky 06:57 #4 DECIDED; id PROPOSED Gilfoyle Build 24 |
| K01 | 6 | wire | U2b out (R1 100k to GND — the ONLY pull-down) | Kelly throttle input (pin 3) | throttle | 0-5 V | BU/WH | 22 shielded | none | K01 KELLY THR | CRITICAL | UNVERIFIED | docs/THROTTLE_CLAMP.md; docs/KELLY_SETTINGS.md (100k, crew 2026-10-22); Kelly pin from manual |
| K02 | 6 | wire | Kelly pin 11 (12 V out) | Park pole 3 (NC) -> contact closed only while BOTH Teensys are alive (series heartbeat contacts) -> Kelly pin 15 Foot_SW | foot switch enable | 12 V | RD/WH -> RD/YE | 20 | none | K02 FOOTSW PARK | CRITICAL | UNVERIFIED | docs/PARK_BRAKE.md; docs/KELLY_SETTINGS.md:64 (Sparky) |
| K03 | 6 | wire | Kelly pin 11 12 V -> relay 30; relay 87a (NC) | Kelly pin 22 ECO | Low cap (fail-safe: relay NC, dead Teensy = Low) | 12 V | RD/WH -> RD/VT | 20 | none | K03 ECO LOWCAP | CRITICAL | UNVERIFIED | docs/DRIVE_MODES.md; docs/KELLY_SETTINGS.md:67; Kelly pin 22 from manual |
| P20 | 6 | wire | TB-5V | pedal 5 V + U1..U4 VCC (shared rail) | +5V | 5 V | OR | 20 | F-BUCK | P20 5V CLAMP | CRITICAL | PROPOSED | docs/THROTTLE_CLAMP.md |
| S01 | 6 | board | Teensy A:11 via 100 Ω (10k pull-down to GND) | FET Q-B+ gate; drain -> EV200 B+ coil - (no external diode: EV200 economizer clamps, DECIDED Sparky 2026-10-23 05:41) | coil drive | 3.3 V gate / 12 V coil | RD/BU (coil lead) | 18 (coil lead) | F-COIL | S01 TA11 MAIN+ | CRITICAL | PROPOSED | Config.h:182; wiring_data precharge net |
| S02 | 6 | board | Teensy A:12 via 100 Ω (10k pull-down) | FET Q-PRE gate; drain -> AEV14012 precharge coil - (D-PRE 1N4007 across the coil AT the relay, cathode to coil + (DECIDED chief 06:57 #6), no zener: DECIDED Sparky 2026-10-23 05:41) | coil drive | 3.3 / 12 V | RD/GN | 18 | F-COIL | S02 TA12 PRE | CRITICAL | PROPOSED | Config.h:183 |
| S03 | 6 | board | Teensy A:21 via 100 Ω (10k pull-down) | FET Q-B- gate; drain -> EV200 B- coil - (no external diode: EV200 economizer clamps, DECIDED Sparky 2026-10-23 05:41) | coil drive | 3.3 / 12 V | RD/BK | 18 | F-COIL | S03 TA21 MAIN- | CRITICAL | PROPOSED | Config.h:184 |
| S05 | 6 | wire | LEM A DHAB pin 4 (ch1) | 10k/18k + 100 nF -> Teensy A:14 | LEM A low range | 0-5 V -> 0-3.2 V | BU | 22 | none | S05 LEMA CH1 | CRITICAL | SOURCED | Config.h:187 |
| S06 | 6 | wire | LEM A DHAB pin 3 (ch2) | 10k/18k + 100 nF -> Teensy A:15 | LEM A high range | 0-5 V -> 0-3.2 V | BU/WH | 22 | none | S06 LEMA CH2 | CRITICAL | SOURCED | Config.h:188 |
| S07 | 6 | wire | TB-5V -> LEM A 5 V; sense via 10k/18k 1 % divider | Teensy A:25 | LEM supply ref | 5 V -> 3.2 V | OR | 22 | F-BUCK | S07 LEMA VC | CRITICAL | SOURCED | Config.h:189; divider 10k/18k 1 % (same ratio as the LEM outputs) DECIDED Sparky 06:57 #6 |
| S10 | 6 | board | Teensy A:35 DATA / 36 CLK / 38 CS | U4 74AHCT125 -> U3 MCP4131 SDI/SCK/CS | bit-bang SPI | 3.3 -> 5 V | — | board | none | S10 TA SPI | CRITICAL | PROPOSED | Config.h:221-236 |
| S11 | 6 | board | Teensy A:4 (R6 10k pull-down) | U4 buffer -> U1 B1 | A alive 1 kHz | 3.3 -> 5 V | VT | 22 | none | S11 TA4 ALIVE | CRITICAL | PROPOSED | Config.h:243 |
| S12 | 6 | board | Teensy B:7 (R4 10k pull-down) | U1 B2 | B alive 1 kHz | 3.3 V | VT/WH | 22 | none | S12 TB7 ALIVE | CRITICAL | PROPOSED | Config.h:218 |
| S14 | 6 | wire | Pedal wiper (Hall, 0-5 V ASSUMED) | 10k/18k + 100 nF -> Teensy A:40 and MCP4131 P0A | throttle | 0-5 V -> 0-3.2 V | BU | 22 | none | S14 PEDAL A | CRITICAL | UNVERIFIED | Config.h:215; docs/THROTTLE_CLAMP.md (pedal not chosen; chief review #9 calibration) |
| S15 | 6 | wire | Pedal track 2 | own 10k/18k + 100 nF -> Teensy B:41 (A17); A reads track 1 on pin 40 | throttle B | 0-3.2 V | BU/WH | 22 | none | S15 PEDAL B | CRITICAL | PROPOSED | crew 2026-10-22 (track 2 on B:41); Config.h PEDAL_IN_B |
| S16 | 6 | wire | Park switch pole 1 (NO) -> GND | Teensy A:32 (10k pull-up 3V3 + 100 nF) | park A, LOW = set | 3.3 V | BU/BK | 22 | none | S16 PARK A | CRITICAL | PROPOSED | Config.h:249; docs/PARK_BRAKE.md (switch fit UNVERIFIED) |
| S17 | 6 | wire | Park switch pole 2 (NO) -> GND | Teensy B:32 | park B | 3.3 V | BU/RD | 22 | none | S17 PARK B | CRITICAL | PROPOSED | Config.h:253 |
| S20 | 6 | board | Teensy A:20 (100 Ω, 10k pull-down) | low-side FET -> Low-cap relay coil 85 (86 = vehicle 12 V bus via F-COIL, 1N4007) — coil NEVER on Kelly pin 11 (40 mA max) | Low-cap release | 3.3 V / 12 V | — | board | F-COIL | S20 TA20 LOWCAP | CRITICAL | PROPOSED | Config.h:167; docs/DRIVE_MODES.md; docs/KELLY_SETTINGS.md:71 |
| S25 | 6 | wire | LEM B DHAB pin 4 | divider -> Teensy B:14 | LEM B low | 0-3.2 V | BU | 22 | none | S25 LEMB CH1 | CRITICAL | SOURCED | Config.h:187 |
| S26 | 6 | wire | LEM B DHAB pin 3 | divider -> Teensy B:15 | LEM B high | 0-3.2 V | BU/WH | 22 | none | S26 LEMB CH2 | CRITICAL | SOURCED | Config.h:188 |
| S27 | 6 | wire | LEM B 5 V sense via 10k/18k 1 % divider | Teensy B:25 | LEM supply ref | 3.2 V | OR | 22 | F-BUCK | S27 LEMB VC | CRITICAL | SOURCED | Config.h:189; divider 10k/18k 1 % (same ratio as the LEM outputs) DECIDED Sparky 06:57 #6 |
| S40 | 6 | wire | 12V bus via divider (PROPOSED 47k/10k + 100 nF) | Teensy B:26 (A12) — free on B per this map | 12 V sense | ≤3.3 V | BU | 22 | PROPOSED 1 A | S40 12V SENSE | CRITICAL | PROPOSED | crew 2026-10-22; wiring_data power12 |
| S41 | 6 | wire | LM339 ch1 OUT1 (pin 2, open collector) — the ONE shared LM339 (DECIDED re-pin; NOT OWNED: pending purchase approval) | Teensy A:33 with its own 10k pull-up to Teensy A 3.3 V + 100 nF, LOW = plugged | plug-in detect A (drive blocked on A when plugged, like park) | 3.3 V pin (A rail) | BU/GN | 22 | none (signal; 5 V feed already fused) | S41 PLUG A33 | CRITICAL | PROPOSED | Adam t284 2026-10-22 (one comparator for both boards); Sparky 2026-10-22 (network values); chief 2026-10-23 06:57 #1 DECIDED (bms_master_answers.md): LM393 -> LM339 re-pin; Sparky 2026-10-23 06:57 #1/#3 DECIDED (bms_master_answers.md); LM339 pinout SOURCED from the TI LM339 datasheet (1 OUT2, 2 OUT1, 3 VCC, 4/5 IN1−/+, 6/7 IN2−/+, 8/9 IN3−/+, 10/11 IN4−/+, 12 GND, 13 OUT4, 14 OUT3); chief review #4 (in.plugIn never set) |
| S42 | 6 | wire | Stock Teryx key switch SWITCHED output (SOURCED Adam 2026-10-22; wire colour UNVERIFIED — Adam backprobes: 0 V key off, 12 V key on), fed from the TDC-JH 12 V bus, through its OWN 1 A fuse (Sparky 2026-10-22) -> opto ch1 LED (PROPOSED PC827 + 1k5) | opto ch1 transistor -> Teensy A:34 (10k pull-up 3V3 + 100 nF), LOW = key on | KEY-ON A | 12 V in / 3.3 V pin | BU/YE | 22 | own 1 A fuse (Sparky 2026-10-22, PROPOSED) | S42 KEY A34 | CRITICAL | PROPOSED | Adam 2026-10-22 (stock key switch source); Sparky 2026-10-22; chief review #4 (key-on is serial e only) |
| S43 | 6 | wire | Same stock key switch switched output (after the S42 fuse) -> opto ch2 LED (PROPOSED PC827 + 1k5) | opto ch2 transistor -> Teensy B:34 (10k pull-up 3V3 + 100 nF), LOW = key on | KEY-ON B | 12 V in / 3.3 V pin | BU/YE | 22 | shares S42 fuse | S43 KEY B34 | CRITICAL | PROPOSED | Sparky 2026-10-22 |
| S44 | 6 | wire | LM339 ch2 OUT2 (pin 1, open collector) — same chip as S41 | Teensy B:33 with its own 10k pull-up to Teensy B 3.3 V + 100 nF, LOW = plugged | plug-in detect B mirror (drive blocked on B when plugged, like park) | 3.3 V pin (B rail) | BU/GN | 22 | none (signal; 5 V feed already fused) | S44 PLUG B33 | CRITICAL | PROPOSED | Adam t284 2026-10-22 (one comparator for both boards); Sparky 2026-10-22 (network values); chief 2026-10-23 06:57 #1 DECIDED (bms_master_answers.md): LM393 -> LM339 re-pin; Sparky 2026-10-23 06:57 #1/#3 DECIDED (bms_master_answers.md); LM339 pinout SOURCED from the TI LM339 datasheet (1 OUT2, 2 OUT1, 3 VCC, 4/5 IN1−/+, 6/7 IN2−/+, 8/9 IN3−/+, 10/11 IN4−/+, 12 GND, 13 OUT4, 14 OUT3) |
| S45 | 6 | wire | NACS inlet PROXIMITY pin (SOURCED Adam 2026-10-22: ~150 Ω latched / ~480 Ω button pressed = plugged, open = not plugged; plug side returns to inlet PE) | proximity node: 330 Ω pull-up to the shared 5 V + 2.7k to GND (Sparky; same values as the J1772 vehicle side) | plug proximity (one network for both boards) | 0–5 V (open ≈4.4 V, 480 Ω ≈2.8 V, 150 Ω ≈1.5 V, calculated) | BU/GN | 22 | none (330 Ω limits it to 15 mA) | S45 PROX NET | CRITICAL | PROPOSED | Adam t284 2026-10-22 (one comparator for both boards); Sparky 2026-10-22 (network values); chief 2026-10-23 06:57 #1 DECIDED (bms_master_answers.md): LM393 -> LM339 re-pin; Sparky 2026-10-23 06:57 #1/#3 DECIDED (bms_master_answers.md); LM339 pinout SOURCED from the TI LM339 datasheet (1 OUT2, 2 OUT1, 3 VCC, 4/5 IN1−/+, 6/7 IN2−/+, 8/9 IN3−/+, 10/11 IN4−/+, 12 GND, 13 OUT4, 14 OUT3) |
| S46 | 6 | wire | proximity node -> 10k (top) / 20k (to GND) divider | LM339 IN+ of ALL FOUR channels (pins 5, 7, 9, 11 tied: ch1/ch2 plug A/B, ch3/ch4 release A/B) — one divider feeds all | divided proximity (×2/3, keeps it inside the LM339 input range at 5 V) | ≤3 V (open ≈2.9 V, 480 Ω ≈1.8 V, 150 Ω ≈1.0 V, calculated) | BU/GN | 22 | none | S46 PROX DIV | CRITICAL | PROPOSED | Adam t284 2026-10-22 (one comparator for both boards); Sparky 2026-10-22 (network values); chief 2026-10-23 06:57 #1 DECIDED (bms_master_answers.md): LM393 -> LM339 re-pin; Sparky 2026-10-23 06:57 #1/#3 DECIDED (bms_master_answers.md); LM339 pinout SOURCED from the TI LM339 datasheet (1 OUT2, 2 OUT1, 3 VCC, 4/5 IN1−/+, 6/7 IN2−/+, 8/9 IN3−/+, 10/11 IN4−/+, 12 GND, 13 OUT4, 14 OUT3) |
| S47 | 6 | wire | shared 5 V rail (5 V terminal block, same rail as the throttle clamp chips) | LM339 VCC pin 3 (+ 100 nF to GND pin 12), the 330 Ω proximity pull-up and both reference dividers | LM339 + proximity supply | 5 V | OR | 22 | 5 V rail (3 A mini blade upstream) | S47 LM339 5V | CRITICAL | PROPOSED | Adam t284 2026-10-22 (one comparator for both boards); Sparky 2026-10-22 (network values); chief 2026-10-23 06:57 #1 DECIDED (bms_master_answers.md): LM393 -> LM339 re-pin; Sparky 2026-10-23 06:57 #1/#3 DECIDED (bms_master_answers.md); LM339 pinout SOURCED from the TI LM339 datasheet (1 OUT2, 2 OUT1, 3 VCC, 4/5 IN1−/+, 6/7 IN2−/+, 8/9 IN3−/+, 10/11 IN4−/+, 12 GND, 13 OUT4, 14 OUT3) |
| S48 | 6 | wire | 2.5 V plug reference (10k/10k from the shared 5 V). NO hysteresis resistors: 100 nF at each Teensy pin + 20 ms firmware debounce (DECIDED Sparky 06:57 #3) | LM339 IN1− / IN2− (pins 4 and 6 tied; ch1/ch2 plug A/B) | 2.5 V threshold | 2.5 V | BU | 22 | none | S48 LM339 REF | CRITICAL | PROPOSED | Adam t284 2026-10-22 (one comparator for both boards); Sparky 2026-10-22 (network values); chief 2026-10-23 06:57 #1 DECIDED (bms_master_answers.md): LM393 -> LM339 re-pin; Sparky 2026-10-23 06:57 #1/#3 DECIDED (bms_master_answers.md); LM339 pinout SOURCED from the TI LM339 datasheet (1 OUT2, 2 OUT1, 3 VCC, 4/5 IN1−/+, 6/7 IN2−/+, 8/9 IN3−/+, 10/11 IN4−/+, 12 GND, 13 OUT4, 14 OUT3) |
| S49 | 6 | wire | LM339 GND pin 12 (also the 2.7k, 20k and both reference bottoms) | single-point ground (bms_master board star, same as the Teensy A/B grounds) | LM339 ground | 0 V | BK | 22 | none | S49 LM339 GND | CRITICAL | PROPOSED | Adam t284 2026-10-22 (one comparator for both boards); Sparky 2026-10-22 (network values); chief 2026-10-23 06:57 #1 DECIDED (bms_master_answers.md): LM393 -> LM339 re-pin; Sparky 2026-10-23 06:57 #1/#3 DECIDED (bms_master_answers.md); LM339 pinout SOURCED from the TI LM339 datasheet (1 OUT2, 2 OUT1, 3 VCC, 4/5 IN1−/+, 6/7 IN2−/+, 8/9 IN3−/+, 10/11 IN4−/+, 12 GND, 13 OUT4, 14 OUT3) |
| S51 | 6 | board | LM339 ch3 OUT3 (pin 14, open collector) -> 1k series | Teensy A:2 with its own 10k pull-up to A 3.3 V + 100 nF (firmware must set A:2 INPUT; boots as output) | release-button detect A: LOW = plugged + latched, HIGH = pressed or unplugged | 3.3 V pin (A rail) | BU/YE | 22 | none (1k series) | S51 REL A2 | CRITICAL | PROPOSED | Adam t284 2026-10-22 (one comparator for both boards); Sparky 2026-10-22 (network values); chief 2026-10-23 06:57 #1 DECIDED (bms_master_answers.md): LM393 -> LM339 re-pin; Sparky 2026-10-23 06:57 #1/#3 DECIDED (bms_master_answers.md); LM339 pinout SOURCED from the TI LM339 datasheet (1 OUT2, 2 OUT1, 3 VCC, 4/5 IN1−/+, 6/7 IN2−/+, 8/9 IN3−/+, 10/11 IN4−/+, 12 GND, 13 OUT4, 14 OUT3); id S51 PROPOSED Gilfoyle Build 24 (S50 taken) |
| S52 | 6 | board | LM339 ch4 OUT4 (pin 13, open collector) -> 1k series | Teensy B:35 with its own 10k pull-up to B 3.3 V + 100 nF | release-button detect B (mirror) | 3.3 V pin (B rail) | BU/YE | 22 | none (1k series) | S52 REL B35 | CRITICAL | PROPOSED | Adam t284 2026-10-22 (one comparator for both boards); Sparky 2026-10-22 (network values); chief 2026-10-23 06:57 #1 DECIDED (bms_master_answers.md): LM393 -> LM339 re-pin; Sparky 2026-10-23 06:57 #1/#3 DECIDED (bms_master_answers.md); LM339 pinout SOURCED from the TI LM339 datasheet (1 OUT2, 2 OUT1, 3 VCC, 4/5 IN1−/+, 6/7 IN2−/+, 8/9 IN3−/+, 10/11 IN4−/+, 12 GND, 13 OUT4, 14 OUT3); id PROPOSED Gilfoyle Build 24 |
| S53 | 6 | board | 1.40 V release reference: 10k from the shared 5 V over 3.9k to GND (no hysteresis resistors) | LM339 IN3− / IN4− (pins 8 and 10 tied; ch3/ch4 release A/B) | 1.40 V threshold (latched ≈1.0 V below, pressed ≈1.8 V above) | 1.40 V (calculated) | BU | 22 | none | S53 REL REF | CRITICAL | PROPOSED | Adam t284 2026-10-22 (one comparator for both boards); Sparky 2026-10-22 (network values); chief 2026-10-23 06:57 #1 DECIDED (bms_master_answers.md): LM393 -> LM339 re-pin; Sparky 2026-10-23 06:57 #1/#3 DECIDED (bms_master_answers.md); LM339 pinout SOURCED from the TI LM339 datasheet (1 OUT2, 2 OUT1, 3 VCC, 4/5 IN1−/+, 6/7 IN2−/+, 8/9 IN3−/+, 10/11 IN4−/+, 12 GND, 13 OUT4, 14 OUT3); id PROPOSED Gilfoyle Build 24 |
| X02 | 6 | none | Teensy A/B park inputs | software Parked logic | software (no wire) | — | — | — | none | X02 SW PARK | CRITICAL | SOURCED | docs/PARK_BRAKE.md |
| H01 | 7 | wire | String 1..4 B+ | A6T200 fuse 1..4 (bus bar, insulated standoffs) | HV+ | ≤147.6 V | OR HV, RED tape | 1/0 (each A6T200 string lead, ~138 A at a 550 A launch) | F-S1..S4 | H01-H04 STRING n B+ | LATER | PROPOSED | docs/WIRING.md:66-69; crew 2026-10-22 |
| H05 | 7 | wire | 4 fuses | common B+ bus | HV+ | HV | OR HV, RED tape | 1/0 (string side of each fuse) | F-S1..S4 | H05 FUSE-BUS | LATER | PROPOSED | wiring_data overview net |
| H06 | 7 | wire | common B+ bus (NO MSD — service = pull 4 fuses) | EV200 B+ line | HV+ | HV | OR HV, RED tape | 3/0 owned TEMCo (pack-to-Kelly main) | F-S1..S4 | H06 BUS-EV200 | LATER | SOURCED | wiring_data overview net (MSD removed 2026-10-22) |
| H07 | 7 | wire | EV200 B+ load | Kelly B+ | HV+ | HV | OR HV, RED tape | 3/0 owned TEMCo (pack-to-Kelly main) | none | H07 EV200-KELLY+ | LATER | SOURCED | wiring_data |
| H08 | 7 | wire | precharge relay + 50 Ω 100 W | across EV200 B+ | HV precharge | HV | OR HV | 14 HV | none | H08 PRECHARGE | LATER | UNVERIFIED | wiring_data precharge net; precharge contactor = salvaged AEV14012, 0.35 A at 12 V coil (34 Ω, 4.2 W, no economizer; ≈0.41 A at 14.0 V; coil allowed 10-16 V; SOURCED Panasonic EV relay catalog, crew 2026-10-23) — crew 2026-10-22/23; https://industry.panasonic.com/ac/cdn/e/control/relay/vehicle/catalog/mech_eng_ev.pdf |
| H09 | 7 | wire | Kelly U/V/W + Sin/Cos | Zero 75-7R | motor | HV | OR HV | match bus | none | H09 MOTOR | LATER | SOURCED | wiring_data |
| H10 | 7 | wire | Kelly B- | EV200 B- -> LEM A/B -> string B- | HV- | HV | OR HV, BLACK tape | 3/0 owned TEMCo (pack-to-Kelly main) | none | H10 KELLY-EV200- | LATER | SOURCED | wiring_data |
| H11 | 7 | wire | String B- leads | through LEM A (S1+S2) / LEM B (S3+S4) | HV- | HV | OR HV, BLACK tape | 1/0 | none | H11 B- LEM | LATER | SOURCED | wiring_data overview |
| H12 | 7 | wire | HV bus | F-DCDC -> DC-DC HV in | HV | HV | OR HV | 14 HV | F-DCDC | H12 DCDC IN | LATER | UNVERIFIED | gen.py fdi |
| H13 | 7 | wire | Elcon out | F-ELC -> pack bus | HV | HV | OR HV | 6 HV (SOURCED crew 2026-10-23) | F-ELC | H13 ELCON OUT | LATER | SOURCED | crew 2026-10-23: Elcon HK-LF-144-46 46 A max out, F-ELC Mersen A6T60 60 A on 6 AWG (holder TBD) |
| H14 | 7 | wire | NACS inlet (AC) | Elcon AC in | AC 240 V | AC | per AC code | per charger | none | H14 AC IN | LATER | UNVERIFIED | wiring_data |
| S08 | 7 | wire | NOT FITTED (room DECIDED 2026-10-23): isolated HV sensor dropped for now; precharge uses Kelly CAN B+. Only if the Kelly B+ bench gate fails: AMC1311EVM isolated output (never a plain 60.6:1 divider) | Teensy A:24 (reserved spare, HV_ANALOG_ENABLE=0) | HV sense (isolated, not fitted) | ≤3.3 V | BU | 22 | PROPOSED 0.5 A HV-rated | S08 RESERVED | LATER | PROPOSED | room 2026-10-23 (DROP isolated sense); Sparky 2026-10-23 (never a plain divider); Config.h:196 |
| A01 | 8 | wire | 12V bus (switched) -> F-AUX | aux relay 30 | +12V load | 12 V | RD/WH | 14 (size to load) | F-AUX | A01 AUX FEED | LATER | PROPOSED | Adam 2026-10-22 (NO contacts, relay/fuse >1 A) |
| A02 | 8 | wire | 12V switched (small fuse) | NO push button (ZBE101) -> relay 86 | button | 12 V | RD/WH | 20 | PROPOSED 2 A | A02 AUX BTN | LATER | PROPOSED | Adam 2026-10-22 |
| A03 | 8 | wire | relay 85 | GND | GND | 0 V | BK | 20 | none | A03 RELAY GND | LATER | PROPOSED | standard automotive relay |
| A04 | 8 | wire | relay 87 | horn / lights + | +12V load | 12 V | RD/GN | 14 (size to load) | F-AUX | A04 AUX LOAD | LATER | PROPOSED | Adam 2026-10-22 |
| A05 | 8 | wire | load - | GND | GND | 0 V | BK | 14 | none | A05 LOAD GND | LATER | PROPOSED | standard |

## Open UNVERIFIED / UNDEFINED wires (17)

- **P01** UNVERIFIED: DC-DC TDC-JH-144-12:+12V out → F-MAIN:in (gen.py; wiring_data power12)
- **P03** UNVERIFIED: 12V bus bar:+ → F-KEL:in -> Kelly PWR (gen.py fkel)
- **P05** UNVERIFIED: 12V bus bar:+ → F-EBOX:in (electronics box) (gen.py fesp)
- **P06** UNVERIFIED: 12V bus bar:+ → F-TAB -> tablet USB-C PD (gen.py ftab)
- **P10** UNVERIFIED: TB-5V → F-BICM 1 A -> BICM X2 pin 9 (705 GY/RD) x4 strings (gen.py f705; wiring_data bms)
- **C11** UNVERIFIED: SN65 #1 CANH/CANL → String 1 X2 pin 12 CAN_H (6435 WH/YE) / pin 11 CAN_L (6434 L-BU/YE) (wiring_data bms net (H/L unconfirmed: swap if no cells))
- **C12** UNVERIFIED: SN65 #2 CANH/CANL → String 2 X2 pins 12/11 (as C11)
- **C13** UNVERIFIED: SN65 #3 CANH/CANL → String 3 X2 pins 12/11 (as C11)
- **C14** UNVERIFIED: SN65 #4 CANH/CANL → String 4 X2 pins 12/11 (as C11)
- **E03** UNVERIFIED: E-stop block 2:12 → precharge relay coil + AND Kelly KSI/PWR pin 7 (docs/ESTOP.md:113 (KSI draw: Adam meters; Kelly pin 7 from manual))
- **S14** UNVERIFIED: Pedal wiper (Hall, 0-5 V ASSUMED) → 10k/18k + 100 nF -> Teensy A:40 and MCP4131 P0A (Config.h:215; docs/THROTTLE_CLAMP.md (pedal not chosen; chief review #9 calibration))
- **K01** UNVERIFIED: U2b out (R1 100k to GND — the ONLY pull-down) → Kelly throttle input (pin 3) (docs/THROTTLE_CLAMP.md; docs/KELLY_SETTINGS.md (100k, crew 2026-10-22); Kelly pin from manual)
- **K02** UNVERIFIED: Kelly pin 11 (12 V out) → Park pole 3 (NC) -> contact closed only while BOTH Teensys are alive (series heartbeat contacts) -> Kelly pin 15 Foot_SW (docs/PARK_BRAKE.md; docs/KELLY_SETTINGS.md:64 (Sparky))
- **K03** UNVERIFIED: Kelly pin 11 12 V -> relay 30; relay 87a (NC) → Kelly pin 22 ECO (docs/DRIVE_MODES.md; docs/KELLY_SETTINGS.md:67; Kelly pin 22 from manual)
- **H08** UNVERIFIED: precharge relay + 50 Ω 100 W → across EV200 B+ (wiring_data precharge net; precharge contactor = salvaged AEV14012, 0.35 A at 12 V coil (34 Ω, 4.2 W, no economizer; ≈0.41 A at 14.0 V; coil allowed 10-16 V; SOURCED Panasonic EV relay catalog, crew 2026-10-23) — crew 2026-10-22/23; https://industry.panasonic.com/ac/cdn/e/control/relay/vehicle/catalog/mech_eng_ev.pdf)
- **H12** UNVERIFIED: HV bus → F-DCDC -> DC-DC HV in (gen.py fdi)
- **H14** UNVERIFIED: NACS inlet (AC) → Elcon AC in (wiring_data)
