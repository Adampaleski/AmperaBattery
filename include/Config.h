#pragma once

#include <Arduino.h>

#define SERIALCONSOLE Serial

// Serial U (fake HV bus volts) / I (fake pack amps) bench inputs — chief review #2: they fed the real precharge
// gate and forced currentValid. Compiled OUT unless a bench env sets BENCH_SIM_INPUTS=1, and never with outputs live.
#ifndef BENCH_SIM_INPUTS
#define BENCH_SIM_INPUTS 0
#endif
#if BENCH_SIM_INPUTS && !(defined(TELEMETRY_ONLY) && TELEMETRY_ONLY)
#error "BENCH_SIM_INPUTS=1 requires TELEMETRY_ONLY=1 (chief review #2)"
#endif

// CAN bus (BICM internal network on K16 X2) — CAN3 pins 30/31 (string 1).
// Dual-string build: CAN1 pins 22/23 @125k = string 2 (same factory IDs, own bus).
constexpr uint32_t kCanBitrate        = 125000;
constexpr uint32_t kKeepAliveIntervalMs = 1000;
constexpr uint32_t kCanRecoveryNoRxMs   = 12000;
constexpr uint32_t kCanRecoveryRetryMs  = 15000;
constexpr uint16_t kCanFrameProcessBudget = 256;
constexpr uint8_t  kKeepAliveMaxTxQueueDepth = 2;

// Elcon / TC charger CAN — CAN2 pins 0/1, second SN65HVD230.
// 250 kbit, 29-bit. Not Brusa 500k. Not BICM 125k.
// Winding unread (HK-MF-108 vs 144 vs 72) — do not hardcode a range.
constexpr uint32_t kChargerCanBitrate     = 250000;
constexpr uint32_t kChargerPeriodMs       = 1000;  // Elcon expects ~1 s; timeout 5 s
constexpr uint8_t  kChargerMaxTxQueueDepth = 2;

constexpr uint16_t kCanScanMaxIds = 64;

#ifndef PACK_S_CELLS
#define PACK_S_CELLS 24
#endif

// Teryx pack layout (SOURCED: Adam 2026-10-21): 36S x 4P, four 36S strings in parallel.
// Documentation constants only; nothing reads them for control. See docs/PACK_LAYOUT.md.
constexpr int   kTeryxStringS        = 36;      // SOURCED: Adam
constexpr int   kTeryxStringsP       = 4;       // SOURCED: Adam
constexpr float kVoltGen1GroupAhAssumed = 45.0f; // ASSUMED: 3P x 15 Ah Gen1 cells, not measured
constexpr float kTeryxPackAhAssumed  = kTeryxStringsP * kVoltGen1GroupAhAssumed;  // 180 Ah ASSUMED
constexpr float kTeryxNominalV       = kTeryxStringS * 3.7f;  // ~133 V nominal (ASSUMED 3.7 V/cell); 147.6 V is FULL
// Monitoring (SOURCED: Adam 2026-10-21): 2 BICMs per string (one 24S M2/M3 + one M4 18S using c1-12) = 8 BICMs,
// 2 BICMs per string on 4 buses (Adam 2026-10-21): A CAN1=S1, CAN2=S2, CAN3=Kelly 250k; B CAN1=S3, CAN2=S4, CAN3=shared 250k vehicle bus (Kelly + Elcon + A-B link) (FlexCanCompat.h CAN_LAYOUT_V2)
constexpr int   kBicmPerString       = 2;
constexpr int   kBicmBuses           = 4;       // one per string; identical M4 IDs cannot share a bus
static_assert(kBicmBuses == kTeryxStringsP && kBicmPerString * kTeryxStringsP == 8, "pack needs 8 BICMs on 4 buses");
static_assert(24 + 12 == kTeryxStringS, "per string: 24S board + 12 channels of an M4 = 36S");
static_assert(kTeryxStringS * 4.10f < 168.0f, "36S charge ceiling must stay under the 168 V Kelly");

