# Ampera BICM — Teensy 4.1

Clean restart for **Chevy Volt / Opel Ampera Gen1** BICM bench work on a **24S (4×6S)** pack.

Based on [Tom-evnut/AmperaBattery](https://github.com/Tom-evnut/AmperaBattery) documentation and lessons from prior Teensy 4.1 ports.

## Hardware

| Signal | Connection |
|--------|------------|
| BICM CAN | Teensy 4.1 **CAN3** — pin 30 RX, pin 31 TX @ 125 kbps |
| BICM transceiver | 3.3V SN65HVD230 (or TJA1051 / MCP2562) |
| Pack bus | K16 **X2**: pin 10 GND, 11 CAN_L, 12 CAN_H |
| Brusa CAN | Teensy 4.1 **CAN2** — pin 0 RX, pin 1 TX @ 500 kbps |
| Brusa transceiver | **Second** SN65HVD230 (required; CAN1 22/23 collides with OUT5/OUT6) |
| Contactors | OUT1/pin11 = main+, OUT2/pin12 = precharge, OUT4/pin21 = main− (FET gates) |
| Termination | 120 Ω at one end of each bus |

Do **not** connect pack 5V (X2 pin 9) to Teensy I/O.

## Build / flash

```bash
cd ~/Documents/PlatformIO/Projects/ampera-bicm-teensy41
pio run -e teensy41_sniffer
pio run -e teensy41_sniffer -t upload
pio device monitor -b 115200
```

BMS build (gated contactor GPIO only):

```bash
pio run -e teensy41_bms -t upload
```

## Serial commands

| Key | Action |
|-----|--------|
| `c` | Toggle candump (every frame: `ms,id,dlc,bytes...`) |
| `s` | Print CAN ID histogram |
| `r` | Reset sniffer counters |
| `k` | Toggle keep-alive TX (`0x200` @ 1 Hz) |
| `d` | Print decoded module/cell voltages |
| `b` | Print would-be balance `0x300`/`0x310` |
| `e` | Toggle dead-man (starts/aborts contactor sequence; dry-run when flag=0) |
| `p` | Print contactor state + intended vs driven pins |
| `g` | Toggle charge request (prints would-be Brusa `0x618`) |
| `?` | Help |

Every 500 ms a JSON summary line is printed (`rx_total`, `unique_ids`, `keepalive_tx`, etc.).

## Bench validation

With BICMs powered on X2:

1. Flash `teensy41_sniffer`, open monitor — expect JSON lines with `last_rx_age_ms` &lt; 500 within a few seconds.
2. Press `S` — expect IDs in `0x460–0x47F`, `0x470–0x47F`, `0x7E0–0x7EF`.
3. Press `K` to turn keep-alive **OFF** — traffic should stop within a few seconds; `K` again to restore.
4. Press `d` — expect `modules=4 cells=24 stable=Y` after ~3 s of good traffic.

Scripts: `scripts/monitor.sh`, `scripts/bench_checklist.md`.

## Reference

- [`docs/Volt_BMS.dbc`](docs/Volt_BMS.dbc) — cell/temp scaling
- [`docs/WIRING.md`](docs/WIRING.md) — CAN2 Brusa + contactor FET map
- [`docs/SAFETY.md`](docs/SAFETY.md) — capability flags stay 0
- Upstream wiring: AmperaBattery README (K16 X1 = 500k BECM, X2 = 125k BICM)
