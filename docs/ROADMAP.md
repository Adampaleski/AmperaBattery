# Roadmap (your target architecture)

## Target

- Up to **4× 36S strings in parallel** (144S pack-level, 4 parallel branches).
- Each **36S string**: **24S + 12S** = **2 physical KICMs** on one internal CAN daisy chain.
- **8 BICMs** total if every 36S string uses two KICMs (2 × 4).
- **6 BICMs** only if one 24S-capable KICM is shared across two strings electrically — confirm with pack design before counting on this.

## Software phases

| Phase | Scope | This repo today |
|-------|--------|----------------|
| 1 | Monitor + decode one K112 (24S) | Done (`teensy41_k112_24s`) |
| 2 | Monitor + decode two KICMs (36S) | Done (`teensy41_k112_36s`) |
| 3a | Balance TX for one 36S string (`0x300`/`0x310`) | Implemented, gated by `BMS_CAP_BALANCE_TX=0` — not production |
| 3b | Charge TX (Brusa NLG5 on CAN2) + precharge/contactor SM | Implemented, gated by `BMS_CAP_CHARGE_TX=0` and `BMS_CAP_CONTACTOR_DRV=0`. Serial dry-run: `e` / `p` / `g` |
| 4 | Four parallel 36S packs (pack ID, separate CAN or addressing) | Future — not started |

Default `teensy41_k112_36s` stays `TELEMETRY_ONLY=1`. Weekend bench does not need charger or coils.

Phase 4 likely needs either four CAN transceivers, a switched bus, or one bus with clear per-string addressing — design when hardware is fixed. Do not implement 4-string/4-bus in this tree yet.
