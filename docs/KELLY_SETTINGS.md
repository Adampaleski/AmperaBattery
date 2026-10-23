# Teryx EV: Kelly KLS-8080NPS settings (first-drive set)
Source: Kelly KLS-8080N/NPS/NA User Manual Rev 2.1.0 (Sep 2024). Parameter numbers (#) and field names are from section 4.3 of that manual.

## 0. Check these before you write anything
- **Controller (Adam's purchase email, chief 2026-10-23):** KLS8080 NPS, **168 V, 550 A peak**, all options incl. high-speed and anti-slip. Supersedes the earlier KLS14601 (144 V / 600 A) assumption. Every % below is of **550 A**. 168 V is treated as the max operating voltage (conservative) until Kelly or the label says otherwise.
- **Current limits are percentages of the controller's peak, not amps.** #10 Motor_Current limits phase current (20–100%). #11 Battery_Current limits battery current (20–100%, default 50%).
- **Motor = Zero 75-7R (used) with a sin/cos position sensor** (SOURCED (Adam 2026-10-23); that is why the NPS variant was bought — the N variant uses Halls). Motor bench spin test DONE (SOURCED (Adam 2026-10-23)). The sensor-type setting #60 is **UNVERIFIED for Adam's programming check**: READ it back and confirm before writing; don't change it blindly. Motor nameplate (pole count #59, thermistor #63) is still OPEN for Adam.
- **PWR (pin 7) is the controller's KSI/logic feed.** It takes an 8–20 V supply that must be isolated from pack B+/B−, and draws about 30 mA typical (manual §2.3). That 30 mA is the number for e-stop block 2.

## 1. Parameter table
| # | Field | Value | Why |
|---|---|---|---|
| 6 | Low Volt | **105 V** (2.92 V/cell) | Backstop only. The BMS cuts at 108 V (3.00 V/cell) for 2 s, so it acts first. Kelly faults after 5 s below this value. Its own taper start can't be set, so the Teensy owns the 3.50→3.25 V derate. |
| 7 | Over Volt | **158 V** | Sparky 2026-10-23, was 162 V on the 173 V assumption. 8.6 V above the 149.4 V BMS fault, 10 V under the 168 V rating (range 18–180). Regen over-voltage makes Kelly limit or stop regen (error 1,2). |
| 10 | Motor_Current | **100%** (550 A phase) | Sparky 2026-10-23, was 92% of 600 A. Same 550 A peak target on the 550 A unit. |
| 11 | Battery_Current | **25%** (≈138 A) for wheels-up bench tests on 1 string; **49%** (≈270 A) for first drive on 2 strings; later **75%** (≈413 A) only on 4 strings after the LEM sign check | DECIDED (chief 2026-10-23): amps unchanged from the decision (≈135 A ≈ 3C on one ~45 Ah string; ≈270 A on 2 strings); % re-based to 550 A by Sparky 2026-10-23. Add strings 3 and 4 one at a time after first drive; each string must be within **0.5 V of the bus** before its fuse goes in (supersedes the earlier 1 V figure). All stay under the LEM 400 A/1 s trip and the 650 A/100 ms pack trip. Amps assume the 550 A peak. |
| 12 | Identify Angle | 170 → auto-ID → 85 | See section 4. |
| 15 | TPS Type | **2** (Hall, 0–5 V) | Dual-Hall pedal. |
| 13 | TPS Low Err | **0%** | Lets the 4066 clamp to 0 V read as zero throttle instead of a fault. |
| 14 | TPS High Err | **90%** (4.5 V) | Catches a shorted or open-high pedal. Set it above the pedal's real full-travel voltage, measured at the Kelly. |
| 16/17 | TPS Dead Low / High | **20% / 80%** (1.0 / 4.0 V) | Defaults. Adjust to the measured pedal: dead low about 0.2 V above rest, dead high about 0.2 V below full. |
| 18 | TPS Fwd MAP | **30%** (default) | Soft midpoint for low-speed trail control. |
| 19 | TPS Rev MAP | **20%** | Gentle reverse. |
| 20 | Brake Type | **0** (switch regen) | Brake_AN is deferred. |
| 23 | Max Output Fre | **500 Hz** | 6000 RPM × 8 poles / 120 = 400 Hz, plus margin. Recompute once the pole count is confirmed. |
| 24 | Max Speed | **6000 RPM** | Speed cap of about 49 mph. Back-EMF limits it to around 5,100–5,700 RPM anyway. |
| 25/26 | Max Fwd / Rev Speed | **100% / 31%** | PROPOSED (Adam request 2026-10-22), was 100% / 25%. Reverse tops out around 15 mph (31% of 6000 = 1860 RPM ≈ 15.2 mph). Kelly side UNVERIFIED until the parameters are read back from the controller. |
| 27/28 | MidSpeed Fwd / Rev | **60% / 31%** | PROPOSED (Adam request 2026-10-22), was 60% / 25%. Forward about 29 mph (3600 RPM); reverse about 15 mph (1860 RPM). Kelly side UNVERIFIED until the parameters are read back. |
| 29/30 | LowSpeed Fwd / Rev | **30% / 20%** | DECIDED (Adam 2026-10-23): LowSpeed Fwd 30% ≈ 15 mph (1800 RPM), Rev 20% ≈ 10 mph (1200 RPM). This is the Kid hardware backstop through ECO. Kid's normal 10 mph forward cap is enforced by the Teensy; if both Teensys die, the Kelly holds Kid at about 15 mph forward and 10 mph reverse. Kelly side UNVERIFIED until read back. |
| 31 | Three Speed | **1** | DECIDED (chief 2026-10-23): 1 avoids the pin-12 collision with F/N/R (#90). Which pin selects mid is unverified, so bench-test it. |
| 32 | PWM frequency | 16 kHz (default) | Leave it. |
| 33–40, 55–57 | Current/speed loop gains | **Leave at default** | Only tune after auto-ID if the motor jitters. |
| 41 | BRK_AD Brk | 0% | Analog brake is not used yet. |
| 43 | Brk_Speed Limit | **100 RPM** | No regen near standstill, which stops low-speed lurch. |
| 44 | RLS_TPS Brk | **0%** for first drive, later **5%** | Lift-off regen stays off until behavior is known. |
| 45 | NTL Brk | 0% | |
| 46 | Accel Time | **10** (1.0 s) | Smooth torque ramp on a 1300 lb machine. Drop it to 5 later. |
| 47 | Accel Rls Time | 1 (default) | Torque drops fast when you lift, which is the safe direction. |
| 48/49 | Brake Time / Rls | **10 / 1** | Regen ramps in over 1 s. |
| 50 | BRK_SW Brk | **10%** (default) | Fixed brake-switch regen for first drive. Try 15–20% later. |
| 51 | Change Dir Brk | **0%** | No regen when changing direction. |
| 53/54 | IVT BRK Max / Min | defaults | Not used while #51 is 0. |
| 58 | Motor Nominal | 80 A (default) | Current used during auto-ID. |
| 59 | Motor Poles | **8 (UNVERIFIED for 75-7R)** | OPEN for Adam: motor nameplate / Zero / Sevcon config. If it's wrong, the RPM cap is wrong. |
| 60 | Speed Sensor Type | **4** (sin/cos) — UNVERIFIED for Adam's programming check | Motor sensor is sin/cos (SOURCED (Adam 2026-10-23)). Value 4 = sin/cos is from the manual; READ the controller's current value and confirm the option name in Adam's Kelly program before writing. Do not change it blindly; auto-ID (section 4) depends on it. |
| 63 | Motor Temp | **1** (KTY84-130/150), if the 75-7R thermistor is a KTY84 (UNVERIFIED; motor nameplate OPEN for Adam) | Meter it: KTY84-130 reads about 600 Ω at 25 °C. If it doesn't match, set 0 and let the Teensy monitor it. |
| 64/65 | High Temp Cut / Resume | **130 °C / 100 °C** | Conservative. Zero's own limit is unverified. |
| 66/67 | High Temp Str / weak | **100 °C / 50%** | Derate before the cut. |
| — | Controller temp | Fixed: derate at 90 °C, cut at 100 °C | Not configurable. |
| 86 | Startup H-Pedal | **checked** | High pedal disable at power-up. |
| 87 | Brake H-Pedal | **checked** | Brake plus throttle is a hard stop. This backs up the 0.5 s firmware brake override. |
| 88 | NTL H-Pedal | **checked** | Stops a lunge after a gear change. |
| 89 | Joystick | unchecked | |
| 90 | Three Gears Switch | **checked** (F/N/R) | See the conflict in section 3. |
| 91 | Boost | **unchecked** | Keeps pin 2 free for analog brake later. |
| 92 | Foot Switch | **checked** | A second, independent motor-off. See section 2. |
| 95/96/97 | Cruise / Anti-theft / Anti-Slip | **unchecked** | Not wanted on an off-road machine (PROPOSED). The controller has the anti-slip and high-speed add-ons (SOURCED (Adam 2026-10-23), purchase email); whether to use them is a later decision, UNVERIFIED. |
| 98 | Change Direction | Only if the motor runs backwards after ID | |
| 85 | J CAN Address | default 5 | Broadcast only. |

## 2. Zero-throttle and motor-off wiring (pin numbers from manual §3.2.1)
- **Throttle (pin 3, green)**: pedal track → CD74HC4066 (A) → CD74HC4066 (B) in series → pin 3. Use ONE pull-down from pin 3 to GND (pin 20): the existing **100k** (R1) from the clamp design — not 10k (corrected 2026-10-22). A 10k is a PROPOSED bench option only if the input floats, and only after checking that the pedal and digipot still reach full voltage through the 4066s under that load. Bench checks: pin 3 < 0.2 V with both 4066s open; full pedal voltage at pin 3 at full pedal. If either HCT123 heartbeat dies, its switch opens and pin 3 falls to 0 V, which reads as zero throttle.
- **Pedal 5V (pin 4, purple, under 40 mA) and GND (pin 20)**: pedal supply. Don't hang the Teensy dividers off this supply. Keep their loads separate, or check the total stays under 40 mA.
- **Foot_SW (pin 15, gray)**: Kelly 12V (pin 11) → contact of a relay or opto that only closes when **both** heartbeats are alive (series contacts) → pin 15. With #92 checked, losing either Teensy removes throttle enable even if the 4066 fails shorted.
- **Brake switch (pin 25)**: Kelly 12V (pin 11) → mechanical brake-light switch → pin 25. **Don't drive this from the Teensy.** Regen and the Brake H-Pedal stop should work with the Teensys dead. Teensy B can read the switch through an opto in parallel.
- **REV_SW (pin 14, orange) and FWD (pin 12, white)**: F/N/R rocker fed from pin 11. Neutral means both are open.
- **ECO (pin 22, blue)**: low speed. **The switch input is active-high, so a broken wire means full speed, not Low.** For a fail-safe Kid mode, feed pin 22 from pin 11 through the **NC** contact of a relay. Energize the relay to unlock full speed, so a dead relay, a dead Teensy or Kid mode all mean Low.
- **PWR (pin 7, pink)**: isolated 12 V → e-stop block 2 → pin 7. This is the KSI. GND goes to pin 6. Block 2 is lightly loaded at about 30 mA typical.
- **CAN-H/L (pins 33/34)**: to CAN3 at 250k, read-only.
- **Temp (pin 1) and GND (pin 21)**: motor thermistor. Sin/cos signals (motor sensor sin/cos, SOURCED (Adam 2026-10-23)) go to Hall A (pin 18) and Hall B (pin 17), with the sensor on 5V (pin 5); pin mapping from the manual, harness UNVERIFIED.
- **12V output (pin 11) is limited to 40 mA total.** Use it only for switch signals. Relay coils must run off the vehicle's 12 V bus; only the contacts carry the pin 11 signal.

## 3. Cross-check and conflicts
1. **Pin 12 is shared.** It's FWD when Three Gears (#90) is on, and High-speed when Three Speed (#31) is 2. The manual says the two can't be used together by default. Pick one of these:
   - (a) Three Gears plus Three Speed = 1 (mid and max). Which pin selects mid is unverified, so bench-test it.
   - (b) Three Gears with Three Speed = 0, and Kid mode done in firmware plus a hardware cap through the Foot_SW and throttle scaling instead of ECO.

   Verify on the bench before committing to either.
2. **Low-voltage order is correct.** The Teensy derates from 3.50 to 3.25 V/cell (126→117 V), the BMS cuts at 3.00 V (108 V) for 2 s, then Kelly cuts at 105 V after 5 s. Kelly also tapers on its own somewhere above Low Volt, and that start point isn't documented, so expect a little extra derate near the bottom.
3. **Over-voltage order is correct.** The BMS faults at 4.15 V/cell (149.4 V) and charging stops at 4.10 V (147.6 V). Kelly's limit is 158 V and the hardware rating is 168 V. Keep regen low near full charge, and keep RLS regen at 0 when the pack is above about 95% SOC.
4. **Current.** Battery_Current 25% (≈138 A, 1 string, wheels up) then 49% (≈270 A, 2 strings, first drive), DECIDED chief 2026-10-23; both leave margin under the 400 A/1 s LEM trip and the 650/750 A fast trips. Phase current (550 A) is higher than battery current at low speed, which is normal and doesn't show up on the LEMs.
5. **The LEM S/44 range is asymmetric.** Discharge must land on the −420 A side. Confirm that before raising #11.
6. **Cell temperature.** The 40 °C derate and 55 °C cut are handled by the Teensy and BMS. Kelly doesn't see cell temperature.
7. **High Pedal Disable plus the clamp.** If a heartbeat drops and comes back while the pedal is pressed, the throttle can come back mid-pedal. Firmware should latch zero until the pedal returns to rest. Kelly's #86 only checks at power-up.

## 4. Re-learning checklist
1. Power PWR from 12 V+ (≥18 V when programming per §4 note 3 — **bench-test whether 12 V is enough on this unit**; the manual's 18 V note conflicts with the 8–20 V isolated PWR spec). Connect, press **READ**, and **screenshot or save every page** as the stock backup.
2. Write the parameters in groups. After each **WRITE**, power-cycle, then **READ** and compare against this sheet.
3. Run auto-ID **with the wheels up, chain off or the machine on stands, and the shaft unloaded**. Set #12 to 170, write, power off, then power on. Wait 2–3 min for beep code 3-2. Power-cycle and confirm #12 reads 85. **Never power off with it at 170.** If the motor spins the wrong way, check #98; don't redo ID.
4. Run the wheels-up tests: throttle, the 4066 clamp (pull each heartbeat in turn and confirm pin 3 < 0.2 V with both 4066s open, and full pedal voltage at pin 3 at full pedal), the Foot_SW drop, the brake switch regen, F/N/R, ECO Low with the relay de-energized, and the e-stop.
5. First drive: Battery_Current 49% on 2 strings (25% for 1-string wheels-up bench), RLS regen 0, Accel Time 1.0 s.

## 5. Config software and cable (Adam's setup problems)
- **What exists:** Kelly's Support page lists a "KLS Configuration Program (PC Version)", an "Old Android Version" and a "New Android Version". The page also lists USB-RS232 drivers for Win 10, 7, Vista and XP. https://kellycontroller.com/support/
- **Cables:** "USB and bluetooth Adapter", **$19.00** on Kelly's site (live as of 2026-10-22). It comes as either the Kelly USB Cable (PC) or the Bluetooth Adapter (Android). https://kellycontroller.com/shop/usb-adapter/
  - **The Kelly USB cable is a CH340/CH341 chip.** The manual gives the driver at https://media.kellycontroller.com/new/CH341SER.zip
  - **Older RS232-USB cable plus SM-4P adapter:** driver at https://media.kellycontroller.com/new/USB-CONVERTER-RS232-Win10.zip
  - The manual warns that **only one driver can be installed**, because two together conflict.
- **Android** works over the Bluetooth adapter only. The manual lists no USB-OTG cable.
- **iPhone/iOS:** no Kelly app and no support. A Kelly reseller states the Bluetooth module is not compatible with iPhone or iPad (https://electroandcompany.com/products/kelly-bluetooth-module).
- **Recommendation:** buy the **$19 Kelly USB Cable (PC)** and use a Windows 10/11 laptop. It's the documented path and gets the newest PC program. Get the Bluetooth adapter only if he wants to tune in the field from an Android phone.
- **Windows setup:**
  1. Uninstall any old Prolific/FTDI/RS232 driver for that cable: Device Manager → device → Uninstall, and tick "delete driver".
  2. Install CH341SER (run SETUP.EXE as Administrator, click INSTALL), then reboot (manual step 3).
  3. Plug in the cable. Device Manager → Ports should show "USB-SERIAL CH340 (COMx)". If the COM number is above 9, set it to COM3 or 4 under Port Settings → Advanced, since older tools often only scan low ports (common fix, unverified for Kelly).
  4. Unzip the latest PC program from the Support page to a local folder, not inside the zip. Right-click → Run as administrator. If it won't launch, try Properties → Compatibility → Windows 7.
  5. Power the controller (PWR plus B−), then press READ. If it times out, check the cable is on the SM-4P port, the PWR voltage, and that no other serial program is holding the COM port.
