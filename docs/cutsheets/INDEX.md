# Cut sheets (links only)

Cut sheets for the wiring aid Parts panel. url = manufacturer document (official); mirror = distributor copy used when the maker's server blocks automated fetches. Specs here are copied only from the linked document (SOURCED) or the shop log/ledger; anything else says UNVERIFIED. Copies are NOT stored in the repo or gist: these PDFs are copyrighted by their makers with no redistribution licence stated, so we link only (sha256 recorded for the version checked). Verified 2026-10-22 (curl 200 + part number found in the first pages).

Check: `python3 scripts/check_cutsheets.py` (network). Shown in tools/wiring.html → Parts / tap a part.

| Part | Document | Link | Copy / note | sha256 (16) |
|---|---|---|---|---|
| Kelly KLS-8080N/NPS motor controller (owned: KLS8080 NPS, 72–168 V) | KLS-8080N/NPS/NA User Manual Rev 2.1.1 (Dec 2024) | https://media.kellycontroller.com/new/Kelly-KLS-8080NUserManuaV2.11.pdf |  | 22f67830ccbc3eef |
| TE KILOVAC EV200HAANA main contactor ×2 (owned) | KILOVAC EV200 Series datasheet | https://www.te.com/en/product-1-1618002-8.html | https://eveurope.eu/wp-content/uploads/2024/03/EV200HAANA_TE.pdf te.com returns 403 to automated fetches (opens in a browser); mirror checked 200 = same TE datasheet | 47458e2f15008e96 |
| Panasonic AEV14012 precharge / branch relay (Volt pack) | EV Relays (DC Contactors) catalog 2026.4 | https://industry.panasonic.com/ac/cdn/e/control/relay/vehicle/catalog/mech_eng_ev.pdf |  | ff46d63a9bf59a17 |
| Mersen A6T200 Class T fuse ×4 (bought 10/20) | A3T/A6T Class T fast-acting datasheet | https://www.mersen.com/sites/default/files/medias/PIM/files/DS-A3T-A6T-Class-T-Fast-Acting-Mersen.pdf |  | abd1c065df9695b0 |
| LEM DHAB S/44 current sensor ×2 (Volt pack) | DHAB S/44 datasheet | https://www.lem.com/en/dhab | https://www.thunderstruck-ev.com/images/companies/1/Display-Sensor/DHAB-S-44.pdf LEM's DHAB page lists current S/1xx models; the S/44 PDF is from a distributor (Thunderstruck) — same LEM document | 7ca61184daa3a559 |
| PJRC Teensy 4.1 ×2 | Teensy 4.1 product + pinout page | https://www.pjrc.com/store/teensy41.html |  | — |
| TI SN65HVD230 CAN transceiver (3.3 V) ×6 | SN65HVD23x datasheet | https://www.ti.com/lit/ds/symlink/sn65hvd230.pdf |  | e98fc0c59168c035 |
| Microchip MCP23017 I/O expander (BODY_IO_EXPANDER) | MCP23017/MCP23S17 datasheet DS20001952C | https://ww1.microchip.com/downloads/en/devicedoc/20001952c.pdf |  | 14159c6f5655e943 |
| Microchip MCP4131-103E/P digipot (throttle clamp U3) | MCP413X/415X/423X/425X datasheet | https://ww1.microchip.com/downloads/en/DeviceDoc/22060b.pdf |  | 6c60b083b2ef0a92 |
| TI CD74HC4066E analog switch (clamp U2a/U2b) | CD74HC4066 datasheet SCHS208E | https://www.ti.com/lit/ds/symlink/cd74hc4066.pdf |  | 337c5381f31a9b36 |
| TI CD74HCT123E dual one-shot (clamp U1) | CD74HC123/HCT123 datasheet | https://www.ti.com/lit/ds/symlink/cd74hct123.pdf |  | 062aa4848f2f6eae |
| TI SN74AHCT125N buffer (clamp U4) | SNx4AHCT125 datasheet SCLS264R | https://www.ti.com/lit/ds/symlink/sn74ahct125.pdf |  | 66da72e586dbca7a |
| Infineon IRLB8721PbF logic FET — coil-driver cd_fet Q-B+/Q-PRE/Q-B− (cutsheet:irlb8721; discontinued/EOL) | IRLB8721PbF datasheet (IR 04/2009) | https://www.infineon.com/assets/row/public/documents/24/49/infineon-irlb8721-datasheet-en.pdf | product page https://www.infineon.com/part/IRLB8721 | 8a313aecdd1e7725 |
| Infineon IRLB3813 logic FET — coil_fet Q4-Q6 cash-ledger to-buy (cutsheet:irlb3813; NOT the coil-driver FET) | IRLB3813PbF datasheet | https://www.infineon.com/assets/row/public/documents/24/49/infineon-irlb3813-datasheet-en.pdf?fileId=5546d462533600a4015356603cfe258b | product page https://www.infineon.com/part/IRLB3813 | 8406328ff856ce1a |
| Elcon HK-LF-144-46 charger (owned, CAN3865) | HK-LF 6.6 kW on-board charger user manual V1.3 | https://elconchargers.com/?page_id=93 | https://evshop.fr/en/index.php?controller=attachment&id_attachment=221 Elcon's page has no direct HK-LF PDF link; the manual PDF is distributor-hosted (EV Parts Group/evshop) | 0a20a3355f5df0f6 |
| Elcon TDC-JH-144-12 DC-DC (owned, CAN5100) | 1.5 kW DC/DC Converter User Manual Ve.161220 | https://evshop.eu/en/index.php?controller=attachment&id_attachment=10 | Maker's manual, distributor-hosted; no tccharger.com direct link found. Isolation (input↔12 V−) UNVERIFIED: do the isolation test before bonding 12 V− to chassis (shop log 10/15 20:46) | 2ed1c1c424dedf2d |
| E-stop: Harmony XB4/XB5 40 mm twist-release (XB4BS8442 head includes 1 NC) + 2× extra ZBE102 (crew spec, not bought) | ZBE102 contact block datasheet | https://static.datasheets.com/doc/13749203-schneider-zbe102-ds.pdf | 0.55 A @125 V DC-13, 10 A Ith; block 1 OK (0.26 A break measured); block 2 pending KSI | e52a10d9b77be132 |
| Dual-output Hall throttle pedal | **NO DATASHEET FOUND** | — | No part chosen yet (shop log 2026-10-21 20:03/20:04: part TBD; track 2 half-scale, 5 V). No datasheet. | — |
| Volt BICMs (M2/M3 24S, M4 18S) + Volt pack | **NO DATASHEET FOUND** | — | No public manufacturer datasheet (GM service info only). Pinout from the factory harness in repo docs (X2: 9 5 V, 10 GND, 11 CAN_L, 12 CAN_H). | — |
