# Saturday bench — flash + monitor (36S)

One-page bring-up for **this branch tip**. Monitor / print-only only.  
**Do not** flip `BMS_CAP_BALANCE_TX`, `BMS_CAP_CHARGE_TX`, or `BMS_CAP_CONTACTOR_DRV` off 0.  
**Do not** reflash with those flags set. Serial **cannot** enable them (compile-time in `include/BmsCapabilities.h`).

Elcon charger and contactor coils are **not needed** this weekend.

## 1. Flash

```bash
cd /path/to/AmperaBattery
git checkout cursor/k112-24s-subpack-decode
pio run -e teensy41_k112_36s -t upload
pio device monitor -b 115200
```

Env **`teensy41_k112_36s`** keeps `TELEMETRY_ONLY=1` and all three TX/drive flags at 0.

## 2. Wiring minimum

| Item | Connection |
|------|------------|
| BICM CAN | Teensy **CAN3** pin 30 RX / 31 TX @ **125 kbit** via SN65HVD230 |
| Pack tap | K16 **X2**: GND pin 10, CAN_L 11, CAN_H 12 |
| 5 V daisy | BICM daisy 5 V as usual — **do not** feed pack 5 V (X2 pin 9) into Teensy I/O |
| Termination | **120 Ω** at one end of the BICM bus |

Leave **CAN2 / Elcon** and **coil FETs** unwired this weekend.

## 3. Expected boot (caps lines)

Look for something like:

```
=== Ampera BICM Teensy 4.1 ===
*** BUILD: MONITOR / DRY-RUN — FLAG=0  DRIVE=OFF  no live bleed/charge/coil ***
Pack: 36S / 2 physical BICM(s) on daisy chain
Mode: MONITOR — keep-alive + decode only; charge/contactor PRINT-ONLY
CAN3 @ 125k — keep-alive 0x200/1s  |  balance FLAG=0 PRINT-ONLY (b)
CAN2 @ 250k pins 0/1 — Elcon FLAG=0 PRINT-ONLY (g); bus optional this weekend
Capabilities: cells=1 keepalive=1 balance=0 charge=0 contactors=0
NOTE: BMS_CAP_BALANCE_TX / CHARGE_TX / CONTACTOR_DRV are compile-time.
```

If `balance` / `charge` / `contactors` are not **0**, stop — wrong build.

## 4. Get cells stable

1. Power BICMs; wait a few seconds with keep-alive on (default).
2. Press **`d`** (or wait for the 10 s auto report).
3. Goal: **`cells=36 stable=Y`** (and both BICM streams present).
4. If B cells stay 0: press **`s`**, check sparse IDs (`0x46D`–`0x47E`), see `docs/K112_36S.md`.

## 5. Print-only dry-runs (`b` / `e` / `p` / `g`)

These **print** would-be frames or pin wants. They do **not** TX bleed/charge or drive coils.

| Key | What you should see |
|-----|---------------------|
| `b` | `*** BALANCE PRINT-ONLY  FLAG=0  DRIVE=OFF  CAN NOT fired ***` then `WOULD-BE 0x300#…` / `0x310#…` |
| `e` | Dead-man dry-run; SM may advance **WANT** pins; `DRIVE=OFF` / coils stay LOW |
| `p` | Contactor state + `WANT` vs `DRIVEN` (driven stays `0/0/0`) |
| `g` | `*** CHARGE PRINT-ONLY … CAN2 NOT fired ***` then `WOULD-BE 0x1806E5F4#…` |

Any unknown key refuses and reminds you: flags are compile-time — reflash required to change them.

## 6. Do not (this weekend)

- Do **not** set `BMS_CAP_BALANCE_TX`, `BMS_CAP_CHARGE_TX`, or `BMS_CAP_CONTACTOR_DRV` to 1.
- Do **not** expect serial to enable live bleed, Elcon TX, or coil drive.
- Elcon on CAN2 and contactor coils: leave for later.

## Later (not this weekend)

- Full safety notes: [`docs/SAFETY.md`](SAFETY.md)
- Full wiring (CAN2 Elcon + FET map): [`docs/WIRING.md`](WIRING.md)
- Sparky’s Elcon/Kelly notes and Mechanic’s buy-list — when you actually wire charger / HV kit
