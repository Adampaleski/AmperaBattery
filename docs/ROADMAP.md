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
| 2 | Monitor + decode two KICMs (36S) | `teensy41_k112_36s` |
| 3 | Balance TX, charge TX, contactors, faults | Not started — see VoltBMS_Teensy41 for reference |
| 4 | Four parallel 36S packs (pack ID, separate CAN or addressing) | Future |

Phase 4 likely needs either four CAN transceivers, a switched bus, or one bus with clear per-string addressing — design when hardware is fixed.
