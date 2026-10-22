#pragma once
// Kelly CAN link watchdog (backlog: REQUIRED BEFORE FIRST DRIVE "Kelly CAN timeout -> derate + fault").
// Pure C++, header-only: firmware, sim parity, scripts/test_kelly_link.cpp. NOT WIRED (BMS_CAP_KELLY_RX=0).
// The Kelly broadcasts 0x0CF11E05 and 0x0CF11F05 every 50 ms (SOURCED, Kelly broadcast CAN doc,
// UNVERIFIED on Adam's unit). Kelly CAN is telemetry, not pack protection: losing it means the Teensys
// no longer know RPM, direction, Kelly temps or Kelly errors, so they cap power and refuse Park.
// It never opens or closes a contactor and never commands the Kelly (the Kelly accepts no CAN commands).
// All timings and the cap are PROPOSED.
#include <stdint.h>

namespace KellyLink {

constexpr uint32_t kStaleMs   = 200;   // PROPOSED: either frame older than this = stale (4 missed frames)
constexpr uint32_t kBootGrace = 2000;  // PROPOSED: Kelly boot time after KSI before a missing frame counts
constexpr uint32_t kRecoverMs = 1000;  // PROPOSED: both frames fresh this long before the cap lifts
constexpr uint8_t  kCapPct    = 0;     // PROPOSED: 0 = request the Kid/Low hardware cap (OUT3 ECO), not a % derate

enum Code : uint8_t { OK = 0, NEVER_SEEN = 1, LOST = 2 };
inline const char *name(uint8_t c) {
    return c == LOST ? "KELLY-CAN-LOST" : c == NEVER_SEEN ? "KELLY-CAN-NEVER-SEEN" : "";
}

struct Watchdog {
    uint32_t ksiOnMs = 0;            // when the Kelly KSI was powered (key on)
    bool     ksiOn = false;
    bool     seenRpm = false, seenThr = false;
    uint32_t lastRpm = 0, lastThr = 0;
    uint8_t  code = OK;              // live
    uint8_t  latched = OK;           // first fault this key cycle (for the fault log); cleared by keyOff()
    bool     capActive = false;      // request Low cap while true
    uint32_t freshSince = 0; bool fresh = false;
    uint16_t lossCount = 0;          // LOST edges this key cycle

    void keyOn(uint32_t now) { *this = Watchdog(); ksiOn = true; ksiOnMs = now; }
    void keyOff() { *this = Watchdog(); }
    void rpmFrame(uint32_t now) { seenRpm = true; lastRpm = now; }
    void thrFrame(uint32_t now) { seenThr = true; lastThr = now; }

    bool rpmFresh(uint32_t now) const { return seenRpm && now - lastRpm <= kStaleMs; }
    bool thrFresh(uint32_t now) const { return seenThr && now - lastThr <= kStaleMs; }

    uint8_t step(uint32_t now) {
        if (!ksiOn) { code = OK; capActive = false; return code; }
        const bool both = rpmFresh(now) && thrFresh(now);
        uint8_t c = OK;
        if (!both) {
            if (!seenRpm || !seenThr) c = (now - ksiOnMs >= kBootGrace) ? NEVER_SEEN : OK;
            else c = LOST;
            // a frame that showed up once and then went stale is LOST even inside the grace window
            if ((seenRpm && !rpmFresh(now)) || (seenThr && !thrFresh(now))) c = LOST;
        }
        if (c == LOST && code != LOST) lossCount++;
        code = c;
        if (code != OK) { capActive = true; fresh = false; if (latched == OK) latched = code; }
        else if (capActive) {
            if (!fresh) { fresh = true; freshSince = now; }
            if (now - freshSince >= kRecoverMs) { capActive = false; fresh = false; }
        }
        return code;
    }
    bool parkTrusted(uint32_t now) const { return rpmFresh(now) && thrFresh(now); }  // feed ParkState rpmValid
};

}  // namespace KellyLink
