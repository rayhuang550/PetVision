#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

arduino-cli monitor -p "$MONITOR_PORT" -c "baudrate=$MONITOR_BAUD"
