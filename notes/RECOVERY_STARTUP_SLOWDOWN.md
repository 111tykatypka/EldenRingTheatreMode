# Recovery build — startup slowdown quarantine

2026-10-07. User reports low FPS/slowness from main menu through world with SmoothPose-WorldTimescale. This is a regression report, not successful runtime validation.

Actual latest log: corrected binding accepted, live scalar 1.0, active=false, no TIMESCALE_APPLY. Therefore a stuck replay scalar is NOT demonstrated. Exact startup cause UNKNOWN. No claim that interpolation or timing reads caused it. Do not repeat world-writing tests yet.

Recovery changes: native timescale update returns before all reads, mutexes, resolution and writes, irrespective of environment. New bone interpolation disabled by default; recorded poses copied as before. Prior continuous slider/root interpolation retained. Optional THEATER_POSE_INTERPOLATION=1 exists for later supervised isolation, but do not set it for recovery. Draw task cadence logged every five seconds; it is not GPU FPS.

The old DLL cannot be unloaded safely in place. Exit game completely and old host. Launch outputs/Recovery-NoWorldWrites/EldenRingTheaterMode.exe and ensure YAFSML uses its matching TheaterMode.dll. Restart offline game. Stay in main menu for at least15 seconds, then load world and wait15 seconds WITHOUT opening a replay. Check normal speed/FPS first. Do not load IGCS/other timing tools. No rerecording required.

If normal, open previous recording and test1x before slowdown. Continuous timescale changes replay progression only in this recovery build; world scaling deliberately quarantined.

If still slow, close/restart using the previously preserved ContinuousTimescale build with its own matching DLL, without playback; compare same menu/location. This distinguishes a new-code regression from inherited renderer/launcher/environment. Do not alter saves/game files.

Send %TEMP%/TheaterModeGame.log, %TEMP%/TheaterModeRender.log, %LOCALAPPDATA%/EldenRingTheaterMode/logs/TheaterModeRecorder.log and observed menu/world FPS. Expected log TIMESCALE_QUARANTINED, plus DRAW_CALLBACK_CADENCE. Runtime recovery NOT VERIFIED until user tests.
