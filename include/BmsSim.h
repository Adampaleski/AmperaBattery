#pragma once
// Plant model + scripted fault scenarios around BmsCore (pure C++).
// Used by scripts/sim_bms.cpp (PC) and tools/bms_sim (wasm in the browser).
//
// Bank modelled: Teensy A owns string 1 = M2 24S + M4 c1-12 (CAN3) and sees
// string 2 = M3 24S + M4 c1-12 over the link from Teensy B (36S each, 2P here;
// one board of each type per bus because the BICM IDs are fixed). The M4 top
// circuit (0x46E, c13-18) is empty by design: not modelled as cells, never planned.
#include <math.h>
#include "BmsCore.h"
#include "FactoryBalancePlan.h"

namespace BmsSim {

enum Board { B_M2 = 0, B_M4A, B_M3, B_M4B, kBoards };
constexpr int kBoardCells[kBoards] = {24, 12, 24, 12};   // cells USED (M4: c1-12 only)
constexpr int kStringCells = 36;
constexpr const char *kBoardLabel[kBoards] = {"M2 24S (A, string 1)", "M4 18S, 12S used (A, string 1)",
                                              "M3 24S (B, string 2)", "M4 18S, 12S used (B, string 2)"};
constexpr int kBoardString[kBoards] = {0, 0, 1, 1};

struct World {
    BmsCore::Core core;
    BmsCore::Inputs in;
    float    cell[kBoards][24];
    bool     silent[kBoards] = {};
    uint32_t lastSeen[kBoards] = {};
    uint32_t now = 0;
    // plant
    float    busV = 0;
    float    capF = 0.002f;       // Kelly bus capacitance guess (2000 uF) -> tau 0.1 s through 50 ohm
    float    preR = 50.0f;        // shop 50 ohm precharge resistor
    float    bleedTau = 1.0f;     // s, bus self-discharge when isolated (MEASURE on the bench)
    bool     preOpenCircuit = false;  // fault: precharge resistor/relay open -> timeout
    bool     welded = false;          // fault: main+ and main- stuck closed
    bool     linkUp = true;
    uint32_t linkLastMs = 0;
    float    packA = 0;
    bool     currentFitted = true;
    float    tMin = 20, tMax = 22;
    bool     tempsValid = true;   // sim assumes verified temps
    bool     keyOn = false, chargeReq = false;
    bool     chargerOnline = true;
    float    busForce = -1;       // interactive page: >= 0 forces the HV bus reading (volts)
    float    chargeSoakA = 0;     // internal: current from the charger into the pack

    void reset() {
        core.reset();
        in = BmsCore::Inputs{};
        now = 0; busV = 0; preOpenCircuit = false; welded = false; linkUp = true; linkLastMs = 0;
        packA = 0; tMin = 20; tMax = 22; tempsValid = true; keyOn = false; chargeReq = false; chargeSoakA = 0;
        currentFitted = true; busForce = -1;
        for (int b = 0; b < kBoards; b++) {
            silent[b] = false; lastSeen[b] = 0;
            for (int c = 0; c < 24; c++) cell[b][c] = 3.80f + 0.002f * static_cast<float>((c * 7 + b * 3) % 5);
        }
    }

    float stringV(int s) const {
        float v = 0;
        for (int b = 0; b < kBoards; b++) if (kBoardString[b] == s) for (int c = 0; c < kBoardCells[b]; c++) v += cell[b][c];
        return v;
    }

