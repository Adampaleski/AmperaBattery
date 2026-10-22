#include "SocRun.h"
#include <Arduino.h>
#include <string.h>
#include "Config.h"
#include "StringConfig.h"
#include "ContactorSeq.h"
#include "CurrentSense.h"
#include "Settings.h"
#include "SocEstimator.h"

namespace SocRun {

#if DUAL_STRING && ROLE_IS_A
namespace {
// LEM B (strings 3+4) is read on Teensy B; sharing it over the link is not
// built. Flip to 1 only when Link carries B's amps and they are summed below.
constexpr bool kHaveLemB = false;

BmsCore::Soc g_soc;
bool g_started = false;
bool g_anchorNow = false;

const char *state() {
    if (!g_started) return "off";
    if (!g_soc.out().known) return "unknown";
    return g_anchorNow ? "anchored" : "counting";
}
bool pctOk() { return g_started && kHaveLemB && g_soc.out().pct >= 0; }
void printQ(Print &out, const char *s) {
    out.print('"');
    for (const char *p = s; *p; p++) { if (*p == '"' || *p == '\\') out.print('\\'); out.print(*p); }
    out.print('"');
}
}  // namespace

void begin() {
    BmsCore::SocConfig c;          // capAh 0 = UNKNOWN (UNVERIFIED)
    g_soc.reset(c);
    g_anchorNow = false;
    g_started = true;
}

void tick() {
    if (!g_started) return;
    const BmsCore::Outputs &o = ContactorSeq::coreOut();
    BmsCore::SocIn si;
    si.nowMs = millis();
    si.currentValid = CurrentSense::valid();
    si.packA = CurrentSense::amps();   // LEM A only until kHaveLemB (+ = discharge)
    si.atChargeLimit = o.chargeWhy && strcmp(o.chargeWhy, "at charge limit") == 0;
    si.chargePct = Settings::chargePct();
    si.minCellV = o.minCellV;
    si.lowCutV = Settings::lowCutoffV();
    g_anchorNow = si.atChargeLimit;
    g_soc.step(si);
}

void printStatus() {
    const BmsCore::SocOut &s = g_soc.out();
    SERIALCONSOLE.print(F("SOC ")); SERIALCONSOLE.print(state());
    if (s.known) { SERIALCONSOLE.print(F("  used ")); SERIALCONSOLE.print(s.usedAh, 3); SERIALCONSOLE.print(F(" Ah")); }
    SERIALCONSOLE.print(F("  pct "));
    if (s.pct >= 0) SERIALCONSOLE.print(s.pct, 1); else SERIALCONSOLE.print(F("unknown"));
    if (s.drift) SERIALCONSOLE.print(F("  DRIFT"));
    SERIALCONSOLE.print(F("  (")); SERIALCONSOLE.print(s.why); SERIALCONSOLE.print(')');
    if (!kHaveLemB) SERIALCONSOLE.print(F("  LEM A only: half bank, socPct held null"));
    SERIALCONSOLE.println();
}

void printSocPct(Print &out) {
    if (pctOk()) out.print(g_soc.out().pct, 1); else out.print(F("null"));
}

void printJson(Print &out) {
    const BmsCore::SocOut &s = g_soc.out();
    out.print(F("\"soc\":{\"available\":true,\"known\":"));
    out.print(s.known ? F("true") : F("false"));
    out.print(F(",\"state\":")); printQ(out, state());
    out.print(F(",\"usedAh\":")); if (s.known) out.print(s.usedAh, 3); else out.print(F("null"));
    out.print(F(",\"pct\":")); printSocPct(out);
    out.print(F(",\"drift\":")); out.print(s.drift ? F("true") : F("false"));
    out.print(F(",\"why\":")); printQ(out, s.why);
    out.print(F(",\"halfBank\":")); out.print(kHaveLemB ? F("false") : F("true"));
    out.print(F(",\"capAh\":")); if (g_soc.cfg.capAh > 0) out.print(g_soc.cfg.capAh, 1); else out.print(F("null"));
    out.print('}');
}
#else
void begin() {}
void tick() {}
void printStatus() { SERIALCONSOLE.println(F("SOC: only on dual-string role A (where BmsCore runs)")); }
void printSocPct(Print &out) { out.print(F("null")); }
void printJson(Print &out) { out.print(F("\"soc\":{\"available\":false}")); }
#endif

}  // namespace SocRun
