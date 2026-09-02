#pragma once

#include <Arduino.h>

#define SERIALCONSOLE Serial

// CAN bus (BICM internal network on K16 X2) — CAN3 pins 30/31
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

#ifndef PACK_BICM_COUNT
#define PACK_BICM_COUNT 1
#endif

#ifndef K112_SPARSE_SECOND_BICM
#define K112_SPARSE_SECOND_BICM 0
#endif

#ifndef BICM_CELLS_PER_MODULE
#define BICM_CELLS_PER_MODULE 6
#endif

#ifndef HV_ANALOG_ENABLE
#define HV_ANALOG_ENABLE 0
#endif

// Passive-bleed thresholds (used only when BMS_CAP_BALANCE_TX=1).
// Cell must be above both the floor and (pack min + hysteresis).
constexpr float kBalanceMinVoltage  = 3.90f;
constexpr float kBalanceHysteresis  = 0.04f;

// Charge setpoints packed into Elcon 0x1806E5F4 (only TX when BMS_CAP_CHARGE_TX=1).
// Demand is PACK_S_CELLS * cell V — not a charger winding. 36S * 4.15 V = 149.4 V.
// Do not charge a 24S sub-pack. A 72/96 V Elcon cannot finish 36S; a 312 V box
// will not start a drained 36S.
constexpr float    kChargeCellSetpointV   = 4.15f;
constexpr uint16_t kChargeCurrentDeciA    = 50;   // 5.0 A DC, 0.1 A units

// Contactor / precharge timing (coil drive gated by BMS_CAP_CONTACTOR_DRV).
constexpr uint32_t kPrechargeMinMs        = 200;
constexpr uint32_t kPrechargeTimeMs       = 5000;
constexpr uint32_t kPrechargeOverlapMs    = 100;
constexpr float    kPrechargeRatio        = 0.95f;
constexpr float    kPrechargeMinBusV      = 80.0f;
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
constexpr int OUT5 = 22;
constexpr int OUT6 = 23;
constexpr int OUT7 = 5;
constexpr int OUT8 = 6;

// VoltBMSV2 names: OUT1=main+, OUT2=precharge, OUT4=main-.
// 12 V coil FETs downstream — Teensy pin is the FET gate, not the coil.
// Shop precharge coil is TE EV11 / K1KABNA on OUT2, not a third EV200.
constexpr int MAIN_POS  = OUT1;  // pin 11
constexpr int PRECHARGE = OUT2;  // pin 12 — TE EV11 / K1KABNA
constexpr int MAIN_NEG  = OUT4;  // pin 21
constexpr int HV_SENSE  = 24;    // A10, optional HV-bus divider
}  // namespace Pins