    void buildInputs() {
        in = BmsCore::Inputs{};
        in.nowMs = now;
        for (int s = 0; s < 2; s++) {
            BmsCore::StringIn &si = in.s[s];
            si.used = true; si.remote = (s == 1); si.expected = kStringCells;
            float mn = 9, mx = 0, sum = 0; int ok = 0; uint32_t oldest = 0;
            for (int b = 0; b < kBoards; b++) {
                if (kBoardString[b] != s) continue;
                const uint32_t age = now - lastSeen[b];
                if (age > oldest) oldest = age;
                if (age > 3000) continue;  // stale frames: cells not fresh
                for (int c = 0; c < kBoardCells[b]; c++) {
                    const float v = cell[b][c];
                    if (v < 0.5f || v > 4.3f) continue;
                    ok++; sum += v; if (v < mn) mn = v; if (v > mx) mx = v;
                }
            }
            si.cellsOk = static_cast<uint8_t>(ok); si.sumV = sum; si.minV = ok ? mn : 0; si.maxV = mx;
            si.oldestAgeMs = oldest;
            // out-of-range cell still reported for CELL-HIGH: use raw max/min as well
            for (int b = 0; b < kBoards; b++) if (kBoardString[b] == s && now - lastSeen[b] <= 3000)
                for (int c = 0; c < kBoardCells[b]; c++) {
                    if (cell[b][c] > si.maxV) si.maxV = cell[b][c];
                    if (cell[b][c] < si.minV) si.minV = cell[b][c];
                }
        }
        in.linkAgeMs = now - linkLastMs;
        in.linkFresh = in.linkAgeMs <= core.cfg.linkStaleMs;
        in.tempsValid = tempsValid; in.tMinC = tMin; in.tMaxC = tMax;
        in.currentValid = currentFitted; in.packA = packA - chargeSoakA;
        in.busValid = true; in.busV = busV;
        in.keyOn = keyOn; in.chargeReq = chargeReq;
    }

    // Advance dtMs: BICM frames, link heartbeat, HV bus RC, charger current.
    void step(uint32_t dtMs) {
        now += dtMs;
        for (int b = 0; b < kBoards; b++) if (!silent[b]) lastSeen[b] = now;
        if (linkUp) linkLastMs = now;
        const BmsCore::Outputs &o = core.out();
        const bool neg = o.mainNeg || welded, pos = o.mainPos || welded;
        const float pack = stringV(0) < stringV(1) ? stringV(0) : stringV(1);
        const float dt = dtMs * 0.001f;
        if (neg && pos) busV = pack;
        else if (neg && o.pre && !preOpenCircuit) busV += (pack - busV) * (1.0f - expf(-dt / (preR * capF)));
        else busV *= expf(-dt / bleedTau);
        if (busForce >= 0) busV = busForce;
        // charger pushes its requested current into the bank (sim only)
        chargeSoakA = (o.chargeEnable && chargerOnline && neg && pos) ? o.chargeA : 0;
        if (chargeSoakA > 0) {
            const float dv = 0.00002f * chargeSoakA * dt;  // crude: ~0.1 mV/s at 5 A
            for (int b = 0; b < kBoards; b++) for (int c = 0; c < kBoardCells[b]; c++) cell[b][c] += dv * 50;
        }
        buildInputs();
        core.step(in);
    }

    void run(uint32_t ms, uint32_t dtMs = 10) { for (uint32_t t = 0; t < ms; t += dtMs) step(dtMs); }

    bool allOpen() const { const auto &o = core.out(); return !o.pre && !o.mainPos && !o.mainNeg; }

