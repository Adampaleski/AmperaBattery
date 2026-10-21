# BICM temperatures (2026-10-21, CREW REVIEW before any flash)

Gate: `TEMPS_VERIFIED=0` (Config.h) stays 0. Until it is 1, temps are decoded, forwarded and printed, but BmsCore ignores them for drive (charge stays refused, TEMPS-NOT-VERIFIED).

## Decode (UNVERIFIED)
- Frames 0x7E0–0x7EF on each BICM bus (11-bit). StringMon keeps bytes 6–7 per ID per bus.
- Temp = (b6<<8 | b7) × −0.0324 + 150 °C (Tom, VoltBMSV2 BMSModule.cpp 67–68; his resting capture gives 16.0–17.3 °C). **UNVERIFIED on our boards.** Alternative DBC decode `(b0&3)<<8|b1` × 0.0556 − 27.778 is also unverified (printed by the factory decoder).
- Which 0x7Ex ID belongs to which board, and which thermistor sits next to which cell: **UNVERIFIED**. "Hottest index" = thermistor frame n (0x7E0 + n) on that string's bus, not a cell number.
- Readings outside −40…120 °C are ignored. A string's temps are fresh if ≥1 plausible frame arrived within STRING_SILENT_MS (2000 ms).
- Frame rate of 0x7Ex is not measured: **UNVERIFIED**.

## Link 0x6A3 (B → A, 29-bit, every 100 ms)
`[0] S3 min °C+40  [1] S3 max °C+40  [2] S3 hottest idx  [3–5] same for S4  [6] bit0 S3 fresh, bit1 S4 fresh  [7] heartbeat`. 0xFF = none. 1 °C resolution.
A marks a remote string's temps fresh only if the 0x6A3 heartbeat advanced within the link stale limit (200 ms PROPOSED, `T linkMs`) and its fresh bit is set.

## BmsCore (acts only when TEMPS_VERIFIED=1)
- Pack min/max = over all used strings (A: S1/S2 local + S3/S4 via the link), so the heat derate (50 °C start) and the OVERTEMP cut (55 °C) cover all 4 strings. Hottest string + index in `Outputs.hotString/hotIdx`.
- Any used string without fresh temps → **F_TEMP_STALE** (bit 13, drive fault: mains open, charger off, logged).
- Sim scenarios: string 4 at 52 °C → derate, stays closed; 60 °C → open; 0x6A3 missing → TEMP-STALE open in ~210 ms.
