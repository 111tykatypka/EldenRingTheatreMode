# C10c — camera selector, authoring and viewport transforms

Status: IMPLEMENTED / COMPILE_VERIFIED. 14 CTest suites and 49 Rust tests PASS;
one optional real-recording inspection ignored. New in-game controls/projection/gizmos
are NOT yet runtime or visually verified. Existing game/profile/memory hooks unchanged.

## Slider defaults

Middle/right click on Camera float/double sliders restores the property's default
and sends the same update as a normal edit. Tooltips identify the default. Defaults:
FOV 60 degrees, movement 3 units/s, mouse sensitivity .0025 rad/count, movement and
rotation smoothing 0 seconds, position/rotation shake 0, frequency 1 Hz, ease values 0.
Settings scale resets to 1, sound volume to 60 percent and saves immediately. Existing
master timescale reset remains exactly 1 without seeking or transport changes.
Numeric position/quaternion/time editors are not sliders and are not reset implicitly.

## Rotation smoothing

New slider directly after movement smoothing. Real-time exponential filtering acts
on pending yaw/pitch/roll input; applied yaw remains world-Y and pitch local-X.
It does NOT SLERP whole mouse-look poses (which could introduce transient banking).
Zero is direct, positive seconds give an input tail. Default/native/preview changes
reset pending input. Quaternion SLERP remains in the Dolly track evaluator.
Regression tests cover upright horizon during mixed smoothed mouse input and delivery.

## Three-mode top-center selector

Default / Free / Dolly have independently drawn vector icons and labels, selected-state
highlight and Active/Not armed feedback. F3's configurable Cycle Camera action cycles
exactly those three and no longer opens the main overlay. Clicking icons works while
F4 is shown. While ordinary F4-hidden, a noninteractive selector badge remains visible
and the same key cycles it without requiring a cursor. Shift+F4 clean preview retains
its intentional no-UI behavior. Bone mode remains accessible through an advanced
Camera-panel button but is not in the three-mode cycle. Explicit mode selection releases
cut-track ownership so a session cut cannot silently override the selected mode.

Free activation seeds from the freshly observed native camera on the existing callback,
including position/orientation/FOV. Switching between active Free/Dolly preserves the
current camera pose. Default releases interception; the game owns the camera again.
The hidden selector requests a lightweight View without copying path arrays each frame.

## Dolly authoring and ReplayTime

Empty Dolly is now allowed: it behaves like Free, including real-time input, smoothing
and wheel FOV. With a replay loaded and Dolly actually writing, K captures its current
position/quaternion/FOV at the host's master ReplayTime (updates an existing key at the
same timestamp). No separate camera clock or replay parser was introduced.

Authoring moves freely until path preview is enabled. Play automatically enables preview
when entering Playing in Dolly with at least two keys for this replay. Explicit Preview
in Camera can hold/evaluate the path while paused. Turn Preview off to author more keys.
Paused timeline seeks evaluate a current path once so the pose follows the selected time,
then authoring can continue from that pose. Existing interpolation choices (including
centripetal Catmull-Rom and SLERP), camera sidecar Save/Load and safety guards remain.

Timeline diamonds are now selectable; clicking selects the camera key, seeks to its
ReplayTime and opens Camera properties. Selection suppresses competing scrub dragging.
Updating/capturing keys invalidates stale drafts. Markers from a different loaded replay
are excluded from the timeline/viewport rather than silently interpreted in this replay.

## Viewport editor

Shown F4 + Camera tab + Show Dolly cameras/transform handles draws camera frustum
wireframes, labels and selectable origin markers behind other UI panels. This is an
editor overlay projected into the game picture, not a spawned native camera object or
native world mesh. It is not depth-occluded by scene geometry.

Select a camera in the viewport or on the timeline. Move handles edit world XYZ along
red/green/blue axes. Rotate handles use world-axis ring hit tests and ray/plane angles,
then compose normalized quaternions. Edge-on/degenerate rings cannot be dragged; change
the viewing angle or use numeric fields. Axis translation uses the initial projected
pixels-per-world-unit (an approximation for deep perspective drags).

