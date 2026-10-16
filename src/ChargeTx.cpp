#include "ChargeTx.h"
#include <string.h>
#include "BicmDecode.h"
#include "BmsCapabilities.h"
#include "ElconCharger.h"
#include "ChargerCan.h"
#include "Config.h"
#include "ContactorSeq.h"
#if FACTORY_BUS_DECODE
#include "FactoryCellMap.h"
#endif

// Charger voltage basis = cells in ONE series string of the ACTIVE map,
// never PACK_S_CELLS (which can span separate, unparalleled sections).
// 0 = no single string defined -> charging is impossible (always refused).
#if FACTORY_BUS_DECODE
#define CHARGE_STRING_CELLS FACTORY_STRING_CELLS
#elif K112_PACK_DECODE && (PACK_S_CELLS == 36)
#define CHARGE_STRING_CELLS 36
#else
#define CHARGE_STRING_CELLS 0  // 24S/30S/48S/60S bench sub-packs, legacy: never charge
#endif

static_assert(CHARGE_STRING_CELLS == 0 ||
                  (kChargeCellSetpointV * CHARGE_STRING_CELLS >= ELCON_MIN_V &&
                   kChargeCellSetpointV * CHARGE_STRING_CELLS <= ELCON_MAX_V),
              "\n\n*** CHARGE VOLTAGE OUT OF CHARGER RANGE ***\n"
              "kChargeCellSetpointV x CHARGE_STRING_CELLS (e.g. 4.10 V x 36 = 147.6 V) is outside\n"
              "ELCON_MIN_V..ELCON_MAX_V (default 100..202 V, an Elcon HK-LF-144-46 nameplate).\n"
              "FIX: read the Elcon nameplate output range, then set\n"
              "  -D ELCON_MIN_V=<min>f -D ELCON_MAX_V=<max>f  in platformio.ini build_flags,\n"
              "or correct the string cell count in FactoryCellMap.h. Do NOT just widen the range.\n");
static_assert(kChargeFaultCellV > kChargeCellSetpointV && kChargeFaultCellV < kCellFaultMaxV,
              "charge fault threshold must sit between setpoint and cell OV fault");

