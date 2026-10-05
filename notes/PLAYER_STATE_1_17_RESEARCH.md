# Elden Ring 1.17 player-state adapter research

Research date: 2026-10-04

## Executive finding

A credible native adapter basis exists for the requested Worldwide Elden Ring executable `2.7.0.0` (game patch 1.17). The strongest starting point is KamiyamaShiki0704's `nightreign_style_hud` DLL and its pinned immutable `fromsoftware-rs` compatibility revision `3c8c1d7633a99309fb004c9f894ea10b7967d0e0`. Its README explicitly lists Worldwide `2.7.0.0` and Japanese `2.7.0.1` as separately supported executable versions and says unknown versions are rejected. It also identifies a `nightreign-camera-debug` example and states the DLL reads player state each frame. This is a strong static compatibility lead, not our runtime proof.

The relevant narrow access path is `CSTaskImp` → a recurring callback on `CSTaskGroupIndex::ChrIns_PostPhysics` → `WorldChrMan::instance()` → `WorldChrMan.main_player` / `PlayerIns::local_player()` → `player.chr_ins.modules.physics.position` and `orientation`/`orientation_euler`. Public bindings document the physics module as non-`Send` and non-`Sync`, and document the local-player getter as main-thread-only. Do not cache a player reference across callbacks or loading transitions.

## Evidence labels

- **CONFIRMED**: explicitly present in a cited public source or local file.
- **HIGH CONFIDENCE**: multiple compatible sources support the conclusion, but the exact installed game has not been exercised by our code.
- **LIKELY**: plausible reading that still needs source/runtime validation.
- **UNKNOWN**: not established by available evidence.

## Source review

### 1. `KamiyamaShiki0704/nightreign_style_hud`

- Repository: <https://github.com/KamiyamaShiki0704/nightreign_style_hud>
- Source project: `examples/nightreign-camera-debug`, plus the repository README and lockfile.
- Compatibility dependency revision: `KamiyamaShiki0704/fromsoftware-rs` commit `3c8c1d7633a99309fb004c9f894ea10b7967d0e0` (immutable revision stated by the README).
- Claimed executable profiles: Worldwide `2.6.2.0`, `2.7.0.0`, `2.7.1.0`; Japanese `2.6.2.1`, `2.7.0.1`. The README says the runtime mappings reject unknown versions.
- Claimed implementation: native Rust DLL, reads player/enemy state per frame, custom HUD via `hudhook`/Dear ImGui.
- **CONFIRMED (static documentation):** Worldwide `2.7.0.0` is explicitly listed. This is the target executable version for patch 1.17; do not select the separate `2.7.1.0` / patch 1.17.1 profile.
- **HIGH CONFIDENCE:** this immutable binding revision is the best starting point for the requested player adapter because it is pinned by a native DLL project that claims `2.7.0.0` runtime mappings and includes a camera/player debug example.
- **UNKNOWN:** whether our target executable's whole-file hash is among the exact binaries the author tested. The local file's version resource and SHA-256 were read, but the upstream README does not publish an expected hash in the inspected section.

### 2. `KamiyamaShiki0704/fromsoftware-rs`

- Repository: <https://github.com/KamiyamaShiki0704/fromsoftware-rs> (fork of `vswarte/fromsoftware-rs`).
- Revision selected by `nightreign_style_hud`: `3c8c1d7633a99309fb004c9f894ea10b7967d0e0`.
- Relevant public API sources: `crates/eldenring/src/cs/world_chr_man.rs`, `chr_ins.rs`, `task.rs`, and the `nightreign-camera-debug` example in the consuming project.
- `WorldChrMan` exposes `main_player: Option<OwnedPtr<PlayerIns>>`; docs describe it as the local player. It separately exposes `player_chr_set`, the collection for player characters (including more than just the local player in multiplayer/modded contexts).
- `PlayerIns::local_player()` delegates through `WorldChrMan` and `main_player`. Its safety documentation says it must run on the main thread and must not overlap mutable `WorldChrMan` references.
- `ChrIns` has `modules.physics`; `CSChrPhysicsModule` exposes `position: HavokPosition`, `orientation: Quaternion`, and `orientation_euler: F32Vector4`. Docs describe orientation as character movement rotation. `HavokPosition` is a 16-byte aligned four-float physics position; XYZ are the spatial components.
- `CSTaskImp::wait_for_instance` plus recurring tasks provide the game scheduler integration. The upstream `debug-line` sample reads `WorldChrMan` and physics state in `ChrIns_PostPhysics`; the downstream project provides the more relevant `2.7.0.0` compatibility pin.
- **CONFIRMED (API shape):** the current published bindings expose these fields and the local-player access pattern.
- **HIGH CONFIDENCE:** the pinned downstream revision supplies the expected 2.7.0.0 layout/profile, because the consuming project explicitly declares that profile and pins this exact revision.
- **UNKNOWN:** orientation-euler ordering/units and whether any post-physics presentation correction is desired. Preserve the native quaternion and raw Euler fields in diagnostics until the live test confirms the display convention.
- **UNAVAILABLE in the inspected physics API:** a direct linear velocity field. If later required, derive sampled velocity from successive positions and monotonic timestamps, and label it derived.

Relevant public documentation:

