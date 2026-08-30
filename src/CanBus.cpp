#include "CanBus.h"
#include "BalanceTx.h"
#include "Config.h"
#include "FlexCanCompat.h"

namespace CanBus {

namespace {

Stats g_stats;
bool  g_keepAliveEnabled = true;
uint32_t g_keepAliveLastMs = 0;
uint32_t g_recoveryLastMs  = 0;
FrameHandler g_handler     = nullptr;
CAN_error_t  g_lastError;
bool         g_hasError    = false;

void updateErrorStatus() {
    CAN_error_t next;
    for (uint8_t i = 0; i < 16 && Can0.error(next, false); i++) {
        g_lastError = next;
        g_hasError  = true;
        g_stats.errorEventCount++;
    }
}

void sendKeepAlive() {
    if (!g_keepAliveEnabled) {
        return;
    }

    const uint32_t q = Can0.getTXQueueCount();
    if (q > g_stats.txQueueHighWater) {
        g_stats.txQueueHighWater = q;
    }
    if (q >= kKeepAliveMaxTxQueueDepth) {
        g_stats.keepAliveSkipCount++;
        return;
    }

    // Queue bleed bits before the 0x200 trigger (OEM order: 0x300, 0x310, 0x200).
    // No-op when BMS_CAP_BALANCE_TX is 0.
    BalanceTx::sendQueuedBeforeKeepAlive();

    CAN_message_t keepAlive{};
    keepAlive.id  = 0x200;
    keepAlive.len = 3;
    keepAlive.buf[0] = 0x02;
    keepAlive.buf[1] = 0x00;
    keepAlive.buf[2] = 0x00;

    const int tx = Can0.writeStatus(keepAlive);
    g_stats.keepAliveTxCount++;
    if (tx <= 0) {
        g_stats.keepAliveFailCount++;
    }
}

void serviceRecovery() {
    const uint32_t now = millis();
    const uint32_t noRxAge =
        g_stats.lastRxMs == 0 ? now : now - g_stats.lastRxMs;

    if (noRxAge < kCanRecoveryNoRxMs) {
        return;
    }
    if (now - g_recoveryLastMs < kCanRecoveryRetryMs) {
        return;
    }

    g_recoveryLastMs = now;
    g_stats.recoveryCount++;

    SERIALCONSOLE.print(F("CAN recovery #"));
    SERIALCONSOLE.print(g_stats.recoveryCount);
    SERIALCONSOLE.print(F(" no_rx_ms="));
    SERIALCONSOLE.println(noRxAge);

    Can0.restart(kCanBitrate, false);
    g_stats.controllerRestarts++;
    g_keepAliveLastMs = millis() - kKeepAliveIntervalMs;
}

void drainRx(uint16_t maxFrames) {
    CAN_message_t msg;
    uint16_t n = 0;
    while (n < maxFrames && Can0.available()) {
        if (!Can0.read(msg)) {
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
    Can0.begin(kCanBitrate);
    g_keepAliveLastMs = millis() - kKeepAliveIntervalMs;
    g_recoveryLastMs  = millis();
}

void tick() {
    drainRx(kCanFrameProcessBudget);
    updateErrorStatus();

    const uint32_t now = millis();
    if (g_keepAliveEnabled && (now - g_keepAliveLastMs >= kKeepAliveIntervalMs)) {
        g_keepAliveLastMs = now;
        sendKeepAlive();
    }

    serviceRecovery();
}

bool keepAliveEnabled() { return g_keepAliveEnabled; }

void setKeepAliveEnabled(bool on) { g_keepAliveEnabled = on; }

const Stats &stats() { return g_stats; }

uint32_t lastRxAgeMs() {
    if (g_stats.lastRxMs == 0) {
        return 0;
    }
    return millis() - g_stats.lastRxMs;
}

void setFrameHandler(FrameHandler handler) { g_handler = handler; }

int writeFrame(const CAN_message_t &msg) {
    const uint32_t q = Can0.getTXQueueCount();
    if (q > g_stats.txQueueHighWater) {
        g_stats.txQueueHighWater = q;
    }
    const int tx = Can0.writeStatus(msg);
    if (msg.id == 0x300 || msg.id == 0x310) {
        g_stats.balanceTxCount++;
        if (tx <= 0) {
            g_stats.balanceFailCount++;
        }
    }
    return tx;
}

}  // namespace CanBus
