# Components (final set)

No JK BMS. The two Teensy 4.1s are the BMS; Volt BICMs do the sensing. Per string: 24S BICM (M2 or M3) + M4 18S using 12S = 36S, 4 strings.

| Part | Notes | Owned |
|---|---|---|
| Teensy 4.1 A | BMS brain: S1 CAN3 30/31 (M2+M4), S2 CAN1 22/23 (M3+M4) @125k; CAN2 0/1 @250k; Serial4 16/17 to ESP32; contactor master | yes |
| Teensy 4.1 B | BMS brain: S3 CAN3 (M2+M4), S4 CAN1 (M3+M4) @125k; CAN2 A-B link | yes |
| Volt BICM M2 24S x2 | full 24S, strings 1 and 3 | yes |
| Volt BICM M3 24S x2 | full 24S, strings 2 and 4 | yes |
| Volt BICM M4 18S x2 | bottom+middle circuits (0x46C/0x46D) = 12S; 0x46E empty | yes |
| Volt BICM M4 18S x2 (extra) | needed for 4 strings | NO (red) |
| SN65HVD230 transceiver x4 | 3.3 V CAN, one per BICM bus | yes |
| Kelly KLS controller | CAN2 broadcast only | yes |
| Elcon charger HK-LF-144-46 | CAN2 250k | yes |
| Elcon DC-DC 12 V | after main+ | yes |
| ESP32 hub | Wi-Fi/Ethernet/BT; STATUS_SCHEMA v1 JSON from Teensy A Serial4 TX17/RX16 @115200 | yes |
| Surface 3 tablet (Linux) head unit | Wi-Fi to ESP32 | yes |
| EV200 main + |  | NO (red) |
| EV200 main - |  | NO (red) |
| Mersen A6T150 Class T x4 | one per string | NO (red) |
| Precharge relay + 50 ohm resistor |  | yes |
| LEM DHAB S/44 | Volt current sensor | yes |
| Volt AEV14012 x2 | optional charger / DC-DC branch | yes |
