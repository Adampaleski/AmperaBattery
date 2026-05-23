#!/usr/bin/env bash
# Close the serial monitor (Ctrl+C) before running this script.
set -euo pipefail
export PATH="$HOME/.platformio/penv/bin:$PATH"
cd "$(dirname "$0")/.."
ENV="${1:-teensy41_k112_24s}"
echo "Uploading env: $ENV"
echo "If this fails: quit pio device monitor, press Teensy PROGRAM button, run again."
pio run -e "$ENV" -t upload
