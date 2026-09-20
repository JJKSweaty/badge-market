#!/usr/bin/env bash
# Native build/flash entry point. A successful backup is required before flashing.
set -euo pipefail
BADGE_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BADGE_ACTION="${1:-build}"
BADGE_PORT="${2:-/dev/cu.usbmodem1101}"
BADGE_BAUD="${BADGE_BAUD:-921600}"
case "$BADGE_ACTION" in
  build|size|menuconfig)
    exec bash "$BADGE_ROOT/tools/idf.sh" -C "$BADGE_ROOT/firmware" "$BADGE_ACTION" ;;
  monitor)
    exec bash "$BADGE_ROOT/tools/idf.sh" -C "$BADGE_ROOT/firmware" -p "$BADGE_PORT" monitor ;;
  backup|flash)
    if [ "$BADGE_ACTION" = flash ]; then
      bash "$BADGE_ROOT/tools/idf.sh" -C "$BADGE_ROOT/firmware" build
    fi
    export IDF_TOOLS_PATH="$BADGE_ROOT/.tools/espressif"
    set +u
    source "$BADGE_ROOT/.tools/esp-idf/export.sh" >/dev/null
    set -u
    umask 077
    mkdir -p "$BADGE_ROOT/build/backups"
    BADGE_BACKUP_DIR="$(mktemp -d "$BADGE_ROOT/build/backups/badge-XXXXXXXX")"
    BADGE_BACKUP="$BADGE_BACKUP_DIR/flash.partial"
    python -m esptool --chip esp32c3 --port "$BADGE_PORT" --baud "$BADGE_BAUD" read_flash 0 0x400000 "$BADGE_BACKUP"
    python - "$BADGE_BACKUP" <<'PY'
from pathlib import Path
import hashlib, sys
p = Path(sys.argv[1])
data = p.read_bytes()
if len(data) != 4 * 1024 * 1024:
    raise SystemExit('Incomplete backup; refusing to flash.')
p = p.rename(p.with_suffix('.bin'))
p.with_suffix('.sha256').write_text(hashlib.sha256(data).hexdigest() + '  ' + p.name + '\n')
print('Full badge backup:', p)
PY
    if [ "$BADGE_ACTION" = flash ]; then
      python "$IDF_PATH/tools/idf.py" -C "$BADGE_ROOT/firmware" -p "$BADGE_PORT" flash
    fi ;;
  *) echo 'Usage: bash tools/firmware.sh build|size|menuconfig|backup|flash|monitor [PORT]' >&2; exit 2 ;;
esac
