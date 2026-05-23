# Bench checklist (24S / 4×6S)

## Sniffer pass

- [ ] `pio run -e teensy41_sniffer -t upload` succeeds
- [ ] Serial banner shows `24S / 4 modules / 6 cells per module`
- [ ] JSON `last_rx_age_ms` stays below 500 with pack powered
- [ ] `S` shows cell IDs `0x460`–`0x46F` and `0x470`–`0x47F` (four modules)
- [ ] `K` OFF → `rx_total` stops increasing; `K` ON → traffic returns

## Decode pass

- [ ] `d` reports `modules=4 cells=24 stable=Y`
- [ ] Per-module lines show six voltages in ~2.5–4.2 V range
- [ ] Spot-check one cell with a multimeter (within ~50 mV)

## Optional BMS env

- [ ] `teensy41_bms` builds; `E` toggles contactor enable message only (outputs stay safe until enabled)
