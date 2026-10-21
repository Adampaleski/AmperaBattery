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
    F_CELL_LOW    = 1u << 3,   // a cell < lowCutV at rest for lowCutMs, or < hardFloorV for hardFloorMs
    F_OVERTEMP    = 1u << 4,   // pack temp > overTempC
    F_UNDERTEMP_C = 1u << 5,   // charge only: pack temp < chargeMinTempC
    F_OVERCURRENT = 1u << 6,   // |I| > overcurrentA for overcurrentMs, or > instantTripA once
    F_LINK        = 1u << 7,   // B -> A link stale
    F_PRECHARGE   = 1u << 8,   // precharge failed after retries (lockout)
    F_WELDED      = 1u << 9,   // bus stays live with every contactor commanded open
    F_CHARGE_OV   = 1u << 10,  // charge only: cell >= ceiling + 0.05 (latched to reboot)
    F_OVERTEMP_C  = 1u << 11,  // charge only: pack temp > chargeMaxTempC
    F_TEMP_INVALID= 1u << 12,  // charge only: no verified temperatures
    F_TEMP_STALE  = 1u << 13,  // per-string temps verified but a string's temp data is stale/missing (CREW REVIEW)
};
// Faults that open the contactors.
constexpr uint32_t kDriveFaults = F_SILENT | F_COUNT | F_CELL_HIGH | F_CELL_LOW | F_OVERTEMP |
                                  F_OVERCURRENT | F_LINK | F_PRECHARGE | F_WELDED | F_TEMP_STALE;
// Faults that stop the charger (superset).
constexpr uint32_t kChargeFaults = kDriveFaults | F_UNDERTEMP_C | F_CHARGE_OV | F_OVERTEMP_C | F_TEMP_INVALID;

// Derates (power cutbacks with the contactors still closed). Adam 2026-10-21:
// every derate is logged like a fault, with its reason. Codes are stored in
// FaultLog records, so never renumber them.
enum DerateCode : uint8_t { D_NONE = 0, D_LOW_CELL = 1, D_PACK_SAG = 2, D_CELL_SAG = 3, D_PACK_HOT = 4,
                            D_LEM_IMB = 5, D_DISCHARGE_A = 6 };
inline const char *derateName(uint8_t c) {
    switch (c) {
        case D_LOW_CELL: return "low cell";      case D_PACK_SAG: return "pack sag";
        case D_CELL_SAG: return "cell sag";      case D_PACK_HOT: return "pack hot";
        case D_LEM_IMB: return "LEM imbalance";  case D_DISCHARGE_A: return "discharge amps";
        default: return "";
    }
}