    // Balance plan for Teensy A's string-1 bus (M2 circuits 4-6, M4 circuits
    // 10-11 = 0x46C/0x46D). Circuit 12 (0x46E) is empty by design: never used.
    FactoryBalance::PlanInput balanceInput(float deadbandV) const {
        FactoryBalance::PlanInput p;
        const int circ[5] = {4, 5, 6, 10, 11};
        for (int k = 0; k < 5; k++) {
            const int i = circ[k];
            const int b = k < 3 ? B_M2 : B_M4A;
            p.use[i] = 1; p.fresh[i] = (now - lastSeen[b]) <= 2000; p.group[i] = 0;
            for (int bit = 0; bit < FactoryBalance::kCircuit[i].cells; bit++)
                p.v[i][bit] = cell[b][FactoryBalance::kCircuit[i].firstCh - 1 + bit];
        }
        p.deadbandV = deadbandV; p.lowCutoffV = core.cfg.lowCutV; p.minBleedV = 3.90f;
        return p;
    }
};

// Bring the bank up: key on, wait until Closed (or timeout).
inline bool bringUp(World &w, bool charge, uint32_t maxMs = 6000) {
    w.keyOn = true; w.chargeReq = charge;
    for (uint32_t t = 0; t < maxMs; t += 10) {
        w.step(10);
        if (w.core.out().state == BmsCore::CState::Closed && (!charge || w.core.out().chargeEnable)) return true;
    }
    return false;
}

struct Result { const char *name; const char *expect; uint32_t limitMs; int32_t gotMs; bool pass; char note[96]; };

// Run fn(w) to inject a fault at t0, then wait until done(w) or limit.
template <typename Inject, typename Done>
Result scenario(const char *name, const char *expect, uint32_t limitMs, bool charge, Inject inject, Done done) {
    static World w;  // large: keep off the (wasm) stack
    w.reset();
    Result r{name, expect, limitMs, -1, false, ""};
    if (!bringUp(w, charge)) { snprintf(r.note, sizeof r.note, "setup failed: state=%s", BmsCore::stateName(w.core.out().state)); return r; }
    const uint32_t t0 = w.now;
    inject(w);
    for (uint32_t t = 0; t <= limitMs + 5000; t += 10) {
        w.step(10);
        if (done(w)) { r.gotMs = static_cast<int32_t>(w.now - t0); break; }
    }
    r.pass = r.gotMs >= 0 && static_cast<uint32_t>(r.gotMs) <= limitMs;
    snprintf(r.note, sizeof r.note, "state=%s faults=0x%03X chg=%d", BmsCore::stateName(w.core.out().state),
             static_cast<unsigned>(w.core.out().faults | w.core.out().latched), w.core.out().chargeEnable ? 1 : 0);
    return r;
}

constexpr int kScenarios = 13;

inline int runAll(Result out[kScenarios]) {
    using namespace BmsCore;
    int n = 0;
    auto openAndNoCharge = [](World &w) { return w.allOpen() && !w.core.out().chargeEnable; };
    out[n++] = scenario("healthy: key on -> closed + charging", "Closed & charger on <= 3.5 s", 3500, false,
        [](World &w) { w.keyOn = false; w.run(2500); w.keyOn = true; w.chargeReq = true; },
        [](World &w) { return w.core.out().state == CState::Closed && w.core.out().chargeEnable; });
    out[n++] = scenario("board silent (M4 12S, string 1)", "open + charger off <= 2.5 s", 2500, true,
        [](World &w) { w.silent[B_M4A] = true; }, openAndNoCharge);
    out[n++] = scenario("cell high 4.30 V (M2 ch7)", "open + charger off <= 0.2 s", 200, true,
        [](World &w) { w.cell[B_M2][6] = 4.30f; }, openAndNoCharge);
    out[n++] = scenario("cell low 3.20 V (M3 ch12, via link)", "open + charger off <= 0.2 s", 200, true,
        [](World &w) { w.cell[B_M3][11] = 3.20f; }, openAndNoCharge);
    out[n++] = scenario("overtemp 60 C", "open + charger off <= 0.2 s", 200, true,
        [](World &w) { w.tMax = 60; }, openAndNoCharge);
    out[n++] = scenario("undertemp while charging -5 C", "charger off <= 0.2 s, stay closed", 200, true,
        [](World &w) { w.tMin = -5; },
        [](World &w) { return !w.core.out().chargeEnable && w.core.out().state == CState::Closed; });
    out[n++] = scenario("overcurrent 350 A discharge", "open <= 0.7 s (500 ms debounce)", 700, true,
        [](World &w) { w.packA = 350; }, openAndNoCharge);
    out[n++] = scenario("Teensy link loss", "open + charger off <= 1.2 s", 1200, true,
        [](World &w) { w.linkUp = false; }, openAndNoCharge);
    out[n++] = scenario("charge reaches limit (80% = 3.95 V)", "charger off at ceiling <= 1 s", 1000, true,
        [](World &w) { for (int b = 0; b < kBoards; b++) w.cell[b][3] = 3.951f; },
        [](World &w) { return !w.core.out().chargeEnable && w.core.out().state == CState::Closed; });
    out[n++] = scenario("charge taper near limit (3.93 V)", "current < max, > 0 <= 0.2 s", 200, true,
        [](World &w) { w.cell[B_M2][0] = 3.93f; },
        [](World &w) { const auto &o = w.core.out(); return o.chargeEnable && o.chargeA < 4.9f && o.chargeA > 0.9f; });
    // Precharge timeout: resistor/relay open from the start; 3 tries + 2 x 30 s waits -> lockout.
    {
        static World w;
        w.reset();
        w.preOpenCircuit = true; w.keyOn = true;
        Result r{"precharge timeout (resistor open)", "lockout <= 75 s, main+ never closed", 75000, -1, false, ""};
        bool mainEver = false;
        for (uint32_t t = 0; t < 90000; t += 10) {
            w.step(10);
            if (w.core.out().mainPos) mainEver = true;
            if (w.core.out().state == CState::Lockout) { r.gotMs = static_cast<int32_t>(w.now); break; }
        }
        r.pass = r.gotMs >= 0 && r.gotMs <= 75000 && !mainEver && w.allOpen();
        snprintf(r.note, sizeof r.note, "state=%s main+ever=%d faults=0x%03X", stateName(w.core.out().state),
                 mainEver, static_cast<unsigned>(w.core.out().latched));
        out[n++] = r;
    }
    // Normal key-off must NOT look welded (bus bleeds down through the controller).
    {
        static World w;
        w.reset();
        Result r{"normal key-off (no false weld)", "open <= 0.1 s, no WELDED in next 6 s", 100, -1, false, ""};
        if (bringUp(w, true)) {
            const uint32_t t0 = w.now;
            w.keyOn = false; w.chargeReq = false;
            for (int t = 0; t < 100 && r.gotMs < 0; t += 10) { w.step(10); if (w.allOpen()) r.gotMs = static_cast<int32_t>(w.now - t0); }
            w.run(6000);
            r.pass = r.gotMs >= 0 && r.gotMs <= 100 && !(w.core.out().latched & F_WELDED) && w.core.out().state == CState::Open;
        }
        snprintf(r.note, sizeof r.note, "state=%s bus=%.1fV faults=0x%03X", stateName(w.core.out().state), w.busV,
                 static_cast<unsigned>(w.core.out().latched));
        out[n++] = r;
    }
    // Welded: both mains weld while closed, then key off -> bus stays live.
    out[n++] = scenario("welded contactor (bus live after open)", "WELDED + charger off <= 2.5 s", 2500, true,
        [](World &w) { w.welded = true; w.keyOn = false; w.chargeReq = false; },
        [](World &w) { return (w.core.out().latched & F_WELDED) && !w.core.out().chargeEnable && w.allOpen(); });
    return n;
}

inline int formatTable(char *buf, int cap, const Result *r, int n) {
    int o = snprintf(buf, cap, "%-40s %-38s %7s %7s  %s\n", "scenario", "expect", "got ms", "result", "end state");
    int fails = 0;
    for (int i = 0; i < n && o < cap; i++) {
        if (!r[i].pass) fails++;
        o += snprintf(buf + o, cap - o, "%-40s %-38s %7ld %7s  %s\n", r[i].name, r[i].expect,
                      static_cast<long>(r[i].gotMs), r[i].pass ? "PASS" : "FAIL", r[i].note);
    }
    if (o < cap) o += snprintf(buf + o, cap - o, "%d/%d passed\n", n - fails, n);
    return fails;
}

}  // namespace BmsSim
