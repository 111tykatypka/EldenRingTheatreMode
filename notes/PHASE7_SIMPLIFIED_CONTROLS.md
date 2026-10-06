# Phase7 Hotfix5 â€” simplified controls and timeline wheel

## Changes
- Host Timeline/Inspector and in-game overlay use Play and Stop. Pause/Resume buttons removed.
- Recorder exposes Start/F5 and Stop/F6. F7/F8 are not registered and ignored by global dispatch.
- Space starts playback when stopped; it no longer toggles pause.
- Internal paused state and protocol values retained for seek/step and compatibility.
- Timeline canvas owns MouseWheelY/X: up zooms in, down zooms out around cursor without Shift. Previously default child scrolling competed with zoom; Shift avoided vertical scrolling. Native scrollbars remain available for tall actor lists.

## Evidence and scope
- User confirmed moving player replay on flat indoor floor: movement repeated, no falls or jerks; model slid without animations. Animation playback remains unavailable.
- Automated regression injects real ImGui wheel events into an overflowing child: ten alternating zoom in/out pairs, zero child/parent scroll, no Shift. Global hotkey regression checks F5 despite captured keyboard and rejects removed F7/F8.
- Release x64 EXE/DLL build passed. CTest 13/13, Rust 25/25, Python 3/3 passed; separate real-device DX12 smoke passed. Initial restricted-sandbox IPC tests failed; rerun with local IPC permissions passed.
- New control layout and wheel behavior still require user validation in the real application/game.

## Manual check
Close host and Elden Ring before changing DLL. Launch Hotfix5/EldenRingTheaterMode.exe and use its existing YAFSML launcher. Load the same save/location. Open replay; hover timeline and turn wheel both directions without Shift. Verify Play/Stop, recording F5/F6 and in-game overlay. No Pause/Resume controls should appear.

## Hotfix6 — Shift vertical scrolling
- Shift+wheel down/up scrolls timeline actor rows vertically. No-modifier wheel still zooms time around cursor.
- Explicit handling avoids ImGui's default Shift axis swap. Scrolling is font-relative (DPI-scaled), clamped to child scroll limits; parent stays fixed.
- Added event-based regression for Shift down/up, top/bottom clamping, unchanged time scale, and zoom after releasing Shift.
- Release x64 build passed; CTest 13/13, Rust 25/25, Python 3/3 and separate real-device DX12 smoke passed. Real application validation required.
- Launch package Phase7_Runtime_UI_Hotfix6 after closing the previous host; DLL behavior unchanged from Hotfix5.
