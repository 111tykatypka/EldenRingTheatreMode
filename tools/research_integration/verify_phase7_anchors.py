"""Verify small, exact-target static anchors. Never opens live game memory."""
import json
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'ghidra_query'))
from research_query import Image, DEFAULT

ANCHORS = {
    'ChrIns callback object base +508': (0x1403e73e3, '48 8d 9f 08 05 00 00'),
    'ChrIns callback +530 target 3f8fd0': (0x1403e73fe, '48 8d 05 cb 1b 01 00 48 89 43 28'),
    'ChrIns flags initialize +538': (0x1403e7409, '48 c7 87 38 05 00 00 00 00 40 06'),
    'ChrCtrl owner +10 movement test': (0x1403cc231, '48 8b 47 10 f6 80 38 05 00 00 20'),
    'Secondary action bit candidate': (0x1403d0008, 'f6 80 38 05 00 00 10'),
    'Proxy flags setter candidate, ABI unknown': (0x1403c8bb0, '83 8b fc 00 00 00 03'),
}

def main():
    image = Image(DEFAULT.parent)  # verifies full research-copy SHA-256 first
    rows = []
    for name, (address, pattern) in ANCHORS.items():
        expected = bytes.fromhex(pattern)
        actual = image.read(address, len(expected))
        if actual != expected:
            raise SystemExit(f'Anchor mismatch: {name}; no binding accepted')
        rows.append({'name': name, 'rva': hex(address-image.base), 'status': 'STATIC_VERIFIED'})
    print(json.dumps(rows, indent=2))
    print('Static bytes only; no runtime ABI or gameplay ownership claim.')

if __name__ == '__main__':
    main()
