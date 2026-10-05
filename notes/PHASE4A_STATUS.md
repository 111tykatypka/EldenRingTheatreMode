# Phase 4A — safe player transform write probe

2026-10-05 — **IMPLEMENTED — RUNTIME VALIDATION REQUIRED**

Launcher update: `PHASE4A_LAUNCHER_STATUS.md` supersedes the old separate-CMD launch procedure and host hash below. The current host has **START ELDEN RING**; the original probe/DLL checkpoint is unchanged.

Base: current GitHub `main`, `a4ad666` (merge of `aee5afe` Phase 3 snapshot and initial GitHub commit).
Branch: `phase4-in-game-replay-prototype`. The old checkout's uncommitted Phase 3 UI work is deliberately excluded.

## Evidence and current architecture

Inspected adapter `lib.rs`, both replay sources, both ERPLAY sources, monitor, pinned Cargo files, CMake/build script, Phase 1.5/2/3 notes, player access research/implementation notes, and `CSTASK_2_7_0_0_DIAGNOSTIC.md`.
Early Phase 1.5 notes describe pending tests; later task diagnostic and Phase 2 notes plus the user's current confirmation establish successful read-only game sampling. They do not establish transform writes.

The host owns the ERPLAY reader, steady replay clock, position interpolation, and quaternion SLERP. The original pipe is an inbound server in the host and a write-only client in the DLL, carrying the existing 72-byte packets. It remains unchanged. The host additionally reads its connected client PID for pairing control with the same game process.

The pinned revision `3c8c1d7633a99309fb004c9f894ea10b7967d0e0` provides `PlayerIns::local_player_mut()`. Its implementation delegates to `WorldChrMan::instance_mut().main_player.as_deref_mut()` and requires main-thread use with no concurrent WorldChrMan references. The new probe runs before the existing immutable sampler borrow in the same `ChrIns_PostPhysics` callback. No game references/pointers survive a callback or cross IPC. Task lookup/registration signature, shared executable profile and dependencies are unchanged.

## Implemented checkpoint

- Separate local duplex control pipe: `\\.\pipe\EldenRingTheaterMode_1_17_Control`, DLL server and host client. Pipe I/O stays on workers. Game callback uses `try_lock` to consume a copied request; cancellation uses an atomic generation.
- Explicit `PROBE +0.5 X (ONCE)` button. F6, recorder Stop, replay Stop and `PROBE STOP / F6` cancel pending probe requests. F6 registration must succeed before the probe button is enabled.
- Test A: save copied current transform, apply one horizontal 0.5-unit offset, preserve/write the live quaternion, then observe subsequent callbacks for two seconds without more writes. The original transform is logged but not automatically restored, to avoid a second unexpected teleport after manual movement.
- Defaults: OFF. Connection/reconnection never arms a probe. No absolute recorded coordinate can be applied at this checkpoint.
- Invalid data, player loss, runtime not ready, IPC loss or a 500 ms heartbeat expiry disable the experiment. STOP invalidates pending generations; a write already executing cannot be undone by STOP, and subsequent writes are disabled.
- Logs: original/requested/immediate transform, next-callback position/rotation error, final observed transform/callback count, connection/state/STOP transitions. No per-frame log stream.
- Existing offline ReplayPlayer remains independent. There is no Rust ERPLAY parser and no in-game replay clock.

## Control wire v1

64-byte little-endian packet, explicitly encoded/decoded by Rust; C++ layout checked at compile time:

| Offset | Field |
| --- | --- |
| 0 | magic u32 `0x544D4354` |
| 4 | protocol version u16 = 1 |
| 6 | kind u16 |
| 8 | sequence u64 |
| 16 | monotonic boot timestamp ns u64 (GetTickCount64 × 1,000,000) |
| 24 | 3 f32 position values (relative delta for PROBE_NUDGE) |
| 36 | 4 f32 quaternion XYZW (identity reserved payload for probe commands) |
| 52 | state u32 |
| 56 | detail u32 |
| 60 | flags u32 |

