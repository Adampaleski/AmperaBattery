# Connection checklist (GENERATED from docs/PINMAP.yaml — print me)

Build order: one wire at a time. Tick the box only after the meter check passes. Write the label on heat-shrink at BOTH ends.
Nothing HV (step 7) without Sparky present and all 4 string fuses OUT. Status in brackets: anything not SOURCED needs a second look.

## Step 1: 12 V + grounds

- [ ] **G01 12V- CHASSIS** (CRITICAL, PROPOSED) — 12V bus bar:- → frame ground strap · GND · strap AWG BK · fuse none  
  Meter: 12V- to frame <0.5 Ω
- [ ] **G02 KELLY CASE** (CRITICAL, PROPOSED) — Kelly case → frame · GND · strap AWG BK · fuse none  
  Meter: <0.5 Ω
- [ ] **G03 BUCK GND** (CRITICAL, PROPOSED) — 5 V buck OUT- → TB-GND · GND · 20 AWG BK · fuse none  
  Meter: <0.5 Ω to 12V-
- [ ] **P01 12V MAIN** (CRITICAL, UNVERIFIED) — DC-DC TDC-JH-144-12:+12V out → F-MAIN:in · +12V main · TBD: Sparky sizes it; TDC-JH-144-12 = 110 A out (SOURCED) (10 AWG ≤30 A, 8 AWG ≤40 A, 6 AWG ≤60 A) AWG RD · fuse F-MAIN  
  Meter: continuity end-to-end <1 Ω; NOT to chassis (>1 MΩ)
- [ ] **P02 12V BUS** (CRITICAL, PROPOSED) — F-MAIN:out → 12V bus bar:+ · +12V main · TBD: same as P01 AWG RD · fuse F-MAIN  
  Meter: continuity <1 Ω
- [ ] **P03 KELLY PWR** (CRITICAL, UNVERIFIED) — 12V bus bar:+ → F-KEL:in -> Kelly PWR · +12V · 16 AWG RD/WH · fuse F-KEL  
  Meter: continuity; fuse rating written on label
- [ ] **P04 COIL FEED** (CRITICAL, PROPOSED) — 12V bus bar:+ → F-COIL:in · +12V coil feed · 18 AWG RD/WH · fuse F-COIL  
  Meter: continuity
- [ ] **P05 EBOX 12V** (CRITICAL, UNVERIFIED) — 12V bus bar:+ → F-EBOX:in (electronics box) · +12V · 16 AWG RD/WH · fuse F-EBOX  
  Meter: continuity
- [ ] **P06 TABLET** (LATER, UNVERIFIED) — 12V bus bar:+ → F-TAB -> tablet USB-C PD · +12V · 18 AWG RD/WH · fuse F-TAB  
  Meter: continuity
- [ ] **P07 BUCK IN** (CRITICAL, SOURCED) — F-EBOX:out → F-BUCK 3 A -> 5 V buck IN+ · +12V · 18 AWG RD/WH · fuse F-BUCK  
  Meter: continuity; buck output set 5.00 V BEFORE connecting loads
- [ ] **P08 5V** (CRITICAL, PROPOSED) — 5 V buck OUT+ → TB-5V · +5V · 20 AWG OR · fuse F-BUCK  
  Meter: power on with nothing else connected: 4.95-5.05 V
- [ ] **P09 ZENER** (CRITICAL, SOURCED) — Zener 1N5339B cathode → TB-5V (anode to TB-GND) · 5 V clamp · lead AWG — · fuse none  
  Meter: diode test: band end on +5V

## Step 2: Teensys

- [ ] **G04 TA GND** (CRITICAL, PROPOSED) — Teensy A:GND → TB-GND · GND · 20 AWG BK · fuse none  
  Meter: <0.5 Ω
- [ ] **G06 TB GND** (CRITICAL, PROPOSED) — Teensy B:GND → TB-GND · GND · 20 AWG BK · fuse none  
  Meter: <0.5 Ω
