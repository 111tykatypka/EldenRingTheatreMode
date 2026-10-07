import importlib.util
import struct
import tempfile
import unittest
import zlib
from pathlib import Path

spec = importlib.util.spec_from_file_location('inspector', Path(__file__).parents[1] / 'tools/inspect_actor_lifetimes.py')
inspector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inspector)


def pack(raw):
    out = bytearray()
    for value in raw:
        out.extend((0, 0) if value == 0 else (value,))
    return out


def chunk(track, kind, count, first, last, raw):
    data = pack(raw)
    return inspector.HEADER.pack(int.from_bytes(b'CHNK', 'little'), track, kind, count,
                                 first, last, len(data), len(raw), zlib.crc32(data)) + data


class InspectorTests(unittest.TestCase):
    def fixture(self, corrupt=False):
        start = 1000
        # Deliberately synthetic metadata fixture, not Elden Ring gameplay or a valid pose payload.
        records = b''.join(inspector.RECORD.pack(start + sec * 10**9, 7, 1000, 1000, 1000,
                                                3 if available == 0 else 0, flags, 0, 0, 0, 100,
                                                available, 0 if available == 0 else 1)
                           for sec, flags, available in ((0, 0, 0), (12, 1, 0), (15, 0, 1)))
        data = b'ERWORLD1' + struct.pack('<II', 2, 0) + chunk(1, 1, 1, start, start, b'')
        data += chunk(7, 9, 3, start, start + 15 * 10**9, records)
        return data[:-1] if corrupt else data

    def test_death_then_backward_seek_and_gap(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'test.world'
            path.write_bytes(self.fixture())
            result = inspector.inspect(path, (13, 5, 16))['evaluated_at_seconds']
            self.assertEqual(result['13'][7]['state'], 'DEATH_FLAGGED')
            self.assertEqual(result['5'][7]['state'], 'OBSERVED_ALIVE')
            self.assertEqual(result['16'][7]['state'], 'UNKNOWN')

    def test_corruption_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'test.world'
            path.write_bytes(self.fixture(True))
            with self.assertRaises(ValueError):
                inspector.inspect(path)

    def test_malformed_runs_are_rejected(self):
        for data, size in ((b'\0', 1), (b'\0\xff', 2), (b'\x01', 2)):
            with self.assertRaises(ValueError):
                inspector.unpack(data, size)


if __name__ == '__main__':
    unittest.main()
