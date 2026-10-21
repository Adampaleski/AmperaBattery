// WebAssembly API around BmsSim::World + BmsCore::Core for tools/bms_sim.html.
// Same headers as the Teensy firmware and scripts/sim_bms.cpp: no logic here.
// Build: scripts/build_bms_sim_html.sh (zig c++ -target wasm32-wasi, reactor).
#include <string.h>
#include "BmsSim.h"
#include "SocEstimator.h"
#include "FaultLog.h"

using namespace BmsSim;

namespace {
World g_w;
char  g_buf[16384];
float g_deadband = 0.010f;

// ---- SoC (sim only): the real BmsCore::Soc fed from the sim's LEM A + LEM B.
// Firmware does not run this yet (CURRENT_SENSE_ENABLE=0); the real capAh is
// UNVERIFIED (0 = unknown). The page uses the capacity the sim already assumed
// for its range gauge: 4 strings x 45 Ah = 180 Ah, labelled SIM-ASSUMED.
constexpr float kSimCapAh   = 180.0f;              // SIM-ASSUMED, not measured
constexpr float kTruth0V    = 3.30f;               // sim truth map: Config::lowCutV default = 0 %
constexpr float kTruth100V  = BmsCore::kCeil100V;  // ... P100 ceiling 4.10 V = 100 % (linear, SIM-ASSUMED)
BmsCore::Soc g_soc;
bool  g_drain = true;     // sim plant: load current drains every cell along the truth map
bool  g_anchorNow = false;
uint32_t g_lemDropMs = 0; // > 0: LEM A + B read invalid until this sim time

void socReset() { BmsCore::SocConfig c; c.capAh = kSimCapAh; g_soc.reset(c); g_anchorNow = false; g_lemDropMs = 0; }
struct SocInit { SocInit() { socReset(); } } g_socInit;

// ---- Fault log (sim only): the real BmsCore::FaultLog on a simulated 4284-byte
// Teensy 4.1 emulated EEPROM. g_ee survives sim_reset and "power cycle" (that
// is the point); the page also mirrors it to localStorage so it survives a
// page reload. Firmware does not call FaultLog yet (backlog item 5).
uint8_t  g_ee[4284];
bool     g_eeDirty = false;
uint32_t g_eeWrites = 0;          // bytes actually changed (EEPROM.update semantics)
uint8_t  eeRead(int a) { return (a >= 0 && a < static_cast<int>(sizeof g_ee)) ? g_ee[a] : 0xFF; }
void     eeWrite(int a, uint8_t v) { if (a >= 0 && a < static_cast<int>(sizeof g_ee) && g_ee[a] != v) { g_ee[a] = v; g_eeDirty = true; g_eeWrites++; } }
BmsCore::FaultLog g_flog;
uint32_t g_prevFaults = 0;
void flogBegin() { g_flog.begin({eeRead, eeWrite}); g_prevFaults = 0; }
struct EeInit { EeInit() { memset(g_ee, 0xFF, sizeof g_ee); flogBegin(); } } g_eeInit;
float simLowestCell();
int16_t flogValue(uint8_t bit) {   // one context value per record
    if (bit == 3 || bit == 2 || bit == 10) return static_cast<int16_t>(simLowestCell() * 1000.0f + 0.5f);   // cell mV
    if (bit == 6) { const float a = g_w.in.lemA + g_w.in.lemB; return static_cast<int16_t>(a > 32000 ? 32000 : (a < -32000 ? -32000 : a)); }  // amps
    return 0;
}
void flogTick() {
    const uint32_t now = g_w.core.out().faults, up = now & ~g_prevFaults, down = g_prevFaults & ~now;
    for (uint8_t b = 0; b < 32; b++) {
        if (up & (1u << b))   g_flog.add(BmsCore::LogKind::FaultSet, b, g_w.now, flogValue(b));
        if (down & (1u << b)) g_flog.add(BmsCore::LogKind::FaultClear, b, g_w.now, flogValue(b));
    }
    g_prevFaults = now;
}
int flogTorn() {   // slots with the record magic but a bad CRC (cut off mid-write)
    int n = 0;
    for (int i = 0; i < BmsCore::FaultLog::kSlots; i++) {
        uint8_t b[BmsCore::FaultLog::kRecSize];
        const int a = BmsCore::FaultLog::kBase + i * BmsCore::FaultLog::kRecSize;
        for (int k = 0; k < BmsCore::FaultLog::kRecSize; k++) b[k] = g_ee[a + k];
        if (b[0] == 0xF7 && (b[16] | (b[17] << 8)) != BmsCore::FaultLog::crc16(b, 16)) n++;
    }
    return n;
}

float simLowestCell() {
    float lo = 9;
    for (int b = 0; b < kBoards; b++) for (int c = 0; c < kBoardCells[b]; c++) if (g_w.cell[b][c] < lo) lo = g_w.cell[b][c];
    return lo;
}
float simTruthPct() {
    float p = (simLowestCell() - kTruth0V) / (kTruth100V - kTruth0V) * 100.0f;
    return p < 0 ? 0 : (p > 100 ? 100 : p);
}

// One 10 ms tick: plant + Core (BmsSim), sim cell drain, then the estimator.
void tick() {
    const bool drop = g_lemDropMs && g_w.now + 10 < g_lemDropMs;
    const bool fitted = g_w.currentFitted;
    if (drop) g_w.currentFitted = false;
    g_w.step(10);
    flogTick();
    g_w.currentFitted = fitted;
    if (g_lemDropMs && !drop) g_lemDropMs = 0;
    const BmsCore::Outputs &o = g_w.core.out();
    const bool closed = (o.mainPos && o.mainNeg) || g_w.welded;
    if (g_drain && closed && g_w.packA != 0) {   // load current, mains closed; the charger has its own cell model in BmsSim
        const float dv = g_w.packA * 0.01f / 3600.0f / kSimCapAh * (kTruth100V - kTruth0V);
        for (int b = 0; b < kBoards; b++) for (int c = 0; c < kBoardCells[b]; c++) g_w.cell[b][c] -= dv;
    }
    BmsCore::SocIn si;
    si.nowMs = g_w.now;
    si.currentValid = g_w.in.currentValid && g_w.in.lemSplitValid;
    si.packA = g_w.in.lemA + g_w.in.lemB;          // + = discharge
    si.atChargeLimit = g_w.core.out().chargeWhy && strcmp(g_w.core.out().chargeWhy, "at charge limit") == 0;
    si.chargePct = g_w.core.cfg.chargePct;
    float mn = 0;
    for (int i = 0; i < BmsCore::kMaxStrings; i++) {
        const BmsCore::StringIn &s = g_w.in.s[i];
        if (s.used && s.cellsOk && (mn == 0 || s.minV < mn)) mn = s.minV;
    }
    si.minCellV = mn;
    si.lowCutV = g_w.core.cfg.lowCutV;
    g_anchorNow = si.atChargeLimit;
    g_soc.step(si);
}
const char *socState() { return !g_soc.out().known ? "unknown" : (g_anchorNow ? "anchored" : "counting"); }

struct Buf {
    int o = 0;
    void f(const char *fmt, ...) __attribute__((format(printf, 2, 3))) {
        if (o >= static_cast<int>(sizeof g_buf) - 1) return;
        va_list ap; va_start(ap, fmt);
        const int n = vsnprintf(g_buf + o, sizeof g_buf - o, fmt, ap);
        va_end(ap);
        if (n > 0) o += n;
        if (o > static_cast<int>(sizeof g_buf) - 1) o = sizeof g_buf - 1;
    }
    void str(const char *s) {  // JSON-escaped string
        f("\"");
        for (; *s; s++) {
            if (*s == '"' || *s == '\\') f("\\%c", *s);
            else if (*s == '\n') f("\\n");
            else f("%c", *s);
        }
        f("\"");
    }
};
}  // namespace

