# Independent playback improvements — 2026-10-07

## Scope and provenance

Work is in `EldenRingTheatreMode-independent`, branch `codex/independent-development`, based on Claude commit `e30c5e4676209e46a2f678966daf446a550810f6`. Original Claude source, Step2a binaries, the camera reference and game files were not changed. The current source modifications are uncommitted; the output SOURCE_SHA256 and BUILD_MANIFEST describe the built working tree.

This is the **skeletal player replay** baseline. Historical native-ghost status documents and the inherited build feature name do not describe the active playback implementation. The host owns ERPLAY decoding, ReplayPlayer and the unscaled playback clock. A `.erplay.bones` sidecar contains the recorded local/model-space skeleton. Rust applies the pose and physics transform on the existing task callbacks. No native ghost creation was added.

## Incremental changes and results

| Task | Result | Evidence |
|---|---|---|
| Space Play/Pause | COMPILE_VERIFIED; runtime pending | Release Task1-TimelineToggle |
| Unload Replay | COMPILE_VERIFIED; runtime pending | Release Task2-UnloadReplay |
| Speed dropdown and hover wheel | COMPILE_VERIFIED; runtime pending | Release Task3-PlaybackControls |
| Reference timescale mechanism | STATIC_VERIFIED for getter, write and signature consumers | REFERENCE_CAMERA_TIMING.md |
| Theater timescale controller | COMPILE_VERIFIED, experimental; observation is default | Release Task4-TimescaleProbe |
| Low-speed skeletal interpolation | NOT IMPLEMENTED | Await timescale runtime checkpoint, as required by task order |
| Advanced camera changes | NOT IMPLEMENTED | Await timing stability |
| Cross-session coordinate restoration | NOT SOLVED | Requires coordinate capture/restart validation |

No new behavior in this independent copy is RUNTIME_VERIFIED or VISUALLY_VERIFIED. Automated tests were not run. All four incremental Release x64 builds completed successfully. Existing compiler warnings include unused Rust symbols and the non-snake-case DLL crate name.

### 1. Spacebar

`TheaterRenderBackend.cpp` now sends `toggle_playback`; the host chooses Play/Pause under its replay mutex using the authoritative ReplayPlayer status. Previously the overlay used the legacy transform controller's `phase`, which stays inactive during bone playback; this could send Play on every Space press. The snapshot's playing indicator now also reflects `host_playing`.

Existing Player::pause advances to the current clock position, retains that position and stops advancement. Player::play reanchors the clock and resumes that position; only starting at the end restarts from zero. Space autorepeat remains suppressed; normal gameplay Space and text entry retain the existing input gates. Seeking remains paused; the following Space resumes there.

### 2. Unload

The Replays tab has a localized Unload Replay action. Host unload uses emergency_stop, releases ReplayPlayer/Reader and clears opened replay, previews, characters, bookmarks and selection. Calling unload without a loaded replay is a no-op. Files and sidecars are preserved.

The overlay detects the loaded-path change and resets timeline view, track layout, selection, gizmo, scrub/navigation state and pending speed selection. DLL pose ownership follows its existing release/return path. Pending asynchronous bone loads cannot be adopted for an unloaded/different path; stale failures are ignored. Player absence clears saved ownership/input state rather than applying it to a later replacement player.

Limitations: background file reads are invalidated, not forcibly cancelled. Normal release uses the inherited ten callback restoration frames. Bone ownership still needs lifecycle testing, including unload with the overlay visible and player absence. Player replacement without an absent callback is an inherited identity risk.

### 3. Speed UI

`shared/PlaybackSpeeds.h` supplies common presets and validation:

`0.10, 0.20, 0.25, 0.50, 0.75, 1.00, 1.25, 1.50, 2.00, 4.00`.

Hovering the closed speed combo consumes wheel input: up selects faster, down slower, clamped. Clicking still opens the combo. A pending selection is displayed until acknowledged by the host, or for at most one second, to avoid losing rapid wheel changes to delayed snapshots. This does not intentionally affect wheel input away from the control; that requires manual verification.

The common list is accepted by ReplayPlayer and overlay request validation. Overlay IPC version is now **8**; use the matching new EXE and DLL together. The listed speeds change the replay clock. By default they do **not** yet change world speed: the experimental timing controller is observation-only.

