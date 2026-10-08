"""Read-only exact-build force-field accessor trace. Never calls game code."""
import json
import sqlite3
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent / 'ghidra_query'))
import research_query as q

image = q.Image(q.DEFAULT.parent)  # validates the entire analysis copy SHA256
sys.path.insert(0, str(q.DEFAULT.parent.parent / 'tools/python-deps'))
import capstone

decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
# Exact instruction ranges verified manually; this list is deliberately targeted.
entries = {
    0x141CAEAD0: 52, 0x141C956B0: 144, 0x141C957D0: 67,
    0x141CB32D0: 66, 0x140D91D10: 291, 0x140D8D9C0: 131,
    0x141C94E60: 109, 0x141C972E0: 132,
}
database = sqlite3.connect(q.DEFAULT.resolve().as_uri() + '?mode=ro', uri=True)
database.row_factory = sqlite3.Row
result = {'sha256': q.SHA, 'classification': 'STATIC_CANDIDATES_NOT_RUNTIME_ABI', 'functions': {}}
try:
    for address, length in entries.items():
        result['functions'][hex(address)] = {
            'instructions': [{'address': hex(i.address), 'bytes': i.bytes.hex(' '),
                              'op': i.mnemonic, 'args': i.op_str}
                             for i in decoder.disasm(image.read(address, length), address)],
            'callers': [dict(r) for r in database.execute('SELECT * FROM calls WHERE callee=?', (format(address, 'x'),))],
        }
finally:
    database.close()
destination = Path(__file__).resolve().parents[1] / 'research/wind_force_flow_c32.json'
destination.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print(f'Saved read-only evidence: {destination}')
