"""Trace the supplied CT's VFX dispatcher against the exact research image."""
import sys
import json
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent / 'ghidra_query'))
import research_query as q

image = q.Image(q.DEFAULT.parent)  # verifies exact executable SHA before reading
pattern = bytes.fromhex('85 D2 74 0A 83 FA 01 75 0A E9')
hits = []
for section in image.meta['sections']:
    if not int(section['characteristics'], 16) & 0x20000000:
        continue
    data = image.data[section['file_offset']:section['file_offset'] + section['raw_size']]
    start = 0
    while True:
        offset = data.find(pattern, start)
        if offset < 0:
            break
        address = image.base + section['rva'] + offset
        raw = image.read(address, 32)
        hits.append({'address': hex(address), 'rva': hex(address-image.base),
                     'bytes': raw.hex(' '),
                     'mode_1_target': hex(address+14+int.from_bytes(raw[10:14], 'little', signed=True)),
                     'mode_0_target': hex(address+19+int.from_bytes(raw[15:19], 'little', signed=True)),
                     'second_jump_valid': raw[14] == 0xe9})
        start = offset + 1
print(json.dumps({'sha256': q.SHA, 'matches': hits, 'unique': len(hits) == 1}, indent=2))
