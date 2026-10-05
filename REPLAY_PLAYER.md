# Replay Player — Phase 3

## Layers

`ERPLAY file -> erplay::Reader -> replay::Player -> native UI / preview`

- `erplay::Reader` runs full format, checksum, sample-order and transform validation before exposing samples. It builds a timestamp/file-offset index, uses binary search for time lookup, and loads one chunk into its cache at a time.
- `replay::Player` owns the replay clock and playback state. It uses steady-clock elapsed time scaled by the selected speed; it does not alter the source file. Seeking clamps to the recorded duration. Stepping follows sample indices and recorded timestamps.
- Position is linearly interpolated across adjacent samples. Orientation uses shortest-path normalized SLERP.
- The UI is independent of the game process for browsing and playback. Recorder IPC and controls remain in the existing host.
- Bookmarks are sorted nanosecond timestamps in a UTF-8/ASCII numeric sidecar `<replay>.bookmarks`.

## Supported playback speeds

0.1x, 0.25x, 0.5x, 1x, 2x, 4x. The time base is the stored replay timestamp, not a presumed 60 Hz sample interval.

## Preview

The preview draws an X/Z trajectory projection on a grid, with the current position and an orientation indicator. It is a data inspection view; it does not recreate the Elden Ring environment. The preview path is decimated to at most roughly 3,000 points for drawing. Trail visibility and percentage of recorded history are adjustable.

## Compatibility

Reader rejects unknown ERPLAY format versions through the v2 validator. Player currently accepts only game version `2.7.0.0` and mod version `0.2.0`; other values show an incompatible/load error. The test replay is a live capture, not generated test data.

## Limits

Timestamp index memory grows with sample count (24-byte index entries in the current implementation); sample transforms remain chunk-cached. File validation and index creation are synchronous during Open, so very long files may take noticeable time to load. Current state contains only player position/orientation; no game world is reconstructed.
