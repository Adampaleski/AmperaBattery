# Wiring (from AmperaBattery upstream)

## BICM internal CAN (125 kbps) — bench target

Connector **X2** on BECM K16:

| Pin | Signal |
|-----|--------|
| 9 | 5V (do not tie to Teensy) |
| 10 | Ground |
| 11 | CAN_Low |
| 12 | CAN_High |

Teensy 4.1: **CAN3** — pin 30 RX, pin 31 TX, transceiver GND common with pack.

## BECM vehicle CAN (500 kbps) — not used by this firmware

Connector **X1** — 500 kbps when accessory wake lines are held high. See upstream README.
