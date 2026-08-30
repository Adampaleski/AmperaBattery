# Safety — what this firmware does and does not do

## Safe to use on the bench today

| Feature | Status |
|---------|--------|
| Listen on 125k BICM CAN | Yes |
| Decode and print cell voltages | Yes |
| Send keep-alive `0x200` | Yes (required for BICMs to stay awake) |

Default builds use **`TELEMETRY_ONLY`**: no contactor coils, no charger commands.
`BMS_CAP_BALANCE_TX` is **0** — `0x300` / `0x310` packing is compiled, but those frames are **not** sent. Flip the flag to `1` only after the K112 36S bit map has been probed on the real string.

## Does NOT run a production BMS (yet)

| Feature | Status | Risk if assumed |
|---------|--------|-----------------|
| Cell balancing (`0x300` / `0x310`) | **Implemented, gated off** (`BMS_CAP_BALANCE_TX=0`) | Pack will not balance until the flag is 1; even then this is not a production balancer |
| Charger control | **Not implemented** (`BMS_CAP_CHARGE_TX=0`) | No charge current control |
| Contactor / precharge | **Stub only** in `teensy41_bms` (`E` enables GPIO — still your wiring); `BMS_CAP_CONTACTOR_DRV=0` | Could close contactors if enabled |
| Multi-pack parallel (4× 36S) | **Not implemented** | One CAN bus / one decode context only |
| HV isolation / fault trips | **Not implemented** | Software will not open contactors on fault |

Treat this repo as a **monitor and bring-up tool** until a future “master BMS” phase adds charge, contactor logic, and tested balance on hardware. Enabling `BMS_CAP_BALANCE_TX` still does **not** make this a production BMS.

## Keep-alive

The Teensy sends master keep-alive (`0x200#02.00.00`) so slaves wake up. That is normal for bench work and matches Volt BMS master behavior. It does **not** by itself enable charge or balancing. When `BMS_CAP_BALANCE_TX=1`, `0x300`/`0x310` are queued immediately before each keep-alive (OEM order: queue bleed, then trigger).
