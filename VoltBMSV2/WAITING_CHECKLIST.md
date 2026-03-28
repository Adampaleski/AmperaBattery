# While Waiting For The SN65HVD230

These are the useful things you can do before the CAN transceiver arrives.

## 1. Confirm the codebase is in the right place

Working repo:

- `/Users/adampaleski/Documents/PlatformIO/Projects/AmperaBattery-teensy41`

Working branch:

- `teensy41-port`

The branch is already pushed to your fork.

## 2. Confirm the Teensy is alive over USB

The current firmware was built, uploaded, and is exposing a USB serial port on:

- `/dev/cu.usbmodem184399501`

That means:

1. PlatformIO build works
2. Upload works
3. USB serial works

## 3. Know what "normal" looks like right now

Current serial output shows the firmware is running but sees no battery modules yet.

That is expected until the CAN transceiver and battery CAN wiring are connected.

Typical current output:

```text
BMS Status : 5 Error 3    0
Out:0000 Cont:0000 In:0100
Modules: 0 Cells: 0 Strings: 1  Voltage: 0.000V ...
CANbus   0.00mA  0% SOC ...
```

## 4. Gather the non-transceiver items now

Get these ready:

1. Male/female jumper wires
2. Small screw terminal wires or pigtails for the CAN module
3. A clean USB cable for the Teensy
4. A notebook or text file with your wiring notes

## 5. Study the battery CAN pinout now

From the repo README, the internal module CAN bus is:

1. X2 pin 9 = 5V
2. X2 pin 10 = Ground
3. X2 pin 11 = CAN_Low
4. X2 pin 12 = CAN_High

Do not plan to connect the battery-side 5V directly to the Teensy I/O.

## 6. Prepare your first-test wiring plan

For first bring-up, only plan these connections:

1. Teensy pin 30 = `CRX3`
2. Teensy pin 31 = `CTX3`
3. Teensy `GND`
4. Transceiver `CAN_H`
5. Transceiver `CAN_L`

Do not add contactors, charger outputs, or current sensors yet.

## 7. Save the useful commands

Build:

```bash
cd /Users/adampaleski/Documents/PlatformIO/Projects/AmperaBattery-teensy41/VoltBMSV2
$HOME/.platformio/penv/bin/pio run -e teensy41
```

Upload:

```bash
cd /Users/adampaleski/Documents/PlatformIO/Projects/AmperaBattery-teensy41/VoltBMSV2
$HOME/.platformio/penv/bin/pio run -e teensy41 -t upload
```

Monitor serial:

```bash
$HOME/.platformio/penv/bin/pio device monitor -p /dev/cu.usbmodem184399501 -b 115200
```

## 8. What happens when the module arrives

Next step after the SN65HVD230 arrives:

1. Wire the Teensy to the transceiver
2. Wire the transceiver to the battery internal CAN bus
3. Watch serial output for module traffic
4. Confirm module count and voltages start changing from zero
