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
| Kelly KLS-8080N (8080NPS series, Sin/Cos), 144 V / 550 A config | SOURCED: Adam's order #81746 (2025-05-03, $1,214.90): model line 72–168 V, field weakening, bidirectional anti-slip, W3-GT2 liquid-cooling heatsink, 35-pin plug-and-play cable, USB cable, KLS test box. CAN2 broadcast only. 36S full charge ≈148–149 V is inside the 72–168 V model range (SOURCED: order listing #81746). Overvoltage cutout ≈173 V (earlier research), adjustable in Kelly software, factory default UNVERIFIED | yes |
| Kelly coolant pump (Bosch 12 V) | PROPOSED: W3-GT2 loop on the stock Teryx radiator; on with key; unassigned Teensy B FET; fan relay at ~40 °C; no flow/temp sensor yet | NO |
| Seat heaters ×2 | likely aftermarket kit, stages UNVERIFIED; PROPOSED MOSFET PWM 33/66/100 %; ~4–5 A each, 10 A fuse (ESTIMATE) | ? |
| Elcon charger HK-LF-144-46 | CAN2 250k | yes |
| Elcon DC-DC 12 V | after main+ | yes |
| ESP32 hub | Wi-Fi/Ethernet/BT; STATUS_SCHEMA v1 JSON from Teensy A Serial4 TX17/RX16 @115200 | yes |
| Surface 3 tablet (Linux) head unit | Wi-Fi to ESP32 | yes |
| TE EV200HAANA main + | B+, 12–24 V economized coil | yes (bought 2026-10-20) |
| TE EV200HAANA main − | B−, 12–24 V economized coil | yes (bought 2026-10-20) |
| Mersen A6T200 200 A Class T x4 | one per string, 1/0 leads | yes (bought 2026-10-20) |
| Precharge relay Volt AEV14012 + 50 ohm resistor | resistor vs Kelly capacitance (2 mF UNVERIFIED) | yes |
| LEM DHAB S/44 | Volt current sensor | yes |
| Volt AEV14012 x2 | optional charger / DC-DC branch | yes |
