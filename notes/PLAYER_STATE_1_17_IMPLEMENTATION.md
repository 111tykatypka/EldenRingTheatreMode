# Elden Ring 1.17 player-state adapter implementation plan

Date: 2026-10-04

## Decision

Use Rust for the in-process game adapter and C++/CMake for the standalone diagnostic/host tools. This is based on the actual references: the strongest 2.7.0.0-compatible implementation is a Rust DLL built on an immutable, explicitly versioned `fromsoftware-rs` fork, while Visual Studio 18 and CMake are installed for native Windows utilities. Rustup stable MSVC was installed for the current user to enable building that dependency.

Dependency selection for the adapter:

```toml
eldenring = { git = "https://github.com/KamiyamaShiki0704/fromsoftware-rs", rev = "3c8c1d7633a99309fb004c9f894ea10b7967d0e0" }
fromsoftware-shared = { git = "https://github.com/KamiyamaShiki0704/fromsoftware-rs", rev = "3c8c1d7633a99309fb004c9f894ea10b7967d0e0" }
```

Cargo.lock must pin the dependency graph. Do not use a floating Git branch in a Release build. Keep attribution and applicable MIT/Apache notices for dependencies. No reference DLL or proprietary camera source is copied.

## Version profile

Create `GameVersion::EldenRing_1_17` as the Worldwide profile for executable `2.7.0.0`. Initial static identity comprises PE AMD64, file/product version `2.7.0.0`, and the locally observed SHA-256 in the research note. A changed hash is not automatically accepted: the compatibility probe must show that version metadata and the dependency's runtime profile match; otherwise fail closed and require explicit profile review. Do not accept Japanese `2.7.0.1`, WW `2.7.1.0`, or any unknown version in this milestone.

Use the pinned binding's runtime version/singleton resolution and task scheduler; do not scatter RVAs. Where a dependency resolves symbols by runtime signatures, expose its errors in diagnostics. Never fall back to raw guessed addresses.

## Game-thread sampler

1. `DllMain` does minimal process-attach setup, disables thread callbacks where safe, starts a worker and returns promptly.
2. Worker waits for `CSTaskImp` initialization through the pinned library, handling errors instead of unwrapping/panicking.
3. Register a recurring, read-only callback for `ChrIns_PostPhysics`, following the public sample's safe game-thread placement.
4. Each callback re-fetches `WorldChrMan` and the current `main_player`. Do not retain a `PlayerIns` pointer between frames. This makes menus, death, respawn and loading transitions explicit `PLAYER_LOST` / reacquire states.
5. Read physics `position`, `orientation` and `orientation_euler`; validate finite components. Include monotonic timestamp and a frame sequence. Capture quaternion as authoritative orientation; also send raw Euler for the requested XYZ display, marked as radians until verified in game. Velocity is `UNAVAILABLE` in this adapter; optionally derive it later from successive positions/timestamps.
6. Sampling rate is callback-driven, not a timer thread. Measure callbacks/second over windows and report the measured result; nominal 60 Hz is not assumed.

All `WorldChrMan` / `PlayerIns` access occurs on the game callback thread. The binding marks those types `!Send`/`!Sync`; references are not passed to the worker, GUI, or IPC thread.

## IPC boundary

Use a versioned local named pipe. The launcher/host creates a per-run pipe before loading the DLL, and communicates the pipe name through a private config/environment mechanism supported by the chosen loader. Never put game pointers over IPC. Define fixed-width, endian-explicit messages:

- `HELLO` / `MODULE_READY` / `GAME_VERSION`
- `PLAYER_FOUND` / `PLAYER_LOST`
- `PLAYER_STATE { protocol_version, sequence, qpc_ticks, position[3], quaternion[4], euler[3] }`
- `ERROR { code, bounded UTF-8 message }` / `HEARTBEAT`

Bound message lengths, reject unknown protocol versions and stale sequences, and report connection errors explicitly. Keep pipe I/O off the game callback: callback publishes a small copied sample to a bounded latest-value queue; an IPC worker serializes/transmits it. Never block the game thread on GUI or disk I/O.

## Overlay

Use a documented DirectX 12 ImGui hook (hudhook) or an equivalent independently implemented rendering layer. If using hudhook, pin its version and include MIT notices. The overlay reads an atomically published copied state only; it must not dereference game objects. Display profile, module state, player found/lost, XYZ and raw Euler values; label units/convention while not verified. If overlay initialization fails, keep telemetry/IPC failure states explicit rather than showing READY.

## Standalone diagnostic tools

`EldenRingCompatibilityProbe.exe --game-exe <path>` is read-only. It reports canonical path, PE architecture, file/product version, SHA-256, profile selection and each static signature/profile predicate available from our adapter. Static success must say `PLAYER ACCESS: RUNTIME REQUIRED`, never `READY` based solely on file inspection. The in-process module separately reports runtime singleton/task/player readiness.

`EldenRingTheaterMode.exe` remains the user-facing monitor and receives the same IPC schema. For an offline launch, use only a transparent, documented modded workflow that does not modify the original game directory and never exposes the modified process to online play. If a loader is necessary, isolate and identify it; do not hide it or mislabel the launcher as dependency-free.

## Safety and lifecycle

- Reject wrong PE machine, version metadata, unresolved runtime profile, failed signatures, missing task singleton or invalid data.
- Never log every sample. Log state transitions, signature/profile status, sampled-rate windows and failures.
- Do not store process pointers beyond one callback.
- Do not swallow panics across FFI; use `catch_unwind` around Rust callback boundaries where possible and fail the sampler closed.
- DLL unload/shutdown must stop IPC and render hooks safely. If the game library does not expose deregistration/unload-safe task cancellation, document that and avoid unloading the DLL while the process remains alive.
- No online mode, EAC hiding, anti-cheat evasion, executable modification, or game-directory writes.

## Validation sequence

### Static/build

1. Run the read-only compatibility probe on the supplied executable; expected file/product version `2.7.0.0`, x64, profile `EldenRing_1_17`.
2. Verify a known unsupported version/path is rejected (the probe's unit tests use fixture metadata; do not use a different live game to imply compatibility).
3. Build Release x64 host, probe and DLL with the pinned dependency lockfile.
4. Verify binary architectures/exports and that DLL logs state `RUNTIME REQUIRED` until initialized.

### Live offline game

1. Confirm the game is in an explicitly offline/modded state and leave original game files unchanged. The local Game folder has no `steam_appid.txt`; do not create it there.
2. Launch the game through a documented external offline loader or report the exact manual action if Steam interaction is required.
3. Enter a playable world, then verify `MODULE_READY`, runtime version, task callback, PlayerIns found and named-pipe telemetry.
4. Move and rotate; compare live values and the overlay. Record the actual callback rate and any errors.
5. Observe a menu/loading/death/respawn transition if safe; verify reacquisition without stale pointer use.
6. Exit normally and inspect launcher/module logs for clean shutdown.

Only after these live checks pass may the project state `PLAYER ADAPTER VERIFIED` and resume replay recorder work.
