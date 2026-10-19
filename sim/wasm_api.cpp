// WebAssembly API around BmsSim::World + BmsCore::Core for tools/bms_sim.html.
// Same headers as the Teensy firmware and scripts/sim_bms.cpp: no logic here.
// Build: scripts/build_bms_sim_html.sh (zig c++ -target wasm32-wasi, reactor).
#include "BmsSim.h"

using namespace BmsSim;

namespace {
World g_w;
char  g_buf[16384];
float g_deadband = 0.010f;

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

EXPORT void sim_reset() { g_w.reset(); }
EXPORT void sim_step(int ms) { for (int t = 0; t < ms; t += 10) g_w.step(10); }
EXPORT void sim_set_cell(int b, int c, float v) { if (b >= 0 && b < kBoards && c >= 0 && c < kBoardCells[b]) g_w.cell[b][c] = v; }
EXPORT float sim_get_cell(int b, int c) { return (b >= 0 && b < kBoards && c >= 0 && c < 24) ? g_w.cell[b][c] : 0; }
EXPORT void sim_set_board(int b, float v) { if (b >= 0 && b < kBoards) for (int c = 0; c < kBoardCells[b]; c++) g_w.cell[b][c] = v; }
EXPORT void sim_set_silent(int b, int on) { if (b >= 0 && b < kBoards) g_w.silent[b] = on; }
EXPORT void sim_set_temps(float mn, float mx) { g_w.tMin = mn; g_w.tMax = mx; }
EXPORT void sim_set_temps_valid(int on) { g_w.tempsValid = on; }
EXPORT void sim_set_pack_amps(float a) { g_w.packA = a; }
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
    b.f(",\"faults\":%u,\"latched\":%u,\"busV\":%.2f,\"lowestStringV\":%.2f", static_cast<unsigned>(o.faults),
        static_cast<unsigned>(o.latched), g_w.busV, o.lowestStringV);
    b.f(",\"packA\":%.2f,\"chargerA\":%.2f", g_w.in.packA, g_w.chargeSoakA);
    b.f(",\"stringV\":[%.2f,%.2f,%.2f,%.2f]", g_w.stringV(0), g_w.stringV(1), g_w.stringV(2), g_w.stringV(3));
    b.f(",\"silent\":[");
    for (int i = 0; i < kBoards; i++) b.f("%s%d", i ? "," : "", g_w.silent[i] ? 1 : 0);
    b.f("],\"linkUp\":%d,\"welded\":%d,\"preOpen\":%d,\"keyOn\":%d,\"chargeReq\":%d,\"tMin\":%.1f,\"tMax\":%.1f", g_w.linkUp, g_w.welded, g_w.preOpenCircuit, g_w.keyOn, g_w.chargeReq, g_w.tMin, g_w.tMax);
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
