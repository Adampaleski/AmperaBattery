#include "FlexCanCompat.h"
#include "StringConfig.h"
#if DUAL_STRING
#include "Link.h"
#include "ChargerCan.h"
#include "Config.h"
#include "ContactorSeq.h"
#include "StringConfig.h"

namespace Link {
namespace {
uint8_t  g_hb = 0;
uint32_t g_txMs = 0, g_tx = 0, g_txFail = 0, g_rx = 0;

struct Remote {
    StringMon::Summary s;
    uint8_t  hb = 0;
    bool     seen = false;
    uint32_t lastMs = 0;  // last time the heartbeat ADVANCED
};
Remote   g_rem[StringMon::kStrings];
uint8_t  g_remCells[StringMon::kStrings] = {};
[[maybe_unused]] uint32_t g_peerMs = 0;
[[maybe_unused]] bool     g_peerSeen = false;
[[maybe_unused]] uint8_t  g_peerState = 0, g_peerFaults = 0, g_peerHb = 0;

[[maybe_unused]] void put16(uint8_t *p, uint32_t v) { if (v > 0xFFFF) v = 0xFFFF; p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; }
[[maybe_unused]] uint16_t get16(const uint8_t *p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

void send(uint32_t id, const uint8_t *b) {
#if LINK_TX_ENABLE
    CAN_message_t m{};
    m.id = id;
    m.flags.extended = 1;
    m.len = 8;
    memcpy(m.buf, b, 8);
    g_tx++;
    if (ChargerCan::writeFrame(m) <= 0) g_txFail++;
#else
    (void)id; (void)b;
#endif
}

uint32_t g_staleMs = LINK_STALE_MS;
bool stale(const Remote &r) { return !r.seen || (millis() - r.lastMs) > g_staleMs; }
}  // namespace

void begin() {
#if !ROLE_IS_A && B_FAULT_PIN_ENABLE
    digitalWrite(Pins::B_FAULT_OUT, LOW);   // fault until proven healthy
    pinMode(Pins::B_FAULT_OUT, OUTPUT);
#endif
}

void tick() {
    const uint32_t now = millis();
#if !ROLE_IS_A && B_FAULT_PIN_ENABLE
    const uint8_t f = (StringMon::summary(0).faults | StringMon::summary(1).faults) & StringMon::kFaultMask;
    digitalWrite(Pins::B_FAULT_OUT, f ? LOW : HIGH);
#endif
    if (now - g_txMs < LINK_PERIOD_MS) return;
    g_txMs = now;
    g_hb++;
    uint8_t b[8];
#if ROLE_IS_A
    memset(b, 0, 8);
    b[0] = g_hb;
    b[1] = static_cast<uint8_t>(ContactorSeq::state());
    b[2] = (StringMon::summary(0).faults | StringMon::summary(1).faults) & StringMon::kFaultMask;
    send(kLinkIdAStatus, b);
#else
    for (int s = 0; s < StringMon::kStrings; s++) {
        const StringMon::Summary r = StringMon::summary(s);
        put16(b + 0, static_cast<uint32_t>(r.v * 100.0f + 0.5f));
        put16(b + 2, static_cast<uint32_t>(r.minV * 1000.0f + 0.5f));
        put16(b + 4, static_cast<uint32_t>(r.maxV * 1000.0f + 0.5f));
        b[6] = r.faults;
        b[7] = g_hb;
        send(s == 0 ? kLinkIdBString1 : kLinkIdBString2, b);
    }
    memset(b, 0, 8);
    b[0] = g_hb;
    b[1] = 1;
    b[2] = StringMon::summary(0).cells;
    b[3] = StringMon::summary(1).cells;
    b[4] = (StringMon::summary(0).faults | StringMon::summary(1).faults) & StringMon::kFaultMask;
    b[5] = B_FAULT_PIN_ENABLE ? 1 : 0;
    send(kLinkIdBStatus, b);
#endif
}

void onFrame(const CAN_message_t &m) {
    if (!m.flags.extended || m.len < 8) return;
    const uint32_t now = millis();
#if ROLE_IS_A
    int s = -1;
    if (m.id == kLinkIdBString1) s = 0;
    else if (m.id == kLinkIdBString2) s = 1;
    if (s >= 0) {
        g_rx++;
        Remote &r = g_rem[s];
        if (!r.seen || m.buf[7] != r.hb) r.lastMs = now;  // frozen counter does not refresh
        r.seen = true;
        r.hb = m.buf[7];
        r.s.v = get16(m.buf) / 100.0f;
        r.s.minV = get16(m.buf + 2) / 1000.0f;
        r.s.maxV = get16(m.buf + 4) / 1000.0f;
        r.s.faults = m.buf[6];
        r.s.expected = STRING_EXPECT_S;
    } else if (m.id == kLinkIdBStatus) {
        g_rx++;
        g_remCells[0] = m.buf[2];
        g_remCells[1] = m.buf[3];
    }
#else
    if (m.id == kLinkIdAStatus) {
        g_rx++;
        if (!g_peerSeen || m.buf[0] != g_peerHb) g_peerMs = now;
        g_peerSeen = true;
        g_peerHb = m.buf[0];
        g_peerState = m.buf[1];
        g_peerFaults = m.buf[2];
    }
#endif
}

StringMon::Summary remote(int s) {
    StringMon::Summary out;
    if (s < 0 || s >= StringMon::kStrings) return out;
    out = g_rem[s].s;
    out.cells = g_remCells[s];
    if (stale(g_rem[s])) out.faults |= StringMon::kFLinkStale;
    return out;
}

uint8_t remoteFaults() {
    return (remote(0).faults | remote(1).faults) & StringMon::kFaultMask;
}

bool peerAlive() { return g_peerSeen && (millis() - g_peerMs) <= g_staleMs; }
void setStaleMs(uint32_t ms) { g_staleMs = ms; }

void print() {
    SERIALCONSOLE.print(F("LINK " VEH_NAME " @250k ext IDs 0x6A0-0x6AF  role="));
    SERIALCONSOLE.print(ROLE_IS_A ? 'A' : 'B');
    SERIALCONSOLE.print(F("  tx="));
    SERIALCONSOLE.print(g_tx);
    SERIALCONSOLE.print(F(" fail="));
    SERIALCONSOLE.print(g_txFail);
    SERIALCONSOLE.print(F(" rx="));
    SERIALCONSOLE.print(g_rx);
    SERIALCONSOLE.print(F("  tx_enable="));
    SERIALCONSOLE.println(LINK_TX_ENABLE);
#if ROLE_IS_A
    for (int s = 0; s < StringMon::kStrings; s++) {
        const StringMon::Summary r = remote(s);
        SERIALCONSOLE.print(F("REMOTE B STRING "));
        SERIALCONSOLE.print(s + 1);
        SERIALCONSOLE.print(F(" cells="));
        SERIALCONSOLE.print(r.cells);
        SERIALCONSOLE.print(F(" V="));
        SERIALCONSOLE.print(r.v, 2);
        SERIALCONSOLE.print(F(" min="));
        SERIALCONSOLE.print(r.minV, 3);
        SERIALCONSOLE.print(F(" max="));
        SERIALCONSOLE.print(r.maxV, 3);
        SERIALCONSOLE.print(F(" hb="));
        SERIALCONSOLE.print(g_rem[s].hb);
        SERIALCONSOLE.print(F(" age="));
        if (g_rem[s].seen) SERIALCONSOLE.print(millis() - g_rem[s].lastMs); else SERIALCONSOLE.print('-');
        SERIALCONSOLE.print((r.faults & StringMon::kFaultMask) ? F("ms  FAULT ") : F("ms  OK "));
        StringMon::printFaults(r.faults);
        SERIALCONSOLE.println();
    }
#else
    SERIALCONSOLE.print(F("PEER A "));
    SERIALCONSOLE.print(peerAlive() ? F("alive") : F("STALE/none (info only on B)"));
    SERIALCONSOLE.print(F(" contactor_state="));
    SERIALCONSOLE.print(ContactorSeq::stateName(static_cast<ContactorSeq::State>(g_peerState)));
    SERIALCONSOLE.print(F(" faults=0x"));
    SERIALCONSOLE.println(g_peerFaults, HEX);
    SERIALCONSOLE.print(F("B fault pin "));
    SERIALCONSOLE.print(Pins::B_FAULT_OUT);
    SERIALCONSOLE.println(B_FAULT_PIN_ENABLE ? F(" ENABLED (HIGH=ok)") : F(" disabled (B_FAULT_PIN_ENABLE=0)"));
#endif
}
}  // namespace Link

#endif  // DUAL_STRING
