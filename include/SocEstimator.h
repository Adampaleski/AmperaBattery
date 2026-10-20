#pragma once
// State of charge by coulomb counting the split Volt LEM pair (pure C++,
// header-only; host sim, wasm page and Teensy share it). NOT wired into the
// firmware yet: CURRENT_SENSE_ENABLE stays 0, nothing here drives an output.
//
// Input current is the BANK total, + = discharge: I = LEM A (S1+S2) + LEM B (S3+S4).
//
// What is known vs not:
//   - Capacity: capAh = 0 means UNKNOWN (percent stays -1). The real bank Ah
//     (4 parallel 36S strings of Volt/Ampera modules) is UNVERIFIED; measure it
//     with one full -> low-cut discharge and store the counted Ah (learnCapacity).
//   - No open-circuit-voltage table is used: none verified for these cells. So
//     the count starts UNKNOWN at boot and only becomes known at an anchor:
//       full anchor  = charger at its limit (chargeWhy "at charge limit") with
//                      the max cell at the ceiling -> used Ah = 0. If the P
//                      setting is < 100 % that point is NOT true 100 %: it is
//                      tagged anchorPct = P (linear-map assumption, UNVERIFIED).
//       restore      = value saved at power-off (fault/state log, backlog item 5).
//   - Lost count: current invalid for > lostMs while counting -> UNKNOWN again.
//   - Drift check: min cell at low cut + driftBandV while percent still says
//     > driftPct -> flag (capacity or anchor is wrong). No auto-correct.
#include <stdint.h>

namespace BmsCore {

struct SocConfig {
    float    capAh      = 0;       // bank usable Ah; 0 = UNKNOWN (UNVERIFIED, measure)
    uint32_t lostMs     = 2000;    // invalid current this long -> count lost
    float    driftBandV = 0.02f;   // "at low cut" = min cell <= lowCutV + this
    float    driftPct   = 10.0f;   // ... while SoC still above this -> drift flag
};

struct SocIn {
    uint32_t nowMs = 0;
    bool  currentValid = false;
    float packA = 0;               // + = discharge (sum of both LEMs)
    bool  atChargeLimit = false;   // Core chargeWhy == "at charge limit"
    int   chargePct = 80;          // P setting the ceiling was set from
    float minCellV = 0;            // lowest fresh cell, any string
    float lowCutV = 3.30f;         // Config::lowCutV
};

struct SocOut {
    bool  known = false;           // count has a valid anchor
    float usedAh = 0;              // Ah out since the anchor (+ = removed)
    float pct = -1;                // -1 = unknown (no anchor or no capacity)
    bool  drift = false;           // low cut reached with SoC still high
    const char *why = "no anchor yet";
};

class Soc {
public:
    SocConfig cfg;
    void reset() { *this = Soc{}; }
    void reset(const SocConfig &c) { *this = Soc{}; cfg = c; }

    // Restore a saved count (from the power-off log). anchorPct = where the
    // saved count was anchored (100 for a P100 full charge).
    void restore(float usedAh, float anchorPct) {
        known_ = true; usedAh_ = usedAh; anchorPct_ = anchorPct; why_ = "restored";
    }
    // After a measured full -> low-cut run: counted Ah becomes the capacity.
    bool learnCapacity() {
        if (!known_ || anchorPct_ < 99.5f || usedAh_ <= 0) return false;
        cfg.capAh = usedAh_; return true;
    }
    float anchorPct() const { return anchorPct_; }

    const SocOut &step(const SocIn &in) {
        const uint32_t dt = started_ ? in.nowMs - lastMs_ : 0;
        started_ = true; lastMs_ = in.nowMs;
        if (in.atChargeLimit) {
            known_ = true; usedAh_ = 0; invalidMs_ = 0;
            anchorPct_ = static_cast<float>(in.chargePct > 100 ? 100 : in.chargePct);
            why_ = anchorPct_ >= 99.5f ? "full anchor" : "anchor at P limit (not true 100 %)";
        } else if (known_) {
            if (in.currentValid) {
                invalidMs_ = 0;
                usedAh_ += in.packA * static_cast<float>(dt) / 3600000.0f;
            } else {
                invalidMs_ += dt;
                if (invalidMs_ > cfg.lostMs) { known_ = false; why_ = "current lost: count unknown"; }
            }
        }
        out_ = SocOut{};
        out_.known = known_;
        out_.usedAh = known_ ? usedAh_ : 0;
        out_.why = why_;
        if (known_ && cfg.capAh > 0) {
            float p = anchorPct_ - 100.0f * usedAh_ / cfg.capAh;
            if (p < 0) p = 0;
            if (p > 100) p = 100;
            out_.pct = p;
            out_.drift = in.minCellV > 0 && in.minCellV <= in.lowCutV + cfg.driftBandV && p > cfg.driftPct;
            if (out_.drift) out_.why = "drift: low cut but SoC high";
        } else if (known_) {
            out_.why = "no capacity set (UNVERIFIED)";
        }
        return out_;
    }
    const SocOut &out() const { return out_; }

private:
    bool known_ = false, started_ = false;
    float usedAh_ = 0, anchorPct_ = 100;
    uint32_t lastMs_ = 0, invalidMs_ = 0;
    const char *why_ = "no anchor yet";
    SocOut out_;
};

}  // namespace BmsCore