inline const char *faultName(uint32_t bit) {
    switch (bit) {
        case F_SILENT: return "BOARD-SILENT";      case F_COUNT: return "CELL-COUNT/BAD";
        case F_CELL_HIGH: return "CELL-HIGH";      case F_CELL_LOW: return "CELL-LOW";
        case F_OVERTEMP: return "OVERTEMP";        case F_UNDERTEMP_C: return "UNDERTEMP-CHARGE";
        case F_OVERCURRENT: return "OVERCURRENT";  case F_LINK: return "LINK-LOST";
        case F_PRECHARGE: return "PRECHARGE-FAIL"; case F_WELDED: return "WELDED-CONTACTOR";
        case F_CHARGE_OV: return "CHARGE-OV";      case F_OVERTEMP_C: return "OVERTEMP-CHARGE";
        case F_TEMP_INVALID: return "TEMPS-NOT-VERIFIED";
        case F_TEMP_STALE: return "TEMP-STALE";
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
    float    lowCutV        = 3.30f;   // L setting: open contactors if a cell stays below this AT REST ...
    uint32_t lowCutMs       = 1000;    // ... for this long. PLACEHOLDER (Adam 2026-10-21: no instant fixed cut)
    float    cellMaxV       = 4.25f;   // kCellFaultMaxV: open contactors above
    int      chargePct      = 80;      // P setting (EEPROM), 80 % = 3.95 V
    float    chargeHysV     = 0.05f;   // restart charge only after max cell falls this far
    float    taperBandV     = 0.05f;   // current tapers over the last 50 mV (Tom ChargeHys/map)
    float    chargeMaxA     = 5.0f;    // kChargeCurrentDeciA (outlet-limited)
    float    chargeEndA     = 1.0f;    // current at the ceiling before stop (Tom chargecurrentend 5 A on a big charger)
    int      stringCells    = 36;      // charger V basis: cells in ONE series string (24S + M4 12S)
    float    busMaxV        = 168.0f;  // Kelly KLS "168 V" (VERIFY label)
    float    busMarginV     = 5.0f;    // general pack clamp: ceiling x cells <= busMaxV - margin
    // temps
    float    overTempC      = 55.0f;   // drive: open contactors above
    float    chargeMaxTempC = 45.0f;
    float    chargeMinTempC = 0.0f;
    float    derateTempC    = 50.0f;   // Kelly derate starts
    // Charge mode (Adam 2026-10-21): plug-in wakes the BMS without key-on.
    float    chargeColdC    = 10.0f;   // pack below this: charge current reduced ... PLACEHOLDER
    float    chargeColdFrac = 0.50f;   // ... to this fraction of chargeMaxA. PLACEHOLDER (no LG/Chevy curve on file)
    // current
    // Hard trip on LEM A + LEM B (Adam, 2026-10-20; replaces 300 A / 500 ms).
    // Kelly phase + battery current set to 550 A, so the BMS does not cap
    // current in normal driving.
    float    overcurrentA   = 650.0f;  // |I| above this ...
    uint32_t overcurrentMs  = 100;     // ... for this long -> open
    float    instantTripA   = 750.0f;  // |I| above this on ANY sample -> open (no debounce)
    float    maxDischargeA  = 0.0f;    // Kelly "discharge amps" derate; 0 = off (250 A derate superseded 2026-10-20)
    // timing (Config.h / StringConfig.h)
    uint32_t stringSilentMs = 2000;
    uint32_t linkStaleMs    = 200;   // PROPOSED (Adam 2026-10-21): lost B heartbeat -> F_LINK, mains open
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
    // Cell sag (Adam, 2026-10-21): RELATIVE, not a fixed per-cell voltage.
    //  * one cell well below the pack average -> "cell sag" derate (any load)
    //  * whole-pack sag is judged against load: under load (|I| > restA) a
    //    uniform sag only derates near hardFloorV ("pack sag") and only trips
    //    if a cell stays under hardFloorV for hardFloorMs. lowCutV applies at rest.
    //  * no current reading -> treated as rest (conservative).
    // Every value below is a PLACEHOLDER (not a Chevy/LG spec) and adjustable.
    float    restA          = 20.0f;   // |I| at or below this = rest
    float    relSagStartV   = 0.15f;   // weakest cell this far below pack average: derate starts
    float    relSagZeroV    = 0.40f;   // ... reaches 0 % here (contactors stay closed)
    float    hardFloorV     = 3.00f;   // under load: cell below this ...
    uint32_t hardFloorMs    = 500;     // ... for this long -> CELL-LOW (open)
    float    packSagBandV   = 0.10f;   // under load: pack-average taper from floor + band down to floor
    float    imbalanceA     = 100.0f;  // |LEM A - LEM B| above this ...
    uint32_t imbalanceMs    = 1000;    // ... for this long -> derate + warn
    // Low cap (crew 2026-10-21): soft derates only scale kellyPct; the Kelly ECO 10 mph cap (kellyLimp) engages
    // only on HARD faults, after ramping kellyPct down to limpRampPct over limpRampMs. All PROPOSED, adjustable.
    float    limpCellV      = 3.00f;   // any cell below -> hard
    uint32_t limpRampMs     = 1000;    // ~1 s ramp into the cap
    float    limpRampPct    = 30.0f;   // power allowed at the end of the ramp (cap then holds speed)
    float    imbalancePct   = 50.0f;   // allowance while imbalanced. PLACEHOLDER: level not set by Adam
    // Derate logging (Adam 2026-10-21). PLACEHOLDER debounce, adjustable.
    uint32_t derateLogMs    = 100;     // a cutback must hold this long to count as a derate event
    uint32_t derateClearMs  = 1000;    // full power this long before the event is logged as cleared
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
    // Per-string BICM temps (2026-10-21). Local: StringMon 0x7E0-0x7EF; remote: link 0x6A3.
    bool     tFresh = false;       // temp data fresh (local: STRING_SILENT_MS, remote: link stale limit)
    float    tMinC = 20, tMaxC = 20;
    uint8_t  tHot = 0xFF;          // hottest thermistor index on that string (0x7E0 + n), 0xFF none
};

struct Inputs {
    uint32_t nowMs        = 0;
    StringIn s[kMaxStrings];
    bool     linkFresh    = true;  // remote heartbeat newer than linkStaleMs
    uint32_t linkAgeMs    = 0;
    bool     tempsValid   = false; // TEMPS_VERIFIED && fresh
    float    tMinC = 20, tMaxC = 20;
    bool     perStringTemps = false; // true: tMinC/tMaxC are folded from s[].t* (all used strings), stale -> F_TEMP_STALE
    bool     currentValid = false;
    float    packA        = 0;     // + = discharge
    bool     currentExpected = false; // current sensing fitted+enabled (CURRENT_SENSE_ENABLE): !currentValid = LOST
    bool     tempsExpected   = false; // temps verified (TEMPS_VERIFIED): !tempsValid = STALE
    bool     lemSplitValid= false; // both halves read (LEM A on Teensy A, LEM B over the link)
    float    lemA = 0, lemB = 0;   // strings 1+2 and 3+4, + = discharge
    bool     busValid     = false; // HV bus sense present
    float    busV         = 0;
    bool     keyOn        = false; // dead-man / key / drive request
    bool     chargeReq    = false;  // legacy bench request ('g'); charges with key-on contactors
    bool     chargeStop   = false;  // driver pressed Stop on the charge screen (charger off, mains stay closed)
    bool     plugIn       = false;  // charger present / proximity (Adam 2026-10-21). Wakes the BMS in
                                    // CHARGE mode without key-on; drive locked while true. Firmware
                                    // has no proximity pin wired yet (stays false = no behaviour change).
};

// Operating mode (Adam 2026-10-21): charge is separate from key-on.
enum class Mode : uint8_t { Idle = 0, Drive, Charge };
inline const char *modeName(Mode m) { return m == Mode::Drive ? "DRIVE" : (m == Mode::Charge ? "CHARGE" : "IDLE"); }

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
    uint8_t  hotString = 0xFF, hotIdx = 0xFF;   // hottest string (0-3) / thermistor (per-string temps)
    bool     pre = false, mainPos = false, mainNeg = false;  // WANTED coil states
    bool     chargeEnable = false;
    float    chargeV = 0, chargeA = 0;     // Elcon request (0 A + stop byte when disabled)
    float    ceilingV = 0;
    float    kellyPct = 0;                 // throttle/discharge allowance 0..100
    bool     kellyLimp = false;            // assert low-speed input
    bool     kellyRegenOk = false;
    const char *limpWhy = "";              // hard fault holding the Low cap ("" = none)
    const char *kellyWhy = "";             // active limiter ("" = full power)
    const char *regenWhy = "";             // why regen is inhibited ("" = allowed)
    uint8_t  derateNow  = D_NONE;          // raw limiter this tick (no debounce)
    uint8_t  derateCode = D_NONE;          // logged derate event in progress (debounced)
    float    derateMinPct = 100;           // lowest allowance in the current (or last) event
    uint8_t  lastDerate = D_NONE;          // reason of the most recent event this boot (sticky)
    uint32_t derateEvents = 0;             // events since boot
    bool     lemImbalance = false;         // |A - B| > imbalanceA for imbalanceMs (warn + derate)
    float    lemDiffA = 0;                 // |A - B| now (0 when split not valid)
    float    minCellV = 0, avgCellV = 0;   // weakest cell, pack average (used strings)
    bool     underLoad = false;            // |I| > restA with a valid current reading
    uint32_t faults  = 0;                  // active now
    uint32_t latched = 0;                  // latched (drive faults until key-off + clear)
    const char *chargeWhy = "";
    float    lowestStringV = 0;
    Mode     mode = Mode::Idle;            // CHARGE when plugged in, DRIVE on key-on (not plugged), else IDLE
    bool     chargerPath = false;          // WANTED: Volt AEV14012 charger-branch contactor (after B-, pre, B+)
    bool     driveLocked = false;          // plugged in, or key-on refused until a key cycle
    bool     keyRefused  = false;          // key-on seen while plugged in (cleared by key off)
};

