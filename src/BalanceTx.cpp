#include "BalanceTx.h"
#include <string.h>
#include "BicmDecode.h"
#include "BmsCapabilities.h"
#include "CanBus.h"
#include "Config.h"
#include "K112BalanceMap.h"

namespace BalanceTx {

namespace {

uint32_t g_txOk   = 0;
uint32_t g_txFail = 0;

bool validCell(float v) {
    return v > 0.5f && v < 5.5f;
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

}  // namespace

void begin() {
    g_txOk   = 0;
    g_txFail = 0;
}

int packFrames(uint8_t buf300[8], uint8_t buf310[5]) {
    k112ClearBalanceFrames(buf300, buf310);

#if !K112_PACK_DECODE
    (void)buf300;
    (void)buf310;
    return 0;
#else
    const int n = PACK_S_CELLS;
    if (n < 1 || n > 36) {
        return 0;
    }
    if (!BicmDecode::packStable()) {
        return 0;
    }

    float cells[37] = {};
    uint8_t want[37] = {};
    float minV = 5.5f;
    float maxV = 0.0f;
    float sum  = 0.0f;
    int   nOk  = 0;

    for (int c = 1; c <= n; c++) {
        const float v = BicmDecode::cellVoltage(static_cast<uint8_t>(c));
        cells[c] = v;
        if (!validCell(v)) {
            continue;
        }
        nOk++;
        sum += v;
        if (v < minV) {
            minV = v;
        }
        if (v > maxV) {
            maxV = v;
        }
    }

    if (nOk < n) {
        return 0;
    }

    const float avg = sum / static_cast<float>(nOk);
    (void)avg;
    (void)maxV;

    for (int c = 1; c <= n; c++) {
        const float v = cells[c];
        if (!validCell(v)) {
            continue;
        }
        if (v < kBalanceMinVoltage) {
            continue;
        }
        if (v < minV + kBalanceHysteresis) {
            continue;
        }
        want[c] = 1;
    }

    k112DropAdjacent(want, cells, n);
    k112PackWanted(want, n, buf300, buf310);

    int marked = 0;
    for (int c = 1; c <= n; c++) {
        if (want[c]) {
            marked++;
        }
    }
    return marked;
#endif
}

void tick() {
#if !BMS_CAP_BALANCE_TX
    return;
#else
    // TX is driven from CanBus keep-alive so 0x300/0x310 go out before 0x200.
#endif
}

void sendQueuedBeforeKeepAlive() {
#if !BMS_CAP_BALANCE_TX
    return;
#else
    uint8_t buf300[8];
    uint8_t buf310[5];
    packFrames(buf300, buf310);

    CAN_message_t msg{};
    msg.id  = kBalanceCanId300;
    msg.len = kBalance300Len;
    memcpy(msg.buf, buf300, kBalance300Len);
    if (CanBus::writeFrame(msg) <= 0) {
        g_txFail++;
    } else {
        g_txOk++;
    }

    msg = CAN_message_t{};
    msg.id  = kBalanceCanId310;
    msg.len = kBalance310Len;
    memcpy(msg.buf, buf310, kBalance310Len);
    if (CanBus::writeFrame(msg) <= 0) {
        g_txFail++;
    } else {
        g_txOk++;
    }
#endif
}

void printStatus() {
    uint8_t buf300[8];
    uint8_t buf310[5];
    const int marked = packFrames(buf300, buf310);

    SERIALCONSOLE.print(F("BALANCE map=K112-36S  flag="));
    SERIALCONSOLE.print(BMS_CAP_BALANCE_TX);
    SERIALCONSOLE.print(F("  wanted="));
    SERIALCONSOLE.print(marked);
    SERIALCONSOLE.print(F("  tx_ok="));
    SERIALCONSOLE.print(g_txOk);
    SERIALCONSOLE.print(F("  tx_fail="));
    SERIALCONSOLE.println(g_txFail);
    SERIALCONSOLE.print(F("  0x300#"));
    printHexPayload(buf300, kBalance300Len);
    SERIALCONSOLE.println();
    SERIALCONSOLE.print(F("  0x310#"));
    printHexPayload(buf310, kBalance310Len);
    SERIALCONSOLE.println();
#if !BMS_CAP_BALANCE_TX
    SERIALCONSOLE.println(F("  (not TX — BMS_CAP_BALANCE_TX=0)"));
#else
    SERIALCONSOLE.println(F("  (TX queued before each 0x200 keep-alive)"));
#endif
}

}  // namespace BalanceTx
