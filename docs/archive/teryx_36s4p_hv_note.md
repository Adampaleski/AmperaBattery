> SUPERSEDED 2026-10-20: see docs/TERYX_MAIN_POWER_PATH.md

# Teryx EV: 36S4P HV note (fuses, precharge, string matching)
*Sparky, 2026-10-17. Marked **[UNVERIFIED]** = estimate, measure or confirm before relying on it.*

## 0. Check this first: which Kelly do you have?
The KLS-8080NPS comes in several voltage classes. The **120 V model's max operating voltage is 144 V**. A pack full at 4.05–4.10 V/cell is 146–148 V, so it would throw an overvoltage fault on every full charge. The **144 V model (KLS14xxx) allows up to 173 V**, which is fine. Read the model number off the sticker. ([Kelly KLS-8080N/NPS spec table](https://kellycontroller.com/shop/kls-8080n-nps/), [manual](https://www.cloudelectric.com/v/vspfiles/files/technical-documents/kelly/KLS/Kelly%20KLS8080N-NPS%20User%20Manual.pdf))
The 144 V models with a heatsink are rated **300 A or 400 A for 1 minute, and about 120–200 A continuous**. Further numbers below assume **400 A peak and 160–200 A continuous** at the battery. Battery current can be set lower in the Kelly software.

## 1. Fuse for each string
**Current per string** (sharing assumes matched strings; real sharing is within about ±10%):
| Case | Pack | Per string (4 strings) | Per string (1 string dropped, 3 left) |
|---|---|---|---|
| Peak, 1 min | 400 A | 100 A | 133 A |
| Continuous | 160–200 A | 40–50 A | 53–67 A |