#define EXPORT extern "C" __attribute__((visibility("default")))

EXPORT void sim_reset() { g_w.reset(); socReset(); flogBegin(); }   // a reset is a reboot: new boot no., EEPROM kept
EXPORT void sim_step(int ms) { for (int t = 0; t < ms; t += 10) tick(); }
EXPORT void sim_set_cell(int b, int c, float v) { if (b >= 0 && b < kBoards && c >= 0 && c < kBoardCells[b]) g_w.cell[b][c] = v; }
EXPORT float sim_get_cell(int b, int c) { return (b >= 0 && b < kBoards && c >= 0 && c < 24) ? g_w.cell[b][c] : 0; }
EXPORT void sim_set_board(int b, float v) { if (b >= 0 && b < kBoards) for (int c = 0; c < kBoardCells[b]; c++) g_w.cell[b][c] = v; }
EXPORT void sim_set_silent(int b, int on) { if (b >= 0 && b < kBoards) g_w.silent[b] = on; }
EXPORT void sim_set_temps(float mn, float mx) { g_w.tMin = mn; g_w.tMax = mx; }
EXPORT void sim_set_temps_valid(int on) { g_w.tempsValid = on; }
EXPORT void sim_set_pack_amps(float a) { g_w.packA = a; }
EXPORT void sim_set_lem_share(float pctA) { g_w.lemShareA = pctA / 100.0f; }  // % of total through LEM A
EXPORT void sim_set_low_cut(float v) { g_w.core.cfg.lowCutV = v; }             // L setting (what-if)
EXPORT void sim_set_current_fitted(int on) { g_w.currentFitted = on; }
EXPORT void sim_set_bus_force(float v) { g_w.busForce = v; }   // < 0 = plant model
EXPORT void sim_set_link(int up) { g_w.linkUp = up; }
EXPORT void sim_set_precharge_open(int on) { g_w.preOpenCircuit = on; }
EXPORT void sim_set_welded(int on) { g_w.welded = on; }
EXPORT void sim_set_key(int on) { g_w.keyOn = on; }
EXPORT void sim_set_charge(int on) { g_w.chargeReq = on; }
EXPORT void sim_set_charger_online(int on) { g_w.chargerOnline = on; }
EXPORT void sim_set_charge_pct(int pct) { g_w.core.cfg.chargePct = pct; }
EXPORT void sim_set_deadband_mv(float mv) { g_deadband = mv / 1000.0f; }
EXPORT float sim_ceiling_for_pct(int pct) { return BmsCore::ceilingForPct(pct); }
EXPORT int  sim_board_cells(int b) { return (b >= 0 && b < kBoards) ? kBoardCells[b] : 0; }

