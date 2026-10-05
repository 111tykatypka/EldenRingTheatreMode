# Phase 1.5 final status — first native build

Date: 2026-10-04. This report records work performed in this phase. Build/static identity checks pass; live game/player validation remains pending and the requested acceptance test is NOT PASS.

## Environment

- Compiler: MSVC from Visual Studio 18, MSBuild 18.10.1.
- CMake: Visual Studio bundled CMake 4.3.1 (configured project caches).
- Rust: rustc 1.99.0, host/target `x86_64-pc-windows-msvc`.
- Dependencies: immutable `fromsoftware-rs` fork revision `3c8c1d7633a99309fb004c9f894ea10b7967d0e0`; Cargo build used `--locked --offline`.

## Game

- Path: `C:\Users\user\Downloads\ELDEN RING\Game\eldenring.exe`.
- File and product version: 2.7.0.0; PE AMD64.
- SHA-256: `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134`.
- Static compatibility probe: PASS for this exact file identity.
- Game process: not running at initial validation; no PID or live-game check yet.
- The game directory was not changed. No anti-cheat bypass or online session was attempted.

## Integration

- Chosen architecture: C++ console monitor + Rust x64 cdylib + local named pipe.
- Chosen library: pinned Kamiyama `fromsoftware-rs` revision above. The 2.7.0.0 profile and `WorldChrMan.main_player` access are supported by the earlier static research; libER's checked-in snapshot is aimed at 1.16.0 and was not selected for this target.
- DLL initializer starts a sampler worker, waits up to 120 seconds for `CSTaskImp`, registers a read-only `ChrIns_PostPhysics` recurring callback, and has a separate pipe worker.
- The callback re-reads `WorldChrMan` and `main_player`, then copies physics position, quaternion and raw Euler values. No engine pointers cross IPC. Sampling is callback-driven.

## Player Access

- World manager method: `WorldChrMan::instance()` from the pinned binding.
- Player method: current `WorldChrMan.main_player` on each callback; player pointers are not cached.
- Position source: `player.chr_ins.modules.physics.position` XYZ.
- Orientation source: `physics.orientation` quaternion; `orientation_euler` is also sent raw, with convention still unverified.
- Velocity, animation/action state, actors, overlay and events: not implemented.
- The compiled DLL has only been build-validated. No live WorldChrMan/player resolution has been demonstrated.

## Runtime Test

| Check | Result |
|---|---|
| Game launched / live PID | NOT YET |
| DLL loaded in Elden Ring | NOT YET |
| Game task initialized | NOT YET |
| Player found | NOT YET |
| Real position changes on movement | NOT YET |
| Orientation changes on rotation | NOT YET |
| IPC samples received live | NOT YET |
| Reacquisition after transition | NOT YET |
| 60-second stability / crash test | NOT YET |
| Clean module/app shutdown | NOT YET |

The user said they would start the game in their established offline single-player workflow and notify us when ready. We must not claim live testing before that happens and before the DLL is actually loaded through a supported loader.

## Build

- Release C++ monitor build: PASS.
- Release CompatibilityProbe build: PASS.
- Release Rust DLL build (`cargo build --release --locked --offline`): PASS.
- Static probe run against target executable: PASS (2.7.0.0, AMD64, expected SHA-256). It explicitly reports runtime signatures and player access as NOT CHECKED / RUNTIME REQUIRED.
- Outputs copied to `C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode`:
  - `EldenRingTheaterMode.exe`
  - `TheaterMode.dll`
  - `EldenRingCompatibilityProbe.exe`
- `build_release.bat` reproduces configure/build/copy steps for x64 Release.

## Limitations

- The executable monitor currently creates a named pipe and renders received values in a console. It does not locate/launch Elden Ring or load/inject the DLL.
- DLL loading therefore depends on the user's existing offline mod-loader workflow. No loader was identified/configured in this project, and the game installation remains read-only.
- The requested in-game debug overlay and F9/F10 controls are not implemented.
- DLL runtime version check currently reads the process executable version resource; only the standalone static probe also compares the exact local SHA-256. Runtime code profile-fingerprinting should be strengthened before general distribution.
- The sample stream and task scheduling are compile-validated only. Pointer safety, callback lifecycle, coordinate/orientation interpretation, reconnect behavior, and crash behavior require in-game validation.
- The project does not include a graceful task-unregister path or verified module unload. Keep the game module loaded for game-process lifetime until callback teardown is implemented and tested.
- No recorder, replay format, or fabricated state was added.

