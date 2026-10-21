#pragma once
// Park detection (Adam 2026-10-21). Pure C++: firmware, sim parity, scripts/test_park.cpp. CREW REVIEW.
// Hardware: parking-brake switch with TWO contacts (docs/PARK_BRAKE.md):
//   NO contact 1 -> Teensy A pin 18 (IN3), NO contact 2 -> Teensy B pin 18 (IN3), each 10k pull-up to 3.3 V
//   (PROPOSED). Brake SET closes the contact to GND -> LOW = set. Open/broken wire = HIGH = NOT parked.
//   NC contact 3 (separate pole) in the 12 V feed to Kelly Foot_SW (pin 15): brake set -> Kelly cannot drive,
//   whatever the software does (hardware interlock, non-negotiable).
// Software Parked = both contacts set + Kelly RPM == 0 (fresh CAN) + Kelly direction switches neutral.
// All timings PROPOSED.
#include <stdint.h>

namespace ParkState {

constexpr uint32_t kDisagreeMs  = 100;   // PROPOSED: contacts may differ this long (switch bounce)
constexpr uint32_t kMovingMs    = 300;   // PROPOSED: brake set while RPM > kMoveRpm this long -> fault
constexpr uint16_t kMoveRpm     = 50;    // PROPOSED (~0.4 mph at 122.5 RPM/mph, SOURCED gearing)
constexpr uint32_t kCanStaleMs  = 200;   // Kelly frames every 50 ms (SOURCED, Kelly broadcast CAN doc)
#ifndef PRECHARGE_NEEDS_PARK
#define PRECHARGE_NEEDS_PARK 1           // PROPOSED, adjustable: precharge only with the parking brake set
#endif

enum Fault : uint8_t { PF_NONE = 0, PF_DISAGREE = 1, PF_BRAKE_MOVING = 2 };
inline const char *faultName(uint8_t f) {
    return f & PF_BRAKE_MOVING ? "PARK-BRAKE-WHILE-MOVING" : f & PF_DISAGREE ? "PARK-CONTACT-DISAGREE" : "";
}

struct In {
    bool     contactA = false;   // true = contact reads SET (pin LOW)
    bool     contactB = false;   // from B over the link (or B's own pin on B)
    bool     rpmValid = false;   // Kelly 0x0CF11E05 fresh within kCanStaleMs
    uint16_t rpm      = 0;
    bool     neutral  = false;   // Kelly 0x0CF11F05 byte 6: FWD bit5 and REV bit4 both clear
};

struct Detector {
    uint8_t  fault = PF_NONE, latched = PF_NONE;   // live + latched (logged) faults
    bool     parked = false, brakeSet = false;
    uint32_t disSince = 0, movSince = 0; bool dis = false, mov = false;

    void step(uint32_t now, const In &in) {
        const bool both = in.contactA && in.contactB, any = in.contactA || in.contactB;
        brakeSet = both;
        if (in.contactA != in.contactB) { if (!dis) { dis = true; disSince = now; } } else dis = false;
        if (any && in.rpmValid && in.rpm > kMoveRpm) { if (!mov) { mov = true; movSince = now; } } else mov = false;
        fault = PF_NONE;
        if (dis && now - disSince >= kDisagreeMs) fault |= PF_DISAGREE;
        if (mov && now - movSince >= kMovingMs)   fault |= PF_BRAKE_MOVING;
        latched |= fault;
        parked = both && in.rpmValid && in.rpm == 0 && in.neutral && !(fault & PF_DISAGREE);
    }
    void clear() { latched = fault; }
    // Kid mode: entering, leaving or editing Kid only while Parked.
    bool kidChangeAllowed() const { return parked; }
    // Precharge gate (PROPOSED): brake set on both contacts, no contact disagreement.
    bool prechargeAllowed() const { return !PRECHARGE_NEEDS_PARK || (brakeSet && !(fault & PF_DISAGREE)); }
};

}  // namespace ParkState
