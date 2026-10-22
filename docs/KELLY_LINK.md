# Kelly CAN link watchdog

Status: **logic + host test only** (`include/KellyLink.h`, `scripts/test_kelly_link.cpp`, 18/18). Not wired:
`BMS_CAP_KELLY_RX=0`. Backlog: VCU safety item "Kelly CAN timeout → derate + fault (KELLY-CAN-LOST)", required before first drive.

Kelly CAN is telemetry, not pack protection (BICMs + LEMs protect the pack). Losing it means the Teensys can't see
RPM, direction, Kelly/motor temps or Kelly error bits, so they cap power and stop trusting Park.
It never opens or closes a contactor and never commands the Kelly (the KLS accepts no CAN commands).

| Code | Condition (all PROPOSED) |
|---|---|
| NEVER-SEEN | key on > 2 s and either frame (0x0CF11E05 RPM, 0x0CF11F05 temps/dir) never arrived |
| LOST | either frame seen, then older than 200 ms (4 missed 50 ms frames). Not hidden by the boot grace |

- Any fault requests the Kid/Low hardware cap (OUT3 ECO, same path as other hard faults; 1 s ramp per crew review).
- The cap lifts only after both frames stay fresh for 1 s, so a flapping link keeps the cap on.
- `latched` keeps the first fault of the key cycle for the fault log; `lossCount` counts LOST edges. `keyOff()` clears all.
- `parkTrusted()` is what should feed ParkState `rpmValid`.

UNVERIFIED: the 50 ms frame period on Adam's unit (candump it at Gate 0) and the Kelly boot time after KSI (2 s grace is a guess).

Next: on `BMS_CAP_KELLY_RX=1`, call `rpmFrame` / `thrFrame` from KellyDecode, `step` each loop, OR `capActive` into the
Low-cap request, log set/clear edges in FaultLog, and add a sim toggle "Kelly CAN unplugged".