// ------------------------------------------------------------ event log
struct Event { uint32_t ms; char text[72]; };
constexpr int kLogSize = 64;

class Core {
public:
    Config cfg;

    void reset() {
        st_ = CState::Open; stMs_ = 0; retries_ = 0; latched_ = 0; chargeOvLatched_ = false;
        welded_ = false; keyBlock_ = false; ocSinceMs_ = 0; ocActive_ = false; full_ = false; openSinceMs_ = 0;
        imbSinceMs_ = 0; imbOver_ = false; imbOn_ = false;
        hardOn_ = false; hardSinceMs_ = 0; derOn_ = D_NONE; derRaw_ = D_NONE; derSinceMs_ = derFullMs_ = 0; derMin_ = 100; derLast_ = D_NONE; derEvents_ = 0;
        lowSinceMs_ = floorSinceMs_ = 0; lowBelow_ = floorBelow_ = false;
        logHead_ = 0; logCount_ = 0; prevFaults_ = 0; out_ = Outputs{}; started_ = false;
    }

    // Per-string temps (only with in.perStringTemps && in.tempsValid, i.e. TEMPS_VERIFIED):
    // pack min/max = over every used string (A sees strings 3/4 via the link), so heat
    // derate and OVERTEMP cut cover all 4 strings. A used string without fresh temps
    // -> F_TEMP_STALE (drive fault: mains open, logged). CREW REVIEW.
    uint8_t hotS_ = 0xFF, hotI_ = 0xFF;
    uint32_t foldTemps(Inputs &in) {
        hotS_ = 0xFF; hotI_ = 0xFF;
        if (!in.perStringTemps || !in.tempsValid) return 0;
        uint32_t f = 0; bool any = false; float mn = 0, mx = 0;
        for (int i = 0; i < kMaxStrings; i++) {
            const StringIn &s = in.s[i];
            if (!s.used) continue;
            if (!s.tFresh) { f |= F_TEMP_STALE; continue; }
            if (!any || s.tMinC < mn) mn = s.tMinC;
            if (!any || s.tMaxC > mx) { mx = s.tMaxC; hotS_ = static_cast<uint8_t>(i); hotI_ = s.tHot; }
            any = true;
        }
        if (any) { in.tMinC = mn; in.tMaxC = mx; } else in.tempsValid = false;
        return f;
    }

