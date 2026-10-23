# Kelly KLS8080 NPS — CAN ingest plan (Teensy CAN3 vehicle bus)

## Decision (not serial)

Kelly **KLS8080 NPS** already **broadcasts** motor telemetry on CAN:

| Item | Value |
|------|-------|
| Speed | **250 kbit/s** |
| Framing | J1939-style **29-bit** extended IDs |
| IDs | `0x0CF11E05` (RPM / current / V / errors), `0x0CF11F05` (throttle / temps) |
| Teensy bus | **CAN3** pins **30 RX / 31 TX** @ **250k** — the shared **vehicle bus** `CAN-V` (Kelly + Elcon + Teensy A-B link). SOURCED: include/FlexCanCompat.h:42-45 (`CAN_LAYOUT_V2`, Adam 2026-10-21), docs/PINMAP.yaml `CAN-V` |
| Transceiver | SN65HVD230 **#5** on Teensy A (end, 120 Ω), **#6** on Teensy B (stub, no 120 Ω) — SOURCED docs/PINMAP.yaml `CAN-V` |
| Not used | BICM string buses **CAN1 22/23** and **CAN2 0/1 @ 125 kbit**, RS232 “serial sniffer”, separate bridge MCU |

Gap was never “how does Kelly talk?” — it was **decode those frames on the shared CAN3 vehicle bus**. Stale docs that say serial/TBD interface are wrong; fix them here and in `teryx-ev-hub`.

> Single-string envs (`CAN_LAYOUT_V2=0`, e.g. `teensy41_k112_36s`) still map the vehicle bus to CAN2 0/1 (FlexCanCompat.h:62-64). The Teryx build is dual-string (`teensy41_dual_a` / `_b`, `DUAL_STRING=1`), so the vehicle bus is **CAN3 30/31**.

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
- Terminate the CAN3 vehicle bus at its two ends only: 120 Ω at Teensy A and at the far end (Elcon built-in 120 Ω is UNVERIFIED: meter ~60 Ω powered off). Teensy B is a stub with no 120 Ω. All nodes on the same twisted pair (PINMAP `CAN-V`).
- If Elcon floods the bus later, Kelly frames still land in the same RX drain (`ChargerCan::tick`).

## Byte layout (verify before enabling unpack)

**2026-10-21:** superseded by the SOURCED Kelly protocol layout in docs/KELLY_CAN_ROLES.md (16-bit error field, unsigned 0-400 A current, temp offsets +40/+30, b4 direction, b5 switches).

Public NPS map (treat as **hypothesis** until candump confirms):

**`0x0CF11E05`** DLC 8 LE: RPM u16, motor current i16 (0.1 A), battery V u16 (0.1 V), error u8, error bits u8.

**`0x0CF11F05`** DLC 8: throttle u8, controller temp u8, motor temp u8, rest TBD.

With `BMS_CAP_KELLY_RX=0`, firmware only counts IDs and keeps raw bytes for `y`. Flip to 1 only after a bench candump matches.

## Bench next step

1. Flash `teensy41_dual_a` (PROPOSED env for the CAN3 vehicle bus; this branch; Adam does the flashing) — caps stay 0, TELEMETRY_ONLY stays 1.
2. Wire Kelly CANH/L onto the **CAN3 vehicle bus** pair (`CAN-V`, Teensy pins 30/31 via SN65HVD230 #5) — with or without the Elcon present.
3. Key ignition / enable Kelly logic 12 V so NPS broadcast starts.
4. USB serial → press `y` — expect non-zero `0x0CF11E05` / `0x0CF11F05` counts and raw hex.
5. Optional: `c` candump on the vehicle-bus path is Elcon-handler only today (UNVERIFIED that it follows the CAN3 remap); use an external sniffer or add a one-line debug later if raw IDs never appear.
6. **Do not** set `BMS_CAP_KELLY_RX=1` until raw bytes match the map above.

## Hub schema

`drive.*` in STATUS_SCHEMA v1 stays `wired:false` until unpack is trusted and a UART/JSON emitter exists. Optional null stubs already OK.