// SoC estimator (BmsCore::Soc on LEM A + LEM B), sim only.
EXPORT float sim_soc_pct() { return g_soc.out().pct; }            // -1 = unknown
EXPORT float sim_soc_used_ah() { return g_soc.out().usedAh; }
EXPORT int   sim_soc_known() { return g_soc.out().known; }
EXPORT int   sim_soc_drift() { return g_soc.out().drift; }
EXPORT const char *sim_soc_state() { return socState(); }        // unknown / anchored / counting
EXPORT const char *sim_soc_why() { return g_soc.out().why; }
EXPORT float sim_soc_cap_ah() { return g_soc.cfg.capAh; }
EXPORT float sim_soc_truth_pct() { return simTruthPct(); }
EXPORT void  sim_set_soc_drain(int on) { g_drain = on; }
EXPORT void  sim_lem_dropout(int ms) { g_lemDropMs = ms > 0 ? g_w.now + static_cast<uint32_t>(ms) : 0; }

// Fault log (BmsCore::FaultLog on simulated EEPROM), sim only.
EXPORT void sim_power_cycle() { g_w.reset(); socReset(); flogBegin(); }      // RAM lost, EEPROM kept
EXPORT void sim_flog_clear() { g_flog.clear(); flogBegin(); }
EXPORT void sim_flog_tear() {     // power pulled mid-write: next slot gets only its first 12 bytes
    BmsCore::LogRec r;
    int slot = 0;
    if (g_flog.get(0, r)) {
        for (int i = 0; i < BmsCore::FaultLog::kSlots; i++) {
            const int a = BmsCore::FaultLog::kBase + i * BmsCore::FaultLog::kRecSize;
            uint32_t q = 0; for (int k = 0; k < 4; k++) q |= static_cast<uint32_t>(g_ee[a + 4 + k]) << (8 * k);
            if (g_ee[a] == 0xF7 && q == r.seq) { slot = (i + 1) % BmsCore::FaultLog::kSlots; break; }
        }
    }
    const int a = BmsCore::FaultLog::kBase + slot * BmsCore::FaultLog::kRecSize;
    const uint8_t part[12] = {0xF7, 1, 6, 0, 0xEE, 0xEE, 0xEE, 0xEE, 1, 0, 0, 0};
    for (int k = 0; k < 12; k++) eeWrite(a + k, part[k]);
    sim_power_cycle();
}
EXPORT int  sim_ee_size() { return sizeof g_ee; }
EXPORT int  sim_ee_get(int a) { return eeRead(a); }
EXPORT void sim_ee_set(int a, int v) { if (a >= 0 && a < static_cast<int>(sizeof g_ee)) g_ee[a] = static_cast<uint8_t>(v); }   // page restore only
EXPORT void sim_ee_loaded() { flogBegin(); g_eeDirty = false; }
EXPORT int  sim_ee_dirty() { const int d = g_eeDirty; g_eeDirty = false; return d; }
EXPORT const char *sim_flog_json() {
    Buf b;
    b.f("{\"boot\":%u,\"count\":%d,\"slots\":%d,\"written\":%d,\"maxPerBoot\":%d,\"limited\":%d,\"torn\":%d,\"eeWrites\":%u,\"base\":%d,\"end\":%d,\"recs\":[",
        g_flog.boot(), g_flog.count(), BmsCore::FaultLog::kSlots, g_flog.writtenThisBoot(), g_flog.maxPerBoot, g_flog.limited(),
        flogTorn(), static_cast<unsigned>(g_eeWrites), BmsCore::FaultLog::kBase, BmsCore::FaultLog::kEnd);
    BmsCore::LogRec r;
    for (int i = 0; g_flog.get(i, r); i++) {
        b.f("%s{\"seq\":%u,\"boot\":%u,\"kind\":%d,\"code\":%u,\"ms\":%u,\"value\":%d,\"name\":", i ? "," : "",
            static_cast<unsigned>(r.seq), r.boot, static_cast<int>(r.kind), r.code, static_cast<unsigned>(r.ms), r.value);
        if (r.kind == BmsCore::LogKind::Note) b.str(r.code == BmsCore::NOTE_LIMIT ? "log limit reached" : "note");
        else b.str(r.code < 32 ? BmsCore::faultName(1u << r.code) : "?");
        b.f("}");
    }
    b.f("]}");
    return g_buf;
}

