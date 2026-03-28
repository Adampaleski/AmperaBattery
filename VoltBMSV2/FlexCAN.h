#pragma once

#include <Arduino.h>
#include <FlexCAN_T4.h>

struct CAN_message_flags_t {
  uint8_t extended = 0;
  uint8_t remote = 0;
  uint8_t overrun = 0;
};

struct CAN_filter_flags_t {
  uint8_t extended = 0;
};

struct CAN_filter_t {
  CAN_filter_flags_t flags;
};

class CompatFlexCAN {
public:
  void begin(uint32_t baudrate) {
    can_.begin();
    can_.setBaudRate(baudrate);
  }

  void getFilter(CAN_filter_t &filter, uint8_t mailbox) {
    (void)mailbox;
    filter.flags.extended = 0;
  }

  void setFilter(const CAN_filter_t &filter, uint8_t mailbox) {
    (void)filter;
    (void)mailbox;
  }

  bool available() {
    if (has_pending_) {
      return true;
    }
    CAN_message_t next_msg;
    if (can_.read(next_msg)) {
      pending_ = next_msg;
      has_pending_ = true;
      return true;
    }
    return false;
  }

  bool read(CAN_message_t &msg) {
    if (has_pending_) {
      msg = pending_;
      has_pending_ = false;
      return true;
    }
    return can_.read(msg);
  }

  bool write(const CAN_message_t &msg) {
    return can_.write(msg);
  }

private:
  // Use CAN3 for the Teensy 4.1 port so the legacy sketch pins 22/23 remain
  // available for outputs. On Teensy 4.1 this maps to the dedicated CRX3/CTX3
  // pins used for first bring-up.
  FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_16> can_;
  CAN_message_t pending_;
  bool has_pending_ = false;
};

inline CompatFlexCAN Can0;
