# Components (final set)

No JK BMS. The two Teensy 4.1s are the BMS; Volt BICMs do the sensing. Pack: 36S × 4P (SOURCED: Adam 2026-10-21). Current firmware map per string: 24S BICM (M2 or M3) + M4 18S using 12S = 36S, 4 strings = 8 BICMs on 4 BICM CAN buses (SOURCED: Adam 2026-10-21). Owned 6, buy 2 × M4. Transceivers: 6 needed (4 BICM + 2 vehicle bus). See docs/PACK_LAYOUT.md.

| Part | Notes | Owned |
|---|---|---|
| Teensy 4.1 A | BMS brain: S1 CAN1 22/23 (M2+M4), S2 CAN2 0/1 (M3+M4) @125k; CAN3 30/31 @250k Kelly/Elcon/link; Serial4 16/17 to ESP32; contactor master | yes |
| Teensy 4.1 B | BMS aux + bridge: S3 CAN1 (M2+M4), S4 CAN2 (M3+M4) @125k; CAN3 A-B link (vehicle bus) | yes |
| Volt BICM M2 24S x2 | full 24S, strings 1 and 3 | yes |
| Volt BICM M3 24S x2 | full 24S, strings 2 and 4 | yes |
| Volt BICM M4 18S x2 | bottom+middle circuits (0x46C/0x46D) = 12S; 0x46E empty | yes |
| Volt BICM M4 18S x2 (extra) | needed for 4 strings | NO (red) |
| SN65HVD230 transceiver x4 | 3.3 V CAN, one per BICM bus | yes |
| Throttle clamp: CD74HCT123E, CD74HC4066E, SN74AHCT125N + MCP4131-103E/P, R/C per docs/THROTTLE_CLAMP.md | Kelly throttle held at 0 V unless A AND B alive (crew 2026-10-21) | NO (PROPOSED, UNVERIFIED) |
| SN65HVD230 transceiver x2 | 4 BICM buses + vehicle bus CAN3 on A + B = 6 total (4 listed) (Elcon/Kelly/link) | UNVERIFIED: not listed as owned |
| Kelly KLS-8080N (8080NPS series, Sin/Cos), 144 V / 550 A config | SOURCED: Adam's order #81746 (2025-05-03, $1,214.90): model line 72–168 V, field weakening, bidirectional anti-slip, W3-GT2 liquid-cooling heatsink, 35-pin plug-and-play cable, USB cable, KLS test box. CAN2 broadcast only. 36S full charge ≈148–149 V is inside the 72–168 V model range (SOURCED: order listing #81746). Overvoltage cutout ≈173 V (earlier research), adjustable in Kelly software, factory default UNVERIFIED | yes |
| Kelly coolant pump (Bosch 12 V) | PROPOSED: W3-GT2 loop on the stock Teryx radiator; on with key; unassigned Teensy B FET; fan relay at ~40 °C; no flow/temp sensor yet | NO |
| Seat heaters ×2 | likely aftermarket kit, stages UNVERIFIED; PROPOSED MOSFET PWM 33/66/100 %; ~4–5 A each, 10 A fuse (ESTIMATE) | ? |
| Elcon charger HK-LF-144-46 | CAN3 250k (vehicle bus) | yes |
| Elcon DC-DC 12 V | after main+ | yes |
| ESP32 hub | Wi-Fi/Ethernet/BT; STATUS_SCHEMA v1 JSON from Teensy A Serial4 TX17/RX16 @115200 | yes |
| Surface 3 tablet (Linux) head unit | Wi-Fi to ESP32 | yes |
| TE EV200HAANA main + | B+, 12–24 V economized coil | yes (bought 2026-10-20) |
| TE EV200HAANA main − | B−, 12–24 V economized coil | yes (bought 2026-10-20) |
| Mersen A6T200 200 A Class T x4 | one per string, 1/0 leads | yes (bought 2026-10-20) |
| Precharge relay Volt AEV14012 + 50 ohm resistor | resistor vs Kelly capacitance (2 mF UNVERIFIED) | yes |
| LEM DHAB S/44 | Volt current sensor | yes |
| Volt AEV14012 x2 | optional charger / DC-DC branch | yes |