// Live state as JSON (pointer into wasm memory, NUL-terminated).
EXPORT const char *sim_state_json() {
    const BmsCore::Outputs &o = g_w.core.out();
    const BmsCore::Config &c = g_w.core.cfg;
    Buf b;
    b.f("{\"t\":%u,\"state\":", static_cast<unsigned>(g_w.now)); b.str(BmsCore::stateName(o.state));
    b.f(",\"pre\":%d,\"mainPos\":%d,\"mainNeg\":%d", o.pre, o.mainPos, o.mainNeg);
    b.f(",\"chargeEnable\":%d,\"chargeV\":%.1f,\"chargeA\":%.2f,\"ceilingV\":%.4f", o.chargeEnable, o.chargeV, o.chargeA, o.ceilingV);
    b.f(",\"chargeWhy\":"); b.str(o.chargeWhy ? o.chargeWhy : "");
    b.f(",\"kellyPct\":%.1f,\"kellyLimp\":%d,\"kellyRegenOk\":%d", o.kellyPct, o.kellyLimp, o.kellyRegenOk);
    b.f(",\"kellyWhy\":"); b.str(o.kellyWhy ? o.kellyWhy : "");
    b.f(",\"regenWhy\":"); b.str(o.regenWhy ? o.regenWhy : "");
    b.f(",\"lemA\":%.1f,\"lemB\":%.1f,\"lemDiffA\":%.1f,\"lemImbalance\":%d", g_w.in.lemA, g_w.in.lemB, o.lemDiffA, o.lemImbalance);
    b.f(",\"dcfg\":{\"relSagStartV\":%.3f,\"relSagZeroV\":%.3f,\"hardFloorV\":%.3f,\"hardFloorMs\":%u,\"packSagBandV\":%.3f,"
        "\"lowCutMs\":%u,\"restA\":%.0f,\"imbalanceA\":%.0f,\"imbalanceMs\":%u,\"imbalancePct\":%.0f}",
        c.relSagStartV, c.relSagZeroV, c.hardFloorV, static_cast<unsigned>(c.hardFloorMs), c.packSagBandV,
        static_cast<unsigned>(c.lowCutMs), c.restA, c.imbalanceA, static_cast<unsigned>(c.imbalanceMs), c.imbalancePct);
    b.f(",\"minCellV\":%.3f,\"avgCellV\":%.3f,\"underLoad\":%d", o.minCellV, o.avgCellV, o.underLoad);
    b.f(",\"kcfg\":{\"derateLowV\":%.3f,\"derateTempC\":%.1f,\"maxDischargeA\":%.0f,\"overcurrentMs\":%u,\"instantTripA\":%.0f,\"ceil100V\":%.3f}",
        c.derateLowV, c.derateTempC, c.maxDischargeA, static_cast<unsigned>(c.overcurrentMs), c.instantTripA, BmsCore::kCeil100V);
    b.f(",\"faults\":%u,\"latched\":%u,\"busV\":%.2f,\"lowestStringV\":%.2f", static_cast<unsigned>(o.faults),
        static_cast<unsigned>(o.latched), g_w.busV, o.lowestStringV);
    b.f(",\"packA\":%.2f,\"chargerA\":%.2f", g_w.in.packA, g_w.chargeSoakA);
    b.f(",\"stringV\":[%.2f,%.2f,%.2f,%.2f]", g_w.stringV(0), g_w.stringV(1), g_w.stringV(2), g_w.stringV(3));
    b.f(",\"silent\":[");
    for (int i = 0; i < kBoards; i++) b.f("%s%d", i ? "," : "", g_w.silent[i] ? 1 : 0);
    b.f("],\"linkUp\":%d,\"welded\":%d,\"preOpen\":%d,\"keyOn\":%d,\"chargeReq\":%d,\"tMin\":%.1f,\"tMax\":%.1f", g_w.linkUp, g_w.welded, g_w.preOpenCircuit, g_w.keyOn, g_w.chargeReq, g_w.tMin, g_w.tMax);
    {
        const BmsCore::SocOut &so = g_soc.out();
        b.f(",\"soc\":{\"state\":"); b.str(socState());
        b.f(",\"known\":%d,\"pct\":%.2f,\"usedAh\":%.3f,\"drift\":%d,\"anchorPct\":%.0f,\"capAh\":%.0f,\"truthPct\":%.2f,\"drain\":%d,\"lemDrop\":%d,\"why\":",
            so.known, so.pct, so.usedAh, so.drift, g_soc.anchorPct(), g_soc.cfg.capAh, simTruthPct(), g_drain, g_lemDropMs ? 1 : 0);
        b.str(so.why ? so.why : "");
        b.f(",\"truth0V\":%.2f,\"truth100V\":%.2f}", kTruth0V, kTruth100V);
    }
    b.f(",\"faultNames\":[");
    for (int i = 0; i < 13; i++) { if (i) b.f(","); b.str(BmsCore::faultName(1u << i)); }
    b.f("],\"cfg\":{\"chargePct\":%d,\"stringCells\":%d,\"busMaxV\":%.1f,\"lowCutV\":%.3f,\"cellMaxV\":%.3f,"
        "\"overTempC\":%.1f,\"chargeMinC\":%.1f,\"chargeMaxC\":%.1f,\"overA\":%.0f,\"silentMs\":%u,\"linkMs\":%u}}",
        c.chargePct, c.stringCells, c.busMaxV, c.lowCutV, c.cellMaxV, c.overTempC, c.chargeMinTempC, c.chargeMaxTempC,
        c.overcurrentA, static_cast<unsigned>(c.stringSilentMs), static_cast<unsigned>(c.linkStaleMs));
    return g_buf;
}

