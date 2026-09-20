#!/usr/bin/env bash
# Run ESP-IDF in the current directory using this project's local toolchain.
set -e
BADGE_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export IDF_TOOLS_PATH="$BADGE_ROOT/.tools/espressif"
if [ ! -f "$BADGE_ROOT/.tools/esp-idf/export.sh" ]; then
  echo 'Run bash tools/setup_esp_idf.sh first.' >&2
  exit 1
fi
source "$BADGE_ROOT/.tools/esp-idf/export.sh" >/dev/null
exec python "$IDF_PATH/tools/idf.py" "$@"
