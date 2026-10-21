# Kelly high pedal disable (HPD) — chief 2026-10-21

**What it is (SOURCED: Kelly KLS-N user manual, "High Pedal Disable" parameter):** a setting in the Kelly PC/Android programming tool, not a wire. When enabled, if the throttle is above zero at power-on (or the first press after a gear/direction change), the controller reports a high-pedal error and will not drive until the pedal returns to zero. Related "NTL H-Pedal" (neutral-position HPD) exists on some KLS-N firmware.

**Plan:** enable *High Pedal Disable* in the Kelly config (read back and record the setting; nobody has read the KLS8080N config yet). Shown as a Kelly error code over CAN 0x0CF11E05 error bits / the Kelly tool.

**Wiring (UNVERIFIED for our KLS8080N/8080NPS):** the KLS-N HC manual (media.kellycontroller.com, V2.14) lists the throttle analog input (0–5 V) on control-connector pin 3 and the throttle switch input on pin 15 (per a search summary of that manual, not read page by page). KLS connector numbering differs between variants, so check against the manual that shipped with Adam's unit and the harness before wiring. If pin 15 (throttle switch) is used, wire it from a pedal micro-switch so the Kelly also sees "pedal released" independently of the analog signal.

**Not done here:** the hardware throttle clamp and the pedal-input pin move are ON HOLD (architecture dispute: chief proposes wiring the pedal direct to the Kelly and dropping the digipot; Adam to decide). HPD applies either way.
