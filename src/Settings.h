#pragma once
// Runtime settings Adam can change over USB serial without a code change.
// Stored in EEPROM (Teensy 4.1 emulated EEPROM, survives power-off/reflash
// unless the flash is fully erased). Values outside the hard bounds are refused.
//
//   V4.05 <Enter>   charge ceiling per cell, V    bounds 3.90-4.15  default 4.05
//   L3.30 <Enter>   low cutoff per cell, V        bounds 3.00-3.50  default 3.30
//   W10   <Enter>   balance-plan deadband, mV     bounds 2-50       default 10
//   X     <Enter>   restore all defaults
#include <stdint.h>

namespace Settings {
constexpr float kCeilMin = 3.90f, kCeilMax = 4.15f, kCeilDef = 4.05f;
constexpr float kLowMin  = 3.00f, kLowMax  = 3.50f, kLowDef  = 3.30f;
constexpr int   kDbMin   = 2,     kDbMax   = 50,    kDbDef   = 10;

void  begin();          // load from EEPROM (defaults if blank/corrupt) and print
float ceilingV();       // charge stop: any cell >= this
float chargeFaultV();   // latch: ceiling + 0.05 V (<= 4.20 V)
float lowCutoffV();     // string fault (contactors open) below this
float deadbandV();      // balance planner deadband
bool  handleLine(const char *line);  // true if it was a settings/sim command
void  print();
float simBusV();        // 'U' bench simulation of HV bus volts for the dry-run precharge (0 = off)
}  // namespace Settings
