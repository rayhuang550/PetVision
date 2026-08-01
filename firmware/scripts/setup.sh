#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

arduino-cli core update-index
arduino-cli core install "esp32:esp32@$ESP32_CORE_VERSION"
arduino-cli lib install "ArduinoJson@$ARDUINOJSON_VERSION"
arduino-cli lib install "Arducam_Mega@$ARDUCAM_MEGA_VERSION"
arduino-cli lib install "Adafruit NAU7802 Library@$NAU7802_VERSION"

[ -f "$SKETCH/secrets.h" ] || echo "NOTE: cp feeder/secrets.h.example feeder/secrets.h and fill it in"
echo "setup ok: core $ESP32_CORE_VERSION, ArduinoJson $ARDUINOJSON_VERSION, Arducam Mega $ARDUCAM_MEGA_VERSION, NAU7802 $NAU7802_VERSION"
