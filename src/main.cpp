#include <Arduino.h>
#include "Config.h"
#include "CanBus.h"
#include "BicmSniffer.h"
#include "BicmDecode.h"
#include "BmsApp.h"

namespace {

void onCanFrame(const CAN_message_t &msg) {
    BicmSniffer::onFrame(msg);
    BicmDecode::onFrame(msg);
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
            case '?':
            case 'h':
                SERIALCONSOLE.println(
                    F("Keys (no Enter): c=candump s=IDs d=cells k=keep-alive r=reset ?=help"));
                SERIALCONSOLE.println(
                    F("Stats also print automatically every 10 seconds."));
                break;
#if !TELEMETRY_ONLY
            case 'E':
                BmsApp::handleSerial();
                break;
#endif
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
    SERIALCONSOLE.print(F("S / "));
    SERIALCONSOLE.print(PACK_MODULE_COUNT);
    SERIALCONSOLE.print(F(" modules / "));
    SERIALCONSOLE.print(BICM_CELLS_PER_MODULE);
    SERIALCONSOLE.println(F(" cells per module"));
#if TELEMETRY_ONLY
    SERIALCONSOLE.println(F("Mode: sniffer + decode (TELEMETRY_ONLY)"));
#else
    SERIALCONSOLE.println(F("Mode: BMS (contactor outputs gated)"));
#endif
    SERIALCONSOLE.println(F("CAN3 @ 125k — keep-alive 0x200/1s"));
#if K112_24S_SUBPACK
    SERIALCONSOLE.println(F("Profile: ONE K112 BICM, 24S sub-pack (6+12+6 on sense harness)"));
    SERIALCONSOLE.println(F("Expect ~24 cells across CAN IDs 0x460-0x473 when X1-X4 are connected."));
#else
    SERIALCONSOLE.print(F("Expect "));
    SERIALCONSOLE.print(PACK_MODULE_COUNT);
    SERIALCONSOLE.print(F(" CAN module(s) = "));
    SERIALCONSOLE.print(PACK_S_CELLS);
    SERIALCONSOLE.println(F(" cells."));
#endif
    SERIALCONSOLE.println(F("Keys: c s d k r ?  (no Enter — click terminal first)"));
    SERIALCONSOLE.println(F("ID list + cell volts print every 10 s automatically."));

    CanBus::begin();
    CanBus::setFrameHandler(onCanFrame);
    BicmSniffer::begin();
    BicmDecode::begin();
    BmsApp::begin();
}

void loop() {
    CanBus::tick();
    BicmSniffer::tick();
    BicmDecode::tick();
    BmsApp::tick();
    dispatchSerial();

    static uint32_t autoReportMs = 0;
    if (millis() - autoReportMs >= 10000) {
        autoReportMs = millis();
        SERIALCONSOLE.println(F("--- auto report (10s) ---"));
        BicmSniffer::printIdStats();
        BicmDecode::printDecodeDetails();
    }

    static uint32_t ledMs = 0;
    if (millis() - ledMs >= 500) {
        ledMs = millis();
        digitalWrite(Pins::LED, !digitalRead(Pins::LED));
    }
}
