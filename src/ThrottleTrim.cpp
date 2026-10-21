// Reduce-only throttle trim (Adam approved 2026-10-21). CREW REVIEW before THROTTLE_TRIM_ENABLE=1.
// 1 kHz IntervalTimer: read pedal (PEDAL_IN), 5 ms filter, profile curve/cap/ramp/top speed,
// write MCP4131 wiper (bit-banged on LIGHT_DATA/LIGHT_CLK, CS THR_CS).
// Output -> 0 (wiper 0) on: BMS not Closed, any drive fault, Kid without speed, trim disabled.
// Watchdog reset: begin() writes wiper 0 first thing. HARDWARE FLAG: the MCP4131 powers up at
// mid-scale (0x40 = 50 % pass-through) until begin() runs -> needs a hardware default-off (crew).
#include <Arduino.h>
#include <stdlib.h>
#include <string.h>
#include "Config.h"
#include "ThrottleTrim.h"
#include "ContactorSeq.h"
#include "KellyDecode.h"

#ifndef SERIALCONSOLE
#define SERIALCONSOLE Serial
#endif

namespace ThrottleTrim {
namespace {
DriveProfile::Table    g_tab;
DriveProfile::Selector g_sel;
DriveProfile::Trim     g_trim;
IntervalTimer          g_timer;
volatile bool     g_ok = false, g_speedValid = false;
volatile float    g_allow = 0, g_mph = 0;
volatile float    g_pedal = 0, g_out = 0;
volatile uint8_t  g_wiper = 0;
volatile uint32_t g_lastUs = 0, g_maxLatUs = 0, g_maxCompUs = 0, g_ticks = 0;
volatile bool     g_busLock = false;   // future: 595 light shifts must set this (shared DATA/CLK)

void mcpWrite(uint8_t w) {
    if (w > 128) w = 128;
    const uint16_t cmd = static_cast<uint16_t>(w);   // addr 0 (wiper 0), cmd 00 write, 9-bit data
    digitalWriteFast(Pins::THR_CS, LOW);
    for (int i = 15; i >= 0; i--) {
        digitalWriteFast(Pins::LIGHT_DATA, (cmd >> i) & 1);
        digitalWriteFast(Pins::LIGHT_CLK, HIGH);
        delayNanoseconds(100);
        digitalWriteFast(Pins::LIGHT_CLK, LOW);
    }
    digitalWriteFast(Pins::THR_CS, HIGH);
}

void isr() {
    const uint32_t t0 = micros();
    if (g_lastUs) { const uint32_t lat = t0 - g_lastUs; if (lat > g_maxLatUs) g_maxLatUs = lat; }
    g_lastUs = t0;
    DriveProfile::In in;
    in.pedalPct = analogRead(Pins::PEDAL_IN) * (100.0f / 1023.0f);
    in.mph = g_mph; in.speedValid = g_speedValid; in.allowPct = g_allow; in.ok = g_ok;
    const float o = g_trim.step(in, g_tab.p[g_sel.mode], 1.0f);
    g_pedal = in.pedalPct; g_out = o;
    g_wiper = in.ok ? DriveProfile::Trim::wiper(o, in.pedalPct) : 0;
    if (!g_busLock) mcpWrite(g_wiper);
    g_ticks++;
    const uint32_t c = micros() - t0;
    if (c > g_maxCompUs) g_maxCompUs = c;
}
const char *res(DriveProfile::Selector::Result r) {
    switch (r) { case 0: return "ok"; case 1: return "PIN required to leave Kid"; case 2: return "wrong PIN";
                 case 3: return "set a Kid PIN first (MPIN 0 <pin>)"; default: return "unknown mode"; }
}
}  // namespace

void begin() {
#if THROTTLE_TRIM_ENABLE
    static_assert(!HV_ANALOG_ENABLE, "PEDAL_IN shares pin 24 with HV_SENSE");
    pinMode(Pins::THR_CS, OUTPUT); digitalWriteFast(Pins::THR_CS, HIGH);
    pinMode(Pins::LIGHT_DATA, OUTPUT); pinMode(Pins::LIGHT_CLK, OUTPUT);
    mcpWrite(0);   // first thing after any reset (watchdog included): output 0
    g_timer.begin(isr, DriveProfile::kPeriodUs);
#endif
}

void tick() {
    // Inputs for the ISR (loop rate). ok only when closed and no drive fault.
    bool ok = ContactorSeq::state() == ContactorSeq::State::Closed;
    float allow = 100;
#if defined(DUAL_STRING) && DUAL_STRING && (!defined(TEENSY_ROLE) || TEENSY_ROLE == 0)
    const BmsCore::Outputs &o = ContactorSeq::coreOut();
    if (o.faults & BmsCore::kDriveFaults) ok = false;
    allow = o.kellyPct;
#endif
    const KellyKls::Telemetry &t = KellyDecode::telemetry();
    const bool sv = t.haveRpmFrame && (millis() - t.lastRpmMs) < 200;
    // 24 in tire, 8.75 total ratio (SOURCED): mph = rpm x 0.008161
    g_mph = sv ? t.rpm * 0.008161f : 0; g_speedValid = sv;
    g_allow = allow; g_ok = ok && THROTTLE_TRIM_ENABLE;
}

bool handleLine(const char *l) {
    if (l[0] != 'M') return false;
    char k[8] = {}; float v = 0; char f[8] = {};
    if (!strncmp(l, "M?", 2)) { print(); return true; }
    if (!strncmp(l, "MPIN", 4)) {
        unsigned a = 0, b = 0;
        if (sscanf(l + 4, "%u %u", &a, &b) == 2 && (a == g_sel.pin) && b >= 1000 && b <= 9999) { g_sel.pin = b; SERIALCONSOLE.println(F("Kid PIN set")); }
        else SERIALCONSOLE.println(F("MPIN <old|0> <1000-9999>: refused"));
        return true;
    }
    if (!strncmp(l, "MP ", 3)) {
        if (sscanf(l + 3, "%7s %7s %f", k, f, &v) == 3 && g_tab.set(DriveProfile::Table::find(k), f, v)) SERIALCONSOLE.println(F("profile set (RAM, PROPOSED)"));
        else SERIALCONSOLE.println(F("MP <key> expo|cur|ramp|top|regen <v>: out of bounds"));
        return true;
    }
    unsigned pin = 0;
    if (sscanf(l + 1, "%7s %u", k, &pin) >= 1) {
        const auto r = g_sel.select(DriveProfile::Table::find(k), static_cast<uint16_t>(pin));
        SERIALCONSOLE.print(F("mode ")); SERIALCONSOLE.print(k); SERIALCONSOLE.print(F(": ")); SERIALCONSOLE.println(res(r));
        return true;
    }
    return false;
}

uint32_t maxLatencyUs() { return g_maxLatUs; }

void print() {
    const DriveProfile::Profile &p = g_tab.p[g_sel.mode];
    SERIALCONSOLE.print(F("TRIM enable=")); SERIALCONSOLE.print(THROTTLE_TRIM_ENABLE);
    SERIALCONSOLE.print(F(" mode=")); SERIALCONSOLE.print(p.name);
    SERIALCONSOLE.print(F(" pedal=")); SERIALCONSOLE.print(g_pedal, 1);
    SERIALCONSOLE.print(F("% out=")); SERIALCONSOLE.print(g_out, 1);
    SERIALCONSOLE.print(F("% wiper=")); SERIALCONSOLE.print(g_wiper);
    SERIALCONSOLE.print(F(" ok=")); SERIALCONSOLE.print(g_ok);
    SERIALCONSOLE.print(F(" maxPeriodUs=")); SERIALCONSOLE.print(g_maxLatUs);
    SERIALCONSOLE.print(F(" maxComputeUs=")); SERIALCONSOLE.print(g_maxCompUs);
    SERIALCONSOLE.println(F("  (PROPOSED profiles, CREW REVIEW)"));
}
}  // namespace ThrottleTrim
