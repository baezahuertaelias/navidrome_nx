#!/usr/bin/env bash
set -euo pipefail
cmake -S . -B build.ui -G Ninja -DAURORE_PLATFORM=DESKTOP -DAURORE_WINDOW=SDL2 -DUSE_SYSTEM_SDL2=ON -DAURORE_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build.ui --target navidrome_ui_tests --parallel 4
xvfb-run -a python3 tests/ui_server.py
