#include "BmsApp.h"
#include "Config.h"
#include "ContactorSeq.h"

namespace BmsApp {

namespace {

void initPins() {
    pinMode(Pins::IN1, INPUT);
    pinMode(Pins::IN2, INPUT);
    pinMode(Pins::IN3, INPUT);
    pinMode(Pins::IN4, INPUT);

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
    ContactorSeq::begin();
}

void tick() {
    ContactorSeq::tick();
}

void handleSerial() {
    ContactorSeq::toggleDeadman();
}

}  // namespace BmsApp
