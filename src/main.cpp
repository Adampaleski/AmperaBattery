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

void dispatchSerial() {
    while (SERIALCONSOLE.available()) {
        const char c = static_cast<char>(SERIALCONSOLE.read());
        switch (c) {
            case 'c':
                BicmSniffer::setCanDebug(!BicmSniffer::canDebugEnabled());
                break;
            case 'S':
                BicmSniffer::printIdStats();
                break;
            case 'R':
                BicmSniffer::resetStats();
                break;
            case 'K':
                CanBus::setKeepAliveEnabled(!CanBus::keepAliveEnabled());
                SERIALCONSOLE.print(F("Keep-alive "));
                SERIALCONSOLE.println(CanBus::keepAliveEnabled() ? F("ON") : F("OFF"));
                break;
            case 'd':
                BicmDecode::handleSerial(c);
                break;
            case '?':
            case 'h':
                SERIALCONSOLE.println(
                    F("c=candump  S=ID stats  R=reset  K=keep-alive  d=decode  ?=help"));
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
    SERIALCONSOLE.println(F("Commands: c S R K d ?"));

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

    static uint32_t ledMs = 0;
    if (millis() - ledMs >= 500) {
        ledMs = millis();
        digitalWrite(Pins::LED, !digitalRead(Pins::LED));
    }
}
