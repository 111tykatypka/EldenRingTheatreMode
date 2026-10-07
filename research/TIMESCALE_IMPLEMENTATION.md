> Correction 2026-10-07: the prior 0x358DB58 calculation was wrong. Direct PE instruction decoding resolves BOTH sites to RVA 0x458DB58. New SmoothPose-WorldTimescale build enables guarded writes by default; THEATER_WORLD_TIMESCALE=0 disables them. Earlier default/read-only and raw-pose descriptions below describe the previous checkpoint.

# Continuous Theater timescale — implementation checkpoint

2026-10-07. Active source: independent checkout, `codex/independent-development`. Original Claude/Step2a/Phase5 builds are unchanged. Earlier independent Task1–4 outputs are also preserved.

## Implemented (COMPILE_VERIFIED, offline tests passed)

- Removed `shared/PlaybackSpeeds.h`, all active combo/preset indexes/cycling, and the alternate modern UI's preset selector. The retained historical monitor now accepts continuous numeric input rather than presets; it is not the active entry point and was not rebuilt as an application.
- `shared/TheaterTimescale.h`: finite range **0.001–10.000**, default 1; logarithmic mapping `u=log(v/min)/log(max/min)` and its inverse; relative adjustment; numeric parser; adaptive display; exact binary64 IPC encoding.
- Active F4 toolbar: logarithmic slider with native-speed tick at 1.0, exact input field. Normal initial click positions the knob absolutely. Shift+drag changes relative log position at 2% sensitivity; Ctrl at 0.2%. Modifier changes apply immediately during drag. Hover wheel adjusts without clicking, with the same precision modifiers. Right/middle click resets only the value to 1.0.
- Numeric input accepts optional x suffix/whitespace, rejects nonfinite, zero, negative and trailing junk; positive values outside range are clamped. Six decimals below .01, four below .1, three otherwise. Internal double is not rounded to that display precision.
- **Single playback authority:** `ReplayPlayer::State::timescale`, changed through `set_timescale` under the existing host replay mutex. Snapshots and the DLL bone link mirror that value; pending UI selection is only an acknowledgement display, not another simulation clock. New replays start at 1.0; no separate speed persistence was introduced.
- Editor IPC v9 encodes the double's IEEE-754 bits in Request.value, with finite/range validation on both endpoints. No centi-speed quantization. Use matching new EXE/DLL; old protocol versions are rejected.
- Clock change advances the old rate to the command instant, then installs the new rate. Pause state and current position remain intact. Stop/restart/seek reset the fractional remainder. Continuous clock updates retain sub-nanosecond fractional time instead of throwing it away each update.
- Root physics position lerp and shortest-path quaternion SLERP in `adapter/src/replay_interpolation.rs`; PostPhysics and PrePhysicsSafe now use the same evaluated root transform. Endpoint transforms remain byte-equivalent. Raw skeleton arrays still use the bracket's recorded pose, not invented hkQsTransform interpolation.

## Native world timing (experimental; RUNTIME_VERIFIED = no)

The separate Rust `timescale` controller continues observation-first. Default launches log timing values and never write the scalar. Explicit `THEATER_WORLD_TIMESCALE=1` opts into the controlled experiment. No experiment was run unattended.

Same mechanism as traced IGCS: exact-profile/opcode checks at game RVAs 0xDEB30F/0xDEBE2F, agreeing on pointer slot RVA 0x458DB58; scalar at manager+0x2CC. See CAMERATOOLS_REFERENCE_FINDINGS.md for the complete static chain and limits.

Callback conditions: loaded skeletal replay, present/owned player, Playing, fresh host snapshot, no active recording. Save prior scalar; apply requested float; restore on inactive/pause/stop/unload/stale IPC/player loss. Manager replacement/external scalar changes release ownership without touching an old object. Existing opcode, executable SHA/version and writable-memory guards remain. Native scalar is float32; the exact host value is double and can differ by normal float rounding.

Pause freezes ReplayTime and holds the replay body through existing overlay ownership. It currently restores normal **live-world** speed; native whole-world pause is not implemented. A future world-reconstruction mode must freeze all replay tracks, not equate minimum nonzero timescale with Pause. Restoration is callback-driven and cannot be guaranteed if tasks stop, the DLL is abruptly unloaded, or the game hangs.

**UI RANGE:** .001–10. **VERIFIED STABLE RUNTIME RANGE in this independent implementation:** UNKNOWN. The reference's standard scalar-setting path clamps .001–3.0; 10.0 is our requested UI range, not a tested native guarantee. First runtime tests must use moderate rates, never assume safe acceleration.

## Low-speed result and remaining limitation

Root interpolation and clock precision are implemented/tested offline. Full-pose smoothness is not claimed. At a 60 Hz recorded pose rate, .001x can hold one skeleton sample for about 16.7 seconds of real time. Global timescale alone will not interpolate those saved bone arrays. Prove hkQsTransform component layout, bone hierarchy, local/model consistency and discontinuity handling before adding pose interpolation. Bone endpoints must remain exact; large warps/map changes require explicit cuts, not smoothed trajectories.

## Validation

Release x64 checkpoint A built continuous controls; checkpoint B includes root interpolation. Initial B compilation caught a Rust test literal `.5`; fixed to `0.5` and rebuild succeeded. An initial clock assertion at exactly ten .1-ns contributions was too strict for binary floating-point; eleven contributions test accumulation without assuming decimal exactness.

Final focused CTest: replay-player-tests, ingame-editor-tests, render-backend-tests **3/3 passed**. Pure Rust root interpolation tests **2/2 passed**. Coverage: continuous range/math/parser, exact low-value pipe transmission, state-preserving rate changes, fractional clock accumulation, quaternion normalization/shortest path, invalid inputs and endpoints. No real Elden Ring interaction, GUI mouse usability, native slowdown, reference visual comparison or cross-session acceptance was performed.

## Next manual test

Use `outputs/ContinuousTimescale` matching pair. Close old host and game; launch new EXE, then Launch Elden Ring through the unchanged YAFSML workflow. Load a safe flat recording area, F4, open a short replay with `.bones`. Verify pause/resume/seek/unload and all new timescale gestures. First leave world writes off and provide timing observations.

Then, only after candidate review, enable the environment opt-in and verify 1/.5/.25/.1 world coherence and stop/unload restoration. Extend to .05/.01/.005/.001 and higher rates only after basic stability. Logs: `%TEMP%/TheaterModeGame.log` and `%LOCALAPPDATA%/EldenRingTheaterMode/logs/TheaterModeRecorder.log`. Comparison must include NPC animation, physics, effects, environment and editor responsiveness; smoothness is not proven by the timeline alone.

Final checkpoint Release x64 staged as ContinuousTimescale. Final CTest: 3/3 passed in 0.09 seconds with native IPC permissions. A restricted-shell pipe run failed without diagnostics; unchanged binaries passed when rerun with IPC permissions. Environment dependence is likely; no code was changed to hide the failure. Pure Rust: 2/2 passed. Stationary slider holds no longer repeatedly emit unchanged commands.
