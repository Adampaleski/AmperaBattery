# Kelly KLS8080 NPS — CAN ingest plan (Teensy CAN2)

## Decision (not serial)

Kelly **KLS8080 NPS** already **broadcasts** motor telemetry on CAN:

| Item | Value |
|------|-------|
| Speed | **250 kbit/s** |
| Framing | J1939-style **29-bit** extended IDs |
| IDs | `0x0CF11E05` (RPM / current / V / errors), `0x0CF11F05` (throttle / temps) |
| Teensy bus | **CAN2** pins **0 RX / 1 TX** — **same bus as Elcon** |
| Transceiver | Existing SN65HVD230 #2 (shared) |
| Not used | BICM **CAN3 @ 125 kbit**, RS232 “serial sniffer”, separate bridge MCU |

Gap was never “how does Kelly talk?” — it was **decode those frames on the shared Elcon CAN2**. Stale docs that say serial/TBD interface are wrong; fix them here and in `teryx-ev-hub`.

## Firmware hooks (this branch)

| Piece | Path | Default |
|-------|------|---------|
| ID + layout notes | `include/KellyKls.h` | — |
| RX counter + optional unpack | `src/KellyDecode.*` | `BMS_CAP_KELLY_RX=0` |
| Wired into | `ChargerCan` frame handler (alongside Elcon `ChargeTx`) | listen-only |
| Serial | `y` prints frame counts + last raw 8 bytes | always |
| Unpack scales | gated by `BMS_CAP_KELLY_RX` | **0** |

**Does not** TX to Kelly. **Does not** touch `BMS_CAP_BALANCE_TX` / `CHARGE_TX` / `CONTACTOR_DRV` (stay 0).

## Shared-bus notes

- Elcon TX remains gated by `BMS_CAP_CHARGE_TX=0`. Kelly RX works with Elcon silent.
- Terminate CAN2 once (120 Ω). Both nodes + Teensy on the same twisted pair.
- If Elcon floods the bus later, Kelly frames still land in the same RX drain (`ChargerCan::tick`).

## Byte layout (verify before enabling unpack)

Public NPS map (treat as **hypothesis** until candump confirms):

**`0x0CF11E05`** DLC 8 LE: RPM u16, motor current i16 (0.1 A), battery V u16 (0.1 V), error u8, error bits u8.

**`0x0CF11F05`** DLC 8: throttle u8, controller temp u8, motor temp u8, rest TBD.

With `BMS_CAP_KELLY_RX=0`, firmware only counts IDs and keeps raw bytes for `y`. Flip to 1 only after a bench candump matches.

## Bench next step

1. Flash `teensy41_k112_36s` (this branch) — caps stay 0.
2. Wire Kelly CANH/L onto the **Elcon** CAN2 pair (or Kelly alone if Elcon not present).
3. Key ignition / enable Kelly logic 12 V so NPS broadcast starts.
4. USB serial → press `y` — expect non-zero `0x0CF11E05` / `0x0CF11F05` counts and raw hex.
5. Optional: `c` candump on CAN2 path is Elcon-handler only today; use an external sniffer or add a one-line debug later if raw IDs never appear.
6. **Do not** set `BMS_CAP_KELLY_RX=1` until raw bytes match the map above.

## Hub schema

`drive.*` in STATUS_SCHEMA v1 stays `wired:false` until unpack is trusted and a UART/JSON emitter exists. Optional null stubs already OK.
