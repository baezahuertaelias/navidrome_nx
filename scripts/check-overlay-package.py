#!/usr/bin/env python3
"""Check exported overlay + ExeFS identity without installing anything."""
from pathlib import Path
import struct
import sys

root = Path(sys.argv[1])
overlay = (root / 'switch/.overlays/NavidromeNX.ovl').read_bytes()
assert overlay[16:20] == b'NRO0', 'Invalid overlay header'
module = root / 'atmosphere/contents/4200000000000000'
assert (module / 'flags/boot2.flag').is_file(), 'Missing boot flag'
exefs = (module / 'exefs.nsp').read_bytes()
assert exefs[:4] == b'PFS0', 'Invalid ExeFS container'
count, strings_size = struct.unpack_from('<II', exefs, 4)
strings_offset = 16 + count * 24
data_offset = strings_offset + strings_size
files = {}
for index in range(count):
    offset, size, name_offset = struct.unpack_from('<QQI', exefs, 16 + index * 24)
    start = strings_offset + name_offset
    name = exefs[start:exefs.index(b'\0', start)].decode('utf-8')
    files[name] = exefs[data_offset + offset:data_offset + offset + size]
assert files['main'][:4] == b'NSO0', 'Missing main NSO'
npdm = files['main.npdm']
assert npdm[:4] == b'META', 'Invalid NPDM'
aci, _, acid, _ = struct.unpack_from('<IIII', npdm, 0x70)
assert npdm[aci:aci + 4] == b'ACI0'
assert npdm[acid + 0x200:acid + 0x204] == b'ACID'
program_id = 0x4200000000000000
assert struct.unpack_from('<Q', npdm, aci + 0x10)[0] == program_id
assert struct.unpack_from('<QQ', npdm, acid + 0x210) == (program_id, program_id)
assert b'tune' in npdm, 'Missing tune service access'
print('Overlay header, ExeFS, program ID and boot flag checks passed')
