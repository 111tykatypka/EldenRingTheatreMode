# Phase 4C / Phase 5 transform foundation — 2026-10-05

Implemented; new smoothness/input/UI behavior requires user runtime validation.

## Exact 301-sample cause

`monitor.cpp` selected combo index 0 (`TEST: first 5 seconds`). `playback_limit()` returned 5,000,000,000 ns; `Controller::tick/finish` sent FINISH at that endpoint. The new real recording has 301 samples at or before 5 s, 881 in total. This was a deliberate test endpoint, not an ERPLAY/sample storage limit. Default is now FULL (index 2). Explicit 5/10-second diagnostic modes remain available. Mock tests cross sample 301 and 800 and finish at sample 1000 / 20 s.

## Rates and clock ownership

Old real logs: approximately 60 callback applications per one-second log window; 35–41 command-sequence increments per window (includes control exchanges). This is evidence of mismatched update cadence, not a precise send-only benchmark. New host logs measure generated-state Hz and completed replay-exchange Hz; DLL logs measure received requests, callback rate and write rate using elapsed time. Actual new in-game rates are not yet measured.

The sole C++ ReplayPlayer/clock now advances on a dedicated waitable-timer worker (120 Hz requested). UI timer only observes copied State/Summary. Short locks serialize UI commands and Player access; rendering does not retain the lock. Missed worker deadlines are skipped, not replayed in a burst. IPC remains a latest-value mailbox, game-thread access stays ChrIns_PostPhysics. Producer timestamps now use the shared precise Windows interrupt-time clock instead of the quantized tick counter. Microsoft documents 100-ns units and Mincore.lib: [QueryInterruptTimePrecise](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-queryinterrupttimeprecise). A configured timer is not a measured 120 Hz claim.

DLL retains only previous/next host-authoritative transforms and interpolates with **8.333 ms bounded delay**, linear position and normalized shortest-path quaternion SLERP. No file parser, independent gameplay clock or unbounded extrapolation. Missing future data holds the endpoint; freshness lease still cancels after 250 ms. Pause applies the held current target directly. FINISH applies the exact endpoint once and disables writes. STOP invalidates generations.

## Input / physics

Exact pinned `3c8c1d7633a99309fb004c9f894ea10b7967d0e0` documents local `ChrDebugFlags.disabled_movement` and `disabled_secondary_actions`. New game-side lease saves those two original bits and sets only those during active replay. Restoration runs on the game callback after Stop, Finish, Error, disconnect or unload; it reacquires PlayerIns and compares FieldInsHandle, never a saved pointer. Lost owners are discarded. Physical keyboard/mouse/controller devices and camera input are not globally disabled. Actual suppression and restoration remain experimental until user testing.

Old logs also show live movement fighting a held target by ~0.0655 units, with changing orientation. New diagnostics compare actual-before against the previous **applied** interpolated target. No proxy sync flags, gravity, velocity, model matrix or root-motion fields are changed. Field existence is not proof that additional writes are needed.

## UI and lifecycle

UNLOAD stops native replay before destroying Player and clears timeline/bookmarks/preview; the replay file is preserved. Switching stops old playback before replacement. Opening applies the selected speed to the new Player. Bookmarks are rebuilt only when file/content changes, preserve selection by timestamp, and seek disables native writes first. X/Z preview is compact (100–190 DPI-scaled pixels high), paints through an offscreen bitmap/BitBlt, and invalidates only when displayed replay state changes. Repeated text writes are skipped. UI manual flicker/resize tests remain unverified. No world/NPC/camera subsystem is added.

Checkpoint tests: CTest 5/5, Rust 9/9; new quaternion tests cover sign-equivalent q/-q and 0/90/180/270/360-degree targets. Continue into action-track research/capture before the consolidated Phase5 manual test.
