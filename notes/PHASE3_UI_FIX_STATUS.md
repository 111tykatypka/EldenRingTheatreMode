# Phase 3 UI Stabilization Status

Date: 2026-10-05

## Scope

This update stabilizes the offline `.erplay` data viewer only. It does not add Elden Ring world reconstruction, actor rendering, replay simulation, or camera features.

## Findings and fixes

- The source contained one UI timer (`SetTimer` at 33 ms) and one `WM_TIMER` handler; there was no duplicate-timer creation path.
- The handler previously called `advance()` and refreshed the full replay panel on every tick, even while paused/stopped or already at the end. `update_replay_panel()` reset and repopulated the bookmark list and invalidated the trajectory preview each time. This produced a steady 30 Hz repaint workload and unnecessary end-of-replay refreshes.
- The preview paint handler previously erased the visible surface and created/deleted GDI pens and brushes on every paint. It now draws through a cached compatible bitmap/DC and cached GDI objects, then blits the completed frame. The preview has a compact DPI-scaled height (140–190 logical px) and remains secondary to replay controls/state.
- Paused/stopped timer ticks now do not advance replay state, rewrite replay labels, or invalidate the preview. The preview is invalidated on playback updates and explicit user changes (seek, step, trail setting). Bookmark rows are refreshed only after open/add/delete.
- Added in-preview counters for UI timer frequency, paints/second, invalidations/second, seek count, and end-boundary hits. There is one 33 ms timer source. Boundary detection counts the transition to the end once; the `Player` switches to paused at the duration and later timer ticks cannot cause another boundary transition.
- Playback speeds are exposed with a labeled native dropdown containing 0.1x, 0.25x, 0.5x, 1.0x, 2.0x, and 4.0x. The chosen value is retained when opening a replay and is passed to `Player::set_speed`; selection updates a currently open player as well.

## Automated validation

Build: Release configuration succeeded with the existing Visual Studio 18 x64 CMake build.

CTest: 2/2 passed (`erplay-tests`, `replay-player-tests`).

Real fixture:

`C:\Users\user\AppData\Local\EldenRingTheaterMode\replays\replay_2026-10-05_022417.erplay`

- 4,093 samples
- 68,500,000,000 ns (68.5 s)
- 7 chunks
- 59.7372 Hz reported rate
- Seek checks passed at 0%, 50%, 90%, 99%, and 100%.
- 1,000 rapid seeks around 90%, 99%, and 100% passed.
- Seek to 30 s and 55 s and forward sample stepping passed.
- Speed clock checks for all six values passed in order; 100 ms wall-clock increments produce 10, 25, 50, 100, 200, and 400 ms of replay progress (the last clamps at this synthetic replay's 400 ms duration).

## Manual checks not completed

Manual visual interaction (open/play/pause/resume, dropdown popup, and repeated UI dragging) was not completed. The computer-use tool reported that it had been stopped by the physical Escape key, and no further UI automation was attempted. Therefore this report does not claim that visual flicker is manually verified absent or that the combo popup was visually inspected.

The old output executable was still running (PID 19424) and held the standard output path open. The new build was staged as:

`C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\EldenRingTheaterMode-Phase3Fix.exe`

After the old viewer exits, replace the standard `EldenRingTheaterMode.exe` with the built Release binary and complete visual validation. The project build output itself is:

`C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingTheaterMode\build\Release\EldenRingTheaterMode.exe`

## Remaining validation

- Open and close the real fixture in the native UI.
- Visually inspect stable projection rendering while paused at 0%, 50%, 90%, 99%, and 100%, including rapid slider movement.
- Confirm visible dropdown options and compare playback wall-clock progression in-app for all six speeds.
- Manually exercise play, pause, resume, stop, restart, single stepping, bookmarks, and close/reopen.

No Phase 4 work was started.
