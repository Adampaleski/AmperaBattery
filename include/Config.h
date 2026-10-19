#pragma once

#include <Arduino.h>

#define SERIALCONSOLE Serial

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
// ceiling to KELLY_MAX_V / string cells: 42S -> 4.00 V/cell even at 100 %.
#ifndef KELLY_MAX_V
#define KELLY_MAX_V 168.0f
#endif

// Contactor / precharge timing (coil drive gated by BMS_CAP_CONTACTOR_DRV).
constexpr uint32_t kPrechargeMinMs        = 200;
constexpr uint32_t kPrechargeTimeMs       = 5000;
constexpr uint32_t kPrechargeOverlapMs    = 100;
constexpr float    kPrechargeRatio        = 0.95f;
constexpr float    kPrechargeMinBusV      = 80.0f;
// Dual-string precharge (Sparky, 2026-10-17): close main when HV bus >= 95% of
// the LOWEST string voltage AND precharge has run >= 300 ms; not there within
// 3 s -> fault; max 2 retries, 30 s apart, then lockout. Print-only state machine.
constexpr uint32_t kPcDualMinMs       = 300;
constexpr uint32_t kPcDualTimeoutMs   = 3000;
constexpr uint32_t kPcDualRetryWaitMs = 30000;
constexpr uint8_t  kPcDualMaxRetries  = 2;
constexpr float    kCellFaultMinV         = 2.50f;
constexpr float    kCellFaultMaxV         = 4.25f;
constexpr float    kHvAdcVref             = 3.3f;
constexpr float    kHvDividerRatio        = 60.606f;  // 200 V at 3.3 V; unused unless HV_ANALOG_ENABLE=1

// Pin map (matches VoltBMS carrier / Teensy 4.1 CAN3 bring-up)
namespace Pins {
constexpr int LED  = 13;
constexpr int IN1  = 17;
constexpr int IN2  = 16;
constexpr int IN3  = 18;
constexpr int IN4  = 19;
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
constexpr int CAN1_TX = 22;  // string 2 BICM bus (CAN1), SN65HVD230 #3
constexpr int CAN1_RX = 23;
constexpr int OUT7 = 5;
constexpr int OUT8 = 6;

// VoltBMSV2 names: OUT1=main+, OUT2=precharge, OUT4=main-.
// 12 V coil FETs downstream — Teensy pin is the FET gate, not the coil.
// Shop precharge coil is TE EV11 / K1KABNA on OUT2, not a third EV200.
constexpr int MAIN_POS  = OUT1;  // pin 11
constexpr int PRECHARGE = OUT2;  // pin 12 — TE EV11 / K1KABNA
constexpr int MAIN_NEG  = OUT4;  // pin 21
constexpr int HV_SENSE  = 24;    // A10, optional HV-bus divider
// LEM DHAB S/44 (CURRENT_SENSE_ENABLE=1). Every input through 10k/18k divider + 100 nF.
constexpr int CUR_LO    = 14;    // A0  DHAB pin 4 (ch1, +-20 A, 100 mV/A)
constexpr int CUR_HI    = 15;    // A1  DHAB pin 3 (ch2, -420..+230 A, 6.15 mV/A)
constexpr int CUR_VC    = 25;    // A11 DHAB 5 V supply (ratiometric reference)
}  // namespace Pins
