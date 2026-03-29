# ESP32 Dashboard Companion

This project adds a companion ESP32 to the Teensy 4.1 BMS controller.

## What it does

- Reads live BMS telemetry from the Teensy over a UART link
- Hosts a web dashboard over WiFi
- Supports browser-based OTA updates for the ESP32 dashboard firmware
- Stores network and charger-CAN settings in ESP32 NVS (editable from `/config`)
- Bridges Elcon charger CAN commands through an MCP2515 interface

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

## MCP2515 charger CAN bridge wiring

The ESP32 now drives an Elcon CAN bus through MCP2515.

- ESP32 `GPIO18` -> MCP2515 `SCK`
- ESP32 `GPIO19` -> MCP2515 `SO` / `MISO`
- ESP32 `GPIO23` -> MCP2515 `SI` / `MOSI`
- ESP32 `GPIO5` -> MCP2515 `CS`
- ESP32 `GPIO4` -> MCP2515 `INT`
- ESP32 `GND` -> MCP2515 `GND`
- MCP2515 CAN-H / CAN-L -> Elcon charger CAN-H / CAN-L

Important hardware note for many common `MCP2515 + TJA1050` modules:

- Many of these boards are 5V logic on SPI pins.
- ESP32 IO is not 5V tolerant.
- If your board is 5V logic, add logic-level shifting or use a 3.3V-safe CAN module.

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
- Config page: `/config`

## Telemetry link

The Teensy publishes one JSON line roughly once per second over `Serial1` at `115200` baud.

## Elcon command notes

- Default Elcon CAN ID is `0x1806E5F4` (extended).
- Default charger CAN bitrate is `250 kbps`.
- The bridge sends command frames periodically and falls back to zero-current commands when telemetry is stale.
