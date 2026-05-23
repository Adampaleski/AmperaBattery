#!/usr/bin/env bash
set -euo pipefail
export PATH="$HOME/.platformio/penv/bin:$PATH"
cd "$(dirname "$0")/.."
echo "Click inside this window, then press keys: d s k c ?"
echo "You will NOT see the letter as you type — that is normal."
echo "Quit monitor: Ctrl+C"
pio device monitor -b 115200 --filter direct
