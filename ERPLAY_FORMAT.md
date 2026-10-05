# ERPLAY formats 2 and 3

The Phase 2 recorder core writes little-endian `.erplay` files. Strings are UTF-8 byte sequences. The format stores raw position and quaternion floats; it does not store Euler angles or camera data.

## File layout

1. Header: 8-byte magic `ERPLAY02`, `u32` version (`2`), `u32` flags, `u64` Unix start time in ns, `f64` requested Hz, `f64` measured Hz, `u64` sample count, `u64` active replay duration in ns, `u64` total paused wall-clock duration ns; then five `u32` byte-length-prefixed UTF-8 strings: game version, mod version, title, description, tags.
2. Repeated chunks: `u32` marker `CHNK` (`0x4B4E4843`), `u32` sample count, `u64` payload byte length, `u32` CRC-32/ISO-HDLC of payload, then fixed-size samples.
3. Footer: `u32` marker `FOOT` (`0x544F4F46`), `u64` chunk count, `u64` sample count, `u64` duration ns, `u64` paused wall-clock duration ns (must match the header).

Each sample is 52 bytes: `u64` contiguous sample index, `u64` active replay timestamp ns, `u64` source monotonic timestamp ns, `f32` position XYZ, `f32` quaternion XYZW. Source timestamps must be nondecreasing; equal ticks are retained because the validated source clock can have millisecond resolution. Replay timestamps may also be equal, but cannot move backward. Samples and quaternions are checked for finite values; quaternion squared norm must be in `[0.25, 2.25]`.

## Write and recovery behavior

The writer appends bounded chunks to `<name>.erplay.tmp`, flushes each chunk, then writes the footer and finalized summary. It validates the temporary file and renames it to `.erplay` only after validation. An interrupted recording remains a `.tmp` file without a complete footer. Recovery copies complete, checksum-valid chunks to a distinct `<name>.erplay.recovery.tmp`, validates the rebuilt file, and leaves the original `.tmp` intact. A torn trailing chunk is discarded. This is best-effort recovery, not a guarantee against process or storage failure.

There is no configured total sample or duration cap. The per-chunk sample count is configurable. CRC-32 detects accidental chunk corruption; this is not a cryptographic authenticity mechanism.

Format v2 is a new format; the earlier POC format is not migrated by this implementation. Readers must reject unsupported versions rather than interpret them as v2.

## Phase 3 read path

The Replay Player first runs the complete validator, then builds an in-memory index of each sample timestamp and payload offset. Timestamp lookup uses binary search. Decoded sample payloads are read by chunk and held in a one-chunk cache; the entire set of transform samples is not retained in memory. The index is proportional to sample count. Playback preserves stored timestamps, linearly interpolates position, and uses normalized shortest-path quaternion SLERP for orientation.

## Version 3 / typed action track

New Phase5 recordings use magic `ERPLAY03`, format=3, mod version `0.3.0`. The fixed 64-byte header, five UTF-8 strings and **52-byte CHNK transform payload** retain the v2 layout above. A Sample's in-memory optional action is never appended to the transform struct. Existing finalized v2 files and v2 recovery remain supported; no animation is inferred for them. v2 readers must refuse v3, never treat it as v2.

Between transform chunks, v3 adds `TRAK` (0x4B415254) records. The writer writes a transform chunk before its associated action chunk.

| Offset | Type | Meaning |
| --- | --- | --- |
| 0 | u32 | TRAK marker |
| 4 | u32 | track type: 2 = PlayerActionTrack |
| 8 | u32 | flags: bit0 required; all other bits rejected; current writer uses optional=0 |
| 12 | u32 | positive event count |
| 16 | u64 | payload byte count (type2 must equal count * 40) |
| 24 | u32 | CRC-32/ISO-HDLC of payload only |
| 28 | bytes | serialized events |

Each event is exactly 40 bytes, little endian, with no native struct padding:

| Event offset | Type | Field |
| --- | --- | --- |
| 0 | u64 | active replay timestamp, nanoseconds |
| 8 | u32 | normalized action enum |
| 12 | u32 | observation flags |
| 16 | u64 | raw runtime action-request bits; NOT an executed action ID |
| 24 | i32 | raw TAE animation ID, -1 when unavailable |
| 28 | f32 | observed play_time (seconds) |
| 32 | f32 | observed anim_length (seconds) |
| 36 | f32 | observed behavior.animation_speed (raw field; not a verified playback multiplier) |

Action enum: Unknown=0, Idle=1, Walk=2, Run=3, Sprint=4, Turn=5, Jump=6, Fall=7, Land=8, Roll=9, Backstep=10. Unknown values are rejected. Runtime currently emits Unknown except when the actual animation ID equals the runtime's default idle ID: that observation is Idle with provenance bit3. Walk/Run/etc names are representation, NOT a fabricated runtime ID table.

Flags: bit0 animation ID valid; bit1 time/length valid; bit2 rate valid; bit3 default-idle-ID match. Bits outside 0..3 rejected. Absent values are zero except animation_id=-1. All floats must be finite. Valid time requires valid ID, time>=0, length>0, time<=length+1; valid rate is 0..10. No normalized phase is claimed. These plausibility guards do not prove semantic correctness in game.

Events are nondecreasing in active replay time, including across chunks, and cannot exceed the final transform duration. Equal timestamps are allowed; last event at a timestamp wins. Indices/source timestamps remain on the transform track. Pause wall time is omitted identically for both tracks.

The writer emits changes of enum/ID/flags/request bits/rate and observed time wrap (>0.1 s backward). It also emits a time observation at most every 500 ms when unchanged. This shares the action track; `Animation Sync` in UI means stored time observations, **not implemented animation-time playback writes**. Unchanged IDs are not retriggered for those sync observations.

The v3 FOOT is 44 bytes: the v2 36-byte footer followed by u64 count of known PlayerAction events. Transform chunk count excludes typed chunks. The complete validator checks this count, CRC, lengths, enums, timestamps, transform summaries and trailing bytes. Unknown optional tracks are checksum checked and skipped; unknown required tracks reject the replay. Their counts do not contribute to the PlayerAction footer count. Payload length is checked against actual remaining file bytes before allocation.

Recovery preserves complete checksum-valid transform and typed chunks from an unfinished zero-summary header; discards a torn final chunk; rebuilds both summary and known-action footer count. It never invents events for a lost action tail. CRC mismatch or invalid complete events cause failure, not silent salvage. Original .tmp stays untouched. Process/disk failure between final header rewrite and rename can still require external diagnosis; perfect crash recovery is not promised.

Reader retains a compact transform offset/timestamp index, one transform chunk cache and a compact action-event vector. Sequential playback advances a cursor; backward seek/restart uses upper_bound. Action IDs/time are held as observations; they are not linearly interpolated. There is no file-duration cap; index/action memory scales with session length and change frequency.
