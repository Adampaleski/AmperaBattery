# Bench bleed test: ONE cell, 10 seconds

This is the first time the Teensy actually tells a BICM to bleed a cell. The firmware
**only ever bleeds one cell**, for at most **10 s**, and only when you type the command.
Everything else stays off: no charger output, no contactor coils, no Kelly outputs
(all `BMS_CAP_*` flags are 0, `TELEMETRY_ONLY=1`). Bench only, not for the vehicle.

- Firmware file: `firmware/teensy41_bench_bleed.hex` (PlatformIO env `teensy41_bench_bleed`)
- Frames: Tom de Bree's VoltBMSV2 factory layout (`include/FactoryBalanceMap.h`):
  `0x300` (8 bytes) + `0x310` (5 bytes), then trigger `0x200#02.00.00`, resent every 200 ms.
  Every 5th cycle the bits are sent as zeros so the BICM takes a clean (unloaded) reading.

## What the firmware refuses
The `bleed` command is refused (and nothing is sent) unless ALL of these are true:
1. Module is 1-4 and the channel exists on that module (M1 30, M2 24, M3 24, M4 18).
2. Every circuit that has reported is fresh and valid (no stale or out-of-range cell).
3. The chosen cell reads **at least 3.70 V**.
4. No cell is below the low cutoff (`L` setting, default 3.30 V).
5. No other bleed is running.

While it runs it stops by itself with all-off when: 10 s pass, you press `x`, cell data
goes stale, any cell drops below the low cutoff, or a CAN send fails.
"All-off" = `0x300` and `0x310` all zeros + `0x200`, sent 3 times.

## Step by step

1. **Bench setup as usual**: BICMs on the bench 5 V, CAN3 (pins 30/31) wired to the BICM
   bus, Teensy on USB. Nothing else connected.
2. **Flash** with Teensy Loader (pjrc.com/teensy/loader.html):
   - Open Teensy Loader. File → Open HEX File → pick `teensy41_bench_bleed.hex`.
   - Press the white button on the Teensy (or turn on "Auto"). It says "Reboot OK".
3. **Open serial** at **115200** baud (Arduino Serial Monitor, or `pio device monitor -b 115200`).
   Set the line ending to **Newline** (the bleed command needs Enter).
   You should see `*** BENCH BLEED TEST BUILD ... ***` near the top.
4. **Confirm cells read**: press `d`. Every cell you have wired should show a sensible
   voltage, and no `STALE`/`ALARM`/`LOW-LIVE` lines. If not, stop here and send me the output.
5. **Pick a cell at or above 3.70 V.** Write down its module and channel (e.g. M2 ch5) and
   its voltage. A cell in the middle of a board is fine.
6. **Run it**: type `bleed 2 5` and press Enter (use your module and channel).
   - You'll see `*** BLEED ARMED M2 ch5 (circuit 0x464 bit 4) ...`.
   - Each 200 ms it prints the frames it sent, e.g. `TX 0x300#00.00.00.00.10.00.00.00`,
     `TX 0x310#00.00.00.00.00`, `TX 0x200#02.00.00`.
   - Every 500 ms it prints `BLEED t=... M2 ch5 = 3.8xx V`.
   - If it says `BLEED REFUSED ...` it tells you why. Nothing was sent.
7. **What to expect**: during the bleed that one cell should read about **5-20 mV lower**
   (sag from the ~100 mA bleed load), and its neighbours should not move. After the
   `*** BLEED STOPPED: 10 s timeout ***` line, press `d` a few times over 30 s: the cell
   should come back to about where it started (10 s of bleed removes only ~0.3 mAh).
8. **Abort any time**: press `x` (no Enter needed). You'll see `BLEED STOPPED: x pressed`
   and the all-off frames. Unplugging USB or power also ends it (the BICM stops bleeding
   when the frames stop).
9. Optional: try one cell on a different module the same way. Never more than one at a time;
   the firmware won't allow it anyway.

## What to send back
Copy-paste the whole serial log from step 4 to the end, plus:
- module/channel you bled, and its voltage before, during (lowest), and ~30 s after
- whether the neighbouring cells moved
- whether the BICM board got warm near that cell (it shouldn't, much)
- any `REFUSED`, `TX FAILED`, `STALE` or `CAN recovery` lines

If the sag shows up on a **different** cell than the one you picked, stop and tell me:
that means the bit map is off for that circuit. If nothing moves at all, that's also useful.

## Notes
- `b` (balance plan) needs Enter in this build, because `bleed` also starts with b.
- The 1 s keep-alive is paused during the bleed (the 200 ms `0x200` replaces it) and
  restored after.
- Host tests: `g++ -std=c++17 -Iinclude scripts/test_bench_bleed.cpp -o tbb && ./tbb`.
