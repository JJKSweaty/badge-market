#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
"${CXX:-c++}" -std=c++17 -O1 -g -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -Ifirmware/core \
  firmware/core/market.cpp firmware/core/session.cpp firmware/core/play.cpp firmware/core/bomb.cpp \
  firmware/tests/core_test.cpp -o build/native-tests
./build/native-tests
"${CXX:-c++}" -std=c++17 -O1 -g -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Ifirmware/tests/stubs -Ifirmware/core -Ifirmware/main \
  firmware/core/market.cpp firmware/core/session.cpp firmware/core/play.cpp firmware/core/bomb.cpp \
  firmware/tests/menu_test.cpp -o build/native-menu-tests
./build/native-menu-tests
"${CXX:-c++}" -std=c++17 -O1 -g -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -Ifirmware/core \
  firmware/core/market.cpp firmware/core/session.cpp firmware/core/play.cpp \
  firmware/core/bomb.cpp firmware/tests/play_test.cpp -o build/native-play-tests
./build/native-play-tests
