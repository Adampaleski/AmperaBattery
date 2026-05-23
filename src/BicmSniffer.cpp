#include "BicmSniffer.h"
#include "CanBus.h"
#include "Config.h"

namespace BicmSniffer {

namespace {

struct IdEntry {
    uint32_t id;
    uint32_t count;
};

IdEntry g_scan[kCanScanMaxIds];
uint8_t g_scanCount = 0;

bool g_canDebug      = false;
uint32_t g_summaryMs = 0;

uint32_t g_cellFrames  = 0;
uint32_t g_tempFrames  = 0;
uint32_t g_marker4e0   = 0;
uint32_t g_marker500   = 0;

void recordId(uint32_t id) {
    for (uint8_t i = 0; i < g_scanCount; i++) {
        if (g_scan[i].id == id) {
            g_scan[i].count++;
            return;
        }
    }
    if (g_scanCount < kCanScanMaxIds) {
        g_scan[g_scanCount].id    = id;
        g_scan[g_scanCount].count = 1;
        g_scanCount++;
    }
}

void printFrame(const CAN_message_t &msg) {
    SERIALCONSOLE.print(millis());
    SERIALCONSOLE.print(',');
    SERIALCONSOLE.print(msg.id, HEX);
    SERIALCONSOLE.print(',');
    SERIALCONSOLE.print(msg.len);
    for (uint8_t i = 0; i < msg.len; i++) {
        SERIALCONSOLE.print(',');
        if (msg.buf[i] < 0x10) {
            SERIALCONSOLE.print('0');
        }
        SERIALCONSOLE.print(msg.buf[i], HEX);
    }
    SERIALCONSOLE.println();
}

void printSummary() {
    const CanBus::Stats &s = CanBus::stats();
    SERIALCONSOLE.print(F("{\"rx_total\":"));
    SERIALCONSOLE.print(s.rxCount);
    SERIALCONSOLE.print(F(",\"unique_ids\":"));
    SERIALCONSOLE.print(g_scanCount);
    SERIALCONSOLE.print(F(",\"last_id\":\"0x"));
    SERIALCONSOLE.print(s.lastRxId, HEX);
    SERIALCONSOLE.print(F("\",\"last_rx_age_ms\":"));
    SERIALCONSOLE.print(CanBus::lastRxAgeMs());
    SERIALCONSOLE.print(F(",\"keepalive_tx\":"));
    SERIALCONSOLE.print(s.keepAliveTxCount);
    SERIALCONSOLE.print(F(",\"keepalive_on\":"));
    SERIALCONSOLE.print(CanBus::keepAliveEnabled() ? 1 : 0);
    SERIALCONSOLE.print(F(",\"cell_frames\":"));
    SERIALCONSOLE.print(g_cellFrames);
    SERIALCONSOLE.print(F(",\"temp_frames\":"));
    SERIALCONSOLE.print(g_tempFrames);
    SERIALCONSOLE.print(F(",\"marker_4e0\":"));
    SERIALCONSOLE.print(g_marker4e0);
    SERIALCONSOLE.print(F(",\"marker_500\":"));
    SERIALCONSOLE.print(g_marker500);
    SERIALCONSOLE.print(F(",\"recovery\":"));
    SERIALCONSOLE.print(s.recoveryCount);
    SERIALCONSOLE.println(F("}"));
}

}  // namespace

void begin() {
    g_summaryMs = millis();
}

void onFrame(const CAN_message_t &msg) {
    recordId(msg.id);

    if (msg.id >= 0x460 && msg.id < 0x480) {
        g_cellFrames++;
    } else if (msg.id >= 0x7E0 && msg.id < 0x7F0) {
        g_tempFrames++;
    } else if (msg.id == 0x4E0) {
        g_marker4e0++;
    } else if (msg.id == 0x500) {
        g_marker500++;
    }

    if (g_canDebug) {
        printFrame(msg);
    }
}

void tick() {
    if (millis() - g_summaryMs >= 500) {
        g_summaryMs = millis();
        printSummary();
    }
}

bool canDebugEnabled() { return g_canDebug; }

void setCanDebug(bool on) {
    g_canDebug = on;
    SERIALCONSOLE.println(on ? F("CAN candump ON") : F("CAN candump OFF"));
}

void printIdStats() {
    SERIALCONSOLE.println(F("--- CAN ID histogram ---"));
    for (uint8_t i = 0; i < g_scanCount; i++) {
        SERIALCONSOLE.print(F("0x"));
        SERIALCONSOLE.print(g_scan[i].id, HEX);
        SERIALCONSOLE.print(F("  count="));
        SERIALCONSOLE.println(g_scan[i].count);
    }
    SERIALCONSOLE.print(F("unique_ids="));
    SERIALCONSOLE.println(g_scanCount);
}

void resetStats() {
    g_scanCount  = 0;
    g_cellFrames = 0;
    g_tempFrames = 0;
    g_marker4e0  = 0;
    g_marker500  = 0;
    SERIALCONSOLE.println(F("Sniffer stats reset"));
}

}  // namespace BicmSniffer
