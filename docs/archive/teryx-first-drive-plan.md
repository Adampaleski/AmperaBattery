> SUPERSEDED 2026-10-20: see docs/TERYX_MAIN_POWER_PATH.md

# Teryx first drive — simple plan

One 36S string. Three 12S Volt bricks under the seats. Your Teensy is the BMS. No Vero. No four-pack. Flag stays off until a meter says a bleed bit is real.

Code is on GitHub: `Adampaleski/AmperaBattery` branch `cursor/k112-24s-subpack-decode` (`cd1bc46`). Mac can stay off.

## Locked build (so you don’t re-litigate)

| | |
|---|---|
| Vehicle | 2008 Teryx, engine deleted, motor **up front** |
| Motor / controller | Zero **75-7R**, Kelly **KLS8080 NPS** (sticker still needed) |
| Gearing | 16T → 32T **530** on #50 sprockets, stock 8/35 diffs → **8.75:1** |
| Tires | **27" all around** on 12s, matched. Kelly cap ~**6000 RPM** ≈ 55 mph |
| Pack | **36S** 2014 Volt NMC, **one string first** (~133 V nom / ~149 V full). 4p later |
| BMS | Your Teensy 4.1. Balance TX is in the tree, **compiled off** (`BMS_CAP_BALANCE_TX=0`) |

## Schematic

```
  5 V supply ──► K112 BICM (24S = two 12S bricks) ──daisy──► 12S BICM
                      │  CAN_H / CAN_L / GND  125 kbps
                      ▼
              3.3 V transceiver ◄── Teensy 4.1 CAN3
                                      pin 31 TX / 30 RX
              120 Ω across the bus

  Pack B+ ── fuse ── precharge ── + contactor ──► Kelly (up front) ── motor
  Pack B- ─────────────────────── − contactor ──┘
           contactors / fuse sit OFF the brick stack

  Kelly logic PWR: isolated 8–20 V. Never pack B+.
  BICM 5 V is NOT a Teensy pin. Never put X2 pin 9 (5 V) on the Teensy.
```

## Parts

**In hand**
- Three 12S bricks from the pile (not the mismatched pair from the other project)
- K112 + 12S BICM daisy
- Teensy 4.1 (CAN already talked May 24)
- 3.3 V CAN transceiver (was in circuit unless you pulled it)
- Zero 75-7R, Kelly, 16/32 sprockets, 530 chain

**Need (bin or buy)**
- 5 V supply for the BICM daisy (not 3.3 V)
- 120 Ω terminator
- Fuse on B+ (P/N still open)
- Precharge resistor around the + contactor
- + and − contactors (coil P/Ns still open)
- Isolated 8–20 V for Kelly logic
- Meter for the one bleed-bit check

## Bench order (do this, in this order)

1. Pick **three 12S** that sit at about the same voltage. Daisy K112 + 12S. **5 V on the daisy.**
2. Teensy + transceiver + 120 Ω. **No contactors yet.**
3. Serial: `cells=36 stable=Y` (you already had this May 24).
4. Serial `b` — dry-run. Flag is 0, nothing bleeds.
5. Meter on **one** bleed bit. Then Gilfoyle sets `BMS_CAP_BALANCE_TX=1`.
6. Only then: fuse, precharge, contactors, charger.

## What’s left in code (Gilfoyle)

1. Flag still 0 — on purpose until step 5.
2. Charger CAN (Brusa vs Eltek still open).
3. Real precharge/contactor sequence (`E` today is just a GPIO toggle).

That’s days, not a summer. Four-bus 4p stays on the shelf.

## Don’t

- Don’t buy Vero or a Daly.
- Don’t hang four strings on this one CAN bus.
- Don’t series bricks that are far apart in voltage.
- Don’t put 5 V or pack HV on Teensy pins.
