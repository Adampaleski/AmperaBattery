#include "SocRun.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <string.h>
#include "Config.h"
#include "StringConfig.h"
#include "ContactorSeq.h"
#include "CurrentSense.h"
#include "Settings.h"
#include "SocEstimator.h"
#include "SocStore.h"

// From src/Link.h (declared here so SocRun stays free of FlexCAN for host tests).
namespace Link { bool remoteLemB(float *amps); }

namespace SocRun {

#if DUAL_STRING && ROLE_IS_A
namespace {
// Bank current = LEM A (strings 1+2, read here) + LEM B (strings 3+4, read on
// Teensy B, sent on CAN2 0x6A5, include/LemLink.h). Both must be valid and fresh
// in the same tick, otherwise the tick is "current lost" (fail closed: no
// half-bank counting; SocEstimator goes UNKNOWN after 2 s).
bool g_lemAOk = false, g_lemBOk = false;

BmsCore::Soc g_soc;
BmsCore::SocStore g_store;
bool g_started = false;
bool g_anchorNow = false;
bool g_restored = false;     // a saved count was loaded at boot
bool g_anchoredThisBoot = false;
bool g_wasKnown = false;     // count known at some point this boot (so a loss gets saved as known=0)
BmsCore::Mode g_prevMode = BmsCore::Mode::Idle;

uint8_t eeRead(int a) { return EEPROM.read(a); }
void    eeWrite(int a, uint8_t v) { EEPROM.update(a, v); }   // skips unchanged bytes

const char *state() {
    if (!g_started) return "off";
    if (!g_soc.out().known) return "unknown";
    if (g_anchorNow) return "anchored";
    return (g_restored && !g_anchoredThisBoot) ? "restored" : "counting";
}
bool pctOk() { return g_started && g_soc.out().pct >= 0; }   // pct < 0 while capAh unknown
void printQ(Print &out, const char *s) {
    out.print('"');
    for (const char *p = s; *p; p++) { if (*p == '"' || *p == '\\') out.print('\\'); out.print(*p); }
    out.print('"');
}
}  // namespace

void begin() {
    BmsCore::SocConfig c;          // capAh 0 = UNKNOWN (UNVERIFIED)
    g_store.begin({eeRead, eeWrite});
    BmsCore::SocSaved sv;
    g_restored = g_store.load(sv);   // false = blank / bad CRC / known=0: stay UNKNOWN (fail closed)
    if (g_restored && sv.capAh > 0) c.capAh = sv.capAh;   // learned capacity only, never a guess
    g_soc.reset(c);
    if (g_restored) g_soc.restore(sv.usedAh, sv.anchorPct);
    g_wasKnown = g_restored;
    g_anchoredThisBoot = false;
    g_prevMode = BmsCore::Mode::Idle;
    g_anchorNow = false;
    g_started = true;
}

void tick() {
    if (!g_started) return;
    const BmsCore::Outputs &o = ContactorSeq::coreOut();
    BmsCore::SocIn si;
    si.nowMs = millis();
    float lemB = 0;
    g_lemAOk = CurrentSense::valid();
    g_lemBOk = Link::remoteLemB(&lemB);
    si.currentValid = g_lemAOk && g_lemBOk;
    si.packA = si.currentValid ? CurrentSense::amps() + lemB : 0;   // + = discharge
    si.atChargeLimit = o.chargeWhy && strcmp(o.chargeWhy, "at charge limit") == 0;
    si.chargePct = Settings::chargePct();
    si.minCellV = o.minCellV;
    si.lowCutV = Settings::lowCutoffV();
    g_anchorNow = si.atChargeLimit;
    if (g_anchorNow) g_anchoredThisBoot = true;
    const BmsCore::SocOut &so = g_soc.step(si);
    // Persist (SocStore wear rules). Force on the DRIVE/CHARGE -> IDLE edge
    // (key off / unplugged: BmsCore opens the mains). No KEY-ON pin is read yet,
    // so this edge is BmsCore's wake, not a power-loss detector (UNVERIFIED gap).
    const bool sleepEdge = g_prevMode != BmsCore::Mode::Idle && o.mode == BmsCore::Mode::Idle;
    g_prevMode = o.mode;
    if (so.known) g_wasKnown = true;
    if (!g_wasKnown) return;          // never known this boot: nothing to save, no wear
    BmsCore::SocSaved cur;
    cur.known = so.known; cur.drift = so.drift;
    cur.usedAh = so.known ? so.usedAh : 0;
    cur.anchorPct = so.known ? g_soc.anchorPct() : 100;
    cur.capAh = g_soc.cfg.capAh;
    g_store.update(si.nowMs, cur, sleepEdge);
}

void printStatus() {
    const BmsCore::SocOut &s = g_soc.out();
    SERIALCONSOLE.print(F("SOC ")); SERIALCONSOLE.print(state());
    if (s.known) { SERIALCONSOLE.print(F("  used ")); SERIALCONSOLE.print(s.usedAh, 3); SERIALCONSOLE.print(F(" Ah")); }
    SERIALCONSOLE.print(F("  pct "));
    if (s.pct >= 0) SERIALCONSOLE.print(s.pct, 1); else SERIALCONSOLE.print(F("unknown"));
    if (s.drift) SERIALCONSOLE.print(F("  DRIFT"));
    SERIALCONSOLE.print(F("  (")); SERIALCONSOLE.print(s.why); SERIALCONSOLE.print(')');
    SERIALCONSOLE.print(F("  LEM A ")); SERIALCONSOLE.print(g_lemAOk ? F("ok") : F("--"));
    SERIALCONSOLE.print(F(" LEM B ")); SERIALCONSOLE.print(g_lemBOk ? F("ok") : F("--"));
    if (!(g_lemAOk && g_lemBOk)) SERIALCONSOLE.print(F(" (both needed, not counting)"));
    SERIALCONSOLE.print(F("  EEPROM ")); SERIALCONSOLE.print(BmsCore::SocStore::kBase); SERIALCONSOLE.print('-');
    SERIALCONSOLE.print(BmsCore::SocStore::kEnd - 1);
    SERIALCONSOLE.print(g_restored ? F(" restored at boot") : F(" nothing restored"));
    SERIALCONSOLE.print(F(", saves ")); SERIALCONSOLE.print(g_store.writes());
    if (g_store.limited()) SERIALCONSOLE.print(F(" (limit reached)"));
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
    out.print(F(",\"halfBank\":false"));   // A + B summed; never counts half the bank
    out.print(F(",\"lemA\":")); out.print(g_lemAOk ? F("true") : F("false"));
    out.print(F(",\"lemB\":")); out.print(g_lemBOk ? F("true") : F("false"));
    out.print(F(",\"capAh\":")); if (g_soc.cfg.capAh > 0) out.print(g_soc.cfg.capAh, 1); else out.print(F("null"));
    out.print(F(",\"restored\":")); out.print(g_restored ? F("true") : F("false"));
    out.print(F(",\"saves\":")); out.print(g_store.writes());
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
