#include "KellyDecode.h"
#include "BmsCapabilities.h"
#include "Config.h"

namespace KellyDecode {

namespace {

KellyKls::Telemetry g_tel;

void copyRaw(uint8_t dst[8], const CAN_message_t &msg) {
    for (uint8_t i = 0; i < 8; i++) {
        dst[i] = (i < msg.len) ? msg.buf[i] : 0;
    }
}

uint16_t rdU16LE(const uint8_t *p) {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

int16_t rdI16LE(const uint8_t *p) {
    return static_cast<int16_t>(rdU16LE(p));
}

void unpackRpmFrame(const CAN_message_t &msg) {
    copyRaw(g_tel.lastRpmRaw, msg);
    g_tel.haveRpmFrame = true;
    g_tel.lastRpmMs = millis();
    g_tel.rpmFrameCount++;
#if BMS_CAP_KELLY_RX
    if (msg.len >= 7) {
        g_tel.rpm = rdU16LE(&msg.buf[0]);
        g_tel.motorCurrentDeciA = rdI16LE(&msg.buf[2]);
        g_tel.batteryDeciV = rdU16LE(&msg.buf[4]);
        g_tel.errorCode = msg.buf[6];
        g_tel.errorBits = (msg.len >= 8) ? msg.buf[7] : 0;
    }
#endif
}

void unpackThrottleFrame(const CAN_message_t &msg) {
    copyRaw(g_tel.lastThrottleRaw, msg);
    g_tel.haveThrottleFrame = true;
    g_tel.lastThrottleMs = millis();
    g_tel.throttleFrameCount++;
#if BMS_CAP_KELLY_RX
    if (msg.len >= 3) {
        g_tel.throttleRaw = msg.buf[0];
        g_tel.controllerTempRaw = msg.buf[1];
        g_tel.motorTempRaw = msg.buf[2];
    }
#endif
}

}  // namespace

void begin() {
    g_tel = KellyKls::Telemetry{};
}

void onFrame(const CAN_message_t &msg) {
    // CAN2 carries extended 29-bit Elcon + Kelly. Match by ID only.
    if (msg.id == KellyKls::kIdRpmCurrentV) {
        unpackRpmFrame(msg);
        return;
    }
    if (msg.id == KellyKls::kIdThrottleTemp) {
        unpackThrottleFrame(msg);
        return;
    }
}

void tick() {
    // Listen-only. No TX. No contactor/charge interaction.
}

const KellyKls::Telemetry &telemetry() { return g_tel; }

void printStatus() {
    SERIALCONSOLE.println(F("--- Kelly KLS8080 NPS (CAN2 @ 250k, share w/ Elcon) ---"));
    SERIALCONSOLE.print(F("  FLAG BMS_CAP_KELLY_RX="));
    SERIALCONSOLE.print(BMS_CAP_KELLY_RX);
#if BMS_CAP_KELLY_RX
    SERIALCONSOLE.println(F("  (unpack ON — verify scales vs candump)"));
#else
    SERIALCONSOLE.println(F("  (count+raw only; unpack OFF)"));
#endif
    SERIALCONSOLE.print(F("  0x0CF11E05 frames="));
    SERIALCONSOLE.print(g_tel.rpmFrameCount);
    SERIALCONSOLE.print(F(" age_ms="));
    SERIALCONSOLE.println(g_tel.lastRpmMs ? (millis() - g_tel.lastRpmMs) : 0);
    SERIALCONSOLE.print(F("  0x0CF11F05 frames="));
    SERIALCONSOLE.print(g_tel.throttleFrameCount);
    SERIALCONSOLE.print(F(" age_ms="));
    SERIALCONSOLE.println(g_tel.lastThrottleMs ? (millis() - g_tel.lastThrottleMs) : 0);

    SERIALCONSOLE.print(F("  last 1E05 raw:"));
    for (uint8_t i = 0; i < 8; i++) {
        SERIALCONSOLE.print(' ');
        if (g_tel.lastRpmRaw[i] < 16) {
            SERIALCONSOLE.print('0');
        }
        SERIALCONSOLE.print(g_tel.lastRpmRaw[i], HEX);
    }
    SERIALCONSOLE.println();
    SERIALCONSOLE.print(F("  last 1F05 raw:"));
    for (uint8_t i = 0; i < 8; i++) {
        SERIALCONSOLE.print(' ');
        if (g_tel.lastThrottleRaw[i] < 16) {
            SERIALCONSOLE.print('0');
        }
        SERIALCONSOLE.print(g_tel.lastThrottleRaw[i], HEX);
    }
    SERIALCONSOLE.println();

#if BMS_CAP_KELLY_RX
    SERIALCONSOLE.print(F("  rpm="));
    SERIALCONSOLE.print(g_tel.rpm);
    SERIALCONSOLE.print(F(" I_deciA="));
    SERIALCONSOLE.print(g_tel.motorCurrentDeciA);
    SERIALCONSOLE.print(F(" V_deciV="));
    SERIALCONSOLE.print(g_tel.batteryDeciV);
    SERIALCONSOLE.print(F(" err="));
    SERIALCONSOLE.print(g_tel.errorCode);
    SERIALCONSOLE.print(F("/"));
    SERIALCONSOLE.println(g_tel.errorBits);
    SERIALCONSOLE.print(F("  thr_raw="));
    SERIALCONSOLE.print(g_tel.throttleRaw);
    SERIALCONSOLE.print(F(" ctl_t="));
    SERIALCONSOLE.print(g_tel.controllerTempRaw);
    SERIALCONSOLE.print(F(" mot_t="));
    SERIALCONSOLE.println(g_tel.motorTempRaw);
#else
    SERIALCONSOLE.println(
        F("  Unpack gated. Flip BMS_CAP_KELLY_RX only after candump confirms layout."));
#endif
    SERIALCONSOLE.println(
        F("  No Kelly TX from this firmware. Caps balance/charge/contactor unchanged."));
}

}  // namespace KellyDecode