- [WorldChrMan fields](https://docs.rs/eldenring/latest/eldenring/cs/struct.WorldChrMan.html)
- [CSChrPhysicsModule fields](https://docs.rs/eldenring/latest/eldenring/cs/struct.CSChrPhysicsModule.html)
- [PlayerIns source and local_player safety notes](https://docs.rs/eldenring/latest/src/eldenring/cs/chr_ins.rs.html)
- [fromsoftware-rs debug-line task sample](https://github.com/vswarte/fromsoftware-rs/blob/main/examples/debug-line/src/lib.rs)

The docs.rs site reflects the currently published crate and is useful for API/structure evidence; it is not, by itself, a 2.7.0.0 validation. The downstream compatibility pin is the evidence for choosing the version profile.

### 3. `KamiyamaShiki0704/ERSoundBankLoader`

- Repository: <https://github.com/KamiyamaShiki0704/ERSoundBankLoader>
- Relevant release: v0.4.0, abbreviated release commit `eeac7d3` (2026-09-04 per release page).
- Dependency pin in its README: `eldenring` and `fromsoftware-shared` at `902851865d05069eda7dcfd6385c8881eadf29bf` from the canonical `vswarte/eldenring-rs` repository.
- Compatibility claims: 2.7.0.0, 2.7.1.0 and 2.6.2.0 for the Sound Bank path; native SFX work is separately guarded by executable and code fingerprints.
- The project scans `WorldChrMan.player_chr_set`, waits for `CSTask`, and has a compatibility probe for task-registration signatures, Wwise exports, FD4 singleton-reflection signatures and singleton names.
- **CONFIRMED (static documentation):** independent native DLL projects can schedule work and enumerate real player objects in the 2.7 family; the author explicitly separates static executable compatibility checks from narrower runtime validation.
- **HIGH CONFIDENCE:** its fail-closed compatibility probe design is useful for our own diagnostic probe.
- It is not the primary local-player adapter because `player_chr_set` enumerates the player character collection; for the local player, `main_player` / `PlayerIns::local_player()` is narrower and explicitly documented.

### 4. `KamiyamaShiki0704/ERGparamPreloadPatch`

- Repository: <https://github.com/KamiyamaShiki0704/ERGparamPreloadPatch>
- README documents profile WW `2.7.0.0`, per-executable address profiles, and a version 0.1.2 task-runtime map for registering a task callback.
- **CONFIRMED (static documentation):** further evidence that 2.7.0.0-specific native task registration is actively maintained.
- This component has a different GPARAM purpose. Its raw addresses are not relevant to our player read and must not be copied into the player adapter.
- The README distinguishes a local 2.7.1 build from runtime validation, reinforcing that static profile support and in-game verification are separate.

### 5. Canonical `vswarte/fromsoftware-rs`

- Repository: <https://github.com/vswarte/fromsoftware-rs>
- The v0.14.0 release notes mention an Elden Ring 1.16.2 update; this release tag alone is not an adequate 1.17 profile source.
- It supplies the upstream concepts and examples (`WorldChrMan`, `PlayerIns`, `CSTaskImp`, post-physics task sampling), but use the immutable Kamiyama compatibility revision above for this target rather than floating `main` or v0.14.0.
- **CONFIRMED:** the release tag and its version note.
- **UNKNOWN:** any newer untagged canonical commit's 2.7.0.0 coverage not stated by a compatibility profile.

### 6. Supplied Otis_Inf camera tool v1.0.18

- Local read-only reference: `C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRing_CameraTools_v1018`.
- Its `Readme.txt` states the client injects `EldenRingCameraTools.dll` after anti-cheat is disabled and the user directly starts `eldenring.exe`. The changelog explicitly says version 1.0.14 fixed game patch 1.12; the inspected readme does not claim patch 1.17 support.
- **CONFIRMED:** external client + in-process DLL architecture, manual anti-cheat-off offline workflow, and v1.12 changelog entry.
- **UNKNOWN:** whether v1.0.18 happens to run on 1.17. It is not evidence of our `PlayerIns` layout, and its binary must not be reused or modified.

## Local target identity

- Path: `C:\Users\user\Downloads\ELDEN RING\Game\eldenring.exe`
- FileVersion/ProductVersion read from PE version resources: `2.7.0.0`.
- SHA-256 measured locally: `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134`.
- PE target: x64 is expected from the installed executable; the compatibility probe must independently read the PE machine field and reject anything except AMD64.
- **CONFIRMED:** file metadata/hash observed locally on 2026-10-04.
- **NOT VERIFIED:** title-screen App/Regulation version, process memory, singleton resolution, player pointer, live transform values or stability.

## Proposed concrete access path

```text
Version guard (only WW file/product version 2.7.0.0; x64 PE)
  → wait for CSTask scheduler
  → recurring callback at ChrIns_PostPhysics
  → WorldChrMan::instance()
  → main_player / PlayerIns::local_player()
  → player.chr_ins.modules.physics.position
  → physics.orientation + orientation_euler
```

Run the callback every game frame, timestamp with a monotonic clock, and report the measured callback rate. Do not claim a fixed 60 Hz until measured. Re-fetch the singleton and main-player option in each callback; on absence, emit one `PLAYER_LOST`, then retry until found. Do not keep references/pointers across callback invocations or loading transitions. Validate finite values and preserve both quaternion and Euler form until live conventions are confirmed.

## Static facts vs live verification

**Static evidence is strong enough to begin an independently written adapter** for the exact WW `2.7.0.0` profile. It is not enough to claim success. A live test must still prove:

1. the installed file boots in an explicitly offline test mode;
2. our DLL loads and task registration succeeds;
3. the local `PlayerIns` appears after entering the world;
4. XYZ changes during movement and orientation changes during rotation;
5. transitions produce loss/reacquisition instead of stale dereferences;
6. the game exits without a fault.

No live runtime test has yet been performed for our code.
