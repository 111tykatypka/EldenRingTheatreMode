# Smooth player poses + native world timescale — 2026-10-07

## Diagnosed

User verified continuous slider, but player looked like 5–10 FPS and world speed was unchanged. Actual game log contains TIMESCALE_BINDING=REJECTED mode=READ_ONLY. Two independent problems prevented world scaling: disabled-by-default writes and incorrect root RVA.

Direct read-only decoding of the actual executable: RVA DEB30F has displacement 037A2842; DEBE2F has 037A1D22. RIP-relative addition includes the site RVA plus seven bytes, yielding **0458DB58**, not 0358DB58. Existing opcode/field checks were correct. This fixes our arithmetic error; no identity/signature check removed. Research reports were annotated/corrected. `tools/check_timing_sites.py executable` reproduces the calculation.

## Implemented

- Native world timescale enabled during fresh active skeletal replay by default; no environment setup needed. THEATER_WORLD_TIMESCALE=0 is an optional diagnostic disable. Writes remain on the existing game callback, behind exact profile, both instruction checks, memory/finite checks and exclusive scalar ownership. Pause/stop/unload/stale pipe/player loss restore prior value when still owned. No global timescale writes when inactive.
- An on-screen WORLD TIMESCALE message reports actual writes and restoration. Game log records binding, live scalar, requested rate, writes and rejection details. These indicate code actions, not verified behavior of every engine clock domain.
- Both saved local/model 48-byte bone-transform arrays now interpolate translation/scale XYZ and shortest-path normalized quaternion SLERP. Padding preserved from left sample; endpoints byte-exact. PrePhysicsSafe uses reusable fixed arrays (no new per-frame heap allocation). Draw accuracy compares against evaluated output instead of the old floor sample.
- Recorded intervals over 250 ms use the left sample until next boundary rather than smearing across missing data. This is a conservative recording-gap heuristic, not complete warp detection. Normal short teleport cuts still require future explicit discontinuity metadata.
- Stale host snapshots no longer keep skeletal ownership enabled indefinitely; normal release path runs.

Local/model interpolation is independent, without skeleton hierarchy reconstruction: intermediate model/local consistency and artifacts need visual validation. No engine model-matrix writes or changed pose pointer paths. Native scalar range remains .001–10 in UI; stable game behavior at extremes remains UNKNOWN. This is world *speed control*, not full-world replay reconstruction.

## Evidence/tests

CHAGGPT.erplay.bones: 1,038 real frames. Both arrays finite semantic components; local quaternion norm range .99999917–1.00000018, model .99999748–1.00000046. Transform order (translation, XYZW quaternion, scale) corroborated by captured data and public [PyNifly transform format documentation](https://github.com/BadDogSkyrim/PyNifly/blob/main/docs/hkx_skeleton_format_fo4.md); this different-game reference is corroboration, not proof of Elden Ring hierarchy semantics.

Pure Rust tests: 4/4 passed. Real-file evaluation: 10,370 array evaluations, 150 bones each, all checked quaternion norms and exact endpoints passed; 153 ms total in optimized offline checker, not measured game overhead. A first standalone compile omitted Rust edition2021 and failed; corrected invocation passed. Release x64 validation is in output BUILD_MANIFEST.txt. RUNTIME_VERIFIED/VISUALLY_VERIFIED for new interpolation/world writes: pending.

## Exact next test

Close Elden Ring and old host so old DLL is unloaded. Open outputs/SmoothPose-WorldTimescale/EldenRingTheaterMode.exe. Ensure existing YAFSML launcher loads **this folder's TheaterMode.dll**, then launch offline game normally. Do not run IGCS/another time-scaling tool concurrently. Load the same safe area as CHAGGPT recording; F4, open CHAGGPT.erplay and Play.

First test 1x, then .5x, .25x, .1x. Watch player limbs/root plus ambient/NPC/world animation. WORLD TIMESCALE should show requested rate, and log should show TIMESCALE_BINDING=STATIC_VALIDATED and TIMESCALE_APPLY. Stop and Unload should show restored1x and normal control. No need to rerecord. Test lower rates only after this succeeds; do not begin with10x.

Send %TEMP%/TheaterModeGame.log, %TEMP%/TheaterModeRender.log and %LOCALAPPDATA%/EldenRingTheaterMode/logs/TheaterModeRecorder.log if it fails. Specify rate and whether world, limbs or root are wrong. Abrupt DLL unload, stopped tasks or game hang cannot guarantee callback-based restoration.
