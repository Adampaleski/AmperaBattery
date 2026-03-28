# VoltBMSV2 Teensy 4.1 Port Notes

This branch is the clean local restart for the Teensy 4.1 port.

Current known blockers:

1. `VoltBMSV2.ino` still includes `FlexCAN.h` and uses the old `Can0` API.
2. `VoltBMSV2.ino` includes `Serial_CAN_Module_TeensyS3.h`, but that file is not present in this repo.
3. The sketch hard-codes pins and serial ports for an older Teensy layout.
4. Reset and watchdog register usage may need Teensy 4.1-specific replacements.
5. The code currently initializes one CAN bus at 125 kbps, while the repo README also documents a separate 500 kbps BECM bus.

Immediate next steps:

1. Create a GitHub fork under your account and add it as `origin`.
2. Install PlatformIO locally so the new `platformio.ini` can be built and tested.
3. Replace or adapt the CAN layer for Teensy 4.1.
4. Restore, replace, or temporarily disable the serial-CAN adapter feature.