#ifndef PACK_MODULE_COUNT
#define PACK_MODULE_COUNT 4
#endif

#ifndef K112_PACK_DECODE
#if defined(K112_24S_SUBPACK) && K112_24S_SUBPACK
#define K112_PACK_DECODE 1
#else
#define K112_PACK_DECODE 0
#endif
#endif

// Four factory-strapped Volt BICMs on one CAN3 bus (include/FactoryCellMap.h).
#ifndef FACTORY_BUS_DECODE
#define FACTORY_BUS_DECODE 0
#endif

#ifndef PACK_BICM_COUNT
#define PACK_BICM_COUNT 1
#endif

#ifndef K112_SPARSE_SECOND_BICM
#define K112_SPARSE_SECOND_BICM 0
#endif

#ifndef BICM_CELLS_PER_MODULE
#define BICM_CELLS_PER_MODULE 6
#endif

// Factory Volt gen1 pack current sensor (LEM DHAB S/44) on A0/A1 + 5 V sense on A11.
// 0 = not fitted: pins untouched, current reads "not fitted" (I<amps> can simulate).
#ifndef CURRENT_SENSE_ENABLE
#define CURRENT_SENSE_ENABLE 0
#endif

#ifndef HV_ANALOG_ENABLE
#define HV_ANALOG_ENABLE 0
#endif

// Passive-bleed thresholds (used only when BMS_CAP_BALANCE_TX=1).
// Cell must be above both the floor and (pack min + hysteresis).
constexpr float kBalanceMinVoltage  = 3.90f;
constexpr float kBalanceHysteresis  = 0.04f;

// Charge setpoints packed into Elcon 0x1806E5F4 (only TX when BMS_CAP_CHARGE_TX=1).
// Demand is PACK_S_CELLS * cell V — not a charger winding. 36S * 4.10 V = 147.6 V.
// Do not charge a 24S sub-pack. A 72/96 V Elcon cannot finish 36S; a 312 V box
// will not start a drained 36S.
constexpr float    kChargeCellSetpointV   = 4.10f;  // stop charge when ANY cell >= this
constexpr float    kChargeFaultCellV      = 4.12f;  // latch charge fault (until reboot)
constexpr uint32_t kCellStaleMs           = 3000;   // cell older than this = not fresh
constexpr uint16_t kChargeCurrentDeciA    = 50;   // 5.0 A DC, 0.1 A units (keep <= ~8 A on a 120 V/15 A outlet)
#ifndef ELCON_MAX_A
#define ELCON_MAX_A 46.0f    // nameplate max output current
#endif
static_assert(kChargeCurrentDeciA <= (uint16_t)(ELCON_MAX_A * 10.0f), "charge current request exceeds Elcon nameplate max (ELCON_MAX_A)");
static_assert(kChargeCurrentDeciA <= 80, "charge current > 8 A: only on a 240 V circuit sized for it; raise this guard deliberately");

// Temperatures are NOT decoded/verified yet. Charge is refused unless this is 1.
// Only set 1 after the 0x7E0+ temp byte layout is verified at the bench AND
// temp limits are enforced in code.
// Throttle trim (drive-mode pedal maps, include/DriveProfile.h, Adam approved 2026-10-21).
// 0 = MCP4131 never written, pedal not read. CREW REVIEW before setting 1. PROPOSED pedal input
// pin 24 (A10) = HV_SENSE, so it cannot be used with HV_ANALOG_ENABLE=1.
#ifndef THROTTLE_TRIM_ENABLE
#define THROTTLE_TRIM_ENABLE 0
#endif
#ifndef TEMPS_VERIFIED
#define TEMPS_VERIFIED 0
#endif

