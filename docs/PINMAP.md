# Master pin map (GENERATED — edit docs/PINMAP.yaml, then `python3 tools/build_pinmap.py`)

Status: SOURCED (file:line or doc) · ASSUMED · PROPOSED · UNVERIFIED · UNDEFINED (nothing chosen yet).
Checked against `include/Config.h` and `include/FlexCanCompat.h` by `scripts/test_pinmap.py`.

- **A**: Teensy 4.1 A, env teensy41_dual_a (TEENSY_ROLE=0) — BMS master, contactors, throttle
- **B**: Teensy 4.1 B, env teensy41_dual_b (TEENSY_ROLE=1) — watchdog, can only open/cap
- **E**: ESP32 DevKit gateway (teryx-ev-hub firmware/esp32-gateway), read-only

## Pin conflicts and undefined pins (fix or accept before wiring)

1. PROPOSED (Sparky 2026-10-22): KEY-ON on pin 34 (IN1 spare) on A and B — no free GPIO on A; IN2-IN4 deferred. LIGHT_OE (retired) also names 34; nothing drives it. Firmware does not read key-on or plug-in yet (later commit, chief #4).
2. RESOLVED (Adam 2026-10-22): plug-in detect B mirror = S44 on B:33 (free per this map and Config.h B spare list).
3. RESOLVED: IN1/IN2 were Serial4 17/16 — IN1 moved to pin 34 (free on A and B), IN2 retired (-1). A pin 16 (RX4) stays unconnected on purpose: the gateway is receive-only (TX17 -> ESP32 GPIO16).
4. RESOLVED: IN3/IN4 were I2C 18/19 — retired (-1); 18/19 are only I2C_SDA/I2C_SCL now. No other free GPIO exists on A (33 reserved charge detect).
5. INTENDED (not a conflict): A uses pins 4/40 (A_THR_ALIVE, PEDAL_IN_A), B uses 7/41 (B_THR_ALIVE, PEDAL_IN_B). Pin 7 = RAD_FAN on A, 41 = POS_4WD on A, 4 = B_FAULT_OUT on B, 40 = ST_4WD_B on B — different boards; label harness wires TA/TB.
6. RESOLVED (PROPOSED pins, free on B per this map): e-stop block 3 -> B pin 9; 12 V low alarm -> B pin 26 (A12). No firmware const yet.
7. ESP32 GPIO17: teryx-ev-hub firmware/esp32-gateway/docs/PINMAP.md says GPIO17 TX <- Teensy RX 16 (crossed pair); GATEWAY.md:18 says leave unconnected (RX-only). This map follows GATEWAY.md. Hub PINMAP.md is stale (other repo, not edited).
8. FIXED: src/HubStatus.cpp:14 and include/Config.h:37 stale CAN comments corrected to CAN_LAYOUT_V2.
9. FIXED: old wiring overview drew park on 'pin 18'; park is pin 32 on A and B.
10. RESOLVED: precharge contactor = salvaged AEV14012 (~3 A peak coil, OK); TE EV11 dropped.
11. RESOLVED: HV gauge — string leads from each A6T200 = 1/0 (~138 A at a 550 A launch); pack-to-Kelly mains = owned 3/0.
12. RESOLVED: SN65HVD230 — 4 owned enough for the one-string bench; 2 extra on the buy list for Gate 3 (no purchase now).
13. RESOLVED: PEDAL_IN_B = B pin 41, pedal track 2 via 10k/18k; A reads track 1 on pin 40.
14. Kelly pin 3: ONE pull-down, the existing 100k (R1). Sparky's 10k NOT added — PROPOSED bench option only if pin 3 floats, after a full-voltage check through the 4066s.
15. DC-DC on the shelf = Elcon TDC-JH-144-12 (not TDC-IY). F-MAIN and P01/P02 gauge TBD until its rated output is read.

## Teensy A pins

| Pin | Firmware name | Use | Wire | Tier | Status | Source |
|---|---|---|---|---|---|---|
| 0 |  | CAN2 RX -> SN65 #2 (string 2) | C03 |  | SOURCED | FlexCanCompat.h:44 |
| 1 |  | CAN2 TX -> SN65 #2 | C03 |  | SOURCED | FlexCanCompat.h:44 |
| 2 | OUT5 | OUT5 spare output (pinMode LOW at boot, BmsApp.cpp) |  |  | SOURCED | Config.h:180 |
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
| 18 | I2C_SDA | MCP23017 SDA (body expander, LATER) |  | LATER | PROPOSED | Config.h:245 |
| 19 | I2C_SCL | MCP23017 SCL |  | LATER | PROPOSED | Config.h:246 |
| 20 | OUT3 | Kelly Low-cap relay FET gate: HIGH = release cap, LOW/dead = capped | S20 |  | SOURCED | Config.h:176; docs/DRIVE_MODES.md |
| 21 | MAIN_NEG | EV200 B- coil FET gate (OUT4) | S03 |  | SOURCED | Config.h:195 |
| 21 | OUT4 | alias of MAIN_NEG | S03 |  | SOURCED | Config.h:177 |
| 22 | CAN1_TX | CAN1 TX -> SN65 #1 (string 1) | C01 |  | SOURCED | Config.h:185 |
| 23 | CAN1_RX | CAN1 RX | C01 |  | SOURCED | Config.h:186 |
| 24 | HV_SENSE | optional HV bus divider (HV_ANALOG_ENABLE=0) | S08 | LATER | UNVERIFIED | Config.h:196 |
| 25 | CUR_VC | LEM A 5 V supply sense (ratiometric) via divider | S07 |  | SOURCED | Config.h:200 |
| 26 | IS_4WD | BTS7960 #1 current sense |  | LATER | PROPOSED | Config.h:229 |
| 27 | IS_DIFF | BTS7960 #2 current sense |  | LATER | PROPOSED | Config.h:230 |
| 28 | ACTDIF_IN1 | diff-lock H-bridge IN1 |  | LATER | PROPOSED | Config.h:216 |
| 29 | ACTDIF_IN2 | diff-lock H-bridge IN2 |  | LATER | PROPOSED | Config.h:217 |
| 30 |  | CAN3 RX vehicle bus -> SN65 #5 | C05 |  | SOURCED | FlexCanCompat.h:45 |
| 31 |  | CAN3 TX vehicle bus | C05 |  | SOURCED | FlexCanCompat.h:45 |
| 32 | PARK_IN_A | park brake contact 1 (direct GPIO, 10k pull-up, LOW = set) | S16 |  | SOURCED | Config.h:260 (BODY_IO_EXPANDER=1) |
| 33 |  | CHARGE PLUG-IN detect from NACS inlet PROXIMITY (via pilot/proximity board plug-present output -> opto, 10k pull-up, LOW = plugged). Plugged = drive blocked on both boards (like park). Feeds BmsCore plugIn (chief #4); firmware read = later commit | S41 | CRITICAL | PROPOSED | Config.h:254 |
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
| 33 |  | CHARGE PLUG-IN mirror (own opto channel from the same plug-present output, 10k pull-up, LOW = plugged). Plugged = drive blocked on B too (like park). B:33 free per this map (Config.h spare list 33-39). Firmware read = later commit | S44 | CRITICAL | PROPOSED | Adam 2026-10-22 |
| 34 | IN1 | KEY-ON mirror (own opto channel, 10k pull-up, LOW = key on) — reuses IN1 spare, pin 34 free on B. Firmware read = later commit | S43 | CRITICAL | PROPOSED | Config.h:170 |

## ESP32 gateway pins

| Pin | Firmware name | Use | Wire | Tier | Status | Source |
|---|---|---|---|---|---|---|
| GPIO16 |  | UART2 RX <- Teensy A pin 17 | S30 |  | SOURCED | teryx-ev-hub firmware/esp32-gateway/docs/GATEWAY.md:17 |
| GPIO17 |  | UART2 TX — UNUSED, leave unconnected (gateway is RX-only, TX pin = -1) |  |  | SOURCED | GATEWAY.md:5,18 |
| GND |  | common LV ground with Teensy A | G05 |  | SOURCED | GATEWAY.md:19 |
| 5V/VIN |  | 5 V from fused buck (never Teensy 3.3 V) | P08 |  | SOURCED | GATEWAY.md:20 |

## CAN buses

| Bus | Board | Ctrl | TX | RX | Bitrate | Nodes | IDs | Termination | Transceiver | Status | Source |
|---|---|---|---|---|---|---|---|---|---|---|---|
| CAN-A1 | A | CAN1 | 22 | 23 | 125000 | Teensy A + String 1 BICMs (M2 24S + M4 c1-12) | 0x460-0x47E BICM reports; 0x200/0x300/0x310 keep-alive/balance (TX only if flags) | 120 Ω at Teensy transceiver + BICM end (BICM-end built-in UNVERIFIED: meter ~60 Ω powered off) | SN65HVD230 #1 | SOURCED | include/FlexCanCompat.h:43; include/Config.h:9; include/StringConfig.h:11-15 |
| CAN-A2 | A | CAN2 | 1 | 0 | 125000 | Teensy A + String 2 BICMs (M3 24S + M4) | same factory IDs as string 1 (own bus) | 120 Ω both ends (as CAN-A1) | SN65HVD230 #2 | SOURCED | include/FlexCanCompat.h:44 |
| CAN-B1 | B | CAN1 | 22 | 23 | 125000 | Teensy B + String 3 BICMs | factory IDs | 120 Ω both ends | SN65HVD230 #3 | SOURCED | include/FlexCanCompat.h:43 |
| CAN-B2 | B | CAN2 | 1 | 0 | 125000 | Teensy B + String 4 BICMs | factory IDs | 120 Ω both ends | SN65HVD230 #4 | SOURCED | include/FlexCanCompat.h:44 |
| CAN-V | A+B | CAN3 | 31 | 30 | 250000 | Teensy A (end, 120 Ω) · Teensy B (stub, no 120 Ω) · Kelly · Elcon (far end 120 Ω, built-in UNVERIFIED) | Kelly 0x0CF11E05/0x0CF11F05 (include/KellyLink.h:4); Elcon 0x1806E5F4 / 0x18FF50E5 (Config.h:88, StringConfig.h:81); link 0x6A0-0x6A4 B->A, 0x6A8-0x6AA A->B (StringConfig.h:83-93) | 120 Ω at Teensy A and at the far end only | SN65HVD230 #5 (A), #6 (B) — 4 owned cover the one-string bench; 2 extra on the buy list for Gate 3 (no purchase now) | SOURCED | include/FlexCanCompat.h:45 |

## Connectors

| ID | What | Pins | Status | Source |
|---|---|---|---|---|
| X2-S1..S4 | Volt BICM harness K16 X2 (one per string) | 9 = 5 V (705 GY/RD) · 10 = GND (470 BK/VT) · 11 = CAN_L (6434 L-BU/YE) · 12 = CAN_H (6435 WH/YE) | SOURCED | tools/wiring_data.js bms page; schematic/gen.py 'bo' (CAN H/L UNVERIFIED: swap if no cells) |
| KELLY-J | Kelly KLS-8080N signal connector | 3 throttle · 7 KSI/PWR · 11 12 V out · 15 Foot_SW · 22 ECO · 2 Brake_AN | UNVERIFIED | KLS-8080N V2.11 / HC V2.14 manuals via docs/ESTOP.md:18, wiring_data.js CONFLICTS; Adam's harness unread |
| ESTOP | XB5AS8442 (or XB4BS8442) head + 2x ZBE102 = 3 NC blocks | each block: terminals 11 (in) / 12 (out), NC | SOURCED | docs/ESTOP.md; se.com XB5AS8442 page (1 NC in head) |
| EBOX-DT | Electronics box Deutsch DT harness connector | UNDEFINED — pin-out not assigned yet | UNDEFINED | schematic/gen.py 'dt' |
| TB-5V | 5 V terminal block | Teensy A VIN, Teensy B VIN, transceivers? (SN65 run on 3.3 V from each Teensy), LEM 5 V, pedal 5 V, clamp chips | PROPOSED | tools/wiring_data.js power12/clamp |
| TB-GND | GND terminal block (single point) | all LV grounds | SOURCED | schematic/gen.py 'gnd' |

## Fuses

| ID | Where | Rating | Status | Source |
|---|---|---|---|---|
| F-MAIN | DC-DC 12 V out | TBD ≈1.25 × TDC-JH-144-12 rated output (Adam reads label) | PROPOSED | Sparky 2026-10-22 |
| F-COIL | 12 V coil feed -> e-stop blocks 1+2 | 10 A | PROPOSED | Sparky 2026-10-22 |
| F-KEL | Kelly PWR logic | 2 A | PROPOSED | Sparky 2026-10-22 |
| F-EBOX | electronics box 12 V | 5 A (F-BUCK 3 A after it) | PROPOSED | Sparky 2026-10-22 |
| F-BUCK | 12 V -> 5 V buck | 3 A mini blade | SOURCED | gen.py fbx |
| F-BICM | 5 V -> BICM 705 feed | 1 A (resize after measuring BICM draw) | UNVERIFIED | gen.py f705 |
| F-TAB | tablet USB-C PD feed | UNDEFINED | UNVERIFIED | gen.py ftab |
| F-AUX | each aux load (horn, lights) via relay 30 | size to load, PROPOSED 10-15 A | PROPOSED | Adam 10/22 NO aux buttons through relay/fuse |
| F-S1..S4 | string fuses on bus bar (service disconnect) | Mersen A6T200 200 A Class T | SOURCED | docs/WIRING.md:66-69 |
| F-DCDC | HV in to DC-DC | 15-20 A >=250 V DC (confirm label) | UNVERIFIED | gen.py fdi |
| F-ELC | Elcon out -> pack | ~40-60 A >=250 V DC | UNVERIFIED | gen.py fel |

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
- 12 V main (DC-DC out): TBD — Adam reads TDC-JH-144-12 rated output; 10 AWG ≤30 A, 8 AWG ≤40 A, 6 AWG ≤60 A (Sparky)
- HV: 1/0 AWG string leads from each A6T200 (~138 A at a 550 A launch); 3/0 owned TEMCo for pack-to-Kelly mains (crew 2026-10-22)

## Every wire

| Wire | Step | Kind | From | To | Signal | Voltage | Colour | AWG | Fuse | Label | Tier | Status | Source |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| G01 | 1 | wire | 12V bus bar:- | frame ground strap | GND | 0 V | BK | strap | none | G01 12V- CHASSIS | CRITICAL | PROPOSED | wiring_data power12 net (Mechanic) |
| G02 | 1 | wire | Kelly case | frame | GND | 0 V | BK | strap | none | G02 KELLY CASE | CRITICAL | PROPOSED | wiring_data power12 |
| G03 | 1 | wire | 5 V buck OUT- | TB-GND | GND | 0 V | BK | 20 | none | G03 BUCK GND | CRITICAL | PROPOSED | gen.py gnd |
| P01 | 1 | wire | DC-DC TDC-JH-144-12:+12V out | F-MAIN:in | +12V main | 12-14 V | RD | TBD: Adam reads TDC-JH-144-12 rated output (10 AWG ≤30 A, 8 AWG ≤40 A, 6 AWG ≤60 A) | F-MAIN | P01 12V MAIN | CRITICAL | UNVERIFIED | gen.py; wiring_data power12 |
| P02 | 1 | wire | F-MAIN:out | 12V bus bar:+ | +12V main | 12-14 V | RD | TBD: same as P01 | F-MAIN | P02 12V BUS | CRITICAL | PROPOSED | gen.py |
| P03 | 1 | wire | 12V bus bar:+ | F-KEL:in -> Kelly PWR | +12V | 12 V | RD/WH | 16 | F-KEL | P03 KELLY PWR | CRITICAL | UNVERIFIED | gen.py fkel |
| P04 | 1 | wire | 12V bus bar:+ | F-COIL:in | +12V coil feed | 12 V | RD/WH | 18 | F-COIL | P04 COIL FEED | CRITICAL | PROPOSED | docs/ESTOP.md |
| P05 | 1 | wire | 12V bus bar:+ | F-EBOX:in (electronics box) | +12V | 12 V | RD/WH | 16 | F-EBOX | P05 EBOX 12V | CRITICAL | UNVERIFIED | gen.py fesp |
| P06 | 1 | wire | 12V bus bar:+ | F-TAB -> tablet USB-C PD | +12V | 12 V | RD/WH | 18 | F-TAB | P06 TABLET | LATER | UNVERIFIED | gen.py ftab |
| P07 | 1 | wire | F-EBOX:out | F-BUCK 3 A -> 5 V buck IN+ | +12V | 12 V | RD/WH | 18 | F-BUCK | P07 BUCK IN | CRITICAL | SOURCED | gen.py fbx |
| P08 | 1 | wire | 5 V buck OUT+ | TB-5V | +5V | 5.00 V | OR | 20 | F-BUCK | P08 5V | CRITICAL | PROPOSED | gen.py conv/v5 |
| P09 | 1 | board | Zener 1N5339B cathode | TB-5V (anode to TB-GND) | 5 V clamp | 5 V | — | lead | none | P09 ZENER | CRITICAL | SOURCED | gen.py z |
| G04 | 2 | wire | Teensy A:GND | TB-GND | GND | 0 V | BK | 20 | none | G04 TA GND | CRITICAL | PROPOSED | gen.py |
| G06 | 2 | wire | Teensy B:GND | TB-GND | GND | 0 V | BK | 20 | none | G06 TB GND | CRITICAL | PROPOSED | gen.py |
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
| P10 | 4 | wire | TB-5V | F-BICM 1 A -> BICM X2 pin 9 (705 GY/RD) x4 strings | +5V BICM | 5 V | GY/RD (factory) | factory | F-BICM | P10 BICM 5V | CRITICAL | UNVERIFIED | gen.py f705; wiring_data bms |
| S50 | 4 | wire | BICM 5139 fault loop | pull-up few kΩ -> 5 V | fault loop | 5 V | factory | factory | none | S50 5139 PULLUP | LATER | SOURCED | gen.py pu |
| G05 | 5 | wire | Teensy A:GND | ESP32:GND | GND | 0 V | BK | 22 | none | G05 TA-ESP GND | LATER | SOURCED | GATEWAY.md:19 |
| S30 | 5 | wire | Teensy A:17 (Serial4 TX) | ESP32:GPIO16 (UART2 RX) | UART 115200 8N1 | 3.3 V | WH | 22 | none | S30 TA17-ESP16 | LATER | SOURCED | GATEWAY.md:17; HubStatus.cpp:12 |
| X01 | 5 | none | ESP32 Wi-Fi AP 192.168.4.1 | iPad / head unit | Wi-Fi (no wire) | — | — | — | none | X01 WIFI | LATER | SOURCED | GATEWAY.md |
| B01 | 6 | board | U1 /CLR (R5 10k -> 5 V, C5 1 µF, 1N4148) | U1 /CLR1 /CLR2 | power-on clear | 5 V | — | board | none | B01 U1 CLR | CRITICAL | PROPOSED | docs/THROTTLE_CLAMP.md |
| B02 | 6 | board | U3 P0W | U2a -> U2b (series 4066) | throttle | 0-5 V | — | board | none | B02 WIPER-4066 | CRITICAL | PROPOSED | docs/THROTTLE_CLAMP.md |
| B03 | 6 | board | U1 Q1 / Q2 | U2a ctl / U2b ctl | enables | 5 V | — | board | none | B03 Q-CTL | CRITICAL | PROPOSED | docs/THROTTLE_CLAMP.md |
| E01 | 6 | wire | F-COIL:out | E-stop block 1:11 + block 2:11 (jumper) | +12V coil feed | 12 V | RD/WH | 18 | F-COIL | E01 ESTOP FEED | CRITICAL | PROPOSED | docs/ESTOP.md:112-113 |
| E02 | 6 | wire | E-stop block 1:12 | EV200 B+ coil + AND EV200 B- coil + (high sides) | 12V coil (cut) | 12 V | RD/YE | 18 | F-COIL | E02 BLK1 MAIN COILS | CRITICAL | SOURCED | docs/ESTOP.md:112 (0.26 A break measured, OK) |
| E03 | 6 | wire | E-stop block 2:12 | precharge relay coil + AND Kelly KSI/PWR pin 7 | 12V coil + KSI (cut) | 12 V | RD/YE | 18 | F-COIL | E03 BLK2 PRE+KSI | CRITICAL | UNVERIFIED | docs/ESTOP.md:113 (KSI draw: Adam meters; Kelly pin 7 from manual) |
| E09 | 6 | wire | E-stop block 3:11 -> TB-GND; block 3:12 | Teensy B:9 (10k pull-up) — free on B per this map | e-stop sense (latch, never the cut) | 3.3 V | BU/BK | 22 | none | E09 BLK3 TB-ESTOP | CRITICAL | PROPOSED | crew 2026-10-22; docs/ESTOP.md; EstopLatch.h not wired (chief review #5) |
| K01 | 6 | wire | U2b out (R1 100k to GND — the ONLY pull-down) | Kelly throttle input (pin 3) | throttle | 0-5 V | BU/WH | 22 shielded | none | K01 KELLY THR | CRITICAL | UNVERIFIED | docs/THROTTLE_CLAMP.md; kelly-settings.md (100k, crew 2026-10-22); Kelly pin from manual |
| K02 | 6 | wire | Kelly pin 11 (12 V out) | Park pole 3 (NC) -> contact closed only while BOTH Teensys are alive (series heartbeat contacts) -> Kelly pin 15 Foot_SW | foot switch enable | 12 V | RD/WH -> RD/YE | 20 | none | K02 FOOTSW PARK | CRITICAL | UNVERIFIED | docs/PARK_BRAKE.md; kelly-settings.md:64 (Sparky) |
| K03 | 6 | wire | Kelly pin 11 12 V -> relay 30; relay 87a (NC) | Kelly pin 22 ECO | Low cap (fail-safe: relay NC, dead Teensy = Low) | 12 V | RD/WH -> RD/VT | 20 | none | K03 ECO LOWCAP | CRITICAL | UNVERIFIED | docs/DRIVE_MODES.md; kelly-settings.md:67; Kelly pin 22 from manual |
| P20 | 6 | wire | TB-5V | pedal 5 V + U1..U4 VCC (shared rail) | +5V | 5 V | OR | 20 | F-BUCK | P20 5V CLAMP | CRITICAL | PROPOSED | docs/THROTTLE_CLAMP.md |
| S01 | 6 | board | Teensy A:11 via 100 Ω (10k pull-down to GND) | FET Q-B+ gate; drain -> EV200 B+ coil - | coil drive | 3.3 V gate / 12 V coil | RD/BU (coil lead) | 18 (coil lead) | F-COIL | S01 TA11 MAIN+ | CRITICAL | PROPOSED | Config.h:182; wiring_data precharge net |
| S02 | 6 | board | Teensy A:12 via 100 Ω (10k pull-down) | FET Q-PRE gate; drain -> precharge coil - | coil drive | 3.3 / 12 V | RD/GN | 18 | F-COIL | S02 TA12 PRE | CRITICAL | PROPOSED | Config.h:183 |
| S03 | 6 | board | Teensy A:21 via 100 Ω (10k pull-down) | FET Q-B- gate; drain -> EV200 B- coil - | coil drive | 3.3 / 12 V | RD/BK | 18 | F-COIL | S03 TA21 MAIN- | CRITICAL | PROPOSED | Config.h:184 |
| S05 | 6 | wire | LEM A DHAB pin 4 (ch1) | 10k/18k + 100 nF -> Teensy A:14 | LEM A low range | 0-5 V -> 0-3.2 V | BU | 22 | none | S05 LEMA CH1 | CRITICAL | SOURCED | Config.h:187 |
| S06 | 6 | wire | LEM A DHAB pin 3 (ch2) | 10k/18k + 100 nF -> Teensy A:15 | LEM A high range | 0-5 V -> 0-3.2 V | BU/WH | 22 | none | S06 LEMA CH2 | CRITICAL | SOURCED | Config.h:188 |
| S07 | 6 | wire | TB-5V -> LEM A 5 V; sense via divider | Teensy A:25 | LEM supply ref | 5 V -> 3.2 V | OR | 22 | F-BUCK | S07 LEMA VC | CRITICAL | SOURCED | Config.h:189 |
| S10 | 6 | board | Teensy A:35 DATA / 36 CLK / 38 CS | U4 74AHCT125 -> U3 MCP4131 SDI/SCK/CS | bit-bang SPI | 3.3 -> 5 V | — | board | none | S10 TA SPI | CRITICAL | PROPOSED | Config.h:221-236 |
| S11 | 6 | board | Teensy A:4 (R6 10k pull-down) | U4 buffer -> U1 B1 | A alive 1 kHz | 3.3 -> 5 V | VT | 22 | none | S11 TA4 ALIVE | CRITICAL | PROPOSED | Config.h:243 |
| S12 | 6 | board | Teensy B:7 (R4 10k pull-down) | U1 B2 | B alive 1 kHz | 3.3 V | VT/WH | 22 | none | S12 TB7 ALIVE | CRITICAL | PROPOSED | Config.h:218 |
| S14 | 6 | wire | Pedal wiper (Hall, 0-5 V ASSUMED) | 10k/18k + 100 nF -> Teensy A:40 and MCP4131 P0A | throttle | 0-5 V -> 0-3.2 V | BU | 22 | none | S14 PEDAL A | CRITICAL | UNVERIFIED | Config.h:215; docs/THROTTLE_CLAMP.md (pedal not chosen; chief review #9 calibration) |
| S15 | 6 | wire | Pedal track 2 | own 10k/18k + 100 nF -> Teensy B:41 (A17); A reads track 1 on pin 40 | throttle B | 0-3.2 V | BU/WH | 22 | none | S15 PEDAL B | CRITICAL | PROPOSED | crew 2026-10-22 (track 2 on B:41); Config.h PEDAL_IN_B |
| S16 | 6 | wire | Park switch pole 1 (NO) -> GND | Teensy A:32 (10k pull-up 3V3 + 100 nF) | park A, LOW = set | 3.3 V | BU/BK | 22 | none | S16 PARK A | CRITICAL | PROPOSED | Config.h:249; docs/PARK_BRAKE.md (switch fit UNVERIFIED) |
| S17 | 6 | wire | Park switch pole 2 (NO) -> GND | Teensy B:32 | park B | 3.3 V | BU/RD | 22 | none | S17 PARK B | CRITICAL | PROPOSED | Config.h:253 |
| S20 | 6 | board | Teensy A:20 (100 Ω, 10k pull-down) | low-side FET -> Low-cap relay coil 85 (86 = vehicle 12 V bus via F-COIL, 1N4007) — coil NEVER on Kelly pin 11 (40 mA max) | Low-cap release | 3.3 V / 12 V | — | board | F-COIL | S20 TA20 LOWCAP | CRITICAL | PROPOSED | Config.h:167; docs/DRIVE_MODES.md; kelly-settings.md:71 |
| S25 | 6 | wire | LEM B DHAB pin 4 | divider -> Teensy B:14 | LEM B low | 0-3.2 V | BU | 22 | none | S25 LEMB CH1 | CRITICAL | SOURCED | Config.h:187 |
| S26 | 6 | wire | LEM B DHAB pin 3 | divider -> Teensy B:15 | LEM B high | 0-3.2 V | BU/WH | 22 | none | S26 LEMB CH2 | CRITICAL | SOURCED | Config.h:188 |
| S27 | 6 | wire | LEM B 5 V sense | Teensy B:25 | LEM supply ref | 3.2 V | OR | 22 | F-BUCK | S27 LEMB VC | CRITICAL | SOURCED | Config.h:189 |
| S40 | 6 | wire | 12V bus via divider (PROPOSED 47k/10k + 100 nF) | Teensy B:26 (A12) — free on B per this map | 12 V sense | ≤3.3 V | BU | 22 | PROPOSED 1 A | S40 12V SENSE | CRITICAL | PROPOSED | crew 2026-10-22; wiring_data power12 |
| S41 | 6 | wire | NACS inlet PROXIMITY (SOURCED Adam 2026-10-22: ~150 Ω latch released = plugged, ~480 Ω button pressed = plugged, open = not plugged; NOT charger AC-present) -> pilot/proximity board plug-present output -> opto LED (PROPOSED PC827 ch1 + 1k5). FALLBACK if the board has no plug-present output: PROPOSED divider on the proximity line to a free analog pin (values TBD, Sparky) | opto transistor -> Teensy A:33 (10k pull-up 3V3 + 100 nF), LOW = plugged | plug-in detect A (drive blocked on A when plugged, like park) | 12 V in / 3.3 V pin | BU/GN | 22 | PROPOSED 1 A | S41 PLUG A33 | CRITICAL | PROPOSED | Adam 2026-10-22 (NACS proximity); Sparky 2026-10-22 (opto); chief review #4 (in.plugIn never set) |
| S42 | 6 | wire | Stock Teryx key switch SWITCHED output (SOURCED Adam 2026-10-22; wire colour UNVERIFIED — Adam backprobes: 0 V key off, 12 V key on), fed from the TDC-JH 12 V bus, through its OWN small fuse (rating TBD, Sparky) -> opto ch1 LED (PROPOSED PC827 + 1k5) | opto ch1 transistor -> Teensy A:34 (10k pull-up 3V3 + 100 nF), LOW = key on | KEY-ON A | 12 V in / 3.3 V pin | BU/YE | 22 | own small fuse, rating TBD (Sparky) | S42 KEY A34 | CRITICAL | PROPOSED | Adam 2026-10-22 (stock key switch source); Sparky 2026-10-22; chief review #4 (key-on is serial e only) |
| S43 | 6 | wire | Same stock key switch switched output (after the S42 fuse) -> opto ch2 LED (PROPOSED PC827 + 1k5) | opto ch2 transistor -> Teensy B:34 (10k pull-up 3V3 + 100 nF), LOW = key on | KEY-ON B | 12 V in / 3.3 V pin | BU/YE | 22 | shares S42 fuse | S43 KEY B34 | CRITICAL | PROPOSED | Sparky 2026-10-22 |
| S44 | 6 | wire | Same plug-present output as S41 -> opto ch2 LED (PROPOSED PC827 ch2 + 1k5) | opto ch2 transistor -> Teensy B:33 (10k pull-up 3V3 + 100 nF), LOW = plugged | plug-in detect B mirror (drive blocked on B when plugged, like park) | 12 V in / 3.3 V pin | BU/GN | 22 | shares S41 fuse | S44 PLUG B33 | CRITICAL | PROPOSED | Adam 2026-10-22 |
| X02 | 6 | none | Teensy A/B park inputs | software Parked logic | software (no wire) | — | — | — | none | X02 SW PARK | CRITICAL | SOURCED | docs/PARK_BRAKE.md |
| H01 | 7 | wire | String 1..4 B+ | A6T200 fuse 1..4 (bus bar, insulated standoffs) | HV+ | ≤147.6 V | OR HV, RED tape | 1/0 (each A6T200 string lead, ~138 A at a 550 A launch) | F-S1..S4 | H01-H04 STRING n B+ | LATER | PROPOSED | docs/WIRING.md:66-69; crew 2026-10-22 |
| H05 | 7 | wire | 4 fuses | common B+ bus | HV+ | HV | OR HV, RED tape | 1/0 (string side of each fuse) | F-S1..S4 | H05 FUSE-BUS | LATER | PROPOSED | wiring_data overview net |
| H06 | 7 | wire | common B+ bus (NO MSD — service = pull 4 fuses) | EV200 B+ line | HV+ | HV | OR HV, RED tape | 3/0 owned TEMCo (pack-to-Kelly main) | F-S1..S4 | H06 BUS-EV200 | LATER | SOURCED | wiring_data overview net (MSD removed 2026-10-22) |
| H07 | 7 | wire | EV200 B+ load | Kelly B+ | HV+ | HV | OR HV, RED tape | 3/0 owned TEMCo (pack-to-Kelly main) | none | H07 EV200-KELLY+ | LATER | SOURCED | wiring_data |
| H08 | 7 | wire | precharge relay + 50 Ω 100 W | across EV200 B+ | HV precharge | HV | OR HV | 14 HV | none | H08 PRECHARGE | LATER | UNVERIFIED | wiring_data precharge net; precharge contactor = salvaged AEV14012 (~3 A peak coil, OK) — crew 2026-10-22 |
| H09 | 7 | wire | Kelly U/V/W + Sin/Cos | Zero 75-7R | motor | HV | OR HV | match bus | none | H09 MOTOR | LATER | SOURCED | wiring_data |
| H10 | 7 | wire | Kelly B- | EV200 B- -> LEM A/B -> string B- | HV- | HV | OR HV, BLACK tape | 3/0 owned TEMCo (pack-to-Kelly main) | none | H10 KELLY-EV200- | LATER | SOURCED | wiring_data |
| H11 | 7 | wire | String B- leads | through LEM A (S1+S2) / LEM B (S3+S4) | HV- | HV | OR HV, BLACK tape | 1/0 | none | H11 B- LEM | LATER | SOURCED | wiring_data overview |
| H12 | 7 | wire | HV bus | F-DCDC -> DC-DC HV in | HV | HV | OR HV | 14 HV | F-DCDC | H12 DCDC IN | LATER | UNVERIFIED | gen.py fdi |
| H13 | 7 | wire | Elcon out | F-ELC -> pack bus | HV | HV | OR HV | 8 HV PROPOSED | F-ELC | H13 ELCON OUT | LATER | UNVERIFIED | gen.py fel |
| H14 | 7 | wire | NACS inlet (AC) | Elcon AC in | AC 240 V | AC | per AC code | per charger | none | H14 AC IN | LATER | UNVERIFIED | wiring_data |
| S08 | 7 | wire | Kelly-side HV divider (60.6:1) | Teensy A:24 (HV_ANALOG_ENABLE=0) | HV sense | ≤3.3 V | BU | 22 | PROPOSED 0.5 A HV-rated | S08 HV SENSE | LATER | UNVERIFIED | Config.h:185, 156 |
| A01 | 8 | wire | 12V bus (switched) -> F-AUX | aux relay 30 | +12V load | 12 V | RD/WH | 14 (size to load) | F-AUX | A01 AUX FEED | LATER | PROPOSED | Adam 2026-10-22 (NO contacts, relay/fuse >1 A) |
| A02 | 8 | wire | 12V switched (small fuse) | NO push button (ZBE101) -> relay 86 | button | 12 V | RD/WH | 20 | PROPOSED 2 A | A02 AUX BTN | LATER | PROPOSED | Adam 2026-10-22 |
| A03 | 8 | wire | relay 85 | GND | GND | 0 V | BK | 20 | none | A03 RELAY GND | LATER | PROPOSED | standard automotive relay |
| A04 | 8 | wire | relay 87 | horn / lights + | +12V load | 12 V | RD/GN | 14 (size to load) | F-AUX | A04 AUX LOAD | LATER | PROPOSED | Adam 2026-10-22 |
| A05 | 8 | wire | load - | GND | GND | 0 V | BK | 14 | none | A05 LOAD GND | LATER | PROPOSED | standard |

## Open UNVERIFIED / UNDEFINED wires (19)

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
- **K01** UNVERIFIED: U2b out (R1 100k to GND — the ONLY pull-down) → Kelly throttle input (pin 3) (docs/THROTTLE_CLAMP.md; kelly-settings.md (100k, crew 2026-10-22); Kelly pin from manual)
- **K02** UNVERIFIED: Kelly pin 11 (12 V out) → Park pole 3 (NC) -> contact closed only while BOTH Teensys are alive (series heartbeat contacts) -> Kelly pin 15 Foot_SW (docs/PARK_BRAKE.md; kelly-settings.md:64 (Sparky))
- **K03** UNVERIFIED: Kelly pin 11 12 V -> relay 30; relay 87a (NC) → Kelly pin 22 ECO (docs/DRIVE_MODES.md; kelly-settings.md:67; Kelly pin 22 from manual)
- **H08** UNVERIFIED: precharge relay + 50 Ω 100 W → across EV200 B+ (wiring_data precharge net; precharge contactor = salvaged AEV14012 (~3 A peak coil, OK) — crew 2026-10-22)
- **H12** UNVERIFIED: HV bus → F-DCDC -> DC-DC HV in (gen.py fdi)
- **H13** UNVERIFIED: Elcon out → F-ELC -> pack bus (gen.py fel)
- **H14** UNVERIFIED: NACS inlet (AC) → Elcon AC in (wiring_data)
- **S08** UNVERIFIED: Kelly-side HV divider (60.6:1) → Teensy A:24 (HV_ANALOG_ENABLE=0) (Config.h:185, 156)