String capacity is 45 Ah (36S3P of 15 Ah Gen1 cells, **[UNVERIFIED]** cell size, but that's the usual Gen1 figure), so 100 A is about 2.2C. That's fine for these cells.

**Short-circuit current** **[UNVERIFIED estimate]**: assume roughly 1 mΩ per 3P group, so a string is about 36 mΩ plus about 10 mΩ of links, ≈ 45 mΩ. One string shorting on its own gives about 148 V / 0.045 Ω ≈ **3.3 kA**. The worst case is a fault inside one string with the other three feeding into it as well: about **10–13 kA**. Every fuse needs a **DC interrupt rating of at least 20 kA**.

**200 A Class T is too big, and a common Class T is the wrong type:**
- **Littelfuse JLLN200 is rated only 125 V DC** (JLLN 70–1200 A: 125 V DC, 20 kA). That's below your 148 V. **Don't use it.** ([JLLN200](https://www.littelfuse.com/products/fuses-overcurrent-protection/fuses/ul-class-fuses/class-t-fuses/jlln/jlln200))
- Mersen A3T is rated 160 V DC / 50 kA. That's legal, but with little margin. ([A3T200](https://www.mersen.com/en/products/a3t-class-t-fast-acting/a3t200))
- **Pick:** a **300 V DC Class T in 150 A**:
  - **Mersen A6T150** (SUPERSEDED: final = 4× A6T200, bought 2026-10-20): A6T series is 300 V DC, **100 kA DC**. ([A3T/A6T datasheet](https://www.mersen.com/sites/default/files/medias/PIM/files/DS-A3T-A6T-Class-T-Fast-Acting-Mersen.pdf))
  - **Littelfuse JLLS150** (SUPERSEDED): JLLS series is 300 V DC, 20 kA DC (DC rating self-certified by Littelfuse). ([JLLS catalog page](https://pim.galco.com/Manufacturer/Littelfuse/TechDocument/Catalog%20Page/littelfuse_powrgard2020_p24-25_cp.pdf))
  - Part numbers follow each series' naming pattern. Confirm the 150 A size is stocked before ordering **[UNVERIFIED]**.
- **Why 150 A:** it carries 133 A for 1 minute with one string dropped, and 40–67 A continuous without heating. It still blows well before a string cable cooks. Size each string's cable for the fuse, at least **2 AWG** (the mains stay 3/0). With 200 A fuses each string would need 2/0 cable, and a string fault would be cleared much more slowly.
- The pack has a **Volt MSD (~350 A)** as a service disconnect and main backstop. Its exact rating is **[UNVERIFIED]**; use it as the disconnect, not as the main protection.
- Put each fuse **at the string's + end**, as close to the cells as you can.

## 2. Precharge with four strings in parallel
Paralleling the strings doesn't change precharge. The capacitors being charged are on the load side (Kelly input plus the Elcon DC-DC input), and the pack looks like an almost ideal 148 V source.
- **Kelly input capacitance:** not published. **[UNVERIFIED]** assume about 2–5 mF for this size of controller. Measure it, or time a test precharge (see below).
- With 50 Ω: τ = R·C ≈ **0.1–0.25 s**. 95% charge takes 3τ ≈ **0.3–0.75 s**.
- **Resistor energy:** ½·C·V², about **22–55 J** per precharge (at 2–5 mF, 148 V). Peak power is V²/R = 148²/50 ≈ **440 W for a moment**. A 100 W aluminium-clad resistor handles that pulse easily if it's bolted to metal.
- **Danger case:** a short downstream leaves the resistor at 440 W *continuously* and it will burn out within seconds. **That's why the timeout matters.**
- **Firmware rule (for Gilfoyle):**
  1. Close the precharge relay (and negative contactor if used).
  2. Wait until **bus V ≥ 95% of the lowest string voltage**, *and* at least **300 ms** have passed.
  3. Close the main contactor, then open the precharge relay 100 ms later.
  4. **If 95% isn't reached within 3 s, it's a fault:** open everything, lock out until power-cycle, and allow no more than 2 retries spaced 30 s apart so the resistor can cool.
  5. The Kelly reads bus V over CAN or analog. If it's unavailable, use the time rule instead (≥ 5τ, about 1.5 s) **only after timing it on the bench**.
- **Bench check:** precharge with a meter on the Kelly B+ and time how long it takes to go from 0 to 95%. That time divided by 3 gives τ, so C = τ/50.

## 3. Voltage-matching the strings before paralleling
**Inrush math:** two strings joined at different voltages see current ≈ ΔV / (R₁ + R₂) ≈ ΔV / 0.09 Ω **[UNVERIFIED R]**.
| ΔV between strings | Inrush |
|---|---|
| 0.5 V (≈14 mV/cell) | ~6 A |
| 2 V | ~22 A |
| 5 V | ~55 A |
| 10 V | ~110 A (bad: arcing, and the contactor/fuse take a hit) |

**Rule: ≤ 0.5 V between strings is the target, 2 V is the hard limit.** Also no single cell group should be more than 20 mV off the others.

**Procedure** (first assembly, and after any string service):
1. All contactors and the MSD open. Fuses out.
2. Read each string's voltage with one meter, and its cells via the BICMs.
3. Bring each string to a common target of about **3.80 V/cell (136.8 V)**. At that level, small V differences mean only small state-of-charge differences.
   - Above target: discharge that string alone through a load (heater element, bulb bank, or a resistor at 1–5 A).
   - Below target: charge that string alone with the Elcon (current-limited, ceiling = 36 × 3.80 V). A bench supply also works.
4. Let them rest for **at least 1 hour**, then re-measure. ΔV must be ≤ 0.5 V.
5. **Soft join:** connect each string to the common bus through a **resistor of about 10–50 Ω** (the spare 50 Ω precharge resistor works) or a 120 V incandescent bulb. Wait until the voltage across it is **< 0.5 V**, then close the fuse/contactor. If the bulb is still glowing, wait.
6. After joining, check that string currents balance within about ±10% under a light load. If one string carries noticeably less, look for a bad link or a high-resistance joint.

## 4. One contactor per string, or one main contactor? (Adam's call)
| | One main contactor (+ fuses per string) | One contactor per string |
|---|---|---|
| Cost/space | 1 EV200 | 4 EV200s (+ coils, wiring, 4 outputs) |
| Bad string | Pull a fuse with the pack off | Firmware opens it, and you limp home on 3 strings |
| Service | Manual match each time (section 3) | Firmware can refuse to close a string with ΔV > 2 V |
| Failure risk | 1 contactor | A welded string contactor can go unnoticed, so add a welding check |
| Strings always connected | Yes, they stay paralleled even when parked | No, isolated when off |

**My pick:** **one main contactor + 150 A fuse per string, with the strings permanently paralleled.** Parallel strings stay matched by themselves. Four contactors mean more parts, more coil current (EV200 coil draw at 12 V **[UNVERIFIED]**), and more failure points in a vibrating UTV. Add per-string contactors only if being able to drive home on three strings matters to you. If you do, the firmware must check ΔV ≤ 2 V before closing each string, and the strings still need fuses.

## Open items
- Kelly model number off the sticker (120 V vs 144 V class). **This decides whether a 4.05–4.10 V charge cap works.**
- Measure Kelly + DC-DC input capacitance (precharge timing test).
- Measure one string's internal resistance (step load: ΔV/ΔI) to replace the 45 mΩ estimate.
- Confirm Volt Gen1 cell capacity (15 Ah) and the MSD fuse rating from the MSD label.
