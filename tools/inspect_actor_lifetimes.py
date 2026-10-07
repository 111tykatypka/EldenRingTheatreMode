"""Read-only ERWORLD actor-observation inspector. Does not validate/decode pose payloads.
Usage: python tools/inspect_actor_lifetimes.py path.erplay.world --at 5 --at 13
Seconds are relative to the first player chunk timestamp, not filename time.
"""
import argparse
import bisect
import json
import math
import struct
import zlib
from pathlib import Path

HEADER = struct.Struct('<IIIIQQIII')
RECORD = struct.Struct('<QIIiIIIIIiiII')


def unpack(data, size):
    out = bytearray()
    i = 0
    while i < len(data):
        value = data[i]
        i += 1
        if value == 0:
            if i == len(data):
                raise ValueError('truncated zero run')
            run = data[i] + 1
            i += 1
            if len(out) + run > size:
                raise ValueError('raw size exceeded')
            out.extend(bytes(run))
        else:
            out.append(value)
        if len(out) > size:
            raise ValueError('raw size exceeded')
    if len(out) != size:
        raise ValueError('raw size mismatch')
    return out


def inspect(path, seeks=()):
    tracks = {}
    start = None
    chunks = 0
    with Path(path).open('rb') as file:
        prefix = file.read(16)
        if len(prefix) != 16 or prefix[:8] != b'ERWORLD1':
            raise ValueError('invalid ERWORLD header')
        version = struct.unpack_from('<I', prefix, 8)[0]
        if version not in (1, 2):
            raise ValueError('unsupported ERWORLD version')
        while header := file.read(HEADER.size):
            if len(header) != HEADER.size:
                raise ValueError('truncated chunk header')
            marker, track, kind, count, first, last, packed_size, raw_size, crc = HEADER.unpack(header)
            if marker != int.from_bytes(b'CHNK', 'little'):
                raise ValueError('invalid chunk marker')
            payload = file.read(packed_size)
            if len(payload) != packed_size or zlib.crc32(payload) != crc:
                raise ValueError('truncated/corrupted chunk')
            chunks += 1
            if (track, kind) == (1, 1) and start is None:
                start = first
            if (track, kind) != (7, 9):
                continue
            if raw_size != count * RECORD.size:
                raise ValueError('invalid observation count/size')
            raw = unpack(payload, raw_size)
            for values in RECORD.iter_unpack(raw):
                keys = ('time', 'id', 'character_id', 'npc_id', 'model_id', 'known', 'flags',
                        'backread', 'cleanup', 'hp', 'max_hp', 'availability', 'reason')
                record = dict(zip(keys, values))
                if (not record['id'] or record['known'] & ~15 or record['flags'] & ~3
                        or record['availability'] > 1 or record['reason'] > 3
                        or (record['availability'] and record['known'])):
                    raise ValueError('invalid observation fields')
                records = tracks.setdefault(record['id'], [])
                if records and record['time'] <= records[-1]['time']:
                    raise ValueError('observation timestamp regression')
                record['state'] = ('UNKNOWN' if record['availability'] or record['known'] & 3 != 3
                                   else 'DEATH_FLAGGED' if record['flags'] & 1 else 'OBSERVED_ALIVE')
                records.append(record)
    if start is None:
        raise ValueError('no player track')
    evaluations = {}
    for seconds in seeks:
        if not math.isfinite(seconds) or seconds < 0:
            raise ValueError('seek must be finite and nonnegative')
        target = start + round(seconds * 1e9)
        evaluations[str(seconds)] = {}
        for actor, records in tracks.items():
            index = bisect.bisect_right([r['time'] for r in records], target) - 1
            evaluations[str(seconds)][actor] = records[index] if index >= 0 else None
    return {'validation': 'CHUNK_ENVELOPES_AND_OBSERVATIONS_ONLY', 'version': version,
            'chunks': chunks, 'recording_start_ns': start,
            'actor_observations': tracks, 'evaluated_at_seconds': evaluations,
            'warning': 'Observed alive/death flags are not proof of spawned replay actors. Gaps are UNKNOWN, not despawn. Legacy files have no observation track.'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('file', type=Path)
    parser.add_argument('--at', type=float, action='append', default=[])
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    try:
        text = json.dumps(inspect(args.file, args.at), ensure_ascii=False, indent=2)
        if args.output:
            args.output.write_text(text + '\n', encoding='utf-8')
        else:
            print(text)
    except (OSError, ValueError) as error:
        parser.exit(1, f'Inspection failed: {error}\n')
