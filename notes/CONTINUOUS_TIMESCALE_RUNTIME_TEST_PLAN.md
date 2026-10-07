# Continuous timescale — next manual test

Status: COMPILE_VERIFIED, focused offline tests passed. Independent runtime/visual validation REQUIRED.

1. Close the old Theater host and Elden Ring before replacing/loading the DLL. Originals and golden builds remain untouched.
2. The Release pair is in the independent project's outputs/ContinuousTimescale folder. Run its EldenRingTheaterMode.exe. Confirm the launcher/YAFSML module selection points to this folder's matching TheaterMode.dll; do not mix protocol versions. Use the existing offline YAFSML workflow. No new injector.
3. Launch Elden Ring from the host. Load a safe flat location and a short recording made there, with its matching .bones sidecar. Keep normal world writes OFF (default).
4. Press F4 for Theater. Test Play, Space toggle, Stop and Unload first. Check original normal controls return after unload. Seek only within this safe recording.
5. Test Timescale by normal drag; Shift drag without a jump; switching Shift during drag; Ctrl precision; hover wheel with/without modifiers. Right/middle click should reset only rate to 1 without restarting playback.
6. Test exact input 0.00105x, 0.005, 0.1, 0.5, 1, 2; enter 0, -1, NaN and junk to verify rejection. Out-of-range positive input clamps. Check paused timestamp stays unchanged while editing rate. Check slider stability while holding the mouse still.
7. Compare root movement and orientation at 1/.5/.1. Raw bone poses are still sampled; very low rates can visibly step. This is not full interpolated animation.
8. Stop and unload; check normal control, gravity and world behavior. Send logs and a short visual description before testing experimental world writes.

Optional controlled native experiment, after default tests: close game/host, set THEATER_WORLD_TIMESCALE=1 in the environment that launches the new host, then launch through YAFSML. Test only 1/.5/.25/.1 initially and Stop/Unload restoration. The reference standard setting clamps .001–3; our wider UI range is not proven safe in game. Do not jump directly to 10. This experiment does not implement whole-world replay or pause.

Logs: %TEMP%/TheaterModeGame.log and %LOCALAPPDATA%/EldenRingTheaterMode/logs/TheaterModeRecorder.log. Include BUILD_MANIFEST.txt, replay filename and .bones presence, rate/state at failure. No save/game file mutation is required.

Known caveats: callbacks are required for restoration; abrupt unload/hang cannot guarantee restoration. Warp cuts and skeleton interpolation remain unresolved. No native ghost/custom lights/particles/camera/ReShade functionality was added by this checkpoint.