EXPORT int sim_log_count() { return g_w.core.logCount(); }
EXPORT unsigned sim_log_ms(int i) { return g_w.core.logAt(i).ms; }
EXPORT const char *sim_log_text(int i) { return g_w.core.logAt(i).text; }

// Balance plan for Teensy A's string-1 bus, exact would-be 0x300/0x310 bytes.
EXPORT const char *sim_plan_text(int s) {
    if (s < 0 || s >= kStrings) s = 0;
    static const char *kName[kStrings] = {"Teensy A CAN3 (M2 + M4)", "Teensy A CAN1 (M3 + M4)",
                                          "Teensy B CAN3 (M2 + M4)", "Teensy B CAN1 (M3 + M4)"};
    const FactoryBalance::PlanInput in = g_w.balanceInput(g_deadband, s);
    const FactoryBalance::Plan p = FactoryBalance::evaluate(in);
    Buf b;
    FactoryBalance::printPlan([&](const char *line) { b.f("%s\n", line); }, in, p, kName[s]);
    b.f("  (live TX gated: FACTORY_BALANCE_MAP_VERIFIED=0, BMS_CAP_BALANCE_TX=0)\n");
    return g_buf;
}

// Scripted pass/fail table (same as scripts/sim_bms.cpp). Leaves g_w untouched.
EXPORT const char *sim_run_scenarios() {
    static Result r[kScenarios];
    const int n = runAll(r);
    Buf b;
    b.f("[");
    for (int i = 0; i < n; i++) {
        if (i) b.f(",");
        b.f("{\"name\":"); b.str(r[i].name); b.f(",\"expect\":"); b.str(r[i].expect);
        b.f(",\"limitMs\":%u,\"gotMs\":%d,\"pass\":%d,\"note\":", static_cast<unsigned>(r[i].limitMs),
            static_cast<int>(r[i].gotMs), r[i].pass);
        b.str(r[i].note);
        b.f("}");
    }
    b.f("]");
    return g_buf;
}
