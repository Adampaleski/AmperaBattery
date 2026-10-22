#include "BmsApp.h"
#include "Config.h"
#include "ContactorSeq.h"
#include "KellyLimit.h"
#include "CurrentSense.h"

namespace BmsApp {

namespace {

void initPins() {
    // Pins 16/17 are Serial4 RX/TX for the teryx-ev-hub ESP32 link (HubStatus::begin() owns them);
    // 18/19 are I2C for the MCP23017. IN1-IN4 no longer sit on them (Config.h).
#if BODY_IO_EXPANDER
    pinMode(Pins::PARK_IN_B, INPUT_PULLUP);  // pin 32 on A and B: park contact (docs/PARK_BRAKE.md), LOW = set; read only
#endif
#if BODY_IO_EXPANDER  // without the expander pin 34 is the 595 /OE output, not IN1
    if (Pins::IN1 >= 0) pinMode(Pins::IN1, INPUT);  // spare input, read by nothing yet
#endif
    if (Pins::IN2 >= 0) pinMode(Pins::IN2, INPUT);
    if (Pins::IN3 >= 0) pinMode(Pins::IN3, INPUT);
    if (Pins::IN4 >= 0) pinMode(Pins::IN4, INPUT);

    pinMode(Pins::OUT1, OUTPUT);
    pinMode(Pins::OUT2, OUTPUT);
    pinMode(Pins::OUT3, OUTPUT);
    pinMode(Pins::OUT4, OUTPUT);
    pinMode(Pins::OUT5, OUTPUT);
    pinMode(Pins::OUT6, OUTPUT);
    pinMode(Pins::OUT7, OUTPUT);
    pinMode(Pins::OUT8, OUTPUT);
    pinMode(Pins::LED, OUTPUT);

    digitalWrite(Pins::OUT1, LOW);
    digitalWrite(Pins::OUT2, LOW);
    digitalWrite(Pins::OUT3, LOW);
    digitalWrite(Pins::OUT4, LOW);
    analogWrite(Pins::OUT5, 0);
    analogWrite(Pins::OUT6, 0);
    analogWrite(Pins::OUT7, 0);
    analogWrite(Pins::OUT8, 0);
}

}  // namespace

void begin() {
    initPins();
    CurrentSense::begin();
    ContactorSeq::begin();
}

void tick() {
    CurrentSense::tick();
    ContactorSeq::tick();
    KellyLimit::tick();
}

void handleSerial() {
    ContactorSeq::toggleDeadman();
}

}  // namespace BmsApp
