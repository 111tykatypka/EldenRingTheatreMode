# Phase 3 — Replay Player + Timeline (2026-10-05)

## Outcome

The native recorder host now includes an offline player for validated ERPLAY v2 files. The original Phase 2 recorder path, game-side DLL, 72-byte PLAYER_STATE protocol, and named pipe were left unchanged. No Elden Ring world state is injected or reconstructed.

## Implemented

- `erplay::Reader` performs full ERPLAY v2 validation before exposing data. It checks the format version, CRCs, footer/counts, timestamp ordering, finite transforms, and quaternion validity.
- The reader builds a binary-searchable replay timestamp/file-offset index and retains one decoded chunk cache. Sample data is loaded on demand by chunk; current 7-chunk replay is small.
- `replay::Player` supplies stopped, ready, playing, paused, seeking, and error states; an independent steady-clock replay clock; speed selection at 0.1x, 0.25x, 0.5x, 1x, 2x, and 4x; timestamp seek; stop/restart; and exact previous/next sample stepping.
- Position uses linear interpolation. Orientation uses shortest-path quaternion SLERP with normalization.
- The native application has a replay list, Open/Delete/Rename controls, player transport, playback speed, timeline scrubber, keyboard accelerators (Space, arrows, Home, End), position/orientation readout, persistent bookmarks, and an X/Z trajectory preview with trail toggle/length.
- Bookmarks are stored in a `<replay>.bookmarks` sidecar. Playback is independent of game connectivity.

## Real replay validation

Fixture: `%LOCALAPPDATA%\EldenRingTheaterMode\replays\replay_2026-10-05_022417.erplay`

- Format and chunk validation: PASS
- Game version: `2.7.0.0` (Elden Ring 1.17)
- Mod version: `0.2.0`
- Duration: 68.500 seconds
- Samples: 4,093
- Recorded rate: 59.7372 Hz
- Chunks: 7
- Integration test: PASS; loaded the real file, sought to 30s and 55s with expected sample indices, returned to the beginning, and stepped to the next recorded sample.

## Tests

Release CTest: **2/2 passed** (`erplay-tests`, `replay-player-tests`). Assertions are enabled for Release test binaries. Player tests cover loading, index lookup, position interpolation, quaternion SLERP normalization, clock advancement, pause/resume behavior, 0.5x playback timing, seek, sample stepping, stop/restart, and bookmark save/load/delete. The real replay integration run passed separately against the recorded file.

## Build and output

Windows x64 Release host built successfully. Updated executable:
`C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\EldenRingTheaterMode.exe`

`TheaterMode.dll` and `EldenRingCompatibilityProbe.exe` were preserved. The game adapter was not rebuilt or changed for this phase.

## Limitations and unverified items

- The visual preview is a 2D X/Z trajectory projection, not a 3D game-world reconstruction. It shows position and an orientation direction cue; it has no orbit camera.
- Replay loading validates and indexes the full file synchronously. Memory for the compact index grows with sample count; decoded transforms are cached one chunk at a time. Very long files may take noticeable time to open.
- Only player position and quaternion are available in this replay. NPCs, events, game simulation, and camera state are absent.
- The API and file integration were exercised automatically against the real fixture. The native controls compiled and the application starts; a manual mouse-by-mouse UI acceptance run (including DPI/resolution matrix) was not performed in this environment.
- The current running game process is not needed for offline player testing. The playback subsystem does not yet apply replay state inside Elden Ring.

## Next phase

Connect a validated ReplayPlayer state to an isolated replay environment only after actor/world reconstruction design and safety checks. Camera work remains out of scope for this phase.
