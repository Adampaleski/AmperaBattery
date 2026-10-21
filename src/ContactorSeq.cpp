#include "ContactorSeq.h"
#include <Arduino.h>
#include "BicmDecode.h"
#include "BmsCapabilities.h"
#include "Config.h"
#include "Settings.h"
#include "StringConfig.h"
#include "ChargeTx.h"
#include "CurrentSense.h"
#if DUAL_STRING
#include "BmsCore.h"
#include "Link.h"
#include "StringMon.h"
#endif

namespace ContactorSeq {

namespace {

State g_state           = State::Open;
bool g_deadman          = false;
bool g_sawStablePack    = false;
uint32_t g_stateMs      = 0;
uint32_t g_lastPrintMs  = 0;
const char *g_faultWhy  = "";
uint8_t g_retries       = 0;

#if DUAL_STRING
uint8_t systemFaults() {
    uint8_t f = (StringMon::summary(0).faults | StringMon::summary(1).faults) & StringMon::kFaultMask;
#if ROLE_IS_A
    f |= Link::remoteFaults();
#endif
    return f;
}
#if !ROLE_IS_A
// Lowest string voltage across every string in the 4P bank (0 = unknown).
float lowestStringV() {
    float lo = 0.0f;
    auto take = [&](const StringMon::Summary &s) {
        if (s.cells != s.expected || s.v <= 0.0f) { lo = -1.0f; return; }
        if (lo >= 0.0f && (lo == 0.0f || s.v < lo)) lo = s.v;
    };
    take(StringMon::summary(0));
    take(StringMon::summary(1));
#if ROLE_IS_A
    take(Link::remote(0));
    take(Link::remote(1));
#endif
    return lo > 0.0f ? lo : 0.0f;
}
#endif
#endif

bool driveEnabled() {
#if TELEMETRY_ONLY
    return false;
#else
    return BMS_CAP_CONTACTOR_DRV != 0;
#endif
}

Outputs outputsFor(State s) {
    Outputs o;
    switch (s) {
        case State::Precharge:
            o.precharge = true;
            o.mainNeg   = true;  // Tom VoltBMSV2.ino 1195-1198: NEG closes first, then precharge
            break;
        case State::MainClose:
            o.precharge = true;
            o.mainPos   = true;
            o.mainNeg   = true;
            break;
        case State::Closed:
            o.mainPos = true;
            o.mainNeg = true;
            break;
        case State::Open:
        case State::Fault:
        default:
            break;
    }
    return o;
}

void applyPins(const Outputs &want) {
    // Hard gate: FLAG=0 or TELEMETRY_ONLY forces every coil FET LOW.
    // Intended pin wants are printed only; GPIO never follows want when gated.
    if (!driveEnabled()) {
        digitalWrite(Pins::PRECHARGE, LOW);
        digitalWrite(Pins::MAIN_POS, LOW);
        digitalWrite(Pins::MAIN_NEG, LOW);
        return;
    }
    digitalWrite(Pins::PRECHARGE, want.precharge ? HIGH : LOW);
    digitalWrite(Pins::MAIN_POS,  want.mainPos   ? HIGH : LOW);
    digitalWrite(Pins::MAIN_NEG,  want.mainNeg   ? HIGH : LOW);
}

#if HV_ANALOG_ENABLE
float readHvBusV() {
    const int raw = analogRead(Pins::HV_SENSE);
    return (static_cast<float>(raw) / 1023.0f) * kHvAdcVref * kHvDividerRatio;
}
#endif

void printTransition(State from, State to, const char *why) {
    const Outputs want = outputsFor(to);
    if (!driveEnabled()) {
        SERIALCONSOLE.println(
            F("*** CONTACTOR PRINT-ONLY  FLAG=0  DRIVE=OFF  GPIO NOT fired ***"));
    }
    SERIALCONSOLE.print(F("CONTACTOR "));
    SERIALCONSOLE.print(stateName(from));
    SERIALCONSOLE.print(F(" -> "));
    SERIALCONSOLE.print(stateName(to));
    if (why && why[0]) {
        SERIALCONSOLE.print(F("  reason="));
        SERIALCONSOLE.print(why);
    }
    SERIALCONSOLE.print(F("  WANT PRE="));
    SERIALCONSOLE.print(want.precharge ? 1 : 0);
    SERIALCONSOLE.print(F(" MAIN+="));
    SERIALCONSOLE.print(want.mainPos ? 1 : 0);
    SERIALCONSOLE.print(F(" MAIN-="));
    SERIALCONSOLE.print(want.mainNeg ? 1 : 0);
    SERIALCONSOLE.print(F("  DRIVE="));
    SERIALCONSOLE.println(driveEnabled() ? F("ON") : F("OFF"));
}

void enter(State next, const char *why) {
    if (next == g_state) {
        return;
    }
    printTransition(g_state, next, why);
    g_state        = next;
    g_stateMs      = millis();
    g_lastPrintMs  = g_stateMs;
    if (next == State::Fault && why) {
        g_faultWhy = why;
    }
    if (next == State::Open) {
        g_faultWhy = "";
    }
    applyPins(outputsFor(g_state));
}

void openAll(const char *why) {
    enter(State::Open, why);
}

void checkPackFaults() {
    if (g_state == State::Open || g_state == State::Fault || g_state == State::Lockout) {
        return;
    }
#if DUAL_STRING
    if (systemFaults()) enter(State::Fault, "string fault (see STRING/REMOTE lines)");
    return;
#endif
    if (!BicmDecode::packStable()) {
        if (g_sawStablePack) {
            enter(State::Fault, "pack lost");
        }
        return;
    }
    g_sawStablePack = true;
    const float mn = BicmDecode::minCellVoltage();
    const float mx = BicmDecode::maxCellVoltage();
    if (mn > 0.0f && mn < kCellFaultMinV) {
        enter(State::Fault, "undervolt");
        return;
    }
    if (mx > kCellFaultMaxV) {
        enter(State::Fault, "overvolt");
        return;
    }
}

bool analogGateOk(uint32_t elapsed) {
#if HV_ANALOG_ENABLE
    if (elapsed < kPrechargeMinMs) {
        return false;
    }
    const float hv   = readHvBusV();
    const float pack = BicmDecode::packVoltage();
    if (pack > 50.0f) {
        return hv >= pack * kPrechargeRatio;
    }
    return hv >= kPrechargeMinBusV;
#else
    (void)elapsed;
    return false;
#endif
}

}  // namespace

void begin() {
    pinMode(Pins::PRECHARGE, OUTPUT);
    pinMode(Pins::MAIN_POS, OUTPUT);
    pinMode(Pins::MAIN_NEG, OUTPUT);
#if HV_ANALOG_ENABLE
    pinMode(Pins::HV_SENSE, INPUT);
#endif
    g_state        = State::Open;
    g_deadman      = false;
    g_sawStablePack = false;
    g_stateMs      = millis();
    g_faultWhy     = "";
    applyPins(outputsFor(State::Open));
    SERIALCONSOLE.print(F("Contactors: SM ready  PRE=OUT2/12  MAIN+=OUT1/11  MAIN-=OUT4/21  FLAG="));
    SERIALCONSOLE.print(BMS_CAP_CONTACTOR_DRV);
    SERIALCONSOLE.print(F("  DRIVE="));
    SERIALCONSOLE.println(driveEnabled() ? F("ON") : F("OFF (PRINT-ONLY)"));
    if (!driveEnabled()) {
        SERIALCONSOLE.println(
            F("  'e'/'p' print intended pins only — coil FETs stay LOW"));
    }
}

#if DUAL_STRING
namespace {
#if ROLE_IS_A
float busV() {
    if (Settings::simBusV() > 0.0f) return Settings::simBusV();
#if HV_ANALOG_ENABLE
    return readHvBusV();
#else
    return 0.0f;  // no bus measurement: precharge can never pass (fails safe)
#endif
}
#endif

#if ROLE_IS_A
BmsCore::Core g_core;
int g_logShown = 0;

void configureCore() {
    BmsCore::Config &c = g_core.cfg;
    c.lowCutV        = Settings::lowCutoffV();
    c.cellMaxV       = kCellFaultMaxV;
    c.chargePct      = Settings::chargePct();
    c.stringCells    = STRING_EXPECT_S;
    c.busMaxV        = KELLY_MAX_V;
    c.busMarginV     = KELLY_MARGIN_V;
    c.chargeMaxA     = kChargeCurrentDeciA / 10.0f;
    c.stringSilentMs = STRING_SILENT_MS;
    c.linkStaleMs    = LINK_STALE_MS;
    c.pcMinMs        = kPcDualMinMs;
    c.pcTimeoutMs    = kPcDualTimeoutMs;
    c.pcRetryWaitMs  = kPcDualRetryWaitMs;
    c.pcMaxRetries   = kPcDualMaxRetries;
    c.pcRatio        = kPrechargeRatio;
    c.overlapMs      = kPrechargeOverlapMs;
    Settings::applyThresholds(c);   // T overrides (bounded, tighten-only for trips/temps)
#if DUAL_STRING
    Link::setStaleMs(c.linkStaleMs);   // one stale limit for Link + Core (T linkMs)
#endif
}

BmsCore::StringIn toCore(const StringMon::Summary &s, bool remote) {
    BmsCore::StringIn o;
    o.used = true;
    o.remote = remote;
    o.expected = s.expected;
    o.cellsOk = s.cells;
    o.sumV = s.v;
    o.minV = s.minV;
    o.maxV = s.maxV;
    if (s.faults & (StringMon::kFSilent | StringMon::kFNoData)) o.oldestAgeMs = 0xFFFFFFFFu;  // -> F_SILENT
    if (s.faults & (StringMon::kFCellBad | StringMon::kFCount)) o.cellsOk = 0;                // -> F_COUNT
    return o;
}
#endif

void tickDual() {
#if !ROLE_IS_A
    // Role B never sequences contactors; A owns them. Stay open.
    if (g_state != State::Open) enter(State::Open, "role B");
    applyPins(outputsFor(State::Open));
    return;
#else
    configureCore();
    BmsCore::Inputs in;
    in.nowMs = millis();
    in.s[0] = toCore(StringMon::summary(0), false);
    in.s[1] = toCore(StringMon::summary(1), false);
    in.s[2] = toCore(Link::remote(0), true);
    in.s[3] = toCore(Link::remote(1), true);
    in.linkFresh = !(Link::remoteFaults() & StringMon::kFLinkStale);
    in.tempsValid = TEMPS_VERIFIED != 0;  // 0: charge refused (F_TEMP_INVALID), drive unaffected
    in.currentValid = CurrentSense::valid();
    in.packA = CurrentSense::amps();
    const float hv = busV();
    in.busValid = hv > 0.0f || HV_ANALOG_ENABLE;
    in.busV = hv;
    in.keyOn = g_deadman;
    in.chargeReq = ChargeTx::chargeRequested();
    const BmsCore::Outputs &o = g_core.step(in);
    // Print new core events (timestamped fault/transition log).
    const int n = g_core.logCount();
    if (g_logShown > n) g_logShown = 0;
    for (int i = g_logShown; i < n; i++) {
        const BmsCore::Event &e = g_core.logAt(i);
        SERIALCONSOLE.print(F("BMS t="));
        SERIALCONSOLE.print(e.ms);
        SERIALCONSOLE.print(F("ms "));
        SERIALCONSOLE.print(e.text);
        SERIALCONSOLE.println(driveEnabled() ? F("") : F("  [PRINT-ONLY, coils not driven]"));
    }
    g_logShown = n >= BmsCore::kLogSize ? BmsCore::kLogSize : n;
    const State ns = static_cast<State>(static_cast<uint8_t>(o.state));
    if (ns != g_state) {
        g_state = ns;
        g_stateMs = millis();
        if (ns == State::Fault) g_faultWhy = "see BMS log";
    }
    Outputs w;
    w.precharge = o.pre;
    w.mainPos = o.mainPos;
    w.mainNeg = o.mainNeg;
    applyPins(w);  // hard-gated: FETs LOW unless BMS_CAP_CONTACTOR_DRV && !TELEMETRY_ONLY
#endif
}
}  // namespace
#endif

void tick() {
#if DUAL_STRING
    tickDual();
    return;
#endif
    if (!g_deadman && g_state != State::Open && g_state != State::Fault) {
        openAll("deadman");
    }

    checkPackFaults();

    const uint32_t now     = millis();
    const uint32_t elapsed = now - g_stateMs;

    switch (g_state) {
        case State::Open:
            if (g_deadman) {
                enter(State::Precharge, "deadman");
            }
            break;

        case State::Precharge:
            if (!g_deadman) {
                openAll("deadman");
                break;
            }
            if (analogGateOk(elapsed)) {
                enter(State::MainClose, "hv gate");
                break;
            }
            if (elapsed >= kPrechargeTimeMs) {
#if HV_ANALOG_ENABLE
                enter(State::Fault, "precharge timeout");
#else
                enter(State::MainClose, "precharge timer");
#endif
            } else if (now - g_lastPrintMs >= 1000) {
                g_lastPrintMs = now;
                SERIALCONSOLE.print(F("CONTACTOR Precharge t="));
                SERIALCONSOLE.print(elapsed);
                SERIALCONSOLE.print(F("/"));
                SERIALCONSOLE.print(kPrechargeTimeMs);
                SERIALCONSOLE.print(F("ms  WANT PRE=1 MAIN+=0 MAIN-=0  DRIVE="));
                SERIALCONSOLE.println(driveEnabled() ? F("ON") : F("OFF (PRINT-ONLY)"));
            }
            break;

        case State::MainClose:
            if (!g_deadman) {
                openAll("deadman");
                break;
            }
            if (elapsed >= kPrechargeOverlapMs) {
                enter(State::Closed, "mains in");
            }
            break;

        case State::Closed:
            if (!g_deadman) {
                openAll("deadman");
            }
            break;

        case State::Fault:
        case State::RetryWait:
        case State::Lockout:
            applyPins(outputsFor(State::Fault));
            break;
    }

    applyPins(outputsFor(g_state));
}

void setDeadman(bool on) {
    g_deadman = on;
    if (!on) {
        g_retries = 0;
        if (g_state == State::Fault || g_state == State::Lockout || g_state == State::RetryWait) {
            openAll("reset");
        } else if (g_state != State::Open) {
            openAll("deadman");
        }
    }
}

void toggleDeadman() {
    setDeadman(!g_deadman);
    if (!driveEnabled()) {
        SERIALCONSOLE.println(
            F("*** CONTACTOR PRINT-ONLY  FLAG=0  DRIVE=OFF  GPIO NOT fired ***"));
        SERIALCONSOLE.print(F("Dead-man (dry-run) "));
        SERIALCONSOLE.print(g_deadman ? F("ON") : F("OFF"));
        SERIALCONSOLE.println(
            F(" — SM may advance intended pins; coils stay LOW"));
    } else {
        SERIALCONSOLE.print(F("Dead-man "));
        SERIALCONSOLE.println(g_deadman ? F("ON (sequence may close)") : F("OFF (all open)"));
    }
    printStatus();
}

bool deadman() { return g_deadman; }

void trip(const char *why) { enter(State::Fault, why ? why : "fault"); }

State state() { return g_state; }

const char *stateName(State s) {
    switch (s) {
        case State::Open:
            return "Open";
        case State::Precharge:
            return "Precharge";
        case State::MainClose:
            return "MainClose";
        case State::Closed:
            return "Closed";
        case State::Fault:
            return "Fault";
        case State::RetryWait:
            return "RetryWait";
        case State::Lockout:
            return "Lockout";
        default:
            return "?";
    }
}

#if DUAL_STRING && ROLE_IS_A
Outputs intended() {
    Outputs w;
    const BmsCore::Outputs &o = g_core.out();
    w.precharge = o.pre; w.mainPos = o.mainPos; w.mainNeg = o.mainNeg;
    return w;
}
const BmsCore::Outputs &coreOut() { return g_core.out(); }
uint32_t coreFaults() { return g_core.out().faults | g_core.out().latched; }
#else
Outputs intended() { return outputsFor(g_state); }
uint32_t coreFaults() { return 0; }
#endif

Outputs driven() {
    if (!driveEnabled()) {
        return Outputs{};
    }
    return intended();
}

void printStatus() {
    const Outputs want = intended();
    const Outputs drv  = driven();
    if (!driveEnabled()) {
        SERIALCONSOLE.println(
            F("*** CONTACTOR PRINT-ONLY  FLAG=0  DRIVE=OFF  GPIO NOT fired ***"));
    }
    SERIALCONSOLE.print(F("CONTACTOR state="));
    SERIALCONSOLE.print(stateName(g_state));
    SERIALCONSOLE.print(F("  deadman="));
    SERIALCONSOLE.print(g_deadman ? 1 : 0);
    SERIALCONSOLE.print(F("  FLAG="));
    SERIALCONSOLE.print(BMS_CAP_CONTACTOR_DRV);
    SERIALCONSOLE.print(F("  telemetry="));
    SERIALCONSOLE.print(TELEMETRY_ONLY);
    SERIALCONSOLE.print(F("  DRIVE="));
    SERIALCONSOLE.println(driveEnabled() ? F("ON") : F("OFF"));
    SERIALCONSOLE.print(F("  WANT PRE="));
    SERIALCONSOLE.print(want.precharge ? 1 : 0);
    SERIALCONSOLE.print(F(" MAIN+="));
    SERIALCONSOLE.print(want.mainPos ? 1 : 0);
    SERIALCONSOLE.print(F(" MAIN-="));
    SERIALCONSOLE.print(want.mainNeg ? 1 : 0);
    SERIALCONSOLE.print(F("  DRIVEN PRE="));
    SERIALCONSOLE.print(drv.precharge ? 1 : 0);
    SERIALCONSOLE.print(F(" MAIN+="));
    SERIALCONSOLE.print(drv.mainPos ? 1 : 0);
    SERIALCONSOLE.print(F(" MAIN-="));
    SERIALCONSOLE.println(drv.mainNeg ? 1 : 0);
    SERIALCONSOLE.println(F("  map OUT2/12=precharge  OUT1/11=main+  OUT4/21=main-  (VoltBMSV2)"));
    if (g_state == State::Fault && g_faultWhy && g_faultWhy[0]) {
        SERIALCONSOLE.print(F("  fault="));
        SERIALCONSOLE.println(g_faultWhy);
    }
#if HV_ANALOG_ENABLE
    SERIALCONSOLE.print(F("  hv_sense="));
    SERIALCONSOLE.print(readHvBusV(), 1);
    SERIALCONSOLE.print(F("V  pack="));
    SERIALCONSOLE.print(BicmDecode::packVoltage(), 1);
    SERIALCONSOLE.println(F("V"));
#elif DUAL_STRING
    SERIALCONSOLE.print(F("  BmsCore gate: bus>=95% lowest string ("));
#if ROLE_IS_A
    SERIALCONSOLE.print(g_core.out().lowestStringV, 1);
#else
    SERIALCONSOLE.print(lowestStringV(), 1);
#endif
    SERIALCONSOLE.print(F("V) & >=300ms, 3s timeout, 2 retries @30s, weld check 2 s after open"));
#if ROLE_IS_A
    SERIALCONSOLE.print(F("  faults=0x"));
    SERIALCONSOLE.print(coreFaults(), HEX);
    for (int b = 0; b < 13; b++) if (coreFaults() & (1u << b)) { SERIALCONSOLE.print(' '); SERIALCONSOLE.print(BmsCore::faultName(1u << b)); }
#endif
    SERIALCONSOLE.print(F("  bus_src="));
    SERIALCONSOLE.println(Settings::simBusV() > 0.0f ? F("SIM (U cmd)") : F("none (HV_ANALOG_ENABLE=0) -> precharge will time out"));
#else
    SERIALCONSOLE.println(F("  gate=timer 5s (HV_ANALOG_ENABLE=0)"));
#endif
#if !BMS_CAP_CONTACTOR_DRV || TELEMETRY_ONLY
    SERIALCONSOLE.println(
        F("  PRINT-ONLY — WANT pins above; DRIVEN stays 0/0/0 (coils NOT fired)"));
#endif
}

}  // namespace ContactorSeq
