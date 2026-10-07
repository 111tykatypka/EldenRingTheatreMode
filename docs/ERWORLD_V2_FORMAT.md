# ERWORLD v2: lossless pose sidecar

2026-10-07. Implemented in `adapter/src/world_file.rs`, `codec.rs`, and `skeleton.rs`.
This is the `.erplay.world` sidecar, not a new ERPLAY format. The host still owns the
existing ERPLAY reader and replay clock. No game files or assets are embedded.

## Compatibility

New recordings use version 2. The reader accepts versions 1 and 2; the older
`.bones` reader remains available. Version 1 poses retain their original lossy
quantization; loading them cannot recover discarded precision or actual hierarchy.
An older reader must reject version 2, not decode its pose words as version 1.
Host and DLL must also match editor IPC **version 11**.

## Envelope

All fixed-width fields are little endian. File header is 16 bytes:

| Offset | Type | Value |
|---|---|---|
| 0 | bytes[8] | `ERWORLD1` |
| 8 | u32 | 2 |
| 12 | u32 | reserved, writer emits zero |

Each chunk has a 44-byte header followed by its packed payload:

| Offset | Type | Meaning |
|---|---|---|
| 0 | bytes[4] | `CHNK` |
| 4 | u32 | track ID |
| 8 | u32 | record kind |
| 12 | u32 | record count |
| 16 | u64 | first source timestamp in ns |
| 24 | u64 | last source timestamp in ns |
| 32 | u32 | packed payload bytes |
| 36 | u32 | unpacked payload bytes |
| 40 | u32 | IEEE CRC32 of packed payload |

Chunks are independently decodable. Predictors reset to zero at each chunk.
Player/actor chunks cover approximately one second. Smaller tracks may aggregate
longer intervals. Timestamp headers are descriptive; the decoded time columns
are used for seeking. There is currently no final directory/footer.

The 32-bit chunk-length field is a format limit, not a whole-file limit. The writer
rejects an oversized individual chunk rather than truncating its length. Whole
recordings have no duration or file-size cutoff.

## Tracks and payloads

| Track | Kind | Payload |
|---|---|---|
| 1 | 1 | player frames |
| 2 | 2 | world clock samples |
| 3 | 3 | initial flag groups |
| 3 | 4 | observed flag changes |
| 4 | 5 | actor catalog |
| 5 | 7 | companion/ride observations |
| 6 | 8 | actual skeleton definitions |
| 0x1000 + ReplayActorId | 6 | actor frames |

Unsigned integers use base-128 varints. Signed parent indices use zigzag varints.
Outer packing replaces a zero-byte run of length 1..256 with `0, length-1`;
nonzero bytes are literal. Packing and XOR are lossless.

### Player frame

1. Timestamp delta from the preceding frame in this chunk.
2. 32 float words, each encoded as its u32 bits XOR the preceding word at the same
   column, then unsigned varint: physics orientation[4], interpolated orientation[4],
   physics position[4], render/model matrix[16], chunk anchor[4].
3. Integer columns with XOR/varint: block ID, origin block ID, equipment arm style,
   active slots[6], render handles[22], parameter IDs[22]. Signed IDs retain u32 bits.
4. Bone count.
5. Local pose: `bone_count * 12` float words, bitwise XOR against the preceding
   local pose in this chunk, then unsigned varint.
6. Model pose: same representation, with an independent predictor.

Each bone is a 48-byte hkQsTransform: translation XYZW, rotation XYZW, scale XYZW.
The word order and padding are preserved. A bone-count change resets each pose
predictor. No quaternion compression, sign canonicalization, float rounding or
normalization occurs during storage. All bit patterns, including signed zero and
NaN payloads, round-trip. Invalid semantic coordinates/quaternions are rejected
before game writes; preserving a diagnostic bit pattern does not authorize applying it.

Actor frames append HP and maximum HP, u32-bit XOR/varint, to this body payload.
Their equipment payload is currently empty/default; full NPC equipment recreation
is not implemented.

### Skeleton definition

One record per current recording identity:

```
actor_id       unsigned varint (0 = player)
model_id       unsigned varint (ChrIns.character_id)
fingerprint    unsigned varint (u32 CRC32)
bone_count     unsigned varint
parents[]      signed zigzag varints, int16, one per bone
```

Fingerprint input is little-endian model ID (u32), bone count (u32), then every
parent (i16). `-1` identifies a root. Parents must be in range and acyclic. Equality
checks the entire definition, not just its CRC32; this fingerprint is not a security
hash. Bone names, mesh names and attachment mappings are not included yet.

Metadata and the first pose enter the producer queue together, preventing a full
queue from losing the definition while retaining its first pose. A missing/changed
identity causes a reported capture gap rather than mixing incompatible skeletons.
Mid-recording model/topology changes still need time-versioned skeleton definitions.

### Other tracks

Actor catalog records: ID, game handle (an integer identifier, **not a pointer**),
map entity ID, NPC param ID, character type, first observed timestamp.
Companion records: timestamp, actor ID, category bits, ride flags, ride state,
ride param (signed bits), paired mount ReplayActorId. Unknown mount ID is zero;
it is not fabricated from a model-number guess.

World samples preserve time64, date and time-passage multiplier. Initial flags
store group IDs and 125 bytes per 1000 flags; events store timestamp, flag ID,
boolean. Flag and world-clock data are **read-only** during playback pending autosave isolation.

## Lifetime, integrity and resource behavior

Game callbacks copy owned data. The bounded channel passes no engine pointers to
the writer. File creation, encoding, writes, durability flushes and rename run on
the writer thread. Closing the sender drains the queue; Stop does not join the writer
from a game task. A full queue drops samples and counts the gap rather than blocking.

The worker flushes/syncs approximately every five seconds and syncs on finalization.
A crash can lose the active chunk and queued samples. A `.tmp` may be inspectable;
automatic guaranteed recovery is not implemented. An empty player track is not
renamed into a finalized world file.

Before final rename the worker streams the saved file back one chunk at a time,
checking header/version, CRC, raw length, decoding, timestamp order, actor/catalog
references and skeleton/pose-count agreement. It retains only a chunk plus small
identity maps, not an entire long recording. Failure leaves `.tmp` and logs the
diagnostic; successful validation emits `WORLD_FILE_VALIDATED`.

The reader checks version, CRC, unpacked lengths, decoding, timestamps, catalog IDs,
hierarchy/fingerprint and companion references. Unknown tracks are logged/skipped.
It currently reads compressed file bytes into RAM, indexes timestamps and caches four
decoded chunks per pose track. Recording streams to disk, but playback memory still
grows with file bytes/indexes. Do not claim unlimited-memory playback.

## Accuracy boundary

The v2 codec is bit-exact, verified by automated tests. That proves storage fidelity
only. Final rendered IK/cloth/facial/accessory coverage and write-to-draw retention
need in-game verification. Sample endpoints retain recorded local/model poses;
between samples, local TRS interpolates and model-space poses are reconstructed from
the recorded hierarchy. Intermediate rendered frames are therefore an approximation,
not a captured exact native frame.
