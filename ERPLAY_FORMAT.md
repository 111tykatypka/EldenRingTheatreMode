# ERPLAY format version 2

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
