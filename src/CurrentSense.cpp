#include "CurrentSense.h"
#include <Arduino.h>
#include "BmsCore.h"
#include "SocEstimator.h"  // header-only; not used until CURRENT_SENSE_ENABLE
#include "Config.h"
#include "Settings.h"

namespace CurrentSense {
namespace {
BmsCore::CurrentCal g_cal;
float g_amps = 0;
bool  g_valid = false;
bool  g_simOn = false;
float g_simA = 0;
uint32_t g_lastMs = 0;

#if CURRENT_SENSE_ENABLE
float g_a1 = 0, g_a2 = 0, g_vc = 0;  // filtered pin volts
bool  g_primed = false;
// 16x oversample per channel (10-bit default resolution kept: the HV sense
// path assumes /1023). Then a 1st-order IIR, ~0.1 s, matching the 80 Hz sensor.
float readPinV(int pin) {
    uint32_t sum = 0;
    for (int i = 0; i < 16; i++) sum += analogRead(pin);
    return (sum / 16.0f) * (3.3f / 1023.0f);
}
#endif

void loadCal() {
    const float f1 = Settings::curOff1Frac(), f2 = Settings::curOff2Frac();
    if (f1 > 0.3f && f1 < 0.7f) g_cal.off1Frac = f1;
    if (f2 > 0.4f && f2 < 0.8f) g_cal.off2Frac = f2;
}
}  // namespace

void begin() {
    loadCal();
#if CURRENT_SENSE_ENABLE
    pinMode(Pins::CUR_LO, INPUT);
    pinMode(Pins::CUR_HI, INPUT);
    pinMode(Pins::CUR_VC, INPUT);
#endif
}

void tick() {
    const uint32_t now = millis();
    if (now - g_lastMs < 10) return;
    g_lastMs = now;
#if CURRENT_SENSE_ENABLE
    const float a1 = readPinV(Pins::CUR_LO), a2 = readPinV(Pins::CUR_HI), vc = readPinV(Pins::CUR_VC);
    if (!g_primed) { g_a1 = a1; g_a2 = a2; g_vc = vc; g_primed = true; }
    const float k = 0.1f;
    g_a1 += k * (a1 - g_a1); g_a2 += k * (a2 - g_a2); g_vc += k * (vc - g_vc);
    bool ok = false;
    g_amps = BmsCore::dhabAmps(g_cal, g_a1, g_a2, g_vc, &ok);
    g_valid = ok;
#endif
}

bool  valid() { return g_simOn ? true : g_valid; }
float amps()  { return g_simOn ? g_simA : g_amps; }

bool calibrateZero() {
#if CURRENT_SENSE_ENABLE
    if (!g_primed) return false;
    const BmsCore::CurrentCal before = g_cal;
    BmsCore::dhabCalibrateZero(g_cal, g_a1, g_a2, g_vc);
    if (g_cal.off1Frac < 0.3f || g_cal.off1Frac > 0.7f || g_cal.off2Frac < 0.4f || g_cal.off2Frac > 0.8f) {
        g_cal = before;  // implausible: wrong pin / current flowing
        return false;
    }
    Settings::setCurOffsets(g_cal.off1Frac, g_cal.off2Frac);
    return true;
#else
    return false;
#endif
}

void setSimAmps(float a, bool on) { g_simOn = on; g_simA = a; }

void printStatus() {
    SERIALCONSOLE.print(F("CURRENT (LEM DHAB S/44) "));
    if (g_simOn) { SERIALCONSOLE.print(F("SIMULATED ")); SERIALCONSOLE.print(g_simA, 1); SERIALCONSOLE.println(F(" A (I0 = off)")); return; }
#if CURRENT_SENSE_ENABLE
    SERIALCONSOLE.print(g_valid ? F("") : F("INVALID "));
    SERIALCONSOLE.print(g_amps, 2);
    SERIALCONSOLE.print(F(" A  Vc="));
    SERIALCONSOLE.print(g_vc / g_cal.divider, 2);
    SERIALCONSOLE.print(F(" ch1="));
    SERIALCONSOLE.print(g_a1 / g_cal.divider, 3);
    SERIALCONSOLE.print(F(" ch2="));
    SERIALCONSOLE.print(g_a2 / g_cal.divider, 3);
    SERIALCONSOLE.print(F(" V  zero1="));
    SERIALCONSOLE.print(g_cal.off1Frac, 4);
    SERIALCONSOLE.print(F(" zero2="));
    SERIALCONSOLE.println(g_cal.off2Frac, 4);
#else
    SERIALCONSOLE.println(F("not fitted (CURRENT_SENSE_ENABLE=0)"));
#endif
}
}  // namespace CurrentSense
