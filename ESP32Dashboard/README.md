# ESP32 Dashboard Companion

This project adds a companion ESP32 to the Teensy 4.1 BMS controller.

## What it does

- Reads live BMS telemetry from the Teensy over a UART link
- Hosts a web dashboard over WiFi
- Supports browser-based OTA updates for the ESP32 dashboard firmware

## What it does not do

- It does not change BMS safety logic. The Teensy still owns the battery logic.
- It does not OTA-update the Teensy firmware. Stock Teensy bootloading does not make that a simple drop-in feature.

## Default behavior

- If `kWifiSsid` and `kWifiPassword` are left blank in `include/dashboard_config.h`, the ESP32 starts its own access point:
  - SSID: `AmpBMS-Setup`
  - Password: `ampbms123`
- If WiFi credentials are filled in, the ESP32 tries station mode first and falls back to the access point if it cannot connect.

## Wiring

Use a direct 3.3V UART link between the Teensy 4.1 and ESP32.

- Teensy 4.1 `TX1` pin `1` -> ESP32 `GPIO16` (`RX2`)
- Teensy 4.1 `RX1` pin `0` -> ESP32 `GPIO17` (`TX2`)
- Teensy `GND` -> ESP32 `GND`

During first bench bring-up, power both boards from USB and only share ground and UART between them.

Keep the CAN transceiver wiring on the Teensy side:

- Teensy pin `30` = `CRX3`
- Teensy pin `31` = `CTX3`

## Build and upload

```bash
cd /Users/adampaleski/Documents/PlatformIO/Projects/AmperaBattery-teensy41/ESP32Dashboard
$HOME/.platformio/penv/bin/pio run -e esp32dev
$HOME/.platformio/penv/bin/pio run -e esp32dev -t upload
```

## Dashboard URLs

- AP mode: `http://192.168.4.1/`
- OTA page: `http://192.168.4.1/update`
- If station mode works and mDNS resolves: `http://ampbms.local/`

## Telemetry link

The Teensy publishes one JSON line roughly once per second over `Serial1` at `115200` baud.
