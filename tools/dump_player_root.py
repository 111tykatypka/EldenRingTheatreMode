"""Dumps the player's physics position, chunk anchor and block from an ERWORLD v2 file (research tool).

Usage: python dump_player_root.py <file.erplay.world> [every_n_frames]
Prints time (s), physics position, anchor ("chunk_position"), block, origin and the step between
consecutive frames, and marks frames where the anchor changed.
"""
import struct, sys

def varint(b, at):
    v = 0; shift = 0
    while True:
        x = b[at]; at += 1
        v |= (x & 0x7F) << shift
        if not x & 0x80:
            return v, at
        shift += 7

def unpack_zeros(p):
    out = bytearray(); i = 0
    while i < len(p):
        if p[i] == 0:
            out += b"\0" * (p[i + 1] + 1); i += 2
        else:
            out.append(p[i]); i += 1
    return bytes(out)

def player_frames(raw, count):
    at = 0; floats = [0] * 32; ints = [0] * (2 + 7 + 22 * 2); time = 0; bones = {}
    out = []
    for _ in range(count):
        d, at = varint(raw, at); time += d
        for i in range(32):
            v, at = varint(raw, at); floats[i] ^= v
        for i in range(len(ints)):
            v, at = varint(raw, at); ints[i] ^= v
        n, at = varint(raw, at)
        for name in ("local", "model"):
            words = bones.setdefault(name, [0] * (n * 12))
            if len(words) != n * 12:
                words[:] = [0] * (n * 12)
            for i in range(n * 12):
                v, at = varint(raw, at); words[i] ^= v
        f = [struct.unpack("<f", struct.pack("<I", w))[0] for w in floats]
        out.append((time, f[8:12], f[28:32], ints[0], ints[1]))  # position row of transform, anchor, block, origin
    return out

def main(path, every=60):
    data = open(path, "rb").read()
    assert data[:8] == b"ERWORLD1", "not a world file"
    at = 16; frames = []
    while at + 44 <= len(data):
        (marker, track, kind, count) = struct.unpack_from("<IIII", data, at)
        packed, raw_len, _crc = struct.unpack_from("<III", data, at + 32)
        body = at + 44
        if track == 1 and kind == 1:
            raw = unpack_zeros(data[body:body + packed])
            frames += player_frames(raw, count)
        at = body + packed
    t0 = frames[0][0]; prev = None
    for i, (t, pos, anchor, block, origin) in enumerate(frames):
        changed = prev is not None and anchor != prev[2]
        if i % every == 0 or changed:
            step = "" if prev is None else "  step=(%.2f %.2f %.2f)" % tuple(pos[k] - prev[1][k] for k in range(3))
            print("%7.2fs pos=(%8.2f %8.2f %8.2f) anchor=(%7.1f %7.1f %7.1f) block=%d origin=%d%s%s" % (
                (t - t0) / 1e9, pos[0], pos[1], pos[2], anchor[0], anchor[1], anchor[2], block, origin, step, "  <== ANCHOR CHANGED" if changed else ""))
        prev = (t, pos, anchor)

if __name__ == "__main__":
    main(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 60)