## Next phase gate

Complete live validation of this player-state stream first. If it passes, the next phase should consume copied timestamped `PlayerSample` values into a chunked `.erplay` recorder. Do not implement deterministic replay reconstruction based only on player transforms.

## Live session update

A subsequent user-started Elden Ring process was observed at PID 2328 (started 2026-10-04 23:18 local time). The monitor process was started at PID 6248. As of this update, the monitor has not received a pipe connection; no game-side module load or player sample is evidenced. The user said they would load `TheaterMode.dll` with their offline mod loader. Runtime status remains NOT YET for all player/game checks.

## Follow-up: profile rejection diagnosis (2026-10-05)

The generic host error maps to IPC `kind=5`. In the pre-diagnostic DLL, that packet was sent for either (a) `target_version_matches()` failing or (b) `CSTaskImp::wait_for_instance(120s)` returning an error. A caught initialization panic was silently swallowed and did not send that packet. The old wire payload carried no cause code and the old DLL wrote no runtime identity log, so the exact historical branch cannot be reconstructed from the host text alone.

The old runtime check inspected only the process executable's **file version** words (`2.7.0.0`); it did not check product version, PE machine, path, SHA-256, image base, or emit a reason. The static probe separately checked file version, AMD64 and SHA-256, which allowed divergent behavior. This is fixed in source: the static probe and DLL now both call the same compiled `shared/GameProfile.cpp` validator and consume the single `shared/GameProfile.h` `EldenRing_1_17` profile. It checks exact executable path, file version, product version, AMD64 PE machine, on-disk file SHA-256 and (runtime only) loaded image base. The SHA is of the executable file on disk, not the mapped image or `.text` section.

The runtime DLL writes values and per-predicate PASS/FAIL to `%TEMP%\TheaterModeGame.log`. Error codes distinguish each profile check, `CSTaskImp` timeout (`0x201`), initialization panic (`0x202`), and worker creation failures. The host displays the code and likely cause. The DLL logs `MODULE_READY` only after `CSTaskImp` resolution and recurring sampler registration.

The game process still has the older DLL mapped from the final output path, so Windows denied overwriting that file. The rebuilt x64 DLL is staged as `TheaterMode_1_17_diagnostics.dll` for YAFSML reload without replacing the mapped file. Updated host and probe are deployed. Static shared-validator probe again reports all five file identity checks PASS, selecting `EldenRing_1_17`. Runtime cause, `MODULE_READY`, and `PLAYER_FOUND` are pending loading this diagnostic build.

## Profile-rejection fix build status (2026-10-05)

- Exact old host message source: `src/monitor.cpp`, `if (m.kind==5)`. In the pre-fix DLL, `pipe_worker` emitted kind 5 when `PROFILE==2`. `PROFILE` was set to 2 by either `target_version_matches()==false` or `CSTaskImp::wait_for_instance(120s)` returning an error. The old packet contained no reason code; no old runtime log exists. Thus the historical branch cannot be distinguished after the fact. Given the observed running executable path/version and the static probe pass, the old file-version predicate should pass; a task-runtime timeout was another real cause and cannot be ruled out.
- Unified validator: `shared/GameProfile.cpp` is now called by both the static probe and the Rust DLL. Profile data is in `shared/GameProfile.h`. It reports path, file/product versions, PE machine, file-on-disk SHA-256, and runtime image base; every check has an individual flag and an error code.
- DLL diagnostic log: `%TEMP%\TheaterModeGame.log`. Host displays the error code and mapped cause. The module logs `MODULE_READY` after task acquisition and sampler registration.
- New x64 MSVC Release build completed. `build_release.bat` deploys the output files. The final DLL at `outputs\EldenRingTheaterMode\TheaterMode.dll` has SHA-256 `AA6C957616B4412082F1EC11109C2F5A90306AD864F938B2050BB7F232F5EC98`.
- Updated compatibility probe was rerun and passed path, file version 2.7.0.0, product version 2.7.0.0, AMD64, and SHA-256 checks, selecting `EldenRing_1_17`.
- The refreshed host is running and waiting. As of this entry, Elden Ring has not been relaunched with the updated DLL, so runtime profile result, `MODULE_READY`, and `PLAYER_FOUND` are not yet verified.
