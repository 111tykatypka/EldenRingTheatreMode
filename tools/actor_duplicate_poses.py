"""Counts how often each recorded actor's pose repeated unchanged (research tool for the update-LOD override).

Usage: python actor_duplicate_poses.py <file.erplay.world> [more files...]
For every actor: frames, consecutive frames whose local pose is bit-identical to the previous one, the share of such
duplicates, and the mean distance to the player. Actors are grouped by distance (<20 m, 20-60 m, >60 m), because the game
updates far characters less often. Compare a recording made before the override with one made after it.
"""
import struct, sys, collections, math
sys.path.insert(0, __file__.rsplit('tools', 1)[0] + 'tools')
import dump_player_root as d

def frames(raw, count, hp):
    a = 0; floats = [0] * 32; ints = [0] * 53; time = 0; bones = {}; out = []
    for _ in range(count):
        dt, a = d.varint(raw, a); time += dt
        for i in range(32):
            v, a = d.varint(raw, a); floats[i] ^= v
        for i in range(len(ints)):
            v, a = d.varint(raw, a); ints[i] ^= v
        n, a = d.varint(raw, a)
        local = None
        for name in ("l", "m"):
            w = bones.setdefault(name, [0] * (n * 12))
            if len(w) != n * 12: w[:] = [0] * (n * 12)
            for i in range(n * 12):
                v, a = d.varint(raw, a); w[i] ^= v
            if name == "l": local = tuple(w)
        if hp:
            _, a = d.varint(raw, a); _, a = d.varint(raw, a)
        f = [struct.unpack('<f', struct.pack('<I', x))[0] for x in floats]
        out.append((time, f[8:11], local))
    return out

def analyse(path):
    data = open(path, 'rb').read(); at = 16; player = []; actors = collections.defaultdict(list)
    while at + 44 <= len(data):
        marker, track, kind, count = struct.unpack_from('<IIII', data, at)
        packed = struct.unpack_from('<I', data, at + 32)[0]; body = at + 44
        raw = d.unpack_zeros(data[body:body + packed]) if (track == 1 and kind == 1) or (track >= 0x1000 and kind == 6) else None
        if raw is not None:
            (player if track == 1 else actors[track - 0x1000]).extend(frames(raw, count, track != 1))
        at = body + packed
    ptimes = [p[0] for p in player]
    import bisect
    groups = {"<20 m": [0, 0, 0], "20-60 m": [0, 0, 0], ">60 m": [0, 0, 0]}  # frames, duplicates, actors
    seen = {k: set() for k in groups}
    for aid, fr in actors.items():
        for i in range(1, len(fr)):
            t, pos, local = fr[i]
            j = min(bisect.bisect_left(ptimes, t), len(player) - 1)
            dist = math.dist(pos, player[j][1])
            key = "<20 m" if dist < 20 else "20-60 m" if dist < 60 else ">60 m"
            groups[key][0] += 1; groups[key][1] += local == fr[i - 1][2]; seen[key].add(aid)
    print(path.replace("/", chr(92)).split(chr(92))[-1], " actors:", len(actors))
    for k, (n, dup, _) in groups.items():
        print("  %-8s actors=%2d frames=%7d identical-to-previous=%7d (%.1f%%)" % (k, len(seen[k]), n, dup, 100.0 * dup / max(n, 1)))

if __name__ == "__main__":
    for p in sys.argv[1:]:
        analyse(p)
