# Teensy 4.1 First Wiring Plan

Start with the smallest possible test setup.

## Goal

Bring up the Volt/Ampera module-slave CAN bus at 125 kbps and confirm the Teensy can see frames.

Do not start with contactors, charger outputs, or current sensors.

## Hardware you need

1. Teensy 4.1
2. A CAN transceiver module that is safe for 3.3V MCU I/O
3. USB cable for programming
4. Safe low-voltage access to the pack internal CAN wiring
5. Common ground between the Teensy side and the CAN transceiver side

## Teensy wiring for the first test

Use CAN3 on the Teensy 4.1 for this branch.

- Teensy pin 30 = `CRX3` = CAN receive from transceiver TXD/RX output path
- Teensy pin 31 = `CTX3` = CAN transmit to transceiver TXD input path
- Teensy `GND` to transceiver `GND`
- Teensy `3.3V` or suitable module supply as required by your transceiver board

Exact transceiver pin labels vary by module, so match them by function:

- Teensy `CTX3` goes to the transceiver logic input from MCU TX
- Teensy `CRX3` goes to the transceiver logic output back to MCU RX
- Transceiver bus pins go to CAN-H and CAN-L on the battery side

## Battery side for the first test

The repo README documents the internal module CAN bus as:

- X2 pin 9 = 5V
- X2 pin 10 = Ground
- X2 pin 11 = CAN_Low
- X2 pin 12 = CAN_High

This branch currently brings up the internal bus at `125000` baud.

## Important safety notes

1. Teensy 4.1 pins are not 5V tolerant.
2. Do not connect battery-side 5V or any unknown signal directly to a Teensy I/O pin.
3. Only connect CAN through a proper transceiver.
4. Do not connect pack high voltage to the Teensy setup.

## What to do first

1. Wire Teensy 4.1 to one CAN transceiver.
2. Wire that transceiver to the battery internal CAN bus.
3. Power the Teensy from USB only for the first bench test unless your transceiver board requires a different safe supply arrangement.
4. Load the `teensy41` PlatformIO environment.
5. Confirm serial output and CAN activity before adding any other wiring.

## What comes later

After CAN receive is confirmed, the next pass will map:

1. Current sensor inputs
2. Digital inputs
3. Contactor and charger outputs
4. Optional second CAN bus for BECM traffic at 500 kbps

## ESP32 companion wiring

The new web dashboard and OTA companion uses a separate ESP32 over UART.

- Teensy 4.1 pin `1` = `TX1` -> ESP32 `GPIO16` = `RX2`
- Teensy 4.1 pin `0` = `RX1` -> ESP32 `GPIO17` = `TX2`
- Teensy `GND` -> ESP32 `GND`

Notes:

1. Both boards use 3.3V logic, so this UART link is direct.
2. During first bench testing, power the Teensy and ESP32 from their own USB connections and only share `GND` plus the two UART wires.
3. The ESP32 dashboard does not replace the CAN transceiver. The SN65HVD230 still connects to Teensy CAN3 on pins `30` and `31`.
4. OTA in this branch applies to the ESP32 dashboard firmware. The Teensy BMS firmware is still updated over USB.

## Optional charger CAN bridge (ESP32 + MCP2515)

If your charger CAN bitrate differs from the module bus, keep module comms on Teensy CAN3 and use ESP32 + MCP2515 for charger CAN.

- ESP32 `GPIO18` -> MCP2515 `SCK`
- ESP32 `GPIO19` -> MCP2515 `MISO`
- ESP32 `GPIO23` -> MCP2515 `MOSI`
- ESP32 `GPIO5` -> MCP2515 `CS`
- ESP32 `GPIO4` -> MCP2515 `INT`
- Shared `GND`
- MCP2515 `CAN_H` / `CAN_L` -> charger CAN bus

Safety warning:

1. Many low-cost `MCP2515 + TJA1050` boards are 5V SPI logic and are not directly safe for ESP32 IO.
2. Use a level shifter or a 3.3V-safe CAN module if your board does not provide 3.3V-safe SPI levels.
