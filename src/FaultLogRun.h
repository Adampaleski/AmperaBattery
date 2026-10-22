#pragma once
// Firmware side of the power-off fault log (backlog item 5, step 3).
// Runs the same include/FaultLog.h the host checks and the simulator use, on
// the Teensy emulated EEPROM (EEPROM.read / EEPROM.update, bytes 256..1215).
//
// Role A (dual string) only: that is where BmsCore runs. Elsewhere every call
// is a no-op. It only WRITES EEPROM on fault / derate edges (rate-limited by
// FaultLog) and never touches a pin, so it is safe with TELEMETRY_ONLY=1 and
// every BMS_CAP_* = 0. Not bench-tested yet (no power-pull test done).
//
// Serial:  f            print the log, newest first
//          FCLEAR Enter erase the log (explicit, full word only)
namespace FaultLogRun {
void begin();                       // scan EEPROM, start a new boot number
void tick();                        // log BmsCore fault-bit and derate edges
void print();                       // 'f'
bool handleLine(const char *line);  // "FCLEAR"; false if not ours
}  // namespace FaultLogRun
