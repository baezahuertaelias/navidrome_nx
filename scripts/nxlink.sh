#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
test -f navidrome_nx.nro || { echo 'Run scripts/build.sh first' >&2; exit 1; }
exec docker run --rm --init --network host --user "$(id -u):$(id -g)" -v "$PWD:/app:ro" navidrome-nx-build stdbuf -oL -eL nxlink -a "${1:?Usage: scripts/nxlink.sh SWITCH_IP}" -p /switch/navidrome_nx/navidrome_nx.nro -s navidrome_nx.nro
