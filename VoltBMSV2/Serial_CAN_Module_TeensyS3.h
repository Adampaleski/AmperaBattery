#pragma once

#include <stdint.h>

// Temporary compatibility shim for builds where the external serial-CAN
// adapter library is not present. This keeps the core BMS firmware building
// while the adapter feature is ported or restored separately.

enum SerialCanRate : uint8_t {
  CAN_RATE_250 = 0,
  CAN_RATE_500 = 1,
};

class Serial_CAN {
public:
  bool recv(unsigned long *id, unsigned char *data) {
    (void)id;
    (void)data;
    return false;
  }

  bool canRate(uint8_t rate) {
    (void)rate;
    return true;
  }

  void baudRate(uint8_t rate) {
    (void)rate;
  }

  void exitSettingMode() {}

  bool send(unsigned long id, uint8_t extended, uint8_t remote, uint8_t len, unsigned char *data) {
    (void)id;
    (void)extended;
    (void)remote;
    (void)len;
    (void)data;
    return true;
  }
};