- [ ] **P11 TA VIN** (CRITICAL, SOURCED) — TB-5V → Teensy A:VIN (VIN/VUSB pad CUT) · +5V · 20 AWG OR · fuse F-BUCK  
  Meter: before plugging USB: VIN/VUSB pad cut (meter open between VIN and VUSB)
- [ ] **P12 TB VIN** (CRITICAL, SOURCED) — TB-5V → Teensy B:VIN (VIN/VUSB pad CUT) · +5V · 20 AWG OR · fuse F-BUCK  
  Meter: as P11

## Step 3: CAN buses (transceivers + vehicle bus)

- [ ] **C01 A-CAN1** (CRITICAL, SOURCED) — Teensy A:22 TX / 23 RX + 3V3 + GND → SN65HVD230 #1 TXD/RXD/VCC/GND · CAN logic · board AWG — · fuse none  
  Meter: TX->TXD, RX->RXD (NOT crossed for SN65); 3.3 V at VCC
- [ ] **C02 B-CAN1** (CRITICAL, SOURCED) — Teensy B:22/23 → SN65HVD230 #3 · CAN logic · board AWG — · fuse none  
  Meter: as C01
- [ ] **C03 A-CAN2** (CRITICAL, SOURCED) — Teensy A:1 TX / 0 RX + 3V3 + GND → SN65HVD230 #2 · CAN logic · board AWG — · fuse none  
  Meter: as C01
- [ ] **C04 B-CAN2** (CRITICAL, SOURCED) — Teensy B:1/0 → SN65HVD230 #4 · CAN logic · board AWG — · fuse none  
  Meter: as C01
- [ ] **C05 A-CAN3** (CRITICAL, SOURCED) — Teensy A:31 TX / 30 RX → SN65HVD230 #5 + 120 Ω · CAN logic · board AWG — · fuse none  
  Meter: as C01
- [ ] **C06 B-CAN3** (CRITICAL, SOURCED) — Teensy B:31/30 → SN65HVD230 #6 (no 120 Ω) · CAN logic · board AWG — · fuse none  
  Meter: as C01
- [ ] **C10 VEH CAN** (CRITICAL, PROPOSED) — SN65 #5 CANH/CANL (A) → SN65 #6 CANH/CANL (B) -> Kelly -> Elcon (daisy chain) · CAN-V 250k · 22 AWG YE (H) / GN (L) twisted · fuse none  
  Meter: power OFF: CANH-CANL ≈ 60 Ω (two 120 Ω ends); 120 Ω = one end missing; open = none

## Step 4: BICMs

- [ ] **C11 S1 CAN** (CRITICAL, UNVERIFIED) — SN65 #1 CANH/CANL → String 1 X2 pin 12 CAN_H (6435 WH/YE) / pin 11 CAN_L (6434 L-BU/YE) · CAN-A1 125k · 22 AWG YE/GN -> factory WH/YE, L-BU/YE · fuse none  
  Meter: power OFF ≈60 Ω; power on: cells appear on console
- [ ] **C12 S2 CAN** (CRITICAL, UNVERIFIED) — SN65 #2 CANH/CANL → String 2 X2 pins 12/11 · CAN-A2 125k · 22 AWG YE/GN · fuse none  
  Meter: as C11
- [ ] **C13 S3 CAN** (CRITICAL, UNVERIFIED) — SN65 #3 CANH/CANL → String 3 X2 pins 12/11 · CAN-B1 125k · 22 AWG YE/GN · fuse none  
  Meter: as C11
- [ ] **C14 S4 CAN** (CRITICAL, UNVERIFIED) — SN65 #4 CANH/CANL → String 4 X2 pins 12/11 · CAN-B2 125k · 22 AWG YE/GN · fuse none  
  Meter: as C11
- [ ] **G07 BICM GND** (CRITICAL, SOURCED) — X2 pin 10 GND (470 BK/VT) x4 → TB-GND · GND · factory AWG BK/VT (factory) · fuse none  
  Meter: <0.5 Ω
- [ ] **P10 BICM 5V** (CRITICAL, UNVERIFIED) — TB-5V → F-BICM 1 A -> BICM X2 pin 9 (705 GY/RD) x4 strings · +5V BICM · factory AWG GY/RD (factory) · fuse F-BICM  
  Meter: NEVER to Teensy I/O; 5 V at X2 pin 9
