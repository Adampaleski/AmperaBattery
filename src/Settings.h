#pragma once
// Runtime settings Adam can change over USB serial without a code change.
// Stored in EEPROM (Teensy 4.1 emulated EEPROM, survives power-off/reflash
// unless the flash is fully erased). Values outside the hard bounds are refused.
//
//   P80   <Enter>   charge limit %, 100 % = 4.10 V/cell, 80 % = 3.95 V  bounds 50-100  default 80
//   L3.30 <Enter>   low cutoff per cell, V        bounds 3.00-3.50  default 3.30
//   W10   <Enter>   balance-plan deadband, mV     bounds 2-50       default 10
//   X     <Enter>   restore all defaults (P/L/W; thresholds use TX)
//   T     <Enter>   list BMS thresholds; TocA 600 sets one, TocA - resets, TX all (include/Thresholds.h)
#include <stdint.h>

namespace BmsCore { struct Config; }

namespace Settings {
// Charge ceiling is now set as a PERCENT (P80 <Enter>), mapped linearly by
// BmsCore::ceilingForPct: 100 % = 4.10 V, 80 % = 3.95 V, 7.5 mV per %, 50..100 %.
constexpr float kCeilMin = 3.725f, kCeilMax = 4.10f;  // 50 % .. 100 %
constexpr int   kPctMin  = 50,    kPctMax  = 100,   kPctDef  = 80;
constexpr float kLowMin  = 3.00f, kLowMax  = 3.50f, kLowDef  = 3.30f;
constexpr int   kDbMin   = 2,     kDbMax   = 50,    kDbDef   = 10;

void  begin();          // load from EEPROM (defaults if blank/corrupt) and print
int   chargePct();      // P setting, 50..100 (default 80)
float ceilingV();       // charge stop: any cell >= this (pct map; pack clamp KELLY_MAX_V - KELLY_MARGIN_V)
// Current-sensor zero offsets as fraction of the 5 V supply (0 = datasheet default).
float curOff1Frac();
float curOff2Frac();
void  setCurOffsets(float f1, float f2);
float chargeFaultV();   // latch: ceiling + 0.05 V (<= 4.20 V)
float lowCutoffV();     // string fault (contactors open) below this
float deadbandV();      // balance planner deadband
bool  handleLine(const char *line);  // true if it was a settings/sim command
void  print();
void  applyThresholds(BmsCore::Config &c);  // saved T overrides on top of code defaults
float simBusV();        // 'U' bench simulation of HV bus volts for the dry-run precharge (0 = off)
}  // namespace Settings
