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
#include "DriveStore.h"
#include "ThrottleCross.h"
#include "StringConfig.h"
#include <EEPROM.h>

#ifndef SERIALCONSOLE
#define SERIALCONSOLE Serial
#endif

namespace ThrottleTrim {
namespace {
DriveProfile::Table    g_tab;
DriveProfile::Selector g_sel;
DriveProfile::Trim     g_trim;
IntervalTimer          g_timer;
uint8_t eeR(int a) { return EEPROM.read(a); }
void    eeW(int a, uint8_t v) { EEPROM.update(a, v); }
const BmsCore::LogStore kStore{eeR, eeW};
DriveProfile::Store    g_store(kStore);
void persist() { g_store.save(g_tab, g_sel); }
volatile bool     g_ok = false, g_speedValid = false;
volatile float    g_allow = 0, g_mph = 0;
volatile float    g_pedal = 0, g_out = 0;
volatile uint8_t  g_wiper = 0;
volatile uint32_t g_lastUs = 0, g_maxLatUs = 0, g_maxCompUs = 0, g_ticks = 0;
volatile bool     g_busLock = false;   // future: 595 light shifts must set this (shared DATA/CLK)
// ---- two-Teensy cross-check (crew 2026-10-21) ----
#if ROLE_IS_A
constexpr int kPedalPin = Pins::PEDAL_IN_A;
#else
constexpr int kPedalPin = Pins::PEDAL_IN_B;
#endif
ThrottleCross::Checker g_chk;
volatile float    g_peerTrim = 0;
volatile uint32_t g_peerMs = 0, g_nowMs = 0, g_loopMs = 0;
volatile bool     g_peerSeen = false, g_peerCrcOk = false, g_aliveLvl = false;
volatile uint8_t  g_peerHb = 0, g_txHb = 0, g_prevLatched = 0;
uint8_t  g_syncIdx = 0;
uint32_t g_txMs = 0, g_syncMs = 0, g_dirtyMs = 0;
bool     g_dirty = false;
uint8_t crc8of(uint16_t c) { return static_cast<uint8_t>(c ^ (c >> 8)); }
uint8_t myCrc8() { return crc8of(ThrottleCross::profileCrc(g_tab, g_sel.mode)); }

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
    g_nowMs++;
    if (g_lastUs) { const uint32_t lat = t0 - g_lastUs; if (lat > g_maxLatUs) g_maxLatUs = lat; }
    g_lastUs = t0;
    DriveProfile::In in;
    in.pedalPct = analogRead(kPedalPin) * (100.0f / 1023.0f);
    in.mph = g_mph; in.speedValid = g_speedValid; in.allowPct = g_allow; in.ok = g_ok;
    const float o = g_trim.step(in, g_tab.p[g_sel.mode], 1.0f);
    g_pedal = in.pedalPct; g_out = o;
    // loop() must be alive too: a hung loop with a running ISR must still drop the enable
    const bool loopFresh = g_nowMs - g_loopMs <= 20;
    g_chk.step(g_nowMs, o, g_peerTrim, g_peerSeen, g_peerMs, g_peerCrcOk, loopFresh);
    const bool en = g_chk.enable() && loopFresh;
    g_wiper = (in.ok && en) ? DriveProfile::Trim::wiper(o, in.pedalPct) : 0;
#if ROLE_IS_A
    // A's alive signal = MCP4131 CS edges (74HC123 #1 B input). No write -> no edge -> clamp drops in ~5 ms.
    static bool zeroed = false;
    if (en) { if (!g_busLock) mcpWrite(g_wiper); zeroed = false; }
    else if (!zeroed && !g_busLock) { mcpWrite(0); zeroed = true; }   // one last write (wiper 0), then CS stays idle
#else
    // B's alive signal: toggle B_THR_ALIVE (74HC123 #2 B input) only while enabled.
    if (en) { g_aliveLvl = !g_aliveLvl; digitalWriteFast(Pins::B_THR_ALIVE, g_aliveLvl); }
#endif
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
    const bool ok = g_store.load(g_tab, g_sel);   // last mode (Kid stays Kid), profiles, PIN hash
    SERIALCONSOLE.print(F("Drive modes (EEPROM ")); SERIALCONSOLE.print(DriveProfile::Store::kBase);
    SERIALCONSOLE.print(F("): ")); SERIALCONSOLE.print(ok ? F("loaded, mode ") : F("blank/corrupt -> PROPOSED defaults, mode "));
    SERIALCONSOLE.println(g_tab.p[g_sel.mode].name);
#if THROTTLE_TRIM_ENABLE
    pinMode(Pins::THR_CS, OUTPUT); digitalWriteFast(Pins::THR_CS, HIGH);
    pinMode(Pins::LIGHT_DATA, OUTPUT); pinMode(Pins::LIGHT_CLK, OUTPUT);
#if ROLE_IS_A
    mcpWrite(0);   // first thing after any reset (watchdog included): output 0
#else
    pinMode(Pins::B_THR_ALIVE, OUTPUT); digitalWriteFast(Pins::B_THR_ALIVE, LOW);
#endif
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
#if ROLE_IS_A
    const KellyKls::Telemetry &t = KellyDecode::telemetry();
    const bool sv = t.haveRpmFrame && (millis() - t.lastRpmMs) < 200;
    // 24 in tire, 8.75 total ratio (SOURCED): mph = rpm x 0.008161
    g_mph = sv ? t.rpm * 0.008161f : 0; g_speedValid = sv;
    g_allow = allow; g_ok = ok && THROTTLE_TRIM_ENABLE;
#endif
    g_loopMs = g_nowMs;
    const uint8_t lt = g_chk.latched;
    if (lt != g_prevLatched) {   // logged like a fault (serial; FaultLog hook when the firmware gets one)
        SERIALCONSOLE.print(F("THROTTLE FAULT ")); SERIALCONSOLE.print(ThrottleCross::faultName(lt));
        SERIALCONSOLE.println(F(" -> enable off, Kelly throttle clamped to 0 V (key cycle / 'MX' to clear)"));
        g_prevLatched = lt;
    }
    if (g_dirty && millis() - g_dirtyMs > 1000) { g_dirty = false; persist(); }
}

// ---- link frames (called from Link) ----
// 0x6A9 A->B / 0x6A4 B->A every 20 ms: [0-1] trim x100 (0.01 %)  [2] mode  [3] profile CRC8
//   [4] mph x2  [5] bit0 ok, bit1 enable, bit2 speedValid  [6] allow %  [7] heartbeat
// 0x6AA A->B every 50 ms, round robin: [0] mode idx  [1] field 0-4  [2-5] float LE  [6] A mode  [7] hb
void linkTx(void (*send)(uint32_t, const uint8_t *)) {
    const uint32_t now = millis();
    if (now - g_txMs >= ThrottleCross::kFrameMs) {
        g_txMs = now;
        uint8_t b[8];
        const uint16_t t = static_cast<uint16_t>(g_out * 100.0f + 0.5f);
        b[0] = t & 0xFF; b[1] = t >> 8; b[2] = g_sel.mode; b[3] = myCrc8();
        const float mph = g_mph * 2.0f; b[4] = static_cast<uint8_t>(mph > 255 ? 255 : mph);
        b[5] = (g_ok ? 1 : 0) | (g_chk.enable() ? 2 : 0) | (g_speedValid ? 4 : 0);
        b[6] = static_cast<uint8_t>(g_allow < 0 ? 0 : (g_allow > 100 ? 100 : g_allow));
        b[7] = ++g_txHb;
        send(ROLE_IS_A ? kLinkIdAThrottle : kLinkIdBThrottle, b);
    }
#if ROLE_IS_A
    if (now - g_syncMs >= 50) {
        g_syncMs = now;
        const int m = g_syncIdx / 5, f = g_syncIdx % 5;
        const DriveProfile::Profile &p = g_tab.p[m];
        const float v[5] = {p.expo, p.currentPct, p.rampAps, p.topMph, p.regenPct};
        uint8_t b[8]; b[0] = m; b[1] = f; memcpy(b + 2, &v[f], 4); b[6] = g_sel.mode; b[7] = g_txHb;
        send(kLinkIdAProfile, b);
        g_syncIdx = (g_syncIdx + 1) % (5 * DriveProfile::kModes);
    }
#endif
}

void linkRx(uint32_t id, const uint8_t *b) {
    if (id == (ROLE_IS_A ? kLinkIdBThrottle : kLinkIdAThrottle)) {
        if (!g_peerSeen || b[7] != g_peerHb) g_peerMs = g_nowMs;   // frozen heartbeat does not refresh
        g_peerSeen = true; g_peerHb = b[7];
        g_peerTrim = (b[0] | (b[1] << 8)) / 100.0f;
#if !ROLE_IS_A
        // B follows A: mode, BMS ok/allowance and speed come from A; pedal + map are B's own.
        if (b[2] < DriveProfile::kModes && b[2] != g_sel.mode) { g_sel.mode = static_cast<DriveProfile::Mode>(b[2]); g_dirty = true; g_dirtyMs = millis(); }
        g_ok = (b[5] & 1) && THROTTLE_TRIM_ENABLE; g_speedValid = (b[5] & 4) != 0; g_mph = b[4] / 2.0f; g_allow = b[6];
#endif
        g_peerCrcOk = b[3] == myCrc8();
    }
#if !ROLE_IS_A
    else if (id == kLinkIdAProfile && b[0] < DriveProfile::kModes && b[1] < 5) {
        static const char *kF[5] = {"expo", "cur", "ramp", "top", "regen"};
        float v; memcpy(&v, b + 2, 4);
        const DriveProfile::Profile before = g_tab.p[b[0]];
        if (g_tab.set(b[0], kF[b[1]], v) && memcmp(&before, &g_tab.p[b[0]], sizeof before)) { g_dirty = true; g_dirtyMs = millis(); }
    }
#endif
}

bool handleLine(const char *l) {
    if (l[0] != 'M') return false;
    char k[8] = {}; float v = 0; char f[8] = {};
    if (!strncmp(l, "M?", 2)) { print(); return true; }
    if (!strncmp(l, "MX", 2)) { g_chk.clear(); SERIALCONSOLE.println(F("throttle cross-check fault cleared")); return true; }
#if !ROLE_IS_A
    SERIALCONSOLE.println(F("role B: drive mode / profile / PIN are set on Teensy A and synced over the link"));
    return true;
#endif
    if (!strncmp(l, "MPIN", 4)) {
        unsigned a = 0, b = 0;
        if (sscanf(l + 4, "%u %u", &a, &b) == 2 && (g_sel.hasPin() ? g_sel.pinOk(static_cast<uint16_t>(a)) : a == 0) && b >= 1000 && b <= 9999 && g_sel.mode != DriveProfile::M_KID) { g_sel.setPin(static_cast<uint16_t>(b)); persist(); SERIALCONSOLE.println(F("Kid PIN set (stored hashed)")); }
        else SERIALCONSOLE.println(F("MPIN <old|0> <1000-9999>: refused"));
        return true;
    }
    if (!strncmp(l, "MP ", 3)) {
        if (sscanf(l + 3, "%7s %7s %f", k, f, &v) == 3 && g_tab.set(DriveProfile::Table::find(k), f, v)) { persist(); SERIALCONSOLE.println(F("profile set (EEPROM, PROPOSED)")); }
        else SERIALCONSOLE.println(F("MP <key> expo|cur|ramp|top|regen <v>: out of bounds"));
        return true;
    }
    unsigned pin = 0;
    if (sscanf(l + 1, "%7s %u", k, &pin) >= 1) {
        const auto r = g_sel.select(DriveProfile::Table::find(k), static_cast<uint16_t>(pin));
        if (r == DriveProfile::Selector::OK) persist();
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
    SERIALCONSOLE.print(F(" kidPin=")); SERIALCONSOLE.print(g_sel.hasPin() ? F("set") : F("none"));   // never the PIN
    SERIALCONSOLE.print(F(" pedal=")); SERIALCONSOLE.print(g_pedal, 1);
    SERIALCONSOLE.print(F("% out=")); SERIALCONSOLE.print(g_out, 1);
    SERIALCONSOLE.print(F("% wiper=")); SERIALCONSOLE.print(g_wiper);
    SERIALCONSOLE.print(F(" ok=")); SERIALCONSOLE.print(g_ok);
    SERIALCONSOLE.print(F(" enable=")); SERIALCONSOLE.print(g_chk.enable());
    SERIALCONSOLE.print(F(" xfault=")); SERIALCONSOLE.print(ThrottleCross::faultName(g_chk.latched));
    SERIALCONSOLE.print(F(" peerTrim=")); SERIALCONSOLE.print(g_peerTrim, 1);
    SERIALCONSOLE.print(F(" maxPeriodUs=")); SERIALCONSOLE.print(g_maxLatUs);
    SERIALCONSOLE.print(F(" maxComputeUs=")); SERIALCONSOLE.print(g_maxCompUs);
    SERIALCONSOLE.println(F("  (PROPOSED profiles, CREW REVIEW)"));
}
}  // namespace ThrottleTrim
