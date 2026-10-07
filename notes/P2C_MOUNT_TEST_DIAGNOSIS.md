# P2c mount test diagnosis — 2026-10-07

## Runtime evidence

The user confirmed starting `MOUNT TEST` on foot, then mounting during recording.
Read-only decoding of `MOUNT TEST.erplay.world` showed 938 player pose frames,
36 actor tracks, and 63 player ride observations. Player ride flags initially
indicate on-foot state; mounted state first appears about 4.107 seconds after
the first player pose. There are also intermediate mounting observations.
Chunk CRC and decompressed-size checks passed during inspection.

Playback's mount compatibility guard stops body ownership when a recorded
mounted state differs from the current live state. This is deliberate: the
prototype has no verified engine API for reconstructing mounting/dismounting.
The old message implied matching the starting state would fix the entire replay;
that is misleading when the recording itself changes states.

The recording reported zero identified companions, with every player mount_id
equal to zero. Therefore Torrent capture is **not verified**, independently of
the unsupported rider transition. A skipped skeleton warning exists but does
not identify which body failed; it is not proof that Torrent failed that check.

## Diagnostic changes

- Guard logs now include replay source timestamp, recorded ride flags/state/
  parameter/mount ID and the live Ride observation (or unavailable).
- UI distinguishes recorded/live states and explicitly explains the unsupported
  transition. Safety checks and mount-state writes remain unchanged.
- During recording, once per second, log buddy ChrSet capacity, raw load-status
  histogram, Active count, owner-validation rejections and accepted count.
- Before range/pose filtering, log buddy bodies and SDK Torrent candidates
  (character_id or npc_id 8000), including skeleton counts, pose-array and
  transform availability. Model ID is diagnostic evidence only, not permission
  to own a body or a new actor-category heuristic.

## Build and next runtime test

28/28 Release x64 Rust adapter tests pass. Runtime validation of the diagnostic
build remains required. Package: `outputs/P2c1-mount-diagnostics`.

Close Elden Ring and every Theater Mode host before changing the loaded DLL.
Launch the EXE from the diagnostic package, use its existing game launch button,
and load an open flat area where Torrent can be summoned. Mount **before** F5;
record 10–15 seconds while remaining mounted, then F6. Stay mounted for Play.
Do not mount/dismount during this test. If blocked, unload the replay to release
control. Send `%TEMP%\TheaterModeGame.log` and the recorder log from
`%LOCALAPPDATA%\EldenRingTheaterMode\logs\TheaterModeRecorder.log`.

Inspect COMPANION_SCAN, COMPANION_CANDIDATE, COMPANION_RECORDED,
COMPANIONS_SUMMARY and COMPANION_REPLAY_BLOCKED before claiming mount replay
works. The original P2b and earlier P2c package are preserved.