- [ ] **S50 5139 PULLUP** (LATER, SOURCED) — BICM 5139 fault loop → pull-up few kΩ -> 5 V · fault loop · factory AWG factory · fuse none  
  Meter: 5 V idle

## Step 5: ESP32 gateway

- [ ] **G05 TA-ESP GND** (LATER, SOURCED) — Teensy A:GND → ESP32:GND · GND · 22 AWG BK · fuse none  
  Meter: <0.5 Ω
- [ ] **S30 TA17-ESP16** (LATER, SOURCED) — Teensy A:17 (Serial4 TX) → ESP32:GPIO16 (UART2 RX) · UART 115200 8N1 · 22 AWG WH · fuse none  
  Meter: continuity; Teensy pin 16 and ESP32 GPIO17 stay UNCONNECTED

## Step 6: Safety I/O, throttle clamp, LEM, park, Kelly low voltage

- [ ] **B01 U1 CLR** (CRITICAL, PROPOSED) — U1 /CLR (R5 10k -> 5 V, C5 1 µF, 1N4148) → U1 /CLR1 /CLR2 · power-on clear · board AWG — · fuse none  
  Meter: continuity
- [ ] **B02 WIPER-4066** (CRITICAL, PROPOSED) — U3 P0W → U2a -> U2b (series 4066) · throttle · board AWG — · fuse none  
  Meter: Gate 1 clamp test: digipot powers up mid-scale — output must be 0 V with either Teensy off (chief #10)
- [ ] **B03 Q-CTL** (CRITICAL, PROPOSED) — U1 Q1 / Q2 → U2a ctl / U2b ctl · enables · board AWG — · fuse none  
  Meter: continuity
- [ ] **E01 ESTOP FEED** (CRITICAL, PROPOSED) — F-COIL:out → E-stop block 1:11 + block 2:11 (jumper) · +12V coil feed · 18 AWG RD/WH · fuse F-COIL  
  Meter: 12 V at both terminal 11s
- [ ] **E02 BLK1 MAIN COILS** (CRITICAL, SOURCED) — E-stop block 1:12 → EV200 B+ coil + AND EV200 B- coil + (high sides) · 12V coil (cut) · 18 AWG RD/YE · fuse F-COIL  
  Meter: button released: 11-12 closed (<1 Ω); pushed: open
- [ ] **E03 BLK2 PRE+KSI** (CRITICAL, UNVERIFIED) — E-stop block 2:12 → precharge relay coil + AND Kelly KSI/PWR pin 7 · 12V coil + KSI (cut) · 18 AWG RD/YE · fuse F-COIL  
  Meter: as E02; meter KSI current < 0.2 A
- [ ] **E09 BLK3 TB-ESTOP** (CRITICAL, PROPOSED) — E-stop block 3:11 -> TB-GND; block 3:12 → Teensy B:9 (10k pull-up) — free on B per this map · e-stop sense (latch, never the cut) · 22 AWG BU/BK · fuse none  
  Meter: released: pin reads LOW (closed to GND); pushed: HIGH
- [ ] **K01 KELLY THR** (CRITICAL, UNVERIFIED) — U2b out (R1 100k to GND — the ONLY pull-down) → Kelly throttle input (pin 3) · throttle · 22 shielded AWG BU/WH · fuse none  
  Meter: Gate 1: pin 3 < 0.2 V with both 4066s open; full pedal voltage at pin 3 at full pedal. 10k pull-down = PROPOSED bench option only if pin 3 floats, after confirming pedal + digipot still reach full voltage through the 4066 under that load
- [ ] **K02 FOOTSW PARK** (CRITICAL, UNVERIFIED) — Kelly pin 11 (12 V out) → Park pole 3 (NC) -> contact closed only while BOTH Teensys are alive (series heartbeat contacts) -> Kelly pin 15 Foot_SW · foot switch enable · 20 AWG RD/WH -> RD/YE · fuse none  
  Meter: brake set: open; either Teensy dead: open; both alive + released: <1 Ω
- [ ] **K03 ECO LOWCAP** (CRITICAL, UNVERIFIED) — Kelly pin 11 12 V -> relay 30; relay 87a (NC) → Kelly pin 22 ECO · Low cap (fail-safe: relay NC, dead Teensy = Low) · 20 AWG RD/WH -> RD/VT · fuse none  
  Meter: relay off: Kelly pin 22 = 12 V
- [ ] **P20 5V CLAMP** (CRITICAL, PROPOSED) — TB-5V → pedal 5 V + U1..U4 VCC (shared rail) · +5V · 20 AWG OR · fuse F-BUCK  
  Meter: 5 V at each chip VCC pin
- [ ] **S01 TA11 MAIN+** (CRITICAL, PROPOSED) — Teensy A:11 via 100 Ω (10k pull-down to GND) → FET Q-B+ gate; drain -> EV200 B+ coil - (no external diode: EV200 economizer clamps, DECIDED Sparky 2026-10-23 05:41) · coil drive · 18 (coil lead) AWG RD/BU (coil lead) · fuse F-COIL  
  Meter: Gate 0: 10k gate pull-down present (meter gate-GND ≈10 kΩ) — chief review #10
- [ ] **S02 TA12 PRE** (CRITICAL, PROPOSED) — Teensy A:12 via 100 Ω (10k pull-down) → FET Q-PRE gate; drain -> AEV14012 precharge coil - (D-PRE 1N4007 across the coil AT the relay, band/cathode to +12 V, no zener: DECIDED Sparky 2026-10-23 05:41) · coil drive · 18 AWG RD/GN · fuse F-COIL  
  Meter: as S01
- [ ] **S03 TA21 MAIN-** (CRITICAL, PROPOSED) — Teensy A:21 via 100 Ω (10k pull-down) → FET Q-B- gate; drain -> EV200 B- coil - (no external diode: EV200 economizer clamps, DECIDED Sparky 2026-10-23 05:41) · coil drive · 18 AWG RD/BK · fuse F-COIL  
  Meter: as S01
- [ ] **S05 LEMA CH1** (CRITICAL, SOURCED) — LEM A DHAB pin 4 (ch1) → 10k/18k + 100 nF -> Teensy A:14 · LEM A low range · 22 AWG BU · fuse none  
  Meter: no current: ≈2.5 V at DHAB, ≈1.6 V at pin
- [ ] **S06 LEMA CH2** (CRITICAL, SOURCED) — LEM A DHAB pin 3 (ch2) → 10k/18k + 100 nF -> Teensy A:15 · LEM A high range · 22 AWG BU/WH · fuse none  
  Meter: as S05
- [ ] **S07 LEMA VC** (CRITICAL, SOURCED) — TB-5V -> LEM A 5 V; sense via divider → Teensy A:25 · LEM supply ref · 22 AWG OR · fuse F-BUCK  
  Meter: ≈3.2 V at pin 25
- [ ] **S10 TA SPI** (CRITICAL, PROPOSED) — Teensy A:35 DATA / 36 CLK / 38 CS → U4 74AHCT125 -> U3 MCP4131 SDI/SCK/CS · bit-bang SPI · board AWG — · fuse none  
  Meter: continuity per pin
- [ ] **S11 TA4 ALIVE** (CRITICAL, PROPOSED) — Teensy A:4 (R6 10k pull-down) → U4 buffer -> U1 B1 · A alive 1 kHz · 22 AWG VT · fuse none  
  Meter: R6 ≈10 kΩ pin 4 to GND
- [ ] **S12 TB7 ALIVE** (CRITICAL, PROPOSED) — Teensy B:7 (R4 10k pull-down) → U1 B2 · B alive 1 kHz · 22 AWG VT/WH · fuse none  
  Meter: R4 ≈10 kΩ
- [ ] **S14 PEDAL A** (CRITICAL, UNVERIFIED) — Pedal wiper (Hall, 0-5 V ASSUMED) → 10k/18k + 100 nF -> Teensy A:40 and MCP4131 P0A · throttle · 22 AWG BU · fuse none  
  Meter: pedal up / down voltages logged; ≈88 % max reading (chief #9)
- [ ] **S15 PEDAL B** (CRITICAL, PROPOSED) — Pedal track 2 → own 10k/18k + 100 nF -> Teensy B:41 (A17); A reads track 1 on pin 40 · throttle B · 22 AWG BU/WH · fuse none  
  Meter: as S14
- [ ] **S16 PARK A** (CRITICAL, PROPOSED) — Park switch pole 1 (NO) -> GND → Teensy A:32 (10k pull-up 3V3 + 100 nF) · park A, LOW = set · 22 AWG BU/BK · fuse none  
  Meter: brake set: pin 0 V; released: 3.3 V
- [ ] **S17 PARK B** (CRITICAL, PROPOSED) — Park switch pole 2 (NO) -> GND → Teensy B:32 · park B · 22 AWG BU/RD · fuse none  
  Meter: as S16
- [ ] **S20 TA20 LOWCAP** (CRITICAL, PROPOSED) — Teensy A:20 (100 Ω, 10k pull-down) → low-side FET -> Low-cap relay coil 85 (86 = vehicle 12 V bus via F-COIL, 1N4007) — coil NEVER on Kelly pin 11 (40 mA max) · Low-cap release · board AWG — · fuse F-COIL  
  Meter: Teensy off: relay de-energised (NC closed)
- [ ] **S25 LEMB CH1** (CRITICAL, SOURCED) — LEM B DHAB pin 4 → divider -> Teensy B:14 · LEM B low · 22 AWG BU · fuse none  
  Meter: as S05
- [ ] **S26 LEMB CH2** (CRITICAL, SOURCED) — LEM B DHAB pin 3 → divider -> Teensy B:15 · LEM B high · 22 AWG BU/WH · fuse none  
  Meter: as S05
- [ ] **S27 LEMB VC** (CRITICAL, SOURCED) — LEM B 5 V sense → Teensy B:25 · LEM supply ref · 22 AWG OR · fuse F-BUCK  
  Meter: ≈3.2 V
- [ ] **S40 12V SENSE** (CRITICAL, PROPOSED) — 12V bus via divider (PROPOSED 47k/10k + 100 nF) → Teensy B:26 (A12) — free on B per this map · 12 V sense · 22 AWG BU · fuse PROPOSED 1 A  
  Meter: 13.8 V in -> ≈2.4 V at pin
- [ ] **S41 PLUG A33** (CRITICAL, PROPOSED) — LM393 output 1 (pin 1, open collector) — the ONE shared LM393 (PROPOSED, NOT OWNED: pending purchase approval) → Teensy A:33 with its own 10k pull-up to Teensy A 3.3 V + 100 nF, LOW = plugged · plug-in detect A (drive blocked on A when plugged, like park) · 22 AWG BU/GN · fuse none (signal; 5 V feed already fused)  
  Meter: unplugged (prox open): 3.3 V at A:33; plugged latched (~150 Ω) and button pressed (~480 Ω): <0.4 V; bench: plug/unplug confirms drive block on A, open-wire test
- [ ] **S42 KEY A34** (CRITICAL, PROPOSED) — Stock Teryx key switch SWITCHED output (SOURCED Adam 2026-10-22; wire colour UNVERIFIED — Adam backprobes: 0 V key off, 12 V key on), fed from the TDC-JH 12 V bus, through its OWN 1 A fuse (Sparky 2026-10-22) -> opto ch1 LED (PROPOSED PC827 + 1k5) → opto ch1 transistor -> Teensy A:34 (10k pull-up 3V3 + 100 nF), LOW = key on · KEY-ON A · 22 AWG BU/YE · fuse own 1 A fuse (Sparky 2026-10-22, PROPOSED)  
  Meter: BEFORE wiring: backprobe key switch output 0 V key off / 12 V key on (record colour). Then key off: 3.3 V at A:34; key on: <0.4 V
- [ ] **S43 KEY B34** (CRITICAL, PROPOSED) — Same stock key switch switched output (after the S42 fuse) -> opto ch2 LED (PROPOSED PC827 + 1k5) → opto ch2 transistor -> Teensy B:34 (10k pull-up 3V3 + 100 nF), LOW = key on · KEY-ON B · 22 AWG BU/YE · fuse shares S42 fuse  
  Meter: as S42 at B:34
- [ ] **S44 PLUG B33** (CRITICAL, PROPOSED) — LM393 output 2 (pin 7, open collector) — same chip as S41 → Teensy B:33 with its own 10k pull-up to Teensy B 3.3 V + 100 nF, LOW = plugged · plug-in detect B mirror (drive blocked on B when plugged, like park) · 22 AWG BU/GN · fuse none (signal; 5 V feed already fused)  
  Meter: as S41 at B:33; with B powered off, A:33 must still read correctly (separate pull-ups)
- [ ] **S45 PROX NET** (CRITICAL, PROPOSED) — NACS inlet PROXIMITY pin (SOURCED Adam 2026-10-22: ~150 Ω latched / ~480 Ω button pressed = plugged, open = not plugged; plug side returns to inlet PE) → proximity node: 330 Ω pull-up to the shared 5 V + 2.7k to GND (Sparky; same values as the J1772 vehicle side) · plug proximity (one network for both boards) · 22 AWG BU/GN · fuse none (330 Ω limits it to 15 mA)  
  Meter: unplugged ≈4.4 V at the node; plugged latched ≈1.5 V, button pressed ≈2.8 V (calculated, UNVERIFIED until metered). Inlet PE to the single-point ground is UNVERIFIED
- [ ] **S46 PROX DIV** (CRITICAL, PROPOSED) — proximity node -> 10k (top) / 20k (to GND) divider → LM393 IN+ of BOTH halves (pins 3 and 5 tied) — one divider feeds both · divided proximity (×2/3, keeps it inside the LM393 input range at 5 V) · 22 AWG BU/GN · fuse none  
  Meter: unplugged ≈2.9 V on pins 3/5, plugged ≤1.9 V (2.5 V switch point ≈3.75 V at the node)
- [ ] **S47 LM393 5V** (CRITICAL, PROPOSED) — shared 5 V rail (5 V terminal block, same rail as the throttle clamp chips) → LM393 VCC pin 8 (+ 100 nF to pin 4) and the 330 Ω proximity pull-up · LM393 + proximity supply · 22 AWG OR · fuse 5 V rail (3 A mini blade upstream)  
  Meter: 5.0 V between pin 8 and pin 4
- [ ] **S48 LM393 REF** (CRITICAL, PROPOSED) — 2.5 V reference (PROPOSED 10k/10k from the shared 5 V; ~50 mV hysteresis part values TBD by Sparky — both halves share IN+) → LM393 IN− of BOTH halves (pins 2 and 6 tied) · 2.5 V threshold · 22 AWG BU · fuse none  
  Meter: 2.50 V on pins 2/6
- [ ] **S49 LM393 GND** (CRITICAL, PROPOSED) — LM393 GND pin 4 (also the 2.7k, 20k and reference bottoms) → single-point ground (same star point as the Teensy A/B grounds) · LM393 ground · 22 AWG BK · fuse none  
  Meter: <0.1 Ω pin 4 to the single-point ground

## Step 7: HIGH VOLTAGE — LATER, Sparky present, string fuses OUT

- [ ] **H01-H04 STRING n B+** (LATER, PROPOSED) — String 1..4 B+ → A6T200 fuse 1..4 (bus bar, insulated standoffs) · HV+ · 1/0 (each A6T200 string lead, ~138 A at a 550 A launch) AWG OR HV, RED tape · fuse F-S1..S4  
  Meter: torque to cut sheet; fuses OUT until first HV power-up
- [ ] **H05 FUSE-BUS** (LATER, PROPOSED) — 4 fuses → common B+ bus · HV+ · 1/0 (string side of each fuse) AWG OR HV, RED tape · fuse F-S1..S4  
  Meter: torque
- [ ] **H06 BUS-EV200** (LATER, SOURCED) — common B+ bus (NO MSD — service = pull 4 fuses) → EV200 B+ line · HV+ · 3/0 owned TEMCo (pack-to-Kelly main) AWG OR HV, RED tape · fuse F-S1..S4  
  Meter: torque
- [ ] **H07 EV200-KELLY+** (LATER, SOURCED) — EV200 B+ load → Kelly B+ · HV+ · 3/0 owned TEMCo (pack-to-Kelly main) AWG OR HV, RED tape · fuse none  
  Meter: torque
- [ ] **H08 PRECHARGE** (LATER, UNVERIFIED) — precharge relay + 50 Ω 100 W → across EV200 B+ · HV precharge · 14 HV AWG OR HV · fuse none  
  Meter: resistor ≈50 Ω. MOUNT (Mechanic 2026-10-22): 50 Ω/100 W resistor on the HV panel aluminum backplate with thermal paste, away from the loom
- [ ] **H09 MOTOR** (LATER, SOURCED) — Kelly U/V/W + Sin/Cos → Zero 75-7R · motor · match bus AWG OR HV · fuse none  
  Meter: phase order per Kelly autotune
- [ ] **H10 KELLY-EV200-** (LATER, SOURCED) — Kelly B- → EV200 B- -> LEM A/B -> string B- · HV- · 3/0 owned TEMCo (pack-to-Kelly main) AWG OR HV, BLACK tape · fuse none  
  Meter: B- NEVER to chassis (>1 MΩ)
- [ ] **H11 B- LEM** (LATER, SOURCED) — String B- leads → through LEM A (S1+S2) / LEM B (S3+S4) · HV- · 1/0 AWG OR HV, BLACK tape · fuse none  
  Meter: LEM arrow direction = discharge
- [ ] **H12 DCDC IN** (LATER, UNVERIFIED) — HV bus → F-DCDC -> DC-DC HV in · HV · 14 HV AWG OR HV · fuse F-DCDC  
  Meter: fuse rating vs label
- [ ] **H13 ELCON OUT** (LATER, SOURCED) — Elcon out → F-ELC -> pack bus · HV · 6 HV (SOURCED crew 2026-10-23) AWG OR HV · fuse F-ELC  
  Meter: fuse A6T60 fitted; holder TBD
- [ ] **H14 AC IN** (LATER, UNVERIFIED) — NACS inlet (AC) → Elcon AC in · AC 240 V · per charger AWG per AC code · fuse none  
  Meter: —
- [ ] **S08 RESERVED** (LATER, PROPOSED) — NOT FITTED (room DECIDED 2026-10-23): isolated HV sensor dropped for now; precharge uses Kelly CAN B+. Only if the Kelly B+ bench gate fails: AMC1311EVM isolated output (never a plain 60.6:1 divider) → Teensy A:24 (reserved spare, HV_ANALOG_ENABLE=0) · HV sense (isolated, not fitted) · 22 AWG BU · fuse PROPOSED 0.5 A HV-rated  
  Meter: Not fitted: A:24 unconnected. If ever fitted: HV side to Teensy GND open (≥ 1 MΩ, power off) before connecting

## Step 8: Body / aux buttons — LATER

- [ ] **A01 AUX FEED** (LATER, PROPOSED) — 12V bus (switched) -> F-AUX → aux relay 30 · +12V load · 14 (size to load) AWG RD/WH · fuse F-AUX  
  Meter: fuse rating on label
- [ ] **A02 AUX BTN** (LATER, PROPOSED) — 12V switched (small fuse) → NO push button (ZBE101) -> relay 86 · button · 20 AWG RD/WH · fuse PROPOSED 2 A  
  Meter: pressed: 12 V at 86; released: 0 V
- [ ] **A03 RELAY GND** (LATER, PROPOSED) — relay 85 → GND · GND · 20 AWG BK · fuse none  
  Meter: <0.5 Ω
- [ ] **A04 AUX LOAD** (LATER, PROPOSED) — relay 87 → horn / lights + · +12V load · 14 (size to load) AWG RD/GN · fuse F-AUX  
  Meter: load works only while pressed
- [ ] **A05 LOAD GND** (LATER, PROPOSED) — load - → GND · GND · 14 AWG BK · fuse none  
  Meter: <0.5 Ω
