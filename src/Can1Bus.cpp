#include "StringConfig.h"
#if DUAL_STRING
#include "Can1Bus.h"
#include "Config.h"
#include "BmsCapabilities.h"
#include "FlexCanCompat.h"

namespace Can1Bus {
namespace {
FrameHandler g_h = nullptr;
uint32_t g_rx = 0, g_lastRx = 0, g_kaTx = 0, g_kaFail = 0, g_kaMs = 0, g_restartMs = 0;
bool g_started = false;
}  // namespace

void begin() {
    CanString2.begin(kCanBitrate, false);  // 125k, 11-bit; FlexCAN_T4 CAN1 = pins 22/23
    g_started = true;
    g_kaMs = millis() - kKeepAliveIntervalMs;
    g_restartMs = millis();
}

void tick() {
    if (!g_started) return;
    CAN_message_t m;
    for (uint16_t n = 0; n < kCanFrameProcessBudget && CanString2.available(); n++) {
        if (!CanString2.read(m)) break;
        g_rx++;
        g_lastRx = millis();
        if (g_h) g_h(m);
    }
    const uint32_t now = millis();
#if BMS_CAP_KEEPALIVE_TX
    if (now - g_kaMs >= kKeepAliveIntervalMs) {
        g_kaMs = now;
        if (CanString2.getTXQueueCount() < kKeepAliveMaxTxQueueDepth) {
            CAN_message_t ka{};
            ka.id = 0x200; ka.len = 3; ka.buf[0] = 0x02;
            g_kaTx++;
            if (CanString2.writeStatus(ka) <= 0) g_kaFail++;
        } else {
            g_kaFail++;
        }
    }
#endif
    // Same recovery idea as CAN3: no RX for 12 s -> restart controller (15 s apart).
    const uint32_t noRx = g_lastRx ? now - g_lastRx : now;
    if (noRx >= kCanRecoveryNoRxMs && now - g_restartMs >= kCanRecoveryRetryMs) {
        g_restartMs = now;
        CanString2.restart(kCanBitrate, false);
    }
}

void setFrameHandler(FrameHandler h) { g_h = h; }
uint32_t rxCount() { return g_rx; }
uint32_t keepAliveTx() { return g_kaTx; }
uint32_t keepAliveFail() { return g_kaFail; }
uint32_t lastRxAgeMs() { return g_lastRx ? millis() - g_lastRx : 0; }

void printStatus() {
    SERIALCONSOLE.print(F("CAN1 (22/23) @125k string2: rx="));
    SERIALCONSOLE.print(g_rx);
    SERIALCONSOLE.print(F(" last_rx_age="));
    SERIALCONSOLE.print(lastRxAgeMs());
    SERIALCONSOLE.print(F("ms keepalive tx="));
    SERIALCONSOLE.print(g_kaTx);
    SERIALCONSOLE.print(F(" fail="));
    SERIALCONSOLE.println(g_kaFail);
}
}  // namespace Can1Bus

#endif  // DUAL_STRING
