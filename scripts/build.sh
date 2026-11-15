#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
docker build -t navidrome-nx-build .
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD:/app" navidrome-nx-build
