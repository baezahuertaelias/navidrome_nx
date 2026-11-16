#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
docker build --target overlay-artifact --output type=local,dest=dist/ultrahand .
python3 scripts/check-overlay-package.py dist/ultrahand
python3 - <<'PY'
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
root = Path('dist/ultrahand')
output = Path('dist/NavidromeNX-ultrahand.zip')
with ZipFile(output, 'w', compression=ZIP_DEFLATED) as archive:
    for path in sorted(root.rglob('*')):
        if path.is_file():
            archive.write(path, path.relative_to(root))
print(f'Built {output}')
PY
