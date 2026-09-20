#!/usr/bin/env bash
# Install the badge's pinned toolchain locally; does not touch the badge.
set -euo pipefail
BADGE_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export IDF_TOOLS_PATH="$BADGE_ROOT/.tools/espressif"
BADGE_IDF="$BADGE_ROOT/.tools/esp-idf"
mkdir -p "$BADGE_ROOT/.tools"
if [ ! -d "$BADGE_IDF" ]; then
  git clone --branch v5.5.3 --depth 1 --recursive --shallow-submodules https://github.com/espressif/esp-idf.git "$BADGE_IDF"
fi
if [ "$(git -C "$BADGE_IDF" describe --tags --exact-match HEAD)" != v5.5.3 ]; then
  echo 'Expected ESP-IDF v5.5.3; refusing to change an existing checkout.' >&2
  exit 1
fi
"$BADGE_IDF/install.sh" esp32c3
python3 "$BADGE_IDF/tools/idf_tools.py" install cmake ninja
echo 'Toolchain installed. Run: bash tools/idf.sh --version'
