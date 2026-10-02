#include "HubStatus.h"

#include "BicmDecode.h"
#include "BmsCapabilities.h"
#include "Config.h"
#include "ContactorSeq.h"

// Teensy 4.1 Serial4: RX pin 16, TX pin 17 @ 115200 8N1 → ESP32 UART2 hub.
// Payload: one STATUS_SCHEMA v1 JSON object per line (teryx-ev-hub docs/STATUS_SCHEMA.md).
// Do NOT use Serial1 / pins 0/1 — those are Elcon+Kelly CAN2.

namespace HubStatus {

namespace {

constexpr uint32_t kEmitIntervalMs = 1000;
uint32_t g_bootMs = 0;
uint32_t g_lastEmitMs = 0;

const char *contactorPinStr(bool closed) {
    return closed ? "closed" : "open";
}

void printNull(Print &out) { out.print(F("null")); }

void printBool(Print &out, bool v) { out.print(v ? F("true") : F("false")); }

void printFloat(Print &out, float v, uint8_t decimals) {
    out.print(v, decimals);
}

void printQuoted(Print &out, const char *s) {
    out.print('"');
    out.print(s);
    out.print('"');
}

void printBms(Print &out) {
    const bool stable = BicmDecode::packStable();
    const float packV = BicmDecode::packVoltage();
    const float cellMin = BicmDecode::minCellVoltage();
    const float cellMax = BicmDecode::maxCellVoltage();
    const int cells = BicmDecode::seriesCellCount();
    const bool haveCells = stable && cellMin > 0.0f && cellMax > 0.0f;

    out.print(F("\"bms\":{"));
    // Live BICM monitor path — wired even while waiting for first stable burst.
    out.print(F("\"wired\":"));
    printBool(out, true);

    out.print(F(",\"packV\":"));
    if (haveCells && packV > 0.0f) {
        printFloat(out, packV, 2);
    } else {
        printNull(out);
    }

    out.print(F(",\"packA\":"));
    printNull(out);  // no pack current sensor in this firmware yet

    out.print(F(",\"socPct\":"));
    printNull(out);  // SOC not computed

    out.print(F(",\"cellMinV\":"));
    if (haveCells) {
        printFloat(out, cellMin, 3);
    } else {
        printNull(out);
    }

    out.print(F(",\"cellMaxV\":"));
    if (haveCells) {
        printFloat(out, cellMax, 3);
    } else {
        printNull(out);
    }

    out.print(F(",\"cellDeltaV\":"));
    if (haveCells) {
        printFloat(out, cellMax - cellMin, 3);
    } else {
        printNull(out);
    }

    // BMS_CAP_READ_TEMPS is 0 — sniffer counts only, no decoded °C.
    out.print(F(",\"tempMinC\":"));
    printNull(out);
    out.print(F(",\"tempMaxC\":"));
    printNull(out);

    out.print(F(",\"cells\":"));
    out.print(cells > 0 ? cells : PACK_S_CELLS);

    out.print(F(",\"cellVolts\":"));
    if (haveCells) {
        out.print('[');
        const int n = cells > 0 ? cells : PACK_S_CELLS;
        for (int i = 1; i <= n; i++) {
            if (i > 1) {
                out.print(',');
            }
            const float v = BicmDecode::cellVoltage(static_cast<uint8_t>(i));
            if (v > 0.0f) {
                printFloat(out, v, 3);
            } else {
                printNull(out);
            }
        }
        out.print(']');
    } else {
        printNull(out);
    }

    out.print(F(",\"stable\":"));
    printBool(out, stable);

    out.print(F(",\"balancing\":"));
    printBool(out, false);  // live bleed only when BMS_CAP_BALANCE_TX=1

    out.print(F(",\"balanceEnabled\":"));
    printBool(out, BMS_CAP_BALANCE_TX != 0);

    out.print('}');
}

void printContactors(Print &out) {
    const ContactorSeq::Outputs want = ContactorSeq::intended();
    const ContactorSeq::Outputs drv = ContactorSeq::driven();
    const bool drivenLive = BMS_CAP_CONTACTOR_DRV != 0 && !TELEMETRY_ONLY;

    out.print(F("\"contactors\":{"));
    // Cap stays 0 — coils not driven; report dry-run intended with wired:false.
    out.print(F("\"wired\":"));
    printBool(out, false);
    out.print(F(",\"precharge\":"));
    printQuoted(out, contactorPinStr(want.precharge));
    out.print(F(",\"mainPositive\":"));
    printQuoted(out, contactorPinStr(want.mainPos));
    out.print(F(",\"mainNegative\":"));
    printQuoted(out, contactorPinStr(want.mainNeg));
    out.print(F(",\"want\":"));
    printQuoted(out, ContactorSeq::deadman() ? "closed" : "monitor");
    out.print(F(",\"driven\":"));
    printBool(out, drivenLive && (drv.precharge || drv.mainPos || drv.mainNeg));
    out.print('}');
}

void printCharge(Print &out) {
    out.print(F("\"charge\":{"));
    out.print(F("\"wired\":"));
    printBool(out, false);  // BMS_CAP_CHARGE_TX=0 — listen/dry-run only
    out.print(F(",\"elconOn\":"));
    printBool(out, false);
    out.print(F(",\"setV\":"));
    printNull(out);
    out.print(F(",\"setA\":"));
    printNull(out);
    out.print(F(",\"measV\":"));
    printNull(out);
    out.print(F(",\"measA\":"));
    printNull(out);
    out.print(F(",\"complete\":"));
    printBool(out, false);
    out.print(F(",\"fault\":"));
    printBool(out, false);
    out.print(F(",\"faultText\":"));
    printNull(out);
    out.print('}');
}

void printDrive(Print &out) {
    out.print(F("\"drive\":{"));
    // BMS_CAP_KELLY_RX=0 — count/raw only; keep wired:false and null fields.
    out.print(F("\"wired\":"));
    printBool(out, false);
    out.print(F(",\"rpm\":"));
    printNull(out);
    out.print(F(",\"speedMph\":"));
    printNull(out);
    out.print(F(",\"motorTempC\":"));
    printNull(out);
    out.print(F(",\"controllerTempC\":"));
    printNull(out);
    out.print(F(",\"currentLimitA\":"));
    printNull(out);
    out.print(F(",\"throttlePct\":"));
    printNull(out);
    out.print(F(",\"faultCode\":"));
    printNull(out);
    out.print(F(",\"controller\":\"Kelly\""));
    out.print('}');
}

void printEnergy(Print &out) {
    out.print(F("\"energy\":{"));
    out.print(F("\"kWhUsed\":"));
    printNull(out);
    out.print(F(",\"whPerMi\":"));
    printNull(out);
    out.print(F(",\"rangeMiEst\":"));
    printNull(out);
    out.print(F(",\"calibrated\":"));
    printBool(out, false);
    out.print('}');
}

void printAux(Print &out) {
    out.print(F("\"aux12v\":{"));
    out.print(F("\"wired\":"));
    printBool(out, false);
    out.print(F(",\"volts\":"));
    printNull(out);
    out.print('}');
}

void printFaults(Print &out) {
    out.print(F("\"faults\":["));
    if (ContactorSeq::state() == ContactorSeq::State::Fault) {
        out.print('{');
        out.print(F("\"id\":\"contactor-fault\","));
        out.print(F("\"ts\":"));
        out.print((millis() - g_bootMs) / 1000UL);
        out.print(F(",\"level\":\"fault\","));
        out.print(F("\"source\":\"system\","));
        out.print(F("\"text\":\"Contactor SM fault (dry-run / FLAG=0)\""));
        out.print('}');
    }
    out.print(']');
}

}  // namespace

void printFrame(Print &out) {
    const uint32_t uptimeSec = (millis() - g_bootMs) / 1000UL;

    out.print('{');
    out.print(F("\"schema\":1,"));
    // No RTC/NTP on Teensy — monotonic seconds since boot; ESP32 may refresh ts.
    out.print(F("\"ts\":"));
    out.print(uptimeSec);
    out.print(F(",\"uptimeSec\":"));
    out.print(uptimeSec);
    out.print(',');

    printBms(out);
    out.print(',');
    printContactors(out);
    out.print(',');
    printCharge(out);
    out.print(',');
    printDrive(out);
    out.print(',');
    printEnergy(out);
    out.print(',');
    printAux(out);
    out.print(',');
    printFaults(out);
    out.print(F(",\"events\":[]"));
    out.println('}');
}

void begin() {
    g_bootMs = millis();
    g_lastEmitMs = 0;
    // Serial4 claims pins 16/17 (RX/TX). BmsApp skips pinMode on IN1/IN2 for this.
    Serial4.begin(115200);
    SERIALCONSOLE.println(
        F("Serial4 hub emit ON — STATUS_SCHEMA v1 JSON @ 115200 (pins TX17/RX16)"));
}

void tick() {
    const uint32_t now = millis();
    if (g_lastEmitMs != 0 && (now - g_lastEmitMs) < kEmitIntervalMs) {
        return;
    }
    g_lastEmitMs = now;
    printFrame(Serial4);
}

}  // namespace HubStatus
