#include "BalanceTx.h"
#include <string.h>
#include "BicmDecode.h"
#include "BmsCapabilities.h"
#include "CanBus.h"
#include "Config.h"
#include "K112BalanceMap.h"
#if FACTORY_BUS_DECODE
#include "FactoryCellMap.h"
#include "FactoryBalancePlan.h"
#include "Settings.h"
#include "StringConfig.h"
#endif

// Balance is only possible with a verified bleed-bit map for the active decode.
#if FACTORY_BUS_DECODE
#define BALANCE_MAP_OK FACTORY_BALANCE_MAP_VERIFIED
#elif K112_PACK_DECODE
#define BALANCE_MAP_OK 1
#else
#define BALANCE_MAP_OK 0
#endif

namespace BalanceTx {

namespace {

uint32_t g_txOk   = 0;
uint32_t g_txFail = 0;

#if K112_PACK_DECODE && BALANCE_MAP_OK
bool validCell(float v) {
    return v > 0.5f && v < 5.5f;
}
#endif

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

#if FACTORY_BUS_DECODE && !DUAL_STRING
void printLine(const char *s) { SERIALCONSOLE.println(s); }

// Single-bus factory build: every reporting circuit on CAN3. Each board is
// its own min group unless FACTORY_SERIES_STRING (then one group).
void printFactoryPlan() {
    using namespace FactoryBalance;
    PlanInput in;
    for (int i = 0; i < kCircuits; i++) {
        const uint8_t m = kCircuit[i].module;
        uint8_t wired = 0, seen = 0, alarm = 0;
        for (int b = 0; b < kCircuit[i].cells; b++) {
            const uint8_t ch = static_cast<uint8_t>(kCircuit[i].firstCh + b);
            const BicmDecode::ChState st = BicmDecode::channelState(m, ch);
            if (st == BicmDecode::ChState::NotWired) continue;
            wired |= static_cast<uint8_t>(1u << b);
            if (st != BicmDecode::ChState::NoData) seen = 1;
            if (st == BicmDecode::ChState::Alarm) alarm = 1;
            in.v[i][b] = BicmDecode::channelVoltage(m, ch);
        }
        if (!wired || !seen) continue;  // empty circuit: not part of the plan
        in.use[i]   = 1;
        in.wired[i] = wired;
        in.fresh[i] = alarm ? 0 : 1;
        in.group[i] = FACTORY_SERIES_STRING ? 0 : m;
    }
    in.deadbandV  = Settings::deadbandV();
    in.lowCutoffV = Settings::lowCutoffV();
    in.minBleedV  = kBalanceMinVoltage;
    SERIALCONSOLE.print(F("BALANCE PLAN (PRINT-ONLY, nothing transmitted)  groups="));
    SERIALCONSOLE.println(FACTORY_SERIES_STRING ? F("one series string") : F("per board (g1=M1..g4=M4)"));
    const Plan p = evaluate(in);
    printPlan(printLine, in, p, "BUS CAN3");
}
#endif

}  // namespace

void begin() {
    g_txOk   = 0;
    g_txFail = 0;
}

int packFrames(uint8_t buf300[8], uint8_t buf310[5]) {
    k112ClearBalanceFrames(buf300, buf310);

#if !K112_PACK_DECODE || !BALANCE_MAP_OK
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
#if FACTORY_BUS_DECODE
    return;  // factory-ID bleed TX is NOT implemented: print-only planner ('b') only
#elif !BMS_CAP_BALANCE_TX || !BALANCE_MAP_OK
    return;  // no verified map -> never put 0x300/0x310 on the bus
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

#if !BMS_CAP_BALANCE_TX
    SERIALCONSOLE.println(
        F("*** BALANCE PRINT-ONLY  FLAG=0  DRIVE=OFF  CAN NOT fired ***"));
#endif
#if FACTORY_BUS_DECODE
    (void)marked;
    SERIALCONSOLE.println(
        F("BALANCE TX OFF — factory map from Tom's VoltBMSV2 is NOT bench-verified (FACTORY_BALANCE_MAP_VERIFIED=0)"));
#if !DUAL_STRING
    printFactoryPlan();
#endif
    return;
#endif
    SERIALCONSOLE.print(F("BALANCE map=K112-36S  FLAG="));
    SERIALCONSOLE.print(BMS_CAP_BALANCE_TX);
    SERIALCONSOLE.print(F("  wanted="));
    SERIALCONSOLE.print(marked);
#if BMS_CAP_BALANCE_TX
    SERIALCONSOLE.print(F("  tx_ok="));
    SERIALCONSOLE.print(g_txOk);
    SERIALCONSOLE.print(F("  tx_fail="));
    SERIALCONSOLE.print(g_txFail);
#endif
    SERIALCONSOLE.println();
    SERIALCONSOLE.print(F("  WOULD-BE 0x300#"));
    printHexPayload(buf300, kBalance300Len);
    SERIALCONSOLE.println();
    SERIALCONSOLE.print(F("  WOULD-BE 0x310#"));
    printHexPayload(buf310, kBalance310Len);
    SERIALCONSOLE.println();
#if !BMS_CAP_BALANCE_TX
    SERIALCONSOLE.println(
        F("  PRINT-ONLY — frames above are NOT written to CAN3 (BMS_CAP_BALANCE_TX=0)"));
#else
    SERIALCONSOLE.println(F("  LIVE TX — queued before each 0x200 keep-alive"));
#endif
}

}  // namespace BalanceTx