// Elcon/TC output window (compile-time sanity check on charge voltage).
// ASSUMPTION (docs/ASSUMPTIONS.md): TC/Elcon 3.3 kW "144 V class" unit
// (e.g. HK-J/TCCH 144V), usable output ~100-180 V. The nameplate/winding has
// NOT been read. Override per env in platformio.ini build_flags, e.g.
//   -D ELCON_MIN_V=100.0f -D ELCON_MAX_V=180.0f
// 36S x 4.10 V = 147.6 V must fall inside this window or the build fails.
#ifndef ELCON_MIN_V
#define ELCON_MIN_V 100.0f
#endif
#ifndef ELCON_MAX_V
#define ELCON_MAX_V 202.0f   // nameplate: Elcon HK-LF-144-46, CAN3865, 100-202 V / 46 A DC (2026-10-15)
#endif

// Kelly KLS controller max bus voltage ("168 V version" -- VERIFY on the label
// and in the Kelly config's over-voltage setting). BmsCore clamps the charge
// pack charge voltage to KELLY_MAX_V - KELLY_MARGIN_V (36S x 4.10 = 147.6 V: never reached).
#ifndef KELLY_MARGIN_V
#define KELLY_MARGIN_V 5.0f   // charger never asked above KELLY_MAX_V - this
#endif
#ifndef KELLY_MAX_V
#define KELLY_MAX_V 168.0f
#endif

// Contactor / precharge timing (coil drive gated by BMS_CAP_CONTACTOR_DRV).
constexpr uint32_t kPrechargeMinMs        = 300;    // agreed 300 ms (aligned with kPcDualMinMs, chief 2026-10-22)
constexpr uint32_t kPrechargeTimeMs       = 3000;   // agreed 3 s (was 5000)
constexpr uint32_t kPrechargeOverlapMs    = 100;
constexpr float    kPrechargeRatio        = 0.95f;
constexpr float    kPrechargeMinBusV      = 80.0f;
// Dual-string precharge (Sparky, 2026-10-17): close main when HV bus >= 95% of
// the LOWEST string voltage AND precharge has run >= 300 ms; not there within
// 3 s -> PRECHARGE-FAIL fault, everything open, lockout until key cycle. NO automatic retry
// (chief 2026-10-22; the 2 retries / 30 s wait were removed). Print-only state machine.
constexpr uint32_t kPcDualMinMs       = 300;
constexpr uint32_t kPcDualTimeoutMs   = 3000;
constexpr float    kCellFaultMinV         = 2.50f;
constexpr float    kCellFaultMaxV         = 4.25f;
constexpr float    kHvAdcVref             = 3.3f;
constexpr float    kHvDividerRatio        = 60.606f;  // 200 V at 3.3 V; unused unless HV_ANALOG_ENABLE=1