Drag edits are throttled to 30 Hz, committed on release; Escape restores the drag-start
key. Existing other keys/path are retained. Numeric fields include XYZ and UI
pitch/yaw/roll; raw quaternion fields are under Advanced. Euler angles are only an
editing representation, never interpolated. Numeric drafts still require Apply key edit.
The viewport requires a valid observed camera and keys belonging to the loaded replay.
Move the viewer away from a captured camera to see/select its icon.

Projection uses the adapter's right/up/+Z-forward basis, assuming vertical FOV and the
full drawable-window aspect. Correct alignment in the actual game (especially ultrawide
letterboxing) still needs visual validation. No unverified engine projection pointers
or offsets were introduced. Extremely large/behind-camera projections are skipped.

## Clear and keybindings

L uses the existing confirmation. Cancel changes nothing. Confirm clears the active
keys/cuts, removes the viewport camera representations, disables path preview/writes
and selects Default. It does not destroy a native game object, alter gameplay recordings
or automatically delete the previously saved .ercam file. Save/Load remain explicit.
L may show F4 to display its warning; F3 and K do not force F4 open.

All existing actions use the common keybinding table: Settings > search the action >
click its key > press the desired keyboard key. Conflict checks, per-action Reset,
Cancel/Escape and persistence are retained. Selector capture/clear captions reflect
current bindings. No new permanently hardcoded shortcut mechanism was added.

## Build and verification

Final package: outputs/Cinematic-C10c-dolly-editor (matched host/DLL). Intermediate
C10/C10a/C10b packages are superseded. Prior C9a/C8/C7/stable builds are preserved.
No running process was closed, no game file changed, and no DLL hot-unloaded.

Additional tests: three-mode cycle does not show main UI; mathematical viewport
projection, behind-camera/invalid input rejection, ring ray/plane angles and degenerate
cases, world-axis quaternion rotations, UI angle round-trip, angular smoothing and
upright horizon. CPU EN/RU/all-tools/multi-resolution and narrow scroll clipping tests
still pass. Actual widget reset clicks and native scene alignment are manual checks.

## Next manual test

1. Close Elden Ring normally and the old host. Launch C10c/EldenRingTheaterMode.exe
   first, then its existing YAFSML game launcher. If launching the loader manually,
   select C10c/TheaterMode.dll. Restart is required; do not hot-unload an old DLL.
2. Load your offline playable world. F4 should show the top-center three icons.
   Hide F4 and cycle F3: Default -> Free -> Dolly -> Default, with only the mode badge.
   Verify Free begins from the native camera and Default returns normal game camera.
3. F4 Camera: try rotation smoothing ~.12 s; hide F4 and move/look. Reopen and reset
   smoothing/move speed/FOV using middle/right clicks; check scale and volume resets.
4. Load a replay, stop/pause at a chosen timestamp, select Dolly. Hide F4, position
   the camera and press K. Show F4, seek to another timestamp, hide F4, move/rotate,
   press K again. Repeat for a third key. Diamonds should use those timestamps.
5. Select Free and move away from the captured camera positions to inspect them.
   F4 Camera: click a timeline diamond or viewport icon. Try Move and Rotate handles
   on the selected camera; confirm other keys remain. Escape should cancel a drag.
   Test numeric XYZ/Pitch/Yaw/Roll with Apply key edit and Save camera path.
6. Select Dolly and press Play (2+ keys) or enable Preview. Scrub/pause at several
   timestamps; check world/camera timing, FOV and rotation interpolation visually.
7. L: cancel once, then confirm. Camera icons/keys should disappear and Default return.
   Rebind Cycle/Add/Clear in Settings, test their new keys, reset them afterward.
8. Check F6 emergency stop and native camera/FOV/input restoration. If a failure occurs,
   provide TEMP/TheaterModeGame.log and TEMP/TheaterModeRender.log plus the camera
   .ercam file and screenshot/video. Crash log if one exists: TEMP/TheaterModeCrash.log.

Old replay-origin compatibility remains the C8 diagnosed blocker; this camera change
neither removes that guard nor claims to solve automatic world relocation.
