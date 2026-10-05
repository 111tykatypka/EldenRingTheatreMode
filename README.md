# EldenRingTheatreMode

## Phase 4B checkpoint

Start the existing `EldenRingTheaterMode.exe` and click **START ELDEN RING** to launch the installed YAFSML with the `TheaterMode.dll` beside this EXE. **YAFSML...** selects the launcher if its location changes. The original loader configuration and game files are preserved; a separate launch configuration is generated in application data.

The user verified the launcher, YAFSML loading, IPC and Player FOUND in Phase 4A. Phase 4B adds guarded player **position/orientation only** playback using the existing C++ replay clock and interpolation. Animation replay is not implemented.

Build matching Release x64 EXE/DLL with `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build-Phase4B.ps1`. The default output is `C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\Phase4B`; the verified Phase4A package is preserved.

See [manual test](notes/PHASE4B_MANUAL_TEST.md) and [status/evidence](notes/PHASE4B_STATUS.md). First make a new short real recording in a known location, then test only its first 5 seconds, Pause/Resume and F6/Stop. This playback checkpoint is **IMPLEMENTED — RUNTIME VALIDATION REQUIRED**. Compilation and mock IPC tests do not prove in-game movement. Historical Phase4A reports remain in `notes/`.
