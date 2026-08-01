#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

"$(dirname "$0")/build.sh"
arduino-cli upload -p "$UPLOAD_PORT" --fqbn "$FQBN" --input-dir "$BUILD_DIR/feeder" "$SKETCH"
