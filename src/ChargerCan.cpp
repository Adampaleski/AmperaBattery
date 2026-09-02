#include "ChargerCan.h"
#include "Config.h"
#include "FlexCanCompat.h"

namespace ChargerCan {

namespace {

Stats g_stats;
FrameHandler g_handler = nullptr;
CAN_error_t g_lastError;
bool g_hasError = false;

void updateErrorStatus() {
    CAN_error_t next;
    for (uint8_t i = 0; i < 8 && CanCharger.error(next, false); i++) {
        g_lastError = next;
        g_hasError  = true;
        g_stats.errorEventCount++;
    }
}

void drainRx(uint16_t maxFrames) {
    CAN_message_t msg;
    uint16_t n = 0;
    while (n < maxFrames && CanCharger.available()) {
        if (!CanCharger.read(msg)) {
            break;
        }
        n++;
        g_stats.rxCount++;
        g_stats.lastRxMs = millis();
        g_stats.lastRxId = msg.id;
        if (g_handler) {
            g_handler(msg);
        }
    }
}

}  // namespace

void begin() {
    CanCharger.begin(kChargerCanBitrate);
}

void tick() {
    drainRx(kCanFrameProcessBudget);
    updateErrorStatus();
}

const Stats &stats() { return g_stats; }

uint32_t lastRxAgeMs() {
    if (g_stats.lastRxMs == 0) {
        return 0;
    }
    return millis() - g_stats.lastRxMs;
}

void setFrameHandler(FrameHandler handler) { g_handler = handler; }

int writeFrame(const CAN_message_t &msg) {
    const int tx = CanCharger.writeStatus(msg);
    g_stats.txCount++;
    if (tx <= 0) {
        g_stats.txFailCount++;
    }
    return tx;
}

}  // namespace ChargerCan
