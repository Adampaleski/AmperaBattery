#include "ChargeTx.h"
#include <string.h>
#include "BicmDecode.h"
#include "BmsCapabilities.h"
#include "ElconCharger.h"
#include "ChargerCan.h"
#include "Config.h"
#include "ContactorSeq.h"

namespace ChargeTx {

namespace {

bool g_request     = false;
uint32_t g_lastTxMs = 0;
uint32_t g_txOk     = 0;
uint32_t g_txFail   = 0;

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

bool chargeAllowed() {
    if (!g_request) {
        return false;
    }
    if (ContactorSeq::state() != ContactorSeq::State::Closed) {
        return false;
    }
    if (BicmDecode::packStable()) {
        const float mn = BicmDecode::minCellVoltage();
        const float mx = BicmDecode::maxCellVoltage();
        if (mn > 0.0f && mn < kCellFaultMinV) {
            return false;
        }
        if (mx > kCellFaultMaxV) {
            return false;
        }
        if (mx >= kChargeCellSetpointV) {
            return false;
        }
    }
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

    const float demand = kChargeCellSetpointV * static_cast<float>(PACK_S_CELLS);
    const uint16_t volts = static_cast<uint16_t>(demand * 10.0f + 0.5f);
    buf[0] = static_cast<uint8_t>((volts >> 8) & 0xFF);
    buf[1] = static_cast<uint8_t>(volts & 0xFF);

    const uint16_t amps = enable ? kChargeCurrentDeciA : 0;
    buf[2] = static_cast<uint8_t>((amps >> 8) & 0xFF);
    buf[3] = static_cast<uint8_t>(amps & 0xFF);

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
#if !BMS_CAP_CHARGE_TX || TELEMETRY_ONLY
    return;
#else
    const uint32_t now = millis();
    if (now - g_lastTxMs < kChargerPeriodMs) {
        return;
    }
    g_lastTxMs = now;
    sendControl(chargeAllowed());
#endif
}

void setChargeRequest(bool on) { g_request = on; }

bool chargeRequested() { return g_request; }

void toggleChargeRequest() {
    g_request = !g_request;
    SERIALCONSOLE.print(F("Charge request "));
    SERIALCONSOLE.println(g_request ? F("ON") : F("OFF"));
    printStatus();
}

void printStatus() {
    const bool allow = chargeAllowed();
    uint8_t buf[ElconCharger::kCtlLen];
    packControl(buf, allow);

    SERIALCONSOLE.print(F("CHARGE Elcon  flag="));
    SERIALCONSOLE.print(BMS_CAP_CHARGE_TX);
    SERIALCONSOLE.print(F("  req="));
    SERIALCONSOLE.print(g_request ? 1 : 0);
    SERIALCONSOLE.print(F("  allow="));
    SERIALCONSOLE.print(allow ? 1 : 0);
    SERIALCONSOLE.print(F("  contactors="));
    SERIALCONSOLE.println(ContactorSeq::stateName(ContactorSeq::state()));
    SERIALCONSOLE.print(F("  0x1806E5F4#"));
    printHexPayload(buf, ElconCharger::kCtlLen);
    SERIALCONSOLE.println();
    SERIALCONSOLE.print(F("  demand "));
    SERIALCONSOLE.print(kChargeCellSetpointV * static_cast<float>(PACK_S_CELLS), 1);
    SERIALCONSOLE.print(F("V / "));
    SERIALCONSOLE.print(kChargeCurrentDeciA / 10);
    SERIALCONSOLE.print(F("."));
    SERIALCONSOLE.print(kChargeCurrentDeciA % 10);
    SERIALCONSOLE.println(F("A  (Elcon V/I 0.1 BE, ctl 0=charge 1=stop)"));
    SERIALCONSOLE.println(F("  winding unread — 72/96 V box cannot finish 36S; 312 V will not start drained 36S"));
#if !BMS_CAP_CHARGE_TX
    SERIALCONSOLE.println(F("  (not TX — BMS_CAP_CHARGE_TX=0)"));
#elif TELEMETRY_ONLY
    SERIALCONSOLE.println(F("  (not TX — TELEMETRY_ONLY=1)"));
#else
    SERIALCONSOLE.print(F("  tx_ok="));
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
