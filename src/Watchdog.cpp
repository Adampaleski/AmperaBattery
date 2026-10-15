#include "Watchdog.h"
#include <Arduino.h>

namespace Watchdog {

namespace {
uint32_t g_srsr    = 0;
bool     g_running = false;

constexpr uint32_t kUnlockKey  = 0xD928C520u;
constexpr uint32_t kRefreshKey = 0xB480A602u;
constexpr uint32_t kLpoHz      = 32000u;  // WDOG_CS_CLK(1) = LPO on RT1062
}  // namespace

void captureResetCause() {
    g_srsr   = SRC_SRSR;
    SRC_SRSR = g_srsr;  // write-1-to-clear so the next boot reads fresh
}

bool lastResetWasWatchdog() {
    return (g_srsr & (SRC_SRSR_WDOG3_RST_B | SRC_SRSR_WDOG_RST_B)) != 0;
}

uint32_t lastResetRaw() { return g_srsr; }

void begin(uint32_t timeoutMs) {
    uint32_t ticks = (timeoutMs * kLpoHz) / 1000u;
    if (ticks < 256u) ticks = 256u;
    if (ticks > 0xFFFFu) ticks = 0xFFFFu;  // 16-bit TOVAL, no prescaler: max ~2.05 s

    CCM_CCGR5 |= CCM_CCGR5_WDOG3(3);
    __disable_irq();
    WDOG3_CNT = kUnlockKey;  // must be followed by config inside 255 bus clocks
    __enable_irq();
    while (!(WDOG3_CS & WDOG_CS_ULK)) {
    }
    WDOG3_WIN   = 0;
    WDOG3_TOVAL = ticks;
    WDOG3_CS    = WDOG_CS_CLK(1) | WDOG_CS_EN | WDOG_CS_UPDATE | WDOG_CS_CMD32EN;
    while (!(WDOG3_CS & WDOG_CS_RCS)) {
    }
    g_running = true;
}

void feed() {
    if (!g_running) return;
    __disable_irq();
    WDOG3_CNT = kRefreshKey;
    __enable_irq();
}

}  // namespace Watchdog
