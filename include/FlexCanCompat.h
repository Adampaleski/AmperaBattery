#pragma once

#include <Arduino.h>
#include <FlexCAN_T4.h>

// PlatformIO link fix for some FlexCAN_T4 revisions.
inline uint8_t FlexCAN_T4_Base::getFirstTxBoxSize() {
    return 8;
}

struct CAN_message_flags_t {
    uint8_t extended = 0;
    uint8_t remote   = 0;
    uint8_t overrun  = 0;
};

// Thin wrapper: RX mailboxes 0–14, TX on MB15, FIFO off (bench-proven layout).
class FlexCanCompat {
public:
    void begin(uint32_t baudrate);
    void restart(uint32_t baudrate, bool listenOnly = false);

    bool available();
    bool read(CAN_message_t &msg);
    int  writeStatus(const CAN_message_t &msg);
    uint32_t getTXQueueCount();
    bool error(CAN_error_t &err, bool printDetails = false);

private:
    FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_16> can_;
    CAN_message_t pending_{};
    bool has_pending_     = false;
    bool started_         = false;
    uint32_t current_baud_ = 0;
    bool listen_only_     = false;
};

extern FlexCanCompat Can0;
