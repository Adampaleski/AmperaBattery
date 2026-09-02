#include "ContactorSeq.h"
#include <Arduino.h>
#include "BicmDecode.h"
#include "BmsCapabilities.h"
#include "Config.h"

namespace ContactorSeq {

namespace {

State g_state           = State::Open;
bool g_deadman          = false;
bool g_sawStablePack    = false;
uint32_t g_stateMs      = 0;
uint32_t g_lastPrintMs  = 0;
const char *g_faultWhy  = "";

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
    const bool drive = driveEnabled();
    digitalWrite(Pins::PRECHARGE, (drive && want.precharge) ? HIGH : LOW);
    digitalWrite(Pins::MAIN_POS,  (drive && want.mainPos)   ? HIGH : LOW);
    digitalWrite(Pins::MAIN_NEG,  (drive && want.mainNeg)   ? HIGH : LOW);
}

#if HV_ANALOG_ENABLE
float readHvBusV() {
    const int raw = analogRead(Pins::HV_SENSE);
    return (static_cast<float>(raw) / 1023.0f) * kHvAdcVref * kHvDividerRatio;
}
#endif

void printTransition(State from, State to, const char *why) {
    const Outputs want = outputsFor(to);
    SERIALCONSOLE.print(F("CONTACTOR "));
    SERIALCONSOLE.print(stateName(from));
    SERIALCONSOLE.print(F(" -> "));
    SERIALCONSOLE.print(stateName(to));
    if (why && why[0]) {
        SERIALCONSOLE.print(F("  reason="));
        SERIALCONSOLE.print(why);
    }
    SERIALCONSOLE.print(F("  want PRE="));
    SERIALCONSOLE.print(want.precharge ? 1 : 0);
    SERIALCONSOLE.print(F(" MAIN+="));
    SERIALCONSOLE.print(want.mainPos ? 1 : 0);
    SERIALCONSOLE.print(F(" MAIN-="));
    SERIALCONSOLE.print(want.mainNeg ? 1 : 0);
    SERIALCONSOLE.print(F("  drive="));
    SERIALCONSOLE.println(driveEnabled() ? 1 : 0);
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
    if (g_state == State::Open || g_state == State::Fault) {
        return;
    }
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
    SERIALCONSOLE.print(F("Contactors: SM ready  PRE=OUT2/12  MAIN+=OUT1/11  MAIN-=OUT4/21  drv="));
    SERIALCONSOLE.print(BMS_CAP_CONTACTOR_DRV);
    SERIALCONSOLE.println(F("  ('e' dry-runs intended pins)"));
}

void tick() {
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
                SERIALCONSOLE.println(F("ms  want PRE=1 MAIN+=0 MAIN-=0"));
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
            applyPins(outputsFor(State::Fault));
            break;
    }

    applyPins(outputsFor(g_state));
}

void setDeadman(bool on) {
    g_deadman = on;
    if (!on) {
        if (g_state == State::Fault) {
            openAll("reset");
        } else if (g_state != State::Open) {
            openAll("deadman");
        }
    }
}

void toggleDeadman() {
    setDeadman(!g_deadman);
    SERIALCONSOLE.print(F("Dead-man "));
    SERIALCONSOLE.println(g_deadman ? F("ON (sequence may close)") : F("OFF (all open)"));
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
        default:
            return "?";
    }
}

Outputs intended() { return outputsFor(g_state); }

Outputs driven() {
    if (!driveEnabled()) {
        return Outputs{};
    }
    return outputsFor(g_state);
}

void printStatus() {
    const Outputs want = intended();
    const Outputs drv  = driven();
    SERIALCONSOLE.print(F("CONTACTOR state="));
    SERIALCONSOLE.print(stateName(g_state));
    SERIALCONSOLE.print(F("  deadman="));
    SERIALCONSOLE.print(g_deadman ? 1 : 0);
    SERIALCONSOLE.print(F("  flag="));
    SERIALCONSOLE.print(BMS_CAP_CONTACTOR_DRV);
    SERIALCONSOLE.print(F("  telemetry="));
    SERIALCONSOLE.println(TELEMETRY_ONLY);
    SERIALCONSOLE.print(F("  want PRE="));
    SERIALCONSOLE.print(want.precharge ? 1 : 0);
    SERIALCONSOLE.print(F(" MAIN+="));
    SERIALCONSOLE.print(want.mainPos ? 1 : 0);
    SERIALCONSOLE.print(F(" MAIN-="));
    SERIALCONSOLE.print(want.mainNeg ? 1 : 0);
    SERIALCONSOLE.print(F("  drive PRE="));
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
#else
    SERIALCONSOLE.println(F("  gate=timer 5s (HV_ANALOG_ENABLE=0)"));
#endif
#if !BMS_CAP_CONTACTOR_DRV
    SERIALCONSOLE.println(F("  (coils not driven — BMS_CAP_CONTACTOR_DRV=0)"));
#endif
#if TELEMETRY_ONLY
    SERIALCONSOLE.println(F("  (TELEMETRY_ONLY=1 — physical outputs forced open)"));
#endif
}

}  // namespace ContactorSeq
