# Firmware build check — Oct 22 2026 6:29 PM CT

PlatformIO Core 6.2.0 (venv /workspace/tmp/piovenv), platform teensy 6.0.0, toolchain gcc-arm 15.2.1, at commit fb5acb1. Command: `pio run -e <every env>`. All 11 envs SUCCESS, 0 errors. Only warnings in src/: unused ThrottleTrim ISR/helpers when THROTTLE_TRIM_ENABLE=0 (expected).

| env | result | flash bytes (code+data+headers) | RAM1 used | RAM1 free for stack | RAM2 vars |
|---|---|---|---|---|---|
| teensy41_sniffer | SUCCESS | 157692 | 213696 | 310592 | 12416 |
| teensy41_k112_24s | SUCCESS | 158716 | 213152 | 311136 | 12416 |
| teensy41_k112_36s | SUCCESS | 159740 | 213248 | 311040 | 12416 |
| teensy41_k112_48s | SUCCESS | 158716 | 213344 | 310944 | 12416 |
| teensy41_bench_30s | SUCCESS | 159740 | 213184 | 311104 | 12416 |
| teensy41_bench_2x30s | SUCCESS | 158716 | 213472 | 310816 | 12416 |
| teensy41_factory_bus | SUCCESS | 165884 | 217504 | 306784 | 12416 |
| teensy41_1bicm | SUCCESS | 157692 | 213696 | 310592 | 12416 |
| teensy41_dual_a | SUCCESS | 196604 | 230272 | 294016 | 12416 |
| teensy41_dual_b | SUCCESS | 138236 | 193760 | 330528 | 12416 |
| teensy41_bench_bleed | SUCCESS | 169980 | 218560 | 305728 | 12416 |

Teensy 4.1: 7.75 MB flash, RAM1 512 KB (ITCM/DTCM), RAM2 512 KB. Not flashed.