// Pin map (matches VoltBMS carrier / Teensy 4.1 CAN3 bring-up)
namespace Pins {
constexpr int LED  = 13;
// IN1-IN4 moved off 16/17 (Serial4 hub UART) and 18/19 (I2C SDA/SCL) — crew 2026-10-22.
// Only pin 34 is free on BOTH boards (33 = reserved charge detect on A); IN2-IN4 = -1 (no free GPIO on A).
// Spare inputs on B only: 8, 10, 27-29, 33-39 (docs/PINMAP.yaml). Nothing reads IN1-IN4 yet.
constexpr int IN1  = 34;  // was 17; shares spare pin 34 with retired LIGHT_OE (nothing drives it)
constexpr int IN2  = -1;  // was 16
constexpr int IN3  = -1;  // was 18
constexpr int IN4  = -1;  // was 19
constexpr int OUT1 = 11;
constexpr int OUT2 = 12;
constexpr int OUT3 = 20;
constexpr int OUT4 = 21;
// OUT5/OUT6 moved off 22/23 (2026-10-17): pins 22/23 are now CAN1 TX/RX
// (string 2 BICM bus @125k on the dual-string build). Nothing drives 22/23.
constexpr int OUT5 = 2;   // was 22
constexpr int OUT6 = 3;   // was 23
// Role B optional hard-wired fault output (DISABLED unless B_FAULT_PIN_ENABLE=1).
// Level: HIGH = healthy, LOW = fault (fail-safe: unpowered/broken wire = fault).
constexpr int B_FAULT_OUT = 4;
constexpr int CAN1_TX = 22;  // CAN1: string 2 BICM (bench envs) / string 1 BICM (dual, CAN_LAYOUT_V2), SN65HVD230
constexpr int CAN1_RX = 23;
constexpr int OUT7 = 5;
constexpr int OUT8 = 6;

// VoltBMSV2 names: OUT1=main+, OUT2=precharge, OUT4=main-.
// 12 V coil FETs downstream — Teensy pin is the FET gate, not the coil.
// Precharge = salvaged AEV14012 on OUT2 (TE EV11 dropped); coil 0.35 A @12 V, 34 ohm, 4.2 W, no economizer, ~0.41 A @14 V, 10-16 V (SOURCED Panasonic catalog, crew 2026-10-23).
constexpr int MAIN_POS  = OUT1;  // pin 11
constexpr int PRECHARGE = OUT2;  // pin 12 — AEV14012
constexpr int MAIN_NEG  = OUT4;  // pin 21
constexpr int HV_SENSE  = 24;    // A10, RESERVED spare: precharge uses Kelly CAN B+ vs BICM sum (room DECIDED 2026-10-23, firmware TODO); never a plain divider
// LEM DHAB S/44 (CURRENT_SENSE_ENABLE=1). Every input through 10k/18k divider + 100 nF.
constexpr int CUR_LO    = 14;    // A0  DHAB pin 4 (ch1, +-20 A, 100 mV/A)
constexpr int CUR_HI    = 15;    // A1  DHAB pin 3 (ch2, -420..+230 A, 6.15 mV/A)
constexpr int CUR_VC    = 25;    // A11 DHAB 5 V supply (ratiometric reference)

// ---- Body / aux outputs (Adam 2026-10-21) — RESERVED pin map, PROPOSED. ----
// Not pinMode'd or driven by any firmware yet (no flag changed). Hardware: outputs daughterboard v2
// (perfboard/make_body.py). Direct channels = 3.3 V gate to low-side IRLB8721; relay/valve/pump get 1N4007 flyback.
// 2008 Teryx 750 (KRF750A8F): stock 4WD = engine-vacuum diaphragm + solenoid valve pulling a cable to the front
// diff (service manual "vacuum actuator / solenoid valve" tests; teryxforums threads 9680, 17597). No engine = no vacuum.
// Mechanic plan (2026-10-21): replace with two 12 V linear actuators with pot feedback, one on the 4WD cable
// (~3/4 in travel) and one on the mechanical diff-lock cable. Each = H-bridge (2 PWM pins) + 3.3 V-scaled ADC.
// Candidates (docs/BODY_IO.md): Firgelli 4 in 150 lb w/ pot, Glideforce LACT4P. Actuator + H-bridge UNVERIFIED, nothing bought.
constexpr int SEAT_DRV   = OUT7;  // pin 5  PWM  seat heater driver   (kit stages UNVERIFIED; 33/66/100 % PROPOSED)
constexpr int SEAT_PAS   = OUT8;  // pin 6  PWM  seat heater passenger
constexpr int OUT9  = 7;   constexpr int RAD_FAN   = OUT9;   // fan RELAY coil (on ~40 C PROPOSED)
constexpr int OUT10 = 8;   constexpr int COOL_PUMP = OUT10;  // PWM Bosch coolant pump (key-on)
constexpr int ACT4WD_IN1  = 9;    // H-bridge #1 IN1 (PWM) extend  = 4WD
constexpr int ACT4WD_IN2  = 10;   // H-bridge #1 IN2 (PWM) retract = 2WD
constexpr int ACTDIF_IN1  = 28;   // H-bridge #2 IN1 (PWM) extend  = lock (progressive)
constexpr int ACTDIF_IN2  = 29;   // H-bridge #2 IN2 (PWM) retract = unlock
constexpr int POS_4WD     = 41;   // A17 4WD actuator pot wiper, pot fed from 3.3 V (or 5 V via 10k/18k divider)
constexpr int POS_DIFF    = 39;   // A15 diff actuator pot wiper
// Pin 40 (A16) on Teensy A = throttle pedal sense (crew 2026-10-21). The OPTIONAL stock front-diff 4WD
// indicator moved to Teensy B pin 40 (B runs no body I/O; state forwarded later if ever needed, not read yet).
constexpr int PEDAL_IN_A  = 40;   // A16 pedal wiper on A, 10k/18k divider + 100 nF (5 V pedal -> 3.21 V)
constexpr int ST_4WD_B    = 40;   // Teensy B: OPTIONAL stock 4WD indicator switch, divider + 100 nF (not read yet)
constexpr int PEDAL_IN_B  = 41;   // Teensy B A17: same pedal wiper, own 10k/18k divider + 100 nF
constexpr int B_THR_ALIVE = 7;    // Teensy B: 1 kHz alive toggle -> 74HC123 #2 B input (throttle enable B)
// BTS7960 drivers (Sparky): RPWM/LPWM = the IN1/IN2 pins above; R_EN+L_EN of BOTH modules on one pin,
// 10k pull-DOWN on the daughterboard -> disabled at boot. IS current sense via 10k/18k divider (5 V -> 3.21 V) + 100 nF.
constexpr int ACT_EN      = 37;   // both BTS7960 enables (active high)
constexpr int IS_4WD      = 26;   // A12 BTS7960 #1 IS (R_IS+L_IS), over-current ~7 A PROPOSED, scale UNVERIFIED
constexpr int IS_DIFF     = 27;   // A13 BTS7960 #2 IS
// Lights on 2x 74HC595, BIT-BANGED (chief 2026-10-21; SPI0 unusable: 11/12/13 = OUT1/OUT2/LED).
// 595 bits: 0 HEAD_LO 1 HEAD_HI 2 TAIL 3 BRAKE 4 TURN_L 5 TURN_R 6 REVERSE 7 LIGHT_BAR | 8 WORK, 9-15 spare.
// Hazard = TURN_L + TURN_R (no own bit). Blink is done in firmware.
constexpr int LIGHT_DATA  = 35;
constexpr int LIGHT_CLK   = 36;
// BODY_IO_EXPANDER=1 (crew 2026-10-21, PROPOSED, Adam decision): lights + stock 4WD switch on an MCP23017 on Wire
// (SDA 18 / SCL 19 = IN3/IN4), 595s dropped -> 32 and 34 freed. Pin 32 -> park contact A (direct GPIO, safety
// input NEVER on the expander). With BODY_IO_EXPANDER=0 there is NO free direct pin on Teensy A for park.
#ifndef BODY_IO_EXPANDER
#define BODY_IO_EXPANDER 1
#endif
#if BODY_IO_EXPANDER
constexpr int LIGHT_LATCH = -1;   // retired: lights on MCP23017
constexpr int LIGHT_OE    = 34;   // freed (spare). Kept named for the clash check; nothing drives it.
constexpr int I2C_SDA     = 18;   // MCP23017 (IN3)
constexpr int I2C_SCL     = 19;   // MCP23017 (IN4)
#else
constexpr int LIGHT_LATCH = 32;   // 595 RCLK
constexpr int LIGHT_OE    = 34;   // 595 /OE, 10k pull-UP: lights stay off until firmware drives it low
#endif
// MCP4131 throttle digipot shares LIGHT_DATA/LIGHT_CLK (bit-banged, own CS); 595s ignore it (no latch pulse). PROPOSED.
constexpr int THR_CS      = 38;
// A's throttle alive signal (chief 2026-10-21): its OWN heartbeat pin, NOT the MCP4131 CS (a loop stuck rewriting a
// stale wiper would still toggle CS). Toggled in A's 1 kHz ISR only after that cycle's cross-check + loop-fresh pass.
// Pin 4 on A: B_FAULT_OUT is role-B only; nothing on A pinModes 4 (grep 2026-10-21: only Config.h names it).
constexpr int A_THR_ALIVE = 4;    // Teensy A: 1 kHz heartbeat -> 74AHCT125 -> 74HC123 #1 B input (throttle enable A)
// Parking brake (docs/PARK_BRAKE.md, crew 2026-10-21): DIRECT GPIO on both Teensys (never the expander), 10k pull-up
// to 3.3 V + 100 nF, LOW = brake set. A: pin 32 (freed by BODY_IO_EXPANDER). B: pin 32 (A-only body pin, free on B).
#if BODY_IO_EXPANDER
constexpr int PARK_IN_A   = 32;   // Teensy A: park contact 1
#else
constexpr int PARK_IN_A   = -1;   // NO free direct pin on A without the expander
#endif
constexpr int PARK_IN_B   = 32;   // Teensy B: park contact 2
// Spare edge pins on A now: 34 (with expander). 33 RESERVED = charge plug-in detect (PROPOSED). 24 = HV_SENSE.
// Teensy B pins in use: 0/1 22/23 30/31 CAN, 4 B_FAULT_OUT, 2 3 5 6 11 12 20 21 OUT*, 13 LED, 14 15 25 LEM B, 16/17 hub UART,
// 18 19 IN3/IN4, 24 HV_SENSE -> 7, 40, 41 are free on B (A's body-I/O map is not used on B).
// Teensy A pin clash check: heartbeat + park input vs every pin A drives or reads.
static_assert(A_THR_ALIVE != THR_CS && A_THR_ALIVE != LIGHT_DATA && A_THR_ALIVE != LIGHT_CLK && A_THR_ALIVE != LIGHT_LATCH &&
              A_THR_ALIVE != LIGHT_OE && A_THR_ALIVE != PEDAL_IN_A && A_THR_ALIVE != OUT5 && A_THR_ALIVE != OUT6 &&
              A_THR_ALIVE != OUT7 && A_THR_ALIVE != OUT8 && A_THR_ALIVE != OUT9 && A_THR_ALIVE != OUT10 && A_THR_ALIVE != OUT1 &&
              A_THR_ALIVE != OUT2 && A_THR_ALIVE != OUT3 && A_THR_ALIVE != OUT4 && A_THR_ALIVE != LED && A_THR_ALIVE != IN1 &&
              A_THR_ALIVE != IN2 && A_THR_ALIVE != IN3 && A_THR_ALIVE != IN4 && A_THR_ALIVE != CAN1_TX && A_THR_ALIVE != CAN1_RX &&
              A_THR_ALIVE != HV_SENSE && A_THR_ALIVE != CUR_LO && A_THR_ALIVE != CUR_HI && A_THR_ALIVE != CUR_VC &&
              A_THR_ALIVE != ACT4WD_IN1 && A_THR_ALIVE != ACT4WD_IN2 && A_THR_ALIVE != ACTDIF_IN1 && A_THR_ALIVE != ACTDIF_IN2 &&
              A_THR_ALIVE != POS_4WD && A_THR_ALIVE != POS_DIFF && A_THR_ALIVE != ACT_EN && A_THR_ALIVE != IS_4WD &&
              A_THR_ALIVE != IS_DIFF && A_THR_ALIVE != 0 && A_THR_ALIVE != 1 && A_THR_ALIVE != 30 && A_THR_ALIVE != 31 &&
              A_THR_ALIVE != 33, "A heartbeat pin clash");
#if BODY_IO_EXPANDER
static_assert(PARK_IN_A != A_THR_ALIVE && PARK_IN_A != PEDAL_IN_A && PARK_IN_A != THR_CS && PARK_IN_A != LIGHT_DATA &&
              PARK_IN_A != LIGHT_CLK && PARK_IN_A != LIGHT_LATCH && PARK_IN_A != I2C_SDA && PARK_IN_A != I2C_SCL &&
              PARK_IN_A != IN1 && PARK_IN_A != IN2 && PARK_IN_A != HV_SENSE && PARK_IN_A != CUR_LO && PARK_IN_A != CUR_HI &&
              PARK_IN_A != CUR_VC && PARK_IN_A != OUT1 && PARK_IN_A != OUT2 && PARK_IN_A != OUT3 && PARK_IN_A != OUT4 &&
              PARK_IN_A != OUT5 && PARK_IN_A != OUT6 && PARK_IN_A != OUT7 && PARK_IN_A != OUT8 && PARK_IN_A != OUT9 &&
              PARK_IN_A != OUT10 && PARK_IN_A != ACT4WD_IN1 && PARK_IN_A != ACT4WD_IN2 && PARK_IN_A != ACTDIF_IN1 &&
              PARK_IN_A != ACTDIF_IN2 && PARK_IN_A != POS_4WD && PARK_IN_A != POS_DIFF && PARK_IN_A != ACT_EN &&
              PARK_IN_A != IS_4WD && PARK_IN_A != IS_DIFF && PARK_IN_A != CAN1_TX && PARK_IN_A != CAN1_RX && PARK_IN_A != LED &&
              PARK_IN_A != 0 && PARK_IN_A != 1 && PARK_IN_A != 30 && PARK_IN_A != 31 && PARK_IN_A != 33, "A park pin clash");
#endif
// B pins in use: 0/1 2 3 4 5 6 7 11 12 13 14 15 16/17 18/19 20 21 22/23 24 25 30/31 40 41.
static_assert(PARK_IN_B != B_THR_ALIVE && PARK_IN_B != PEDAL_IN_B && PARK_IN_B != ST_4WD_B && PARK_IN_B != B_FAULT_OUT &&
              PARK_IN_B != HV_SENSE && PARK_IN_B != CUR_LO && PARK_IN_B != CUR_HI && PARK_IN_B != CUR_VC && PARK_IN_B != IN1 &&
              PARK_IN_B != IN2 && PARK_IN_B != IN3 && PARK_IN_B != IN4 && PARK_IN_B != LED && PARK_IN_B != CAN1_TX &&
              PARK_IN_B != CAN1_RX && PARK_IN_B != 0 && PARK_IN_B != 1 && PARK_IN_B != 30 && PARK_IN_B != 31 &&
              PARK_IN_B != OUT1 && PARK_IN_B != OUT2 && PARK_IN_B != OUT3 && PARK_IN_B != OUT4 && PARK_IN_B != OUT5 &&
              PARK_IN_B != OUT6 && PARK_IN_B != OUT7 && PARK_IN_B != OUT8, "B park pin clash");
static_assert(B_THR_ALIVE != B_FAULT_OUT && PEDAL_IN_B != HV_SENSE && PEDAL_IN_B != CUR_LO && PEDAL_IN_B != CUR_HI && PEDAL_IN_B != CUR_VC, "B pin clash");
}  // namespace Pins

// MCP23017 body-expander bit reservations (Build 18, LATER, PROPOSED). NAMES ONLY: nothing reads or drives these yet.
// Teensy A has no free edge output (docs/PINMAP.yaml conflicts). PACK_HEAT is NOT reserved (chief review 2026-10-23).
namespace ExpBits {
constexpr int BUZZER    = 13;   // GPB5: 12 V buzzer for the dash master alert
constexpr int MOTOR_FAN = 14;   // GPB6: motor fan relay (low-side IRLB8721 + 1N4007); future trigger Kelly CAN 0x0CF11F05 byte 2
static_assert(BUZZER != MOTOR_FAN && BUZZER >= 9 && BUZZER <= 15 && MOTOR_FAN >= 9 && MOTOR_FAN <= 15,
              "ExpBits: bits 0-8 are the lights; reservations must be unique in 9-15");
}  // namespace ExpBits
