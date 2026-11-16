#!/usr/bin/env bash
set -euo pipefail
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/api_test.cpp -lcurl -ljansson -lmbedcrypto -o /tmp/api_test
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/overlay_test.cpp -lcurl -ljansson -lmbedcrypto -o /tmp/overlay_test
python3 tests/run.py
g++ -std=c++17 -Isrc -isystem vendor/borealis/library/lib/extern/nanovg-deko3d/include tests/artwork_test.cpp -o /tmp/artwork_test
/tmp/artwork_test
python3 tests/i18n_catalog_test.py
borealis=vendor/borealis/library
g++ -std=c++17 '-DBRLS_RESOURCES="/tmp/navidrome-i18n-test/"' \
    -I"$borealis/include" -I"$borealis/include/borealis/extern" \
    -I"$borealis/lib/extern/fmt/include" -I"$borealis/lib/extern/yoga/src" \
    -I"$borealis/lib/extern/nanovg-deko3d/include" \
    -I"$borealis/lib/extern/tweeny/include" -I"$borealis/include/borealis/extern/tinyxml2" \
    tests/i18n_test.cpp "$borealis/lib/core/i18n.cpp" "$borealis/lib/extern/fmt/src/format.cc" \
    -o /tmp/i18n_test
/tmp/i18n_test
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/settings_test.cpp -ljansson -o /tmp/settings_test
/tmp/settings_test
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/remote_test.cpp -ljansson -pthread -o /tmp/remote_test
/tmp/remote_test
gcc -std=c99 -Wall -Wextra -Werror -c vendor/qrcodegen/qrcodegen.c -o /tmp/qrcodegen.o
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/qr_test.cpp /tmp/qrcodegen.o -o /tmp/qr_test
/tmp/qr_test
python3 - <<'PY'
import subprocess
actual = subprocess.check_output(['zbarimg', '--quiet', '--raw', '/tmp/pairing-qr.pgm'], stderr=subprocess.DEVNULL).decode().strip()
assert actual == 'http://192.168.1.91:8080/#0123456789abcdef0123456789abcdef'
print('QR scan round trip passed')
PY
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/login_test.cpp -ljansson -o /tmp/login_test
/tmp/login_test
g++ -std=c++17 -isystem vendor/borealis/library/lib/extern/nanovg-deko3d/include tests/font_test.cpp -o /tmp/font_test
/tmp/font_test
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/waveform_test.cpp -pthread -o /tmp/waveform_test
/tmp/waveform_test
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/lyric_emphasis_test.cpp -o /tmp/lyric_emphasis_test
/tmp/lyric_emphasis_test

g++ -std=c++17 -Wall -Wextra -Werror -Ivendor/borealis/library/include tests/touch_test.cpp -o /tmp/touch_test
/tmp/touch_test
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/song_cache_test.cpp -lcurl -ljansson -lmbedcrypto -o /tmp/song_cache_test
/tmp/song_cache_test
nanovg=vendor/borealis/library/lib/extern/nanovg-deko3d
gcc -std=c99 -I"$nanovg/include" -c "$nanovg/source/nanovg.c" -o /tmp/nanovg-test.o
g++ -std=c++17 -I"$nanovg/include" tests/icon_alignment_test.cpp /tmp/nanovg-test.o -lm -o /tmp/icon_alignment_test
/tmp/icon_alignment_test
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/lyrics_navigation_test.cpp -o /tmp/lyrics_navigation_test
/tmp/lyrics_navigation_test