namespace ChargeTx {

namespace {

bool g_request     = false;
uint32_t g_lastTxMs = 0;
uint32_t g_txOk     = 0;
uint32_t g_txFail   = 0;
bool     g_wasAllowed = false;
bool     g_chargeFault = false;  // latched at any cell >= kChargeFaultCellV, cleared only by reboot
const char *g_refuseWhy = "no request";

uint8_t  g_st[8]    = {};
uint8_t  g_stLen    = 0;
uint32_t g_stMs     = 0;

uint16_t be16(const uint8_t *p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

void printHexPayload(const uint8_t *buf, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        if (buf[i] < 0x10) {
            SERIALCONSOLE.print('0');
        }
        SERIALCONSOLE.print(buf[i], HEX);
        if (i + 1 < len) {
            SERIALCONSOLE.print('.');
        }
    }
}

// Latch fault from any fresh cell >= kChargeFaultCellV. Runs every tick,
// independent of flags/request, so it is armed even in print-only builds.
void checkChargeFault() {
    if (g_chargeFault) {
        return;
    }
    const float mx = BicmDecode::maxCellVoltage();  // fresh cells only
    if (mx >= kChargeFaultCellV) {
        g_chargeFault = true;
        SERIALCONSOLE.print(F("*** CHARGE FAULT LATCHED: cell "));
        SERIALCONSOLE.print(mx, 3);
        SERIALCONSOLE.println(F(" V >= 4.12 V — charge refused until reboot ***"));
    }
}

// Default is REFUSE. Every condition must positively pass.
bool chargeAllowed() {
    if (CHARGE_STRING_CELLS == 0) {
        g_refuseWhy = "no single series string in map (charge impossible)";
        return false;
    }
    if (!TEMPS_VERIFIED) {
        g_refuseWhy = "temps not verified (TEMPS_VERIFIED=0)";
        return false;
    }
    if (g_chargeFault) {
        g_refuseWhy = "charge fault latched (cell >= 4.12 V)";
        return false;
    }
    if (!g_request) {
        g_refuseWhy = "no request";
        return false;
    }
    if (ContactorSeq::state() != ContactorSeq::State::Closed) {
        g_refuseWhy = "contactors not closed";
        return false;
    }
    if (!BicmDecode::allCellsFresh()) {
        g_refuseWhy = "cell data missing/stale (>3 s)";
        return false;
    }
    if (!BicmDecode::packStable()) {
        g_refuseWhy = "pack not stable";
        return false;
    }
    if (BicmDecode::cellAlarmActive()) {
        g_refuseWhy = "cell alarm active";
        return false;
    }
    const float mx = BicmDecode::maxCellVoltage();
    if (mx <= 0.0f || mx >= kChargeCellSetpointV) {
        g_refuseWhy = "a cell reached 4.10 V";
        return false;
    }
    g_refuseWhy = "";
    return true;
}

#if BMS_CAP_CHARGE_TX && !TELEMETRY_ONLY
void sendControl(bool enable) {
    uint8_t buf[ElconCharger::kCtlLen];
    packControl(buf, enable);

    CAN_message_t msg{};
    msg.id  = ElconCharger::kCtlId;
    msg.len = ElconCharger::kCtlLen;
    msg.flags.extended = 1;
    memcpy(msg.buf, buf, ElconCharger::kCtlLen);
    if (ChargerCan::writeFrame(msg) <= 0) {
        g_txFail++;
    } else {
        g_txOk++;
    }
}
#endif

}  // namespace

void begin() {
    g_request  = false;
    g_lastTxMs = 0;
    g_txOk     = 0;
    g_txFail   = 0;
    g_stLen    = 0;
}

void packControl(uint8_t buf[8], bool enable) {
    memset(buf, 0, ElconCharger::kCtlLen);

    const float demand = kChargeCellSetpointV * static_cast<float>(CHARGE_STRING_CELLS);
    const uint16_t volts = static_cast<uint16_t>(demand * 10.0f + 0.5f);
    buf[0] = static_cast<uint8_t>((volts >> 8) & 0xFF);
    buf[1] = static_cast<uint8_t>(volts & 0xFF);

    const uint16_t amps = enable ? kChargeCurrentDeciA : 0;
    buf[2] = static_cast<uint8_t>((amps >> 8) & 0xFF);
    buf[3] = static_cast<uint8_t>(amps & 0xFF);

    // Disabled = explicit stop: control byte 1, zero current (never silence).
    buf[4] = enable ? ElconCharger::kCtlCharge : ElconCharger::kCtlStop;
}

void onFrame(const CAN_message_t &msg) {
    if (msg.id != ElconCharger::kStatusId) {
        return;
    }
    g_stLen = msg.len > 8 ? 8 : msg.len;
    memcpy(g_st, msg.buf, g_stLen);
    g_stMs = millis();
}

void tick() {
    checkChargeFault();
    const bool allow = chargeAllowed();
    const uint32_t now = millis();
    // Allowed -> refused: send STOP immediately (don't wait for the 1 s slot).
    const bool dropped = g_wasAllowed && !allow;
    g_wasAllowed = allow;
#if !BMS_CAP_CHARGE_TX || TELEMETRY_ONLY
    if (dropped) {
        SERIALCONSOLE.print(F("*** CHARGE PRINT-ONLY: WOULD-SEND Elcon STOP (ctl=1, 0 A)  reason="));
        SERIALCONSOLE.println(g_refuseWhy);
    }
    (void)now;
#else
    if (!dropped && now - g_lastTxMs < kChargerPeriodMs) {
        return;
    }
    g_lastTxMs = now;
    sendControl(allow);  // refused -> explicit stop frame every period
#endif
}

void setChargeRequest(bool on) { g_request = on; }

const char *refuseReason() {
    return chargeAllowed() ? "" : g_refuseWhy;
}

int stringCells() { return CHARGE_STRING_CELLS; }

bool faultLatched() { return g_chargeFault; }

bool chargeRequested() { return g_request; }

void toggleChargeRequest() {
    g_request = !g_request;
#if !BMS_CAP_CHARGE_TX || TELEMETRY_ONLY
    SERIALCONSOLE.println(
        F("*** CHARGE PRINT-ONLY  FLAG=0  DRIVE=OFF  CAN2 NOT fired ***"));
    SERIALCONSOLE.print(F("Charge-request latch (dry-run) "));
    SERIALCONSOLE.print(g_request ? F("ON") : F("OFF"));
    SERIALCONSOLE.println(F(" — would-be Elcon frame only; no TX"));
    if (g_request && !chargeAllowed()) {
        SERIALCONSOLE.print(F("Charge REFUSED: "));
        SERIALCONSOLE.println(g_refuseWhy);
    }
#else
    SERIALCONSOLE.print(F("Charge request "));
    SERIALCONSOLE.println(g_request ? F("ON") : F("OFF"));
#endif
    printStatus();
}

void printStatus() {
    const bool allow = chargeAllowed();
    uint8_t buf[ElconCharger::kCtlLen];
    packControl(buf, allow);

#if !BMS_CAP_CHARGE_TX || TELEMETRY_ONLY
    SERIALCONSOLE.println(
        F("*** CHARGE PRINT-ONLY  FLAG=0  DRIVE=OFF  CAN2 NOT fired ***"));
#endif
    SERIALCONSOLE.print(F("CHARGE Elcon  FLAG="));
    SERIALCONSOLE.print(BMS_CAP_CHARGE_TX);
    SERIALCONSOLE.print(F("  telemetry="));
    SERIALCONSOLE.print(TELEMETRY_ONLY);
    SERIALCONSOLE.print(F("  req="));
    SERIALCONSOLE.print(g_request ? 1 : 0);
    SERIALCONSOLE.print(F("  allow="));
    SERIALCONSOLE.print(allow ? 1 : 0);
    SERIALCONSOLE.print(F("  contactors="));
    SERIALCONSOLE.println(ContactorSeq::stateName(ContactorSeq::state()));
    SERIALCONSOLE.print(F("  string_cells="));
    SERIALCONSOLE.print(CHARGE_STRING_CELLS);
    SERIALCONSOLE.print(F("  temps_verified="));
    SERIALCONSOLE.print(TEMPS_VERIFIED);
    SERIALCONSOLE.print(F("  fault_latched="));
    SERIALCONSOLE.print(g_chargeFault ? 1 : 0);
    if (!allow) {
        SERIALCONSOLE.print(F("  REFUSED: "));
        SERIALCONSOLE.print(g_refuseWhy);
    }
    SERIALCONSOLE.println();
    SERIALCONSOLE.print(F("  WOULD-BE 0x1806E5F4#"));
    printHexPayload(buf, ElconCharger::kCtlLen);
    SERIALCONSOLE.println();
    SERIALCONSOLE.print(F("  demand "));
    SERIALCONSOLE.print(kChargeCellSetpointV * static_cast<float>(CHARGE_STRING_CELLS), 1);
    SERIALCONSOLE.print(F("V / "));
    SERIALCONSOLE.print(kChargeCurrentDeciA / 10);
    SERIALCONSOLE.print(F("."));
    SERIALCONSOLE.print(kChargeCurrentDeciA % 10);
    SERIALCONSOLE.println(F("A  (Elcon V/I 0.1 BE, ctl 0=charge 1=stop)"));
    SERIALCONSOLE.println(F("  winding unread — 72/96 V box cannot finish 36S; 312 V will not start drained 36S"));
#if !BMS_CAP_CHARGE_TX
    SERIALCONSOLE.println(
        F("  PRINT-ONLY — frame above is NOT written to CAN2 (BMS_CAP_CHARGE_TX=0)"));
#elif TELEMETRY_ONLY
    SERIALCONSOLE.println(
        F("  PRINT-ONLY — frame above is NOT written to CAN2 (TELEMETRY_ONLY=1)"));
#else
    SERIALCONSOLE.print(F("  LIVE TX  tx_ok="));
    SERIALCONSOLE.print(g_txOk);
    SERIALCONSOLE.print(F("  tx_fail="));
    SERIALCONSOLE.println(g_txFail);
#endif
    SERIALCONSOLE.print(F("  CAN2 250k rx="));
    SERIALCONSOLE.print(ChargerCan::stats().rxCount);
    SERIALCONSOLE.print(F("  age_ms="));
    SERIALCONSOLE.println(ChargerCan::lastRxAgeMs());
    if (g_stLen) {
        SERIALCONSOLE.print(F("  0x18FF50E5#"));
        printHexPayload(g_st, g_stLen);
        SERIALCONSOLE.print(F("  V="));
        SERIALCONSOLE.print(be16(g_st) * 0.1f, 1);
        SERIALCONSOLE.print(F(" I="));
        SERIALCONSOLE.print(be16(g_st + 2) * 0.1f, 1);
        SERIALCONSOLE.print(F("A st="));
        if (g_stLen >= 5) {
            SERIALCONSOLE.print(g_st[4], HEX);
        } else {
            SERIALCONSOLE.print('-');
        }
        SERIALCONSOLE.print(F("  age="));
        SERIALCONSOLE.println(millis() - g_stMs);
    }
}

}  // namespace ChargeTx
