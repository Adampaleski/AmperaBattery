#pragma once
// Pack current from the factory Volt gen1 sensor: LEM DHAB S/44 (open-loop
// Hall, 5 V, two ratiometric analog outputs). Maths in BmsCore::dhabAmps so the
// host sim / wasm page use the identical code.
//
// Wiring (SimpBMS manual pinout; datasheet LEM DHAB S/44):
//   DHAB 1  +5 V   (purple/red)       <- 5 V regulator, also -> 10k/18k -> A11 (pin 25)
//   DHAB 2  GND    (black/grey)       <- Teensy GND
//   DHAB 3  ch2 high range (lt blue/yellow)  -> 10k -> A1 (pin 15), 18k + 100 nF to GND
//   DHAB 4  ch1 low range  (lt green/brown)  -> 10k -> A0 (pin 14), 18k + 100 nF to GND
// Teensy 4.1 analog pins are 3.3 V MAX; never wire DHAB outputs direct.
//
// Gated by CURRENT_SENSE_ENABLE (default 0). With it off, valid()=false unless a
// bench simulation is set with I<amps> (dry-run only).
namespace CurrentSense {
void  begin();
void  tick();                 // call every loop; samples at 100 Hz
bool  valid();
float amps();                 // + = discharge (set invert after a known-direction test)
bool  calibrateZero();        // 'Z': no current flowing; stores offsets to EEPROM
void  setSimAmps(float a, bool on);
void  printStatus();
}  // namespace CurrentSense