### 4–5. Native timing controller

`adapter/src/timescale.rs` separates engine timing from UI/file decoding. `bone_replay::tick` invokes it in ChrIns_PostPhysics after ownership decisions. It checks both timing consumers against the exact 2.7.0.0 instructions and requires both RIP-relative references to agree on the expected global slot. The unchanged outer profile guard requires the exact supported file version, architecture and SHA-256.

By default it only logs the resolved live manager, scalar at +0x2CC, candidate delta at +0x268, requested speed and whether playback is active. No timing writes occur.

`THEATER_WORLD_TIMESCALE=1` enables experimental writes inherited by a game launched from that host. It requires a loaded skeletal replay owning a present player, active playback, a fresh host snapshot (<500 ms), finite speed in 0.10–4.00 and writable timing memory. Ownership begins only at a normal 1.0 scalar and a plausible delta. Stop, pause, unload, stale/disconnected host or player loss request restoration of the saved scalar. External changes or manager replacement release/inhibit ownership; old manager objects are not written. A new inactive transition clears inhibition. This deliberately avoids fighting another speed mod.

Pause currently restores normal world speed; it pauses/holds the replay actor, **not the whole world**. The reference's real pause patch is distinct and was not copied. Exact world freeze remains future work because callback availability during pause must first be established.

Restoration is callback-driven, not a hard guarantee during game hangs, abrupt DLL unload or suspended task execution. The writable-memory check cannot establish an object's lifetime by itself. Manager identity checks and existing callback lifecycle constrain access, but still require runtime testing. Concurrent speed mods are unsupported in the experiment.

No OS clock hooking, FPS-limit patch, second ReplayClock, game executable changes, custom loader, or Y compensation were introduced. Host steady_clock and DLL QueryInterruptTimePrecise stay unscaled; the replay clock multiplies elapsed time once. The world scalar is separate. 2x/4x physics behavior and individual engine clock domains remain UNKNOWN until measured.

## Discovered next blockers

### Low-speed stutter

The current bone renderer selects `partition_point(time <= timeline) - 1` and copies that whole source pose. It does not interpolate adjacent local/model poses or the root transform. At 0.10x a 60 Hz source pose can remain selected for approximately ten display frames. This is a concrete source-level cause of stepping even though the host interpolates position and quaternion for the ERPLAY view.

After timescale validation, establish the 48-byte hkQsTransform layout and units from the pinned SDK and captured data. Then interpolate translation/scale and shortest-path normalized quaternion SLERP at continuous source time, preserving exact endpoint poses and source-time gaps. Do not interpolate raw byte arrays without this layout proof. Investigate whether model-space poses should be rebuilt from local transforms/hierarchy rather than independently interpolated; validate consistency and draw-phase placement.

### Camera

The supplied camera reference contains a native DLL and managed external client, not source/PDBs. Camera AOBs and documented interpolation/shake controls are leads, not proof of exact damping/shake equations or clock selection. No existing camera controller was replaced and no new camera hooks are active. Continue the binary/client trace only after timing validation; independently implement the smallest proven behavior.

### Cross-session world placement

ERBONES1 currently stores absolute source timestamp, physics orientation/interpolated orientation/position, model matrix and 150 local/model transforms; it does not store native BlockId, MSB coordinates or a validated world-origin conversion. The recorded model matrix is measured but not blindly written during playback. Prior model/physics differences do not justify a fixed Y correction.

Investigate BlockPosition/BlockId, map identity/origin, streaming readiness, controller/proxy synchronization and model/root relationship before changing the format. Old raw-coordinate sidecars cannot be promised to reconstruct unknown world metadata. Require the specified close-game/restart test before declaring the issue solved.

Other inherited limitations: bone capture keeps poses in RAM and stops at 600 seconds; the host's ERPLAY stream has a different storage strategy. This was not changed in the present ordered playback task. Pose sidecars are loaded into RAM; finite/quaternion validation and UTF-8 path truncation deserve separate follow-up.

## Next runtime checkpoint

Use `outputs/Task4-TimescaleProbe/RUNTIME_TEST_PLAN.md`. First validate controls and the read-only timing candidate. Only after that enable the experimental scalar in a second controlled run. Do not advance to camera or coordinate writes while the timing candidate/lifecycle remains unverified.
