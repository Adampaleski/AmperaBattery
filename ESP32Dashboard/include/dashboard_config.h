#pragma once

#include <Arduino.h>

// Leave these blank to boot the ESP32 as its own access point.
constexpr char kWifiSsid[] = "";
constexpr char kWifiPassword[] = "";

// Fallback AP settings used when WiFi credentials are blank or connection fails.
constexpr char kFallbackApSsid[] = "AmpBMS-Setup";
constexpr char kFallbackApPassword[] = "ampbms123";
constexpr char kDashboardHostname[] = "ampbms";

// Teensy <-> ESP32 UART link.
constexpr uint32_t kTeensyLinkBaud = 115200;
constexpr int kTeensyLinkRxPin = 16;  // ESP32 RX2, wire to Teensy TX1
constexpr int kTeensyLinkTxPin = 17;  // ESP32 TX2, wire to Teensy RX1

constexpr uint32_t kWifiConnectTimeoutMs = 15000;

