#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

arduino-cli compile --fqbn "$FQBN" --library "$FIRMWARE_DIR/lib/spine" \
  --build-path "$BUILD_DIR/feeder" "$@" "$SKETCH"
