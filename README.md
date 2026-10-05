# EldenRingTheatreMode

## Phase 4B checkpoint

Start the existing `EldenRingTheaterMode.exe` and click **START ELDEN RING** to launch the installed YAFSML with the `TheaterMode.dll` beside this EXE. **YAFSML...** selects the launcher if its location changes. The original loader configuration and game files are preserved; a separate launch configuration is generated in application data.

The user verified the launcher, YAFSML loading, IPC and Player FOUND in Phase 4A. Phase 4B adds guarded player **position/orientation only** playback using the existing C++ replay clock and interpolation. Animation replay is not implemented.

Build matching Release x64 EXE/DLL with `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build-Phase4B.ps1`. The default output is `C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Phase4B`; the verified Phase4A package is preserved.

See [manual test](notes/PHASE4B_MANUAL_TEST.md) and [status/evidence](notes/PHASE4B_STATUS.md). First make a new short real recording in a known location, then test only its first 5 seconds, Pause/Resume and F6/Stop. This playback checkpoint is **IMPLEMENTED — RUNTIME VALIDATION REQUIRED**. Compilation and mock IPC tests do not prove in-game movement. Historical Phase4A reports remain in `notes/`.


## Phase5 checkpoint

Branch `phase4-in-game-replay-prototype`: full-duration transform playback, independent120Hz requested host worker, bounded game-callback interpolation, scoped local input lock, ERPLAYv3 sparse raw animation/action observations, and opt-in next-frame recorded animation-ID request. These new native behaviors are **IMPLEMENTED — RUNTIME VALIDATION REQUIRED**, not verified locomotion/world replay.

Build/test/package: `powershell -ExecutionPolicy Bypass -File scripts\Build-Phase5.ps1`.
Default output: `C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Phase5`.
Use the matching EXE/DLL; close the old game/host first. Start host, then START ELDEN RING with existing YAFSML. Follow [the consolidated manual test](notes/PHASE5_MANUAL_TEST.md); see [status/limitations](notes/PHASE5_STATUS.md), [animation research](notes/PHASE5_PLAYER_ANIMATION_RESEARCH.md), [root motion](notes/PHASE5_ROOT_MOTION.md), and [v2/v3 format](ERPLAY_FORMAT.md). New semantic action mappings, animation phase/freeze/rate and visible smoothness need the new real recording; old v2 files remain transform-only.
