#include <Arduino.h>
#include "Config.h"
#include "CanBus.h"
#include "ChargerCan.h"
#include "BicmSniffer.h"
#include "BicmDecode.h"
#include "BmsApp.h"
#include "BmsCapabilities.h"
#include "BicmPackProfile.h"
#include "BalanceTx.h"
#include "ChargeTx.h"
#include "ContactorSeq.h"

namespace {

void onCanFrame(const CAN_message_t &msg) {
    BicmSniffer::onFrame(msg);
    BicmDecode::onFrame(msg);
}

void onChargerFrame(const CAN_message_t &msg) {
    ChargeTx::onFrame(msg);
}

void runCommand(char c) {
    if (c >= 'A' && c <= 'Z') {
        c = static_cast<char>(c - 'A' + 'a');
    }
    SERIALCONSOLE.print(F(">> cmd "));
    SERIALCONSOLE.println(c);

    switch (c) {
            case 'c':
                BicmSniffer::setCanDebug(!BicmSniffer::canDebugEnabled());
                break;
            case 's':
                BicmSniffer::printIdStats();
                break;
            case 'r':
                BicmSniffer::resetStats();
                break;
            case 'k':
                CanBus::setKeepAliveEnabled(!CanBus::keepAliveEnabled());
                SERIALCONSOLE.print(F("Keep-alive "));
                SERIALCONSOLE.println(CanBus::keepAliveEnabled() ? F("ON") : F("OFF"));
                break;
            case 'd':
                BicmDecode::printDecodeDetails();
                break;
            case 'b':
                BalanceTx::printStatus();
                break;
            case 'e':
                BmsApp::handleSerial();
                break;
            case 'p':
                ContactorSeq::printStatus();
                break;
            case 'g':
                ChargeTx::toggleChargeRequest();
                break;
            case '?':
            case 'h':
                SERIALCONSOLE.println(
                    F("Keys (no Enter): c=candump s=IDs d=cells b=balance k=keep-alive r=reset"));
                SERIALCONSOLE.println(
                    F("  e=dead-man  p=contactors  g=charge-request ?=help"));
                SERIALCONSOLE.println(
                    F("Stats also print automatically every 10 seconds."));
                break;
            default:
                break;
    }
}

void dispatchSerial() {
    while (SERIALCONSOLE.available()) {
        runCommand(static_cast<char>(SERIALCONSOLE.read()));
    }
}

}  // namespace

void setup() {
    pinMode(Pins::LED, OUTPUT);
    digitalWrite(Pins::LED, HIGH);

    SERIALCONSOLE.begin(115200);
    delay(1500);

    SERIALCONSOLE.println();
    SERIALCONSOLE.println(F("=== Ampera BICM Teensy 4.1 ==="));
    SERIALCONSOLE.print(F("Pack: "));
    SERIALCONSOLE.print(PACK_S_CELLS);
    SERIALCONSOLE.print(F("S"));
#if K112_PACK_DECODE
    SERIALCONSOLE.print(F(" / "));
    SERIALCONSOLE.print(kBicmStreamCount);
    SERIALCONSOLE.println(F(" physical BICM(s) on daisy chain"));
#else
    SERIALCONSOLE.print(F(" / "));
    SERIALCONSOLE.print(PACK_MODULE_COUNT);
    SERIALCONSOLE.print(F(" modules / "));
    SERIALCONSOLE.print(BICM_CELLS_PER_MODULE);
    SERIALCONSOLE.println(F(" cells per module"));
#endif
#if TELEMETRY_ONLY
    SERIALCONSOLE.println(F("Mode: MONITOR — no charge TX, no contactor drive"));
#else
    SERIALCONSOLE.println(F("Mode: BMS GPIO (coil drive still gated by BMS_CAP_CONTACTOR_DRV)"));
#endif
#if BMS_CAP_BALANCE_TX
    SERIALCONSOLE.println(F("CAN3 @ 125k — keep-alive 0x200/1s + balance 0x300/0x310"));
#else
    SERIALCONSOLE.println(F("CAN3 @ 125k — keep-alive 0x200/1s (balance TX compiled, flag=0)"));
#endif
#if BMS_CAP_CHARGE_TX
    SERIALCONSOLE.println(F("CAN2 @ 500k pins 0/1 — Brusa NLG5 0x618 TX"));
#else
    SERIALCONSOLE.println(F("CAN2 @ 500k pins 0/1 — Brusa NLG5 (charge TX compiled, flag=0)"));
#endif
    SERIALCONSOLE.print(F("Capabilities: cells="));
    SERIALCONSOLE.print(BMS_CAP_READ_CELL_VOLTS);
    SERIALCONSOLE.print(F(" keepalive="));
    SERIALCONSOLE.print(BMS_CAP_KEEPALIVE_TX);
    SERIALCONSOLE.print(F(" balance="));
    SERIALCONSOLE.print(BMS_CAP_BALANCE_TX);
    SERIALCONSOLE.print(F(" charge="));
    SERIALCONSOLE.print(BMS_CAP_CHARGE_TX);
    SERIALCONSOLE.print(F(" contactors="));
    SERIALCONSOLE.println(BMS_CAP_CONTACTOR_DRV);
#if K112_PACK_DECODE
#if PACK_BICM_COUNT == 1
    SERIALCONSOLE.println(F("Profile: one K112, 24S (0x460/0x470 burst)"));
#elif PACK_BICM_COUNT == 2
#if K112_SPARSE_SECOND_BICM
    SERIALCONSOLE.println(F("Profile: 36S — A burst 0x460/470, B sparse 0x46D-0x47E"));
#else
    SERIALCONSOLE.println(F("Profile: two KICMs, 36S (A=0x460/470, B=0x461/471)"));
#endif
    SERIALCONSOLE.println(F("If B cells stay 0, run `s` and adjust BicmSparseMap.h."));
#endif
#else
    SERIALCONSOLE.print(F("Expect "));
    SERIALCONSOLE.print(PACK_MODULE_COUNT);
    SERIALCONSOLE.print(F(" CAN module(s) = "));
    SERIALCONSOLE.print(PACK_S_CELLS);
    SERIALCONSOLE.println(F(" cells."));
#endif
    SERIALCONSOLE.println(F("Keys: c s d b k r e p g ?  (no Enter — click terminal first)"));
    SERIALCONSOLE.println(F("ID list + cell volts print every 10 s automatically."));

    CanBus::begin();
    CanBus::setFrameHandler(onCanFrame);
    ChargerCan::begin();
    ChargerCan::setFrameHandler(onChargerFrame);
    BicmSniffer::begin();
    BicmDecode::begin();
    BalanceTx::begin();
    ChargeTx::begin();
    BmsApp::begin();
}

void loop() {
    CanBus::tick();
    ChargerCan::tick();
    BicmSniffer::tick();
    BicmDecode::tick();
    BalanceTx::tick();
    ChargeTx::tick();
    BmsApp::tick();
    dispatchSerial();

    static uint32_t autoReportMs = 0;
    if (millis() - autoReportMs >= 10000) {
        autoReportMs = millis();
        SERIALCONSOLE.println(F("--- auto report (10s) ---"));
        BicmSniffer::printIdStats();
        BicmDecode::printDecodeDetails();
        BalanceTx::printStatus();
        ContactorSeq::printStatus();
        ChargeTx::printStatus();
    }

    static uint32_t ledMs = 0;
    if (millis() - ledMs >= 500) {
        ledMs = millis();
        digitalWrite(Pins::LED, !digitalRead(Pins::LED));
    }
}
