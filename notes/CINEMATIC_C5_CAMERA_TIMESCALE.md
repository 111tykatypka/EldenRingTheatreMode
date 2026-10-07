# C5 — CameraTools research and independent timing/input port

Branch: codex/cinematic-editor-pass. Parent source commit: 6ffaeac. Final package: outputs/Cinematic-C5-final. Stable Phase5/C4 packages remain unchanged.

## Verification

STATIC_VERIFIED: CameraTools scalar chain, exact 2.7.0.0 two-site RIP root, CSFlipperImp RTTI/constructor default, native camera copy suppression/writer, QPC-based camera clock, managed/native setting protocol and input hooks.

COMPILE_VERIFIED: Release AMD64 C++ EXE and Rust DLL built. 14/14 CTest passed. Rust49 passed,0 failed,1 optional test ignored. Added keybinding decoder/cut boundary tests; native timing guard rejects test executable. No tests load or mutate Elden Ring.

RUNTIME_VERIFIED / VISUALLY_VERIFIED: no new live evidence. The user's earlier player/bone replay verification does not prove these camera/timing changes.

## Changes

GameTimingAdapter has default-OFF opt-in ownership, profile opcode guards, pointer reacquisition, change-only scalar writes, foreign-owner rejection and conditional restoration. Emergency Stop disables requested timing and camera; restoration happens on next verified game callback. Main-menu startup is not armed. Active replay menu coverage remains UNKNOWN.

Continuous .001..10 slider removes snap and keeps precision drag/wheel/reset/numeric controls. Free Camera keeps real dt; Dolly/Shake/Cuts use master ReplayTime. Rebindable single-key actions persist to keybinds.ini and reload off-hook. Host global hotkeys fall back if new registration fails. UTF-8 key labels, capture cancellation and typing suppression included.

CutTrack is session-only Player/current-Dolly; .ercam path edit/persistence remains. The new stop/cut/timing paths do not refactor NPC replay/lifetime. Version guards and YAFSML stay unchanged.

## Required remaining work

This is NOT full requested CameraTools parity. Missing: true native world pause/frame stepping, live scalar subsystem/menu proof and range stability, modifier chords/controller support, camera-state slots, multi-path persistent cuts, 3D key gizmos/picking, player-relative paths, rotation/FOV damping parity, configurable speed multipliers, HUD/stealth/LOD/aspect adapters.

Research reports include exact evidence and gaps, not guessed addresses. CAMERA_SYSTEM.md contains the staged runtime checklist and log locations. Do not silently enable world writes globally or hot-unload the hook DLL.
