# Safety — what this firmware does and does not do

## Safe to use on the bench today

| Feature | Status |
|---------|--------|
| Listen on 125k BICM CAN | Yes |
| Decode and print cell voltages | Yes |
| Send keep-alive `0x200` | Yes (required for BICMs to stay awake) |

Default builds use **`TELEMETRY_ONLY`**: no contactor coils, no charger commands, no balance commands.

## Does NOT run a production BMS (yet)

| Feature | Status | Risk if assumed |
|---------|--------|-----------------|
| Cell balancing (`0x300` / `0x310`) | **Not implemented** | Pack will not balance |
| Charger control | **Not implemented** | No charge current control |
| Contactor / precharge | **Stub only** in `teensy41_bms` (`E` enables GPIO — still your wiring) | Could close contactors if enabled |
| Multi-pack parallel (4× 36S) | **Not implemented** | One CAN bus / one decode context only |
| HV isolation / fault trips | **Not implemented** | Software will not open contactors on fault |

Treat this repo as a **monitor and bring-up tool** until a future “master BMS” phase adds charge, balance, and contactor logic with testing.

## Keep-alive

The Teensy sends master keep-alive so slaves wake up. That is normal for bench work and matches Volt BMS master behavior. It does **not** by itself enable charge or balancing.
