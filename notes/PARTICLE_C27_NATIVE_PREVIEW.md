# C27 — experimental native FXR preview

## Status

2026-10-08, branch `codex/custom-lights-prototype`. Particle changes remain uncommitted.

- STATIC_VERIFIED: the SHA-checked Elden Ring 2.7.0.0 image has the scene-create wrapper and per-effect stop/release paths described below.
- COMPILE_VERIFIED: Windows x64 Release C++ host/native library and Rust DLL, locked offline dependencies.
- RUNTIME_VALIDATION_REQUIRED: native resource-list reading, create ABI/task-phase suitability, visible effect position/orientation, and cleanup. A non-null handle is not visual success.
- The user's C26 test confirms editor markers, not native effects.

## Native chain and evidence

`CSSfxImp` -> `scene_ctrl` (+0x60) -> scene create RVA 0x1CA0CB0 -> implementation RVA 0x1CA69B0.

The create wrapper takes scene, output handle, FXR ID, optional parameters, aligned 4x4 matrix, and two integers. Disassembly shows matrix transposition to 3x4 before forwarding. Default null parameters are explicitly handled downstream; the payload/configuration is copied into native allocation, not retained on our stack.

The output storage is 0x410 bytes: a tracked handle, supplementary data and copied configuration. Its address must stay fixed because native tracking links refer to the handle itself. We use one static aligned buffer.

Owned stop RVA 0x20B81C0 -> 0x20DAD00 sets an instance stop bit, consumed by 0x20DAA80 -> 0x20DA450. Handle release RVA 0x20B7A40 unlinks tracking via 0x20B8CB0. This is separate from the debug dispatcher's shared scene clear (RVA 0xD91FD0), which is never called. RVA 0x20B8440 is configuration propagation, not the stop API.

Transform-update candidate RVA 0x20B8530 was identified but is not invoked in C27. Matrix conventions are borrowed from our camera transform code and remain visually unverified for this FXR API.

Evidence indexes: `research/particle_dispatcher_c26.json`, `research/particle_create_flow_c27.json`, `tools/particle_control_flow.py`, the local SHA-checked Ghidra SQLite/disassembly index. The bounded tracer follows trampoline constants and records unresolved branches; it is not a complete emulator or proof of all execution paths.

## What C27 actually does

An explicit unchecked experimental checkbox enables a two-real-second native preview at the selected emitter's transform. The resident game FXR list provides actual resource IDs, separate from the editor's placeholder preset IDs. The selected ID is revalidated against the current resource list on the game callback before creation.

UI requests are queued. Only the existing Rust game callback performs native calls. Each attempt reacquires CSSfx/scene access. Code-prefix guards supplement the existing 2.7.0.0 profile guard. Creation, stop and release are protected by thin SEH boundaries; a native exception disables the experiment for that process. SEH cannot guarantee recovery from native assertions, wrong lifecycle assumptions or partial mutations.

The preview expires using real time, so slow native simulation does not leave it enabled indefinitely. Stop/another preview/world-unavailable also retires its owned handle on the callback. Playback Stop queues preview stop. If callbacks cease, cleanup cannot occur until they run again. A disconnected host is not currently a separate immediate stop trigger.

The resident-list scan has an approximately 8 ms budget. A partial list is reported as partial; it is not the complete game's particle catalog. Scans are explicit, not every frame. No semantic names/categories or resource loading are implemented yet.

## Build/package

Separate output: `outputs/Cinematic-C27-native-particle-preview`.
Stable C24/C26 packages and original project/game files were not replaced.
Native build: CMake Ninja Release in `build-particle-c26-release`, VS18 x64 environment. Rust: `cargo build --manifest-path adapter/Cargo.toml --release --locked --offline --target x86_64-pc-windows-msvc`, with `THEATER_NATIVE_LIBRARY_ROOT` set to that native build directory. Its Release library copies serve the existing Rust build script.

## Exact first in-game check

1. Close the old Theater host and Elden Ring to unload the prior DLL. Do not mix host/DLL versions or load multiple Theater DLLs.
2. Launch `outputs/Cinematic-C27-native-particle-preview/EldenRingTheaterMode.exe`. Configure the existing YAFSML workflow to load **that folder's TheaterMode.dll**. No game-file replacement or new injector is required.
3. Launch the game through your existing offline workflow; load a normal playable world. Leave replay stopped and timescale at 1x for this first check.
4. F4 -> Particles -> **Inspect native VFX**. Confirm a resident catalog appears. If empty, report its status; do not guess numeric IDs.
5. **Add emitter at camera**, select it, and place it several units in front of the camera so it is visible rather than behind/inside the near plane.
6. Select an ID under **Resident game effects**. Check **Experimental native FXR preview**. Click **Preview selected FXR here (2 seconds)** once.
7. Observe whether an effect appears at that position and disappears after the preview. **Stop native particle preview** explicitly requests cleanup. Many resident resources can be invisible, attached-effect specific or unsuitable for a standalone preview; an empty-looking effect alone does not prove the call failed.
8. Send the FXR ID, visible result and `%TEMP%/TheaterModeGame.log` lines beginning `PARTICLE_PREVIEW`. Expected diagnostics: `CREATE id=... handle_present=1`, followed by `STOP owned preview handle released`. Also send the displayed native status if the list/guards fail.

## Still missing

Persistent/multiple native emitters, native transform updates after creation, scale/intensity semantics, catalog names/categories, looping/repeat, resource loading, master ReplayTime integration, seek/reconstructed effect age and validated transition/disconnect cleanup. Existing editor loop/scale/intensity settings do not yet control this preview. The scheduler unit tests cover synthetic lifecycle/protocol behavior, not native execution.

The next implementation step depends on which FXRs produce a visible, correctly placed, cleanly stopped effect in this controlled call. Then integrate stable owned handles and transform updates into the existing scheduler rather than respawning on every render frame.