Kinds: HELLO=1, HEARTBEAT=2, PROBE_NUDGE=3, STOP=4, STATUS=0x8000. REPLAY_APPLY is intentionally not accepted before the write test is understood.
Command sequences must increase within each connection; a new connection clears acknowledged command state and invalidates earlier requests. Commands must be fresh (500 ms), finite, normalized, have zero reserved fields, and use recognized kinds. STOP is exempt from the timestamp deadline because it cannot cause a write. Nudge must be nonzero, horizontal, and at most 1 unit. No clamping of unsafe commands.
STATUS returns OFF=0, ARMED=1, OBSERVING=2, COMPLETE=3 or ERROR=4; flag bit 0 means exact-profile runtime/player ready. Transform payload is copied sampler telemetry, never a game pointer. Host pairs the control server PID with its sample-pipe client PID. Heartbeats/status exchanges are approximately 10 Hz. Write/read timeout on the host is 300 ms; the game callback never waits on pipe I/O.
Details: 2=lease/link loss, 3=player missing, 4=invalid live/target transform, 5=runtime not ready, 6=panic/mailbox failure, 7=pipe creation failure, 8=invalid/stale packet.

## Physics synchronization evidence

Exact pinned binding sources:

- `crates/eldenring/src/cs/chr_ins/module/physics.rs`: `position: HavokPosition`, `orientation: Quaternion`, `interpolated_orientation`, `last_update_position`, `chr_proxy_pos_update_requested`.
- `crates/eldenring/src/cs/chr_ins.rs`: ChrCtrl's `chr_proxy_flags`, with documented `position_sync_requested` and `rotation_sync_requested` setters. Documentation states these request copying physics-module transform into the underlying Havok character.
- `position.rs`: Havok/block spaces have matching displacement units, but different coordinate origins. This is another reason not to replay arbitrary coordinates across maps.

None of those synchronization fields are written in Test A. Immediate write equality alone does not establish visual model/Havok synchronization. If the next callback or visual test shows overwrite/no motion, evaluate the documented proxy flags as a separate Test B. No raw offsets, pointer casts, input patches, collision changes or Euler conversion are used.

## Validation

Final packaged Release x64 C++ host and Rust DLL compile. CTest 3/3 and Rust 4/4 passed in the final staging run. Both output PE headers were inspected and have machine `0x8664` (AMD64). The existing real-fixture test also passed: 4093 samples, 68.5 seconds, 7 chunks, 59.7372 Hz.
The control transport test uses a uniquely named mock pipe, never the production pipe or a game/DLL. It covers wire layout, explicit nudge, busy refusal, STOP, and sample connection loss. Rust tests cover malformed packets/sequence/timestamps/non-finite data, normalization, displacement bounds, lease expiry and STOP generation.
These tests do not prove game transform writes, visual movement, collision safety or STOP behavior inside Elden Ring. Final packaging/test results are captured by `scripts/Build-Phase4A.ps1`.

Staged directory: `C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Phase4A`.

- `TheaterMode.dll` SHA-256: `25E0CB1539C8E4DB3FA6AB7325844DD4D1EB718FFC2A63E76C90D262FA2F47E0`.
- `EldenRingTheaterMode.exe` SHA-256: `09E60708B28F07D1867D917F2DB9B41E9AC04F81E2B7E9775EC89C6DE2952A36`.
- `YAFSML_Phase4A.ini` contains only a changed theater_mode DLL path relative to the current user's configuration. `Start-EldenRing-Phase4A.cmd` selects it using the existing loader's documented `-c` option and existing `-p` game executable path. The original config entry remains on the stable DLL.
- Build script ran; no host, YAFSML, DLL, or game was launched for a live test in this task.

## Next gate

Run `PHASE4A_MANUAL_TEST.md`. Need user evidence for normal OFF behavior, the single nudge's visual result, next-frame retention, collision stability and F6 returning normal control. The probe preserves orientation, so this test cannot verify visible rotation playback. Do not start a recorded-transform hold or 5/10/68.5-second replay until the transform-write behavior is understood. Then add guarded REPLAY_APPLY commands from the existing C++ ReplayPlayer, retain one replay clock, and require start-displacement/map checks.

Known lifecycle limitation retained from the stable adapter: do not hot-unload the DLL from a running game; task teardown is not implemented. Restart the game to change DLL builds. The original sample worker's reconnect limitations are also retained; launch the host before starting the game.