    const Outputs &out() const { return out_; }
    int logCount() const { return logCount_; }
    const Event &logAt(int i) const {  // 0 = oldest kept
        const int start = (logHead_ - logCount_ + kLogSize) % kLogSize;
        return log_[(start + i) % kLogSize];
    }

    const Outputs &step(const Inputs &inRaw) {
        Inputs in = inRaw;
        const uint32_t tStale = foldTemps(in);
        const uint32_t now = in.nowMs;
        if (!started_) { started_ = true; stMs_ = now; openSinceMs_ = now; }
        Outputs o;
        o.ceilingV = ceilingForPct(cfg.chargePct);
        // General pack-voltage clamp (safety net). 36S x 4.10 V = 147.6 V < 163 V: inactive.
        if (cfg.stringCells > 0 && o.ceilingV * cfg.stringCells > cfg.busMaxV - cfg.busMarginV)
            o.ceilingV = (cfg.busMaxV - cfg.busMarginV) / static_cast<float>(cfg.stringCells);
        o.lowestStringV = lowestString(in);
        cellStats(in, o);
        uint32_t f = evalFaults(in, o.lowestStringV, o.ceilingV);
        f |= cellLowLogic(in, o);
        f |= tStale;
        o.hotString = hotS_; o.hotIdx = hotI_;

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

        // ---- wake / mode: plug-in OR key-on closes the mains; key-on while
        // plugged in is refused (drive locked) until the key is cycled off.
        const bool wake = in.keyOn || in.plugIn;
        if (in.keyOn && in.plugIn && !keyBlock_) { keyBlock_ = true; logf(now, "key on refused: plugged in (drive locked)"); }
        if (!in.keyOn && keyBlock_) keyBlock_ = false;

        // ---- contactor state machine
        const bool driveFault = (f | latched_) & kDriveFaults;
        if (driveFault && st_ != CState::Open) latched_ |= f & kDriveFaults;
        if (driveFault && st_ != CState::Fault && st_ != CState::Lockout && st_ != CState::Open) {
            enter(CState::Fault, now, "open: fault");
        }
        if (!wake && st_ != CState::Open && st_ != CState::Lockout && st_ != CState::Fault) {
            enter(CState::Open, now, "key off");
        }
        const uint32_t el = now - stMs_;
        switch (st_) {
            case CState::Open:
                // Wait weldCheckMs after any open so a stuck contactor is seen before re-closing.
                if (wake && !driveFault && now - openSinceMs_ >= cfg.weldCheckMs) {
                    retries_ = 0;
                    enter(CState::Precharge, now, in.plugIn ? "plug in: CHARGE (no key)" : "key on");
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
                if (!wake && !(f & kDriveFaults) && !welded_) {
                    latched_ = 0;
                    enter(CState::Open, now, "cleared (key off + unplugged, faults gone)");
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
        o.mode = in.plugIn ? Mode::Charge : ((in.keyOn && !keyBlock_) ? Mode::Drive : Mode::Idle);
        o.keyRefused = keyBlock_;
        o.driveLocked = in.plugIn || keyBlock_;

        // ---- charger (Elcon) request
        if (chargeOvLatched_) f |= F_CHARGE_OV;
        chargeLogic(in, o, f);
        // Charger branch (Volt AEV14012, docs/TERYX_MAIN_POWER_PATH.md): closes only
        // after B-, precharge, B+ are Closed, in CHARGE mode, with no charge fault.
        o.chargerPath = st_ == CState::Closed && o.mode == Mode::Charge && !((f | latched_) & kChargeFaults);

        // ---- LEM pair imbalance timer, then Kelly limits
        imbalanceLogic(in, o);
        kellyLogic(in, o, f);
        derateEvents(now, o);

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
    uint32_t stMs_ = 0, openSinceMs_ = 0, ocSinceMs_ = 0, imbSinceMs_ = 0, lowSinceMs_ = 0, floorSinceMs_ = 0;
    bool     lowBelow_ = false, floorBelow_ = false;
    bool     imbOver_ = false, imbOn_ = false;
    uint8_t  retries_ = 0;
    bool     hardOn_ = false; uint32_t hardSinceMs_ = 0;
    uint8_t  derOn_ = D_NONE, derRaw_ = D_NONE, derLast_ = D_NONE;
    uint32_t derSinceMs_ = 0, derFullMs_ = 0, derEvents_ = 0;
    float    derMin_ = 100;
    uint32_t latched_ = 0, prevFaults_ = 0;
    bool     keyBlock_ = false;
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
            if (cfg.instantTripA > 0 && a > cfg.instantTripA) {
                if (!(prevFaults_ & F_OVERCURRENT)) logf(in.nowMs, "OC instant: %.0f A > %.0f A", a, cfg.instantTripA);
                f |= F_OVERCURRENT;
            }
            if (a > cfg.overcurrentA) {
                if (!ocActive_) { ocActive_ = true; ocSinceMs_ = in.nowMs; }
                if (in.nowMs - ocSinceMs_ >= cfg.overcurrentMs) f |= F_OVERCURRENT;
            } else {
                ocActive_ = false;
            }
        }
        return f;
    }

    bool isUnderLoad(const Inputs &in) const {
        const float a = in.packA < 0 ? -in.packA : in.packA;
        return in.currentValid && a > cfg.restA;
    }

    // Weakest cell and pack-average cell over every used string with a full,
    // fresh cell count (link-stale / silent / short strings are faulted elsewhere).
    void cellStats(const Inputs &in, Outputs &o) const {
        float mn = 0, sum = 0; int n = 0;
        for (int i = 0; i < kMaxStrings; i++) {
            const StringIn &s = in.s[i];
            if (!s.used || s.expected == 0 || s.cellsOk != s.expected) continue;
            if (s.remote && !in.linkFresh) continue;
            if (n == 0 || s.minV < mn) mn = s.minV;
            sum += s.sumV; n += s.expected;
        }
        o.minCellV = n ? mn : 0;
        o.avgCellV = n ? sum / static_cast<float>(n) : 0;
        o.underLoad = isUnderLoad(in);
    }

    // CELL-LOW (Adam 2026-10-21): at rest a cell below lowCutV for lowCutMs;
    // any load, a cell below hardFloorV for hardFloorMs. A short launch sag
    // under lowCutV but above the floor never opens the mains.
    uint32_t cellLowLogic(const Inputs &in, const Outputs &o) {
        const uint32_t now = in.nowMs;
        const bool have = o.minCellV > 0;
        const bool low = have && !o.underLoad && o.minCellV < cfg.lowCutV;
        const bool flo = have && o.minCellV < cfg.hardFloorV;
        if (low && !lowBelow_) lowSinceMs_ = now;
        if (flo && !floorBelow_) floorSinceMs_ = now;
        lowBelow_ = low; floorBelow_ = flo;
        const bool tripLow = low && now - lowSinceMs_ >= cfg.lowCutMs;
        const bool tripFloor = flo && now - floorSinceMs_ >= cfg.hardFloorMs;
        if ((tripLow || tripFloor) && !(prevFaults_ & F_CELL_LOW))
            logf(now, tripFloor ? "CELL-LOW: %.3f V < floor %.2f V for %u ms" : "CELL-LOW: %.3f V < %.2f V at rest for %u ms",
                 o.minCellV, tripFloor ? cfg.hardFloorV : cfg.lowCutV,
                 static_cast<unsigned>(tripFloor ? cfg.hardFloorMs : cfg.lowCutMs));
        return (tripLow || tripFloor) ? F_CELL_LOW : 0;
    }

    bool chargingOrRequested(const Inputs &in) const { return in.chargeReq || in.plugIn || out_.chargeEnable; }

    void chargeLogic(const Inputs &in, Outputs &o, uint32_t f) {
        float maxCell = 0;
        for (int i = 0; i < kMaxStrings; i++) if (in.s[i].used && in.s[i].maxV > maxCell) maxCell = in.s[i].maxV;
        o.chargeV = o.ceilingV * static_cast<float>(cfg.stringCells);
        if (full_ && maxCell < o.ceilingV - cfg.chargeHysV) full_ = false;
        if (!full_ && maxCell >= o.ceilingV) { full_ = true; if (in.chargeReq || in.plugIn) logf(in.nowMs, "charge: reached %.3f V/cell, stop", o.ceilingV); }
        const uint32_t cf = (f | latched_) & kChargeFaults;
        const bool req = in.chargeReq || in.plugIn;
        if (!req)                          o.chargeWhy = "no request";
        else if (in.chargeStop)            o.chargeWhy = "stopped by driver";
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
            o.chargeWhy = "";
            // Cold: 0..chargeColdC reduced (below chargeMinTempC is a fault above).
            if (in.tempsValid && in.tMinC < cfg.chargeColdC) {
                const float cold = cfg.chargeMaxA * cfg.chargeColdFrac;
                if (a > cold) a = cold;
                o.chargeWhy = "cold: reduced";
            }
            o.chargeEnable = true;
            o.chargeA = a;
            return;
        }
        o.chargeEnable = false;
        o.chargeA = 0;
    }

    // Split LEM: strings 1+2 and 3+4 should share current about equally.
    // |A - B| > imbalanceA continuously for imbalanceMs -> warn + derate.
    // Clears as soon as the difference drops back under imbalanceA.
    void imbalanceLogic(const Inputs &in, Outputs &o) {
        const bool closed = st_ == CState::Closed;
        float d = in.lemA - in.lemB; if (d < 0) d = -d;
        o.lemDiffA = in.lemSplitValid ? d : 0;
        const bool over = closed && in.lemSplitValid && d > cfg.imbalanceA;
        if (over && !imbOver_) imbSinceMs_ = in.nowMs;
        imbOver_ = over;
        const bool on = over && in.nowMs - imbSinceMs_ >= cfg.imbalanceMs;
        if (on && !imbOn_) logf(in.nowMs, "LEM imbalance: A %.0f / B %.0f A, derate %.0f%%", in.lemA, in.lemB, cfg.imbalancePct);
        if (!on && imbOn_) logf(in.nowMs, "LEM imbalance cleared");
        imbOn_ = on;
        o.lemImbalance = on;
    }

    void kellyLogic(const Inputs &in, Outputs &o, uint32_t f) {
        if (o.driveLocked) {  // Adam 2026-10-21: no drive (or regen) while plugged in
            o.kellyPct = 0; o.kellyLimp = true; o.kellyRegenOk = false;
            o.kellyWhy = in.plugIn ? "charging - drive locked" : "key cycle needed (was plugged in)";
            o.regenWhy = o.kellyWhy;
            return;
        }
        if (st_ != CState::Closed || (f & kDriveFaults)) {
            o.kellyPct = 0; o.kellyLimp = true; o.kellyRegenOk = false;
            o.kellyWhy = (f & kDriveFaults) ? "drive fault" : "contactors open";
            o.regenWhy = o.kellyWhy;
            return;
        }
        float pct = 100;
        const char *why = "";
        uint8_t code = D_NONE;
        float maxCell = 0;
        for (int i = 0; i < kMaxStrings; i++) if (in.s[i].used && in.s[i].maxV > maxCell) maxCell = in.s[i].maxV;
        const float minCell = o.minCellV, avg = o.avgCellV;
        if (minCell > 0) {
            if (!o.underLoad) {  // at rest: weakest cell vs the low cut
                const float lv0 = cfg.lowCutV, lv1 = cfg.lowCutV + cfg.derateLowV;
                if (minCell < lv1) { const float p = (minCell - lv0) / (lv1 - lv0) * 100; if (p < pct) { pct = p; why = "low cell"; code = D_LOW_CELL; } }
            } else {             // under load: whole-pack average vs the hard floor
                const float f0 = cfg.hardFloorV, f1 = cfg.hardFloorV + cfg.packSagBandV;
                if (cfg.packSagBandV > 0 && avg < f1) { const float p = (avg - f0) / (f1 - f0) * 100; if (p < pct) { pct = p; why = "pack sag"; code = D_PACK_SAG; } }
            }
            const float d = avg - minCell;  // relative: one cell below the rest
            if (cfg.relSagZeroV > cfg.relSagStartV && d > cfg.relSagStartV) {
                const float p = (cfg.relSagZeroV - d) / (cfg.relSagZeroV - cfg.relSagStartV) * 100;
                if (p < pct) { pct = p; why = "cell sag"; code = D_CELL_SAG; }
            }
        }
        if (in.tempsValid && in.tMaxC > cfg.derateTempC) {
            const float k = (cfg.overTempC - in.tMaxC) / (cfg.overTempC - cfg.derateTempC);
            const float p = k * 100; if (p < pct) { pct = p; why = "pack hot"; code = D_PACK_HOT; }
        }
        if (o.lemImbalance && cfg.imbalancePct < pct) { pct = cfg.imbalancePct; why = "LEM imbalance"; code = D_LEM_IMB; }
        if (cfg.maxDischargeA > 0 && in.currentValid && in.packA > cfg.maxDischargeA) {
            const float p = 100 * cfg.maxDischargeA / in.packA; if (p < pct) { pct = p; why = "discharge amps"; code = D_DISCHARGE_A; }
        }
        // HARD faults -> Low cap (ramped). Soft derates above only scale pct.
        const char *hard = "";
        if (in.currentExpected && !in.currentValid) hard = "current sensor lost";
        else if (in.tempsExpected && !in.tempsValid) hard = "temps stale";
        else if (minCell > 0 && minCell < cfg.limpCellV) hard = "cell < 3.0 V";
        else if (o.lemImbalance) hard = "LEM A/B mismatch";
        if (hard[0]) {
            if (!hardOn_) { hardOn_ = true; hardSinceMs_ = in.nowMs; logf(in.nowMs, "LOW CAP: %s (ramp %lu ms)", hard, (unsigned long)cfg.limpRampMs); }
            const uint32_t t = in.nowMs - hardSinceMs_;
            const float k = cfg.limpRampMs ? (t >= cfg.limpRampMs ? 1.0f : (float)t / cfg.limpRampMs) : 1.0f;
            const float rp = 100 - (100 - cfg.limpRampPct) * k;
            if (rp < pct) { pct = rp; why = hard; }
            o.kellyLimp = t >= cfg.limpRampMs;
        } else {
            if (hardOn_) logf(in.nowMs, "LOW CAP released");
            hardOn_ = false; o.kellyLimp = false;
        }
        o.limpWhy = hard;
        if (pct < 0) pct = 0;
        o.kellyPct = pct;
        o.kellyWhy = why;
        o.derateNow = pct < 100 ? code : D_NONE;
        const bool full = maxCell >= kCeil100V - 0.02f;
        const bool cold = in.tempsValid && in.tMinC < cfg.chargeMinTempC;
        o.kellyRegenOk = !full && !cold;
        o.regenWhy = full ? "cells full" : (cold ? "pack cold" : "");
    }

    // Debounced derate events for the log (and FaultLog via Outputs.derateCode).
    // Start: a cutback held derateLogMs. Reason change: logged at once. Clear:
    // full power (or contactors open / drive fault) for derateClearMs.
    void derateEvents(uint32_t now, Outputs &o) {
        const uint8_t raw = o.derateNow;
        if (raw != derRaw_) { derRaw_ = raw; derSinceMs_ = now; }
        if (raw != D_NONE) {
            derFullMs_ = now;
            if (derOn_ == D_NONE && now - derSinceMs_ >= cfg.derateLogMs) {
                derOn_ = raw; derLast_ = raw; derEvents_++; derMin_ = o.kellyPct;
                logf(now, "DERATE %s: power %.0f%%", derateName(raw), o.kellyPct);
            } else if (derOn_ != D_NONE && raw != derOn_) {
                logf(now, "DERATE %s (was %s): power %.0f%%", derateName(raw), derateName(derOn_), o.kellyPct);
                derOn_ = raw; derLast_ = raw;
            }
            if (derOn_ != D_NONE && o.kellyPct < derMin_) derMin_ = o.kellyPct;
        } else if (derOn_ != D_NONE && now - derFullMs_ >= cfg.derateClearMs) {
            logf(now, "derate cleared: %s (lowest %.0f%%)", derateName(derOn_), derMin_);
            derOn_ = D_NONE;
        }
        o.derateCode = derOn_; o.derateMinPct = derMin_; o.lastDerate = derLast_; o.derateEvents = derEvents_;
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
    if (v1 < 0.25f * s) v1 = 0.25f * s;
    if (v1 > 4.75f * s) v1 = 4.75f * s;
    if (v2 < 0.25f * s) v2 = 0.25f * s;
    if (v2 > 4.75f * s) v2 = 4.75f * s;
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
