# Phase 2 — Replay Recorder Status (2026-10-05)

## Live capture result

**A real 68.5-second gameplay capture was recorded and finalized successfully.** The host remained connected to Elden Ring and consumed the verified `PLAYER_STATE` stream. The finalized replay passed the recorder's full validation during finalization, including header, sample ordering, chunk checksums, footer, and size consistency. The temporary file was atomically renamed to the final `.erplay` file.

- Replay: `%LOCALAPPDATA%\EldenRingTheaterMode\replays\replay_2026-10-05_022417.erplay`
- Duration: 68.500 seconds
- Samples: 4,093
- Measured rate: 59.737 Hz (requested: 60 Hz)
- Chunks: 7
- File size: 213,128 bytes (about 3.11 KB/s)
- Game version in replay metadata: 2.7.0.0
- Recording source: live Elden Ring `PLAYER_STATE` IPC samples (player position XYZ and orientation quaternion XYZW)
- Recorder log: `%LOCALAPPDATA%\EldenRingTheaterMode\logs\TheaterModeRecorder.log`

The earlier 18.234-second attempt and shorter hotkey checks are not used as the acceptance capture. The 68.5-second file is the first capture in this run meeting the requested minimum. The recorder's finalization path validated the completed file before renaming it; there were no validation errors in the log.

## Build and control status

- Release x64 host with a native resizable Windows UI is staged at `C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\EldenRingTheaterMode.exe`.
- UI controls: Start, Pause, Resume, Stop, replay list, and replay-folder access.
- The log confirms global F5 Start, F6 Stop, F7 Pause, and F8 Resume hotkeys registered. The successful long recording and finalization were observed through the live recording log; the in-game pipe integration remained connected.
- The DLL/game adapter and named-pipe packet format were not changed in this control/UI work. The loaded DLL SHA-256 is `1CC4A71EABB46EC5F9049EC774F79F1C4EC8BCEEDD338E5BECD640C4191E1934`; it was locked by the running game during the build attempt, so replacing the output DLL filename was blocked. No claim is made that a new DLL was installed.
- CTest: 1/1 passed, including ERPLAY serialization/validation, pause accounting, Unicode metadata, and the same-basename recovery regression.

## Recovery incident

An earlier capture ran for more than eight minutes before the console host was replaced with the native UI. A recovery-path bug opened the original `.tmp` as its own output temporary file and truncated it before validation. That long capture is lost; the damaged 116-byte `.tmp` and recovery scratch are retained. The recovery implementation now writes to a distinct `.recovery.tmp`, preserves the source, and is covered by the passing same-basename regression test.

## Scope and limitations

This phase records the verified player's timestamp, XYZ position, and XYZW orientation only. It does not record NPCs, combat/game events, animation/equipment, map metadata, or camera state. No replay playback, timeline scrubbing, or camera subsystem was implemented. The measured sample rate and file throughput are available, but CPU/memory overhead and game FPS impact were not instrumented, so no performance-impact number is claimed. No dedicated visual DPI/resolution matrix test was performed.

## Phase 2 disposition

The requested live 60-second replay capture is complete and validated. The next phase can build on the existing structured player-state samples; actor/event capture and replay reconstruction require separate implementation and validation.
