#pragma once
// Pure-logic BMS core (no Arduino, no I/O). One step() per loop with a
// snapshot of inputs -> contactor/precharge commands, charger request, Kelly
// limit requests, fault bits and an event log. The SAME code runs:
//   * on the Teensy (ContactorSeq / ChargeTx call it; outputs stay gated by
//     BMS_CAP_* and TELEMETRY_ONLY in those files, never here)
//   * on a PC (scripts/sim_bms.cpp, scripted fault table)
//   * in the browser (tools/bms_sim.html, compiled to wasm32 by zig)
//
// Sources adapted (see docs/BMS_CORE.md for full citations):
//   Tom de Bree VoltBMSV2.ino @ f029e07 (MIT):
//     1192-1218 Prechargecon(): close NEG (OUT4), precharge (OUT2), then POS (OUT1), drop precharge
//     2591-2672 currentlimit(): hard zeros on OV/UV/OT/UT, linear taper map(highCell, Vset-hys, Vset, end, max)
//     2870-2886 chargercomms(): Elcon 0x1806E5F4, V*10 and A*10 big-endian
//   Thresholds: crew decisions in docs/DUAL_STRING.md / Config.h.

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace BmsCore {

// ---------------------------------------------------------------- faults
enum : uint32_t {
    F_SILENT      = 1u << 0,   // expected circuit silent > stringSilentMs
    F_COUNT       = 1u << 1,   // cell count / bad sense reading
    F_CELL_HIGH   = 1u << 2,   // a cell > cellMaxV (4.25)
    F_CELL_LOW    = 1u << 3,   // a cell < lowCutV (L setting)
    F_OVERTEMP    = 1u << 4,   // pack temp > overTempC
    F_UNDERTEMP_C = 1u << 5,   // charge only: pack temp < chargeMinTempC
    F_OVERCURRENT = 1u << 6,   // |I| > overcurrentA for > overcurrentMs
    F_LINK        = 1u << 7,   // B -> A link stale
    F_PRECHARGE   = 1u << 8,   // precharge failed after retries (lockout)
    F_WELDED      = 1u << 9,   // bus stays live with every contactor commanded open
    F_CHARGE_OV   = 1u << 10,  // charge only: cell >= ceiling + 0.05 (latched to reboot)
    F_OVERTEMP_C  = 1u << 11,  // charge only: pack temp > chargeMaxTempC
    F_TEMP_INVALID= 1u << 12,  // charge only: no verified temperatures
};
// Faults that open the contactors.
constexpr uint32_t kDriveFaults = F_SILENT | F_COUNT | F_CELL_HIGH | F_CELL_LOW | F_OVERTEMP |
                                  F_OVERCURRENT | F_LINK | F_PRECHARGE | F_WELDED;
// Faults that stop the charger (superset).
constexpr uint32_t kChargeFaults = kDriveFaults | F_UNDERTEMP_C | F_CHARGE_OV | F_OVERTEMP_C | F_TEMP_INVALID;

inline const char *faultName(uint32_t bit) {
    switch (bit) {
        case F_SILENT: return "BOARD-SILENT";      case F_COUNT: return "CELL-COUNT/BAD";
        case F_CELL_HIGH: return "CELL-HIGH";      case F_CELL_LOW: return "CELL-LOW";
        case F_OVERTEMP: return "OVERTEMP";        case F_UNDERTEMP_C: return "UNDERTEMP-CHARGE";
        case F_OVERCURRENT: return "OVERCURRENT";  case F_LINK: return "LINK-LOST";
        case F_PRECHARGE: return "PRECHARGE-FAIL"; case F_WELDED: return "WELDED-CONTACTOR";
        case F_CHARGE_OV: return "CHARGE-OV";      case F_OVERTEMP_C: return "OVERTEMP-CHARGE";
        case F_TEMP_INVALID: return "TEMPS-NOT-VERIFIED";
        default: return "?";
    }
}

// --------------------------------------------------------------- config
// Charge limit percent -> per-cell ceiling. Linear, 100 % = 4.10 V,
// 80 % = 3.95 V (Sparky), i.e. 7.5 mV per percent. Clamped 50..100 %.
constexpr float kCeil100V = 4.10f;
constexpr float kMvPerPct = 7.5f;
inline float ceilingForPct(int pct) {
    if (pct < 50) pct = 50;
    if (pct > 100) pct = 100;
    return kCeil100V - kMvPerPct * 0.001f * static_cast<float>(100 - pct);
}

struct Config {
    // cells
    float    lowCutV        = 3.30f;   // L setting: open contactors below
    float    cellMaxV       = 4.25f;   // kCellFaultMaxV: open contactors above
    int      chargePct      = 80;      // P setting (EEPROM), 80 % = 3.95 V
    float    chargeHysV     = 0.05f;   // restart charge only after max cell falls this far
    float    taperBandV     = 0.05f;   // current tapers over the last 50 mV (Tom ChargeHys/map)
    float    chargeMaxA     = 5.0f;    // kChargeCurrentDeciA (outlet-limited)
    float    chargeEndA     = 1.0f;    // current at the ceiling before stop (Tom chargecurrentend 5 A on a big charger)
    int      stringCells    = 42;      // charger V basis: cells in ONE series string (24S+18S)
    float    busMaxV        = 168.0f;  // Kelly KLS "168 V" (VERIFY label): ceiling clamped to busMaxV/stringCells
    // temps
    float    overTempC      = 55.0f;   // drive: open contactors above
    float    chargeMaxTempC = 45.0f;
    float    chargeMinTempC = 0.0f;
    float    derateTempC    = 50.0f;   // Kelly derate starts
    // current
    float    overcurrentA   = 300.0f;  // |I| above this ...
    uint32_t overcurrentMs  = 500;     // ... for this long -> open
    float    maxDischargeA  = 250.0f;  // for the Kelly derate request
    // timing (Config.h / StringConfig.h)
    uint32_t stringSilentMs = 2000;
    uint32_t linkStaleMs    = 1000;
    uint32_t pcMinMs        = 300;
    uint32_t pcTimeoutMs    = 3000;
    uint32_t pcRetryWaitMs  = 30000;
    uint8_t  pcMaxRetries   = 2;
    float    pcRatio        = 0.95f;
    uint32_t overlapMs      = 100;     // precharge stays on after main+ closes
    // welded detection
    uint32_t weldCheckMs    = 2000;    // after opening, bus must fall below ...
    float    weldBusFrac    = 0.50f;   // ... this fraction of the lowest string
    float    weldMinV       = 30.0f;   // ... and below this absolute floor never counts
    // Kelly
    float    derateLowV     = 0.15f;   // start derating 150 mV above lowCutV
};

// --------------------------------------------------------------- inputs
constexpr int kMaxStrings = 4;   // A: 2 local + 2 remote (B over the link)

struct StringIn {
    bool     used      = false;    // part of the bank
    bool     remote    = false;    // came over the link (role A view of B)
    uint8_t  expected  = 0;
    uint8_t  cellsOk   = 0;        // fresh, in-range cells
    float    sumV = 0, minV = 0, maxV = 0;
    uint32_t oldestAgeMs = 0;      // age of the stalest expected circuit
};

struct Inputs {
    uint32_t nowMs        = 0;
    StringIn s[kMaxStrings];
    bool     linkFresh    = true;  // remote heartbeat newer than linkStaleMs
    uint32_t linkAgeMs    = 0;
    bool     tempsValid   = false; // TEMPS_VERIFIED && fresh
    float    tMinC = 20, tMaxC = 20;
    bool     currentValid = false;
    float    packA        = 0;     // + = discharge
    bool     busValid     = false; // HV bus sense present
    float    busV         = 0;
    bool     keyOn        = false; // dead-man / key / drive request
    bool     chargeReq    = false;
};

// -------------------------------------------------------------- outputs
enum class CState : uint8_t { Open = 0, Precharge, MainClose, Closed, Fault, RetryWait, Lockout };
inline const char *stateName(CState s) {
    switch (s) {
        case CState::Open: return "Open";           case CState::Precharge: return "Precharge";
        case CState::MainClose: return "MainClose"; case CState::Closed: return "Closed";
        case CState::Fault: return "Fault";         case CState::RetryWait: return "RetryWait";
        case CState::Lockout: return "Lockout";
    }
    return "?";
}

struct Outputs {
    CState   state = CState::Open;
    bool     pre = false, mainPos = false, mainNeg = false;  // WANTED coil states
    bool     chargeEnable = false;
    float    chargeV = 0, chargeA = 0;     // Elcon request (0 A + stop byte when disabled)
    float    ceilingV = 0;
    float    kellyPct = 0;                 // throttle/discharge allowance 0..100
    bool     kellyLimp = false;            // assert low-speed input
    bool     kellyRegenOk = false;
    uint32_t faults  = 0;                  // active now
    uint32_t latched = 0;                  // latched (drive faults until key-off + clear)
    const char *chargeWhy = "";
    float    lowestStringV = 0;
};

// ------------------------------------------------------------ event log
struct Event { uint32_t ms; char text[72]; };
constexpr int kLogSize = 64;

class Core {
public:
    Config cfg;

    void reset() {
        st_ = CState::Open; stMs_ = 0; retries_ = 0; latched_ = 0; chargeOvLatched_ = false;
        welded_ = false; ocSinceMs_ = 0; ocActive_ = false; full_ = false; openSinceMs_ = 0;
        logHead_ = 0; logCount_ = 0; prevFaults_ = 0; out_ = Outputs{}; started_ = false;
    }

    const Outputs &out() const { return out_; }
    int logCount() const { return logCount_; }
    const Event &logAt(int i) const {  // 0 = oldest kept
        const int start = (logHead_ - logCount_ + kLogSize) % kLogSize;
        return log_[(start + i) % kLogSize];
    }

    const Outputs &step(const Inputs &in) {
        const uint32_t now = in.nowMs;
        if (!started_) { started_ = true; stMs_ = now; openSinceMs_ = now; }
        Outputs o;
        o.ceilingV = ceilingForPct(cfg.chargePct);
        if (cfg.stringCells > 0 && o.ceilingV * cfg.stringCells > cfg.busMaxV)
            o.ceilingV = cfg.busMaxV / static_cast<float>(cfg.stringCells);  // 42S: 4.00 V max
        o.lowestStringV = lowestString(in);
        uint32_t f = evalFaults(in, o.lowestStringV, o.ceilingV);

        // ---- welded check (before the state machine): every coil commanded
        // open for weldCheckMs, yet the HV bus is still live.
        const bool allOpen = (st_ == CState::Open || st_ == CState::Fault || st_ == CState::Lockout ||
                              st_ == CState::RetryWait);
        if (!allOpen) openSinceMs_ = now;
        if (allOpen && !welded_ && in.busValid && o.lowestStringV > 0 &&
            now - openSinceMs_ >= cfg.weldCheckMs && in.busV > busLiveV(o.lowestStringV)) {
            welded_ = true;
            logf(now, "WELDED: bus %.0f V, %u ms after open", in.busV, (unsigned)(now - openSinceMs_));
            if (st_ == CState::Open || st_ == CState::RetryWait) enter(CState::Fault, now, "welded contactor");
        }
        if (welded_) { f |= F_WELDED; latched_ |= F_WELDED; }

        // ---- contactor state machine
        const bool driveFault = (f | latched_) & kDriveFaults;
        if (driveFault && st_ != CState::Open) latched_ |= f & kDriveFaults;
        if (driveFault && st_ != CState::Fault && st_ != CState::Lockout && st_ != CState::Open) {
            enter(CState::Fault, now, "open: fault");
        }
        if (!in.keyOn && st_ != CState::Open && st_ != CState::Lockout && st_ != CState::Fault) {
            enter(CState::Open, now, "key off");
        }
        const uint32_t el = now - stMs_;
        switch (st_) {
            case CState::Open:
                // Wait weldCheckMs after any open so a stuck contactor is seen before re-closing.
                if (in.keyOn && !driveFault && now - openSinceMs_ >= cfg.weldCheckMs) {
                    retries_ = 0;
                    enter(CState::Precharge, now, "key on");
                }
                break;
            case CState::Precharge: {
                const bool gate = in.busValid && o.lowestStringV > 0 && in.busV >= o.lowestStringV * cfg.pcRatio;
                if (el >= cfg.pcMinMs && gate) { enter(CState::MainClose, now, "bus >= 95% lowest string"); break; }
                if (el >= cfg.pcTimeoutMs) {
                    if (retries_ < cfg.pcMaxRetries) { retries_++; enter(CState::RetryWait, now, "precharge timeout"); }
                    else { latched_ |= F_PRECHARGE; enter(CState::Lockout, now, "precharge failed 3x: lockout"); }
                }
                break;
            }
            case CState::RetryWait:
                if (el >= cfg.pcRetryWaitMs) enter(CState::Precharge, now, "precharge retry");
                break;
            case CState::MainClose:
                if (el >= cfg.overlapMs) enter(CState::Closed, now, "closed, precharge off");
                break;
            case CState::Closed:
                break;
            case CState::Fault:
            case CState::Lockout:
                // Clear only with key OFF and every drive fault gone. Weld: reboot only.
                if (!in.keyOn && !(f & kDriveFaults) && !welded_) {
                    latched_ = 0;
                    enter(CState::Open, now, "cleared (key off, faults gone)");
                }
                break;
        }

        // ---- coil wants
        switch (st_) {
            case CState::Precharge: o.mainNeg = true; o.pre = true; break;          // Tom 1195-1198
            case CState::MainClose: o.mainNeg = true; o.pre = true; o.mainPos = true; break;
            case CState::Closed:    o.mainNeg = true; o.mainPos = true; break;      // Tom 1201-1213
            default: break;
        }
        o.state = st_;

        // ---- charger (Elcon) request
        if (chargeOvLatched_) f |= F_CHARGE_OV;
        chargeLogic(in, o, f);

        // ---- Kelly limits
        kellyLogic(in, o, f);

        o.faults = f;
        o.latched = latched_ | (welded_ ? F_WELDED : 0) | (chargeOvLatched_ ? F_CHARGE_OV : 0);
        logFaultEdges(now, f);
        out_ = o;
        return out_;
    }

    void logf(uint32_t ms, const char *fmt, ...) __attribute__((format(printf, 3, 4))) {
        Event &e = log_[logHead_];
        e.ms = ms;
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(e.text, sizeof e.text, fmt, ap);
        va_end(ap);
        logHead_ = (logHead_ + 1) % kLogSize;
        if (logCount_ < kLogSize) logCount_++;
    }

private:
    CState   st_ = CState::Open;
    uint32_t stMs_ = 0, openSinceMs_ = 0, ocSinceMs_ = 0;
    uint8_t  retries_ = 0;
    uint32_t latched_ = 0, prevFaults_ = 0;
    bool     welded_ = false, chargeOvLatched_ = false, ocActive_ = false, full_ = false, started_ = false;
    Outputs  out_;
    Event    log_[kLogSize];
    int      logHead_ = 0, logCount_ = 0;

    float busLiveV(float lowest) const {
        const float v = lowest * cfg.weldBusFrac;
        return v > cfg.weldMinV ? v : cfg.weldMinV;
    }

    static float lowestString(const Inputs &in) {
        float lo = 0;
        for (int i = 0; i < kMaxStrings; i++) {
            const StringIn &s = in.s[i];
            if (!s.used) continue;
            if (s.cellsOk != s.expected || s.sumV <= 0) return 0;
            if (lo == 0 || s.sumV < lo) lo = s.sumV;
        }
        return lo;
    }

    uint32_t evalFaults(const Inputs &in, float, float ceil) {
        uint32_t f = 0;
        int nUsed = 0;
        for (int i = 0; i < kMaxStrings; i++) {
            const StringIn &s = in.s[i];
            if (!s.used) continue;
            nUsed++;
            if (s.remote && !in.linkFresh) { f |= F_LINK; continue; }
            if (s.oldestAgeMs > cfg.stringSilentMs) { f |= F_SILENT; continue; }
            if (s.cellsOk != s.expected) { f |= F_COUNT; continue; }
            if (s.maxV > cfg.cellMaxV) f |= F_CELL_HIGH;
            if (s.minV < cfg.lowCutV) f |= F_CELL_LOW;
            if (s.maxV >= ceil + 0.05f && !chargeOvLatched_ && chargingOrRequested(in)) {
                chargeOvLatched_ = true;
                logf(in.nowMs, "CHARGE-OV latched: cell %.3f V >= %.3f V", s.maxV, ceil + 0.05f);
            }
        }
        if (!nUsed) f |= F_COUNT;
        if (in.tempsValid) {
            if (in.tMaxC > cfg.overTempC) f |= F_OVERTEMP;
            if (in.tMaxC > cfg.chargeMaxTempC) f |= F_OVERTEMP_C;
            if (in.tMinC < cfg.chargeMinTempC) f |= F_UNDERTEMP_C;
        } else {
            f |= F_TEMP_INVALID;
        }
        if (in.currentValid) {
            const float a = in.packA < 0 ? -in.packA : in.packA;
            if (a > cfg.overcurrentA) {
                if (!ocActive_) { ocActive_ = true; ocSinceMs_ = in.nowMs; }
                if (in.nowMs - ocSinceMs_ >= cfg.overcurrentMs) f |= F_OVERCURRENT;
            } else {
                ocActive_ = false;
            }
        }
        return f;
    }

    bool chargingOrRequested(const Inputs &in) const { return in.chargeReq || out_.chargeEnable; }

    void chargeLogic(const Inputs &in, Outputs &o, uint32_t f) {
        float maxCell = 0;
        for (int i = 0; i < kMaxStrings; i++) if (in.s[i].used && in.s[i].maxV > maxCell) maxCell = in.s[i].maxV;
        o.chargeV = o.ceilingV * static_cast<float>(cfg.stringCells);
        if (full_ && maxCell < o.ceilingV - cfg.chargeHysV) full_ = false;
        if (!full_ && maxCell >= o.ceilingV) { full_ = true; if (in.chargeReq) logf(in.nowMs, "charge: reached %.3f V/cell, stop", o.ceilingV); }
        const uint32_t cf = (f | latched_) & kChargeFaults;
        if (!in.chargeReq)                 o.chargeWhy = "no request";
        else if (cf)                       o.chargeWhy = "fault";
        else if (st_ != CState::Closed)    o.chargeWhy = "contactors not closed";
        else if (cfg.stringCells <= 0)     o.chargeWhy = "no series string defined";
        else if (full_)                    o.chargeWhy = "at charge limit";
        else {
            float a = cfg.chargeMaxA;
            const float start = o.ceilingV - cfg.taperBandV;
            if (maxCell > start) {  // Tom currentlimit(): linear map over the last band
                const float k = (maxCell - start) / cfg.taperBandV;
                a = cfg.chargeMaxA - k * (cfg.chargeMaxA - cfg.chargeEndA);
            }
            if (a < cfg.chargeEndA) a = cfg.chargeEndA;
            o.chargeEnable = true;
            o.chargeA = a;
            o.chargeWhy = "";
            return;
        }
        o.chargeEnable = false;
        o.chargeA = 0;
    }

    void kellyLogic(const Inputs &in, Outputs &o, uint32_t f) {
        if (st_ != CState::Closed || (f & kDriveFaults)) { o.kellyPct = 0; o.kellyLimp = true; o.kellyRegenOk = false; return; }
        float pct = 100;
        float minCell = 9, maxCell = 0;
        for (int i = 0; i < kMaxStrings; i++) if (in.s[i].used) {
            if (in.s[i].minV < minCell) minCell = in.s[i].minV;
            if (in.s[i].maxV > maxCell) maxCell = in.s[i].maxV;
        }
        const float lv0 = cfg.lowCutV, lv1 = cfg.lowCutV + cfg.derateLowV;
        if (minCell < lv1) { const float k = (minCell - lv0) / (lv1 - lv0); const float p = k * 100; if (p < pct) pct = p; }
        if (in.tempsValid && in.tMaxC > cfg.derateTempC) {
            const float k = (cfg.overTempC - in.tMaxC) / (cfg.overTempC - cfg.derateTempC);
            const float p = k * 100; if (p < pct) pct = p;
        }
        if (in.currentValid && in.packA > cfg.maxDischargeA) {
            const float p = 100 * cfg.maxDischargeA / in.packA; if (p < pct) pct = p;
        }
        if (pct < 0) pct = 0;
        o.kellyPct = pct;
        o.kellyLimp = pct < 100;
        o.kellyRegenOk = maxCell < kCeil100V - 0.02f && (!in.tempsValid || in.tMinC >= cfg.chargeMinTempC);
    }

    void enter(CState s, uint32_t now, const char *why) {
        if (s == st_) return;
        logf(now, "%s -> %s (%s)", stateName(st_), stateName(s), why);
        st_ = s;
        stMs_ = now;
    }

    void logFaultEdges(uint32_t now, uint32_t f) {
        const uint32_t up = f & ~prevFaults_, down = prevFaults_ & ~f;
        for (int b = 0; b < 13; b++) {
            const uint32_t bit = 1u << b;
            if (bit == F_TEMP_INVALID) continue;
            if (up & bit) logf(now, "FAULT %s", faultName(bit));
            if (down & bit) logf(now, "clear %s", faultName(bit));
        }
        prevFaults_ = f;
    }
};

// ------------------------------------------- factory current sensor (DHAB S/44)
// Chevy Volt gen1 pack current sensor = LEM DHAB S/44, open-loop Hall, 5 V,
// two RATIOMETRIC analog outputs (LEM datasheet, @Vc = 5 V):
//   ch1 (low range)  +-20 A,       offset 2.50 V, 100  mV/A
//   ch2 (high range) -420..+230 A, offset 3.08 V, 6.15 mV/A
//   outputs clamp at 0.25 V / 4.75 V; load >= 10 kOhm, 1-100 nF.
// Teensy 4.1 ADC pins are 3.3 V ONLY: each output goes through a divider.
struct CurrentCal {
    float divider    = 18.0f / (10.0f + 18.0f);  // 10k top / 18k bottom: 5 V -> 3.21 V
    float off1Frac   = 2.50f / 5.0f;   // zero point as a fraction of Vc (calibrated 'Z')
    float off2Frac   = 3.08f / 5.0f;
    float g1VperA    = 0.100f;         // @ 5 V
    float g2VperA    = 0.00615f;
    float lowMaxA    = 18.0f;          // use ch1 below this (ch1 saturates at +-20 A)
    float deadbandA  = 0.3f;
    bool  invert     = false;
};

// adc*: volts at the Teensy pins; supplyAdcV: sensor 5 V rail through the same divider.
inline float dhabAmps(const CurrentCal &c, float adc1, float adc2, float supplyAdc, bool *valid = nullptr) {
    const float vc = supplyAdc / c.divider;
    if (vc < 4.5f || vc > 5.5f) { if (valid) *valid = false; return 0; }
    const float s = vc / 5.0f;  // ratiometric
    const float v1 = adc1 / c.divider, v2 = adc2 / c.divider;
    const float clampLo = 0.26f * s, clampHi = 4.74f * s;
    const float i1 = (v1 - c.off1Frac * vc) / (c.g1VperA * s);
    const float i2 = (v2 - c.off2Frac * vc) / (c.g2VperA * s);
    float i;
    const bool ch1ok = v1 > clampLo && v1 < clampHi && (i1 < c.lowMaxA && i1 > -c.lowMaxA);
    if (ch1ok) i = i1;
    else {
        if (v2 <= clampLo || v2 >= clampHi) { if (valid) *valid = false; return 0; }
        i = i2;
    }
    if (i < c.deadbandA && i > -c.deadbandA) i = 0;
    if (valid) *valid = true;
    return c.invert ? -i : i;
}
// Inverse (for the simulator / tests): pin volts for a given current.
inline void dhabPinVolts(const CurrentCal &c, float amps, float vc, float &adc1, float &adc2, float &supplyAdc) {
    const float s = vc / 5.0f;
    if (c.invert) amps = -amps;
    float v1 = c.off1Frac * vc + c.g1VperA * s * amps;
    float v2 = c.off2Frac * vc + c.g2VperA * s * amps;
    if (v1 < 0.25f * s) v1 = 0.25f * s; if (v1 > 4.75f * s) v1 = 4.75f * s;
    if (v2 < 0.25f * s) v2 = 0.25f * s; if (v2 > 4.75f * s) v2 = 4.75f * s;
    adc1 = v1 * c.divider; adc2 = v2 * c.divider; supplyAdc = vc * c.divider;
}
// Zero calibration ('Z' with no current flowing): store offsets as fractions of Vc.
inline void dhabCalibrateZero(CurrentCal &c, float adc1, float adc2, float supplyAdc) {
    const float vc = supplyAdc / c.divider;
    if (vc < 4.5f || vc > 5.5f) return;
    c.off1Frac = (adc1 / c.divider) / vc;
    c.off2Frac = (adc2 / c.divider) / vc;
}

// ------------------------------------------- BICM temperature (Tom, UNVERIFIED)
// VoltBMSV2/BMSModule.cpp 67-68: 0x7Ex bytes 6-7 (BE) * -0.0324 + 150 degC.
// Tom's resting capture (Slave-CAN-Cap) decodes to 16.0-17.3 degC on all 7 IDs.
inline float bicmTempC(uint8_t b6, uint8_t b7) {
    return static_cast<float>((b6 << 8) | b7) * -0.0324f + 150.0f;
}

}  // namespace BmsCore
