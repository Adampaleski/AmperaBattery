# Two-Teensy throttle clamp (crew approved 2026-10-21: chief, Sparky; Adam keeps pedal maps)

`THROTTLE_TRIM_ENABLE=0` stays. Nothing is flashed until the BICM temperature formula and ID mapping are bench-verified. CREW REVIEW for every item below. Firmware: `src/ThrottleTrim.cpp`, logic `include/ThrottleCross.h`, profiles `include/DriveProfile.h`.

## Rule
The Kelly throttle input is held at **0 V unless BOTH Teensy A and Teensy B assert their enable**. Each enable is a *toggling alive signal* into a retriggerable one-shot, so a hung Teensy (or a hung loop with a live ISR) drops it in ≈5 ms. Either Teensy alone can force 0.

## Schematic (text)
```
5 V pedal (hall, 0-5 V ASSUMED) --+-- U3 MCP4131 P0A (P0B = GND) -- P0W --+
                                  +-- 10k/18k + 100 nF -> Teensy A pin 40 (A16)  PEDAL_IN_A
                                  +-- 10k/18k + 100 nF -> Teensy B pin 41 (A17)  PEDAL_IN_B
P0W -- U2a 74HC4066 (ctl = U1 Q1) -- U2b 74HC4066 (ctl = U1 Q2) --+-- Kelly throttle input
                                                                   +-- R1 10k to GND (holds 0 V)
Teensy A 35/36/38 (DATA/CLK/CS, 3.3 V) -> U4 74AHCT125 (5 V) -> MCP4131 SDI/SCK/CS
  buffered CS also -> U1 B1 (rising edge = trigger; A1 = GND).  A's alive = one CS pulse per 1 ms write.
Teensy B pin 7 (B_THR_ALIVE, 10k pull-down R4) -> U1 B2 (A2 = GND).  B toggles it at 1 kHz.
U1 /CLR1 = /CLR2 = R5 10k to 5 V + C5 1 uF to GND (+1N4148 across R5): Q held low ~10 ms after power-up.
U1 timing: Rx1 = Rx2 = 100k, Cx1 = Cx2 = 100 nF  -> tw ~ 0.45 x Rx x Cx ~ 4.5 ms (UNVERIFIED: scope it).
U1, U2, U3, U4 on the same 5 V rail as the pedal: rail loss kills the pedal signal too.
```
- **A's alive = MCP4131 CS (valid):** the ISR writes the wiper every 1 ms only while A's cross-check enables and `loop()` is fresh (≤20 ms). On a fault it writes wiper 0 once, then CS stays idle → no rising edges → Q1 low in ≈4.5 ms. The 595 light code (not written yet) has its own latch line and must never toggle CS (`g_busLock`).
- **B's alive:** pin 7 toggles in B's 1 kHz ISR under the same rule. Pin 7 idles LOW with a pull-down, so the '123 /CLR release at power-up cannot trigger channel 2. Channel 1 can fire one ~4.5 ms pulse at /CLR release (CS idles HIGH), but the AND with B blocks it.
- **Levels:** the Teensy is 3.3 V. A 74HC123 at 5 V needs VIH ≥ 3.5 V, so use the **HCT** part (TTL thresholds, VIH 2.0 V). The MCP4131 at 5 V needs VIH 0.7·VDD = 3.5 V → buffer SDI/SCK/CS through a 74AHCT125.
- **Kelly HPD stays on** (docs/KELLY_HPD.md): if the clamp releases with the pedal down, the Kelly refuses to drive until the pedal returns to zero.

