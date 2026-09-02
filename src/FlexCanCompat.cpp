#include "FlexCanCompat.h"

FlexCanCompat Can0;
FlexCanCompatT<CAN2> CanCharger;

template <CAN_DEV_TABLE Bus>
void FlexCanCompatT<Bus>::begin(uint32_t baudrate, bool extendedIds) {
    if (!started_) {
        can_.begin();
        can_.setMaxMB(16);
        can_.disableFIFO();
        extended_ids_ = extendedIds;
        const FLEXCAN_IDE ide = extendedIds ? EXT : STD;
        for (uint8_t mb = 0; mb < 15; mb++) {
            can_.setMB(static_cast<FLEXCAN_MAILBOX>(mb), RX, ide);
        }
        can_.setMB(MB15, TX, ide);
        if (extendedIds) {
            can_.setMBFilter(ACCEPT_ALL);
        }
        started_ = true;
    }
    if (started_ && baudrate == current_baud_) {
        return;
    }
    can_.setBaudRate(baudrate, listen_only_ ? RX : TX);
    current_baud_ = baudrate;
}

template <CAN_DEV_TABLE Bus>
void FlexCanCompatT<Bus>::restart(uint32_t baudrate, bool listenOnly) {
    started_        = false;
    current_baud_   = 0;
    has_pending_    = false;
    listen_only_    = listenOnly;
    begin(baudrate, extended_ids_);
}

template <CAN_DEV_TABLE Bus>
bool FlexCanCompatT<Bus>::available() {
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

template <CAN_DEV_TABLE Bus>
bool FlexCanCompatT<Bus>::read(CAN_message_t &msg) {
    if (has_pending_) {
        msg          = pending_;
        has_pending_ = false;
        return true;
    }
    return can_.read(msg);
}

template <CAN_DEV_TABLE Bus>
int FlexCanCompatT<Bus>::writeStatus(const CAN_message_t &msg) {
    return can_.write(MB15, msg);
}

template <CAN_DEV_TABLE Bus>
uint32_t FlexCanCompatT<Bus>::getTXQueueCount() {
    return can_.getTXQueueCount();
}

template <CAN_DEV_TABLE Bus>
bool FlexCanCompatT<Bus>::error(CAN_error_t &err, bool printDetails) {
    return can_.error(err, printDetails);
}

template class FlexCanCompatT<CAN3>;
template class FlexCanCompatT<CAN2>;
