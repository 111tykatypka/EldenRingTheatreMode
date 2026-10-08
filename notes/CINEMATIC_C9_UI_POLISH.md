# C9a — scrollable camera/settings UI and smooth wheel FOV

Status: COMPILE_VERIFIED. CPU UI construction/geometry and math tests PASS.
Game visuals, physical wheel delivery and keybinding interaction remain runtime UNVERIFIED.

## Layout and controls

The side panel previously disabled scrolling and drew every tool directly into the
same parent as the log. Long input labels competed with fields in a narrow panel.
Each tool now has its own vertically scrollable/clipped child with its own scroll
position. The event log remains a separate bounded footer. Log copy feedback uses
its actual height when sizing the log. Property values and numeric control labels
are stacked, and checkboxes wrap their descriptions. Camera action pairs are stacked.

Continuous camera properties now use full-width sliders: FOV, movement speed,
mouse sensitivity (logarithmic), movement smoothing and shake parameters. Dolly
Ease In/Out use 0..1 sliders. UI scale/sound volume retain their existing sliders.
Exact values can be entered with Ctrl+click. Bone index, position, quaternion and
keyframe timestamps retain numeric editors with labels above; these are not reduced
to imprecise slider-only editing. Language and discrete camera modes remain choices.

## Keybindings

Searchable three-column action/key/reset table replaces large combined-label buttons.
Action names wrap, key buttons stay bounded, tooltips show full names/VK values.
Capture identifies the selected action and offers Cancel/Escape. Reset restores one
action's default through the existing conflict-checking/persistence implementation.
Existing automatic save, host/DLL reload and emergency-stop handling are preserved.
Capture is cancelled when Settings/overlay closes; no binding changes are made by tests.

## Smooth FOV

Hidden overlay + active Free Camera consumes WM_MOUSEWHEEL from the game window.
One canonical Win32 wheel source avoids doubling DirectInput state/buffered samples.
The callback queues only atomic wheel data; camera state changes on the existing
camera callback. Dolly/Player/Bone and visible overlay do not use wheel FOV.
Shown Camera/Settings use the wheel for normal child scrolling.

Wheel-up narrows FOV, down widens it: 3 degrees/notch, Shift 0.75 degrees,
Ctrl 0.3 degrees (Ctrl takes precedence). FOV is bounded to 1..178 degrees;
120 ms exponential convergence uses real dt, independently of world timescale.
Existing FOV keys adjust the same target. Reset key remains an exact 60 degrees.
The FOV slider sets pose and target immediately. Release, mode change and ownership
initialization clear pending wheel input. Runtime wheel-message delivery still needs
in-game confirmation; no second wheel source is guessed/added.

## Tests/build

14 CTest suites and 49 Rust tests passed; one optional real-recording test ignored.
CPU overlay construction still covers EN/RU, all tools, visibility states and resolutions.
Additional short/narrow panel checks cover Camera/Settings at 1280x720, 1920x1080,
3840x2160 and scale .75/1/1.5: nonzero scroll range, mouse scrolling enabled,
content does not overlap log, log fits parent. These do not prove all game visuals.
FOV tests cover direction, precision, limits, invalid input and equivalent 60/120 Hz
convergence. Existing C8 camera-roll and C7 native-copy regressions still pass.

Final matched package: outputs/Cinematic-C9a-ui-polish. Initial C9 build is superseded;
C8/C7/C6 remain preserved. Native game profile and replay-origin safeguards unchanged.

## Manual use

Close the game normally and close the prior host; launch the C9a EXE and launch game
through its existing YAFSML button (uses the DLL beside the EXE). If launching YAFSML
manually, select C9a/TheaterMode.dll. Do not hot-unload or replace loaded binaries.

Open F4: scroll Camera/Settings to bottom; resize panel and test long labels. Search
and rebind an unused camera action, cancel another, then reset the first; confirm a
conflict displays an error. Select/arm Free Camera, hide F4 and test wheel up/down,
Shift/Ctrl fine adjustment and responsiveness at world slow motion. Reopen F4 and
confirm wheel scrolls the panel without changing FOV. Native camera/FOV restore and
emergency stop remain required checks. No game-success claim until user validation.