## Firmware cross-check (both Teensys, 1 kHz ISR)
- B reads its own pedal pin and computes the same map from its copy of the profile. A sends its commanded trim, mode, profile CRC8, ok/allowance/speed on **0x6A9 every 20 ms**. B answers with its own trim on **0x6A4 every 20 ms**. A streams the full profile table to B on **0x6AA** (one field per 50 ms, 35 fields ≈ 1.75 s). B follows A's mode and saves the synced copy to its own EEPROM (1 s after the last change).
- Each side drops its enable and latches a logged fault (serial `THROTTLE FAULT …`; clear with a key cycle or `MX`) when:
  - |own − peer| > **5 % for 50 ms** (PROPOSED). Own value is taken as the min/max band of the last 32 ms, so frame age doesn't false-trip while ramping. → THROTTLE-DISAGREE
  - the peer's heartbeat hasn't advanced for **200 ms** → THROTTLE-PEER-LOST
  - the profile CRC differs for 50 ms (e.g. B's EEPROM copy is wrong) → THROTTLE-PROFILE-MISMATCH
  - A only: BMS not Closed / drive fault, or `loop()` stale → THROTTLE-LOCAL
- **Boot:** enables stay off, with no fault, until each side has heard the other *and* the CRCs match (B syncs first).
- Single-Teensy builds (no link) can never enable the throttle.

## Sim scenarios (BmsSim, 31/31)
| Scenario | Result |
|---|---|
| A hangs | clamp 0 V in 6 ms; B logs PEER-LOST |
| B hangs | clamp 0 V in 6 ms; A logs PEER-LOST |
| B pedal reads +10 % | both DISAGREE, 0 V in 75 ms |
| B profile copy differs | both PROFILE-MISMATCH, 0 V in 56 ms |
| digipot stuck at 50 % + A fault | A stops alive, clamp holds 0 V in 6 ms |

Also checked in `scripts/test_throttle_cross.cpp`: 120 s of random pedal, speed and mode changes with no false trip; a 3 % offset doesn't trip.

## BOM (PROPOSED, none owned)
| Ref | Part | Qty | Note |
|---|---|---|---|
| U1 | TI CD74HCT123E (dual retriggerable monostable, DIP-16) | 1 | HCT for 3.3 V inputs |
| U2 | TI CD74HC4066E (quad bilateral switch, DIP-14) | 1 | 2 sections in series, 2 spare |
| U3 | Microchip MCP4131-103E/P (10k, 7-bit, DIP-8) | 1 | already in the plan |
| U4 | TI SN74AHCT125N (quad buffer, DIP-14) | 1 | 3.3 → 5 V for MCP4131 + CS |
| Rx1, Rx2 | 100k 1 % | 2 | one-shot timing |
| Cx1, Cx2 | 100 nF C0G/film | 2 | one-shot timing |
| R1 | 10k | 1 | Kelly throttle pull-down |
| R4 | 10k | 1 | B_THR_ALIVE pull-down |
| R5, C5, D5 | 10k, 1 µF, 1N4148 | 1 each | /CLR power-on hold |
| — | 10k + 18k 1 %, 100 nF | 2 sets | pedal sense dividers A pin 40, B pin 41 |
| — | 100 nF decoupling | 4 | one per IC |

## Pins
- **Teensy A:** pedal **pin 40 (A16)**, freed by moving the optional stock 4WD indicator to Teensy B pin 40 (`ST_4WD_B`, not read yet). MCP4131 on 35/36 (shared DATA/CLK) + CS 38 = alive. HV sense stays on 24.
- **Teensy B:** pedal **pin 41 (A17)**, alive **pin 7**, stock 4WD switch pin 40.
  - Pins B already uses: 0/1, 22/23, 30/31 (CAN), 4 (B fault), 2/3/5/6/11/12/20/21 (OUT pinModes), 13 (LED), 14/15/25 (LEM B), 16/17 (hub UART), 18/19 (IN3/IN4), 24 (HV sense).
  - 7, 40 and 41 are body-I/O pins on A only, and nothing on B drives them.

## UNVERIFIED (crew)
- The '123 pulse width formula (0.45·R·C is from memory of the HC/HCT123 datasheet curve): scope it, and target 3–8 ms.
- Kelly throttle input impedance or internal pull-up (the 10k pull-down must win), and its 0–5 V range on the KLS8080N.
- Pedal type and drive capability (MCP4131 10k + two 28k dividers ≈ 5.6k load).
- 74HC4066 on/off behaviour and leakage at 5 V with the Kelly input.
- Whether the pedal 5 V can back-feed an unpowered 4066/MCP4131 (same rail is required).
- Link bus load (below) has not been measured on the bus.

## CAN3 250k load with throttle frames
- Kelly: 40/s
- Elcon: 2/s (ASSUMED)
- Link B → A 0x6A0–0x6A3 every 50 ms: 80/s
- A → B 0x6A8: 10/s
- Throttle 0x6A9 + 0x6A4 every 20 ms: 100/s
- Profile sync 0x6AA: 20/s

Total ≈ **252 frames/s** × 131–160 bit ≈ 33.0–40.3 kbit/s = **13.2–16.1 %** (was 6.9–8.4 % without the throttle frames).

## Fallback: pedal direct to the Kelly (chief's option)
- Pedal signal → Kelly throttle input directly; no MCP4131, no clamp, no pedal maps.
- Keep both Teensys reading the pedal (pins 40 A / 41 B) for display and logging only.
- Power limiting falls back to:
  - the Kelly speed-mode / low-speed switch (OUT3 LIMP)
  - Kelly HPD (enabled in the Kelly config)
  - the BMS opening the contactors on faults
- To switch, set `THROTTLE_TRIM_ENABLE=0` (already), leave U1–U4 unfitted and jumper the pedal to the Kelly. No firmware change is needed.
