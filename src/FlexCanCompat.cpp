#include "FlexCanCompat.h"

FlexCanCompat Can0;

void FlexCanCompat::begin(uint32_t baudrate) {
    if (!started_) {
        can_.begin();
        can_.setMaxMB(16);
        can_.disableFIFO();
        for (uint8_t mb = 0; mb < 15; mb++) {
            can_.setMB(static_cast<FLEXCAN_MAILBOX>(mb), RX, STD);
        }
        can_.setMB(MB15, TX, STD);
        started_ = true;
    }
    if (started_ && baudrate == current_baud_) {
        return;
    }
    can_.setBaudRate(baudrate, listen_only_ ? RX : TX);
    current_baud_ = baudrate;
}

void FlexCanCompat::restart(uint32_t baudrate, bool listenOnly) {
    started_        = false;
    current_baud_   = 0;
    has_pending_    = false;
    listen_only_    = listenOnly;
    begin(baudrate);
}

bool FlexCanCompat::available() {
    if (has_pending_) {
        return true;
    }
    CAN_message_t next;
    if (can_.read(next)) {
        pending_     = next;
        has_pending_ = true;
        return true;
    }
    return false;
}

bool FlexCanCompat::read(CAN_message_t &msg) {
    if (has_pending_) {
        msg          = pending_;
        has_pending_ = false;
        return true;
    }
    return can_.read(msg);
}

int FlexCanCompat::writeStatus(const CAN_message_t &msg) {
    return can_.write(MB15, msg);
}

uint32_t FlexCanCompat::getTXQueueCount() {
    return can_.getTXQueueCount();
}

bool FlexCanCompat::error(CAN_error_t &err, bool printDetails) {
    return can_.error(err, printDetails);
}
