# C11a — Dolly visibility, curves and camera effects

Status: IMPLEMENTED / COMPILE_VERIFIED. In-game visual behavior and camera feel
remain UNKNOWN until tested in Elden Ring. Existing timing hooks, actor replay,
version guard and native camera bridge are unchanged.

## Package

`outputs/Cinematic-C11a-dolly-curves` is the final matched Release x64 EXE/DLL.
`Cinematic-C11-dolly-curves` is an intermediate package; prefer C11a.
Existing C10c and earlier outputs are preserved.

## Controls and visibility

Camera settings now contain Show Dolly cameras / transform handles and Enable
Dolly visibility shortcut (default P). The shortcut is rebindable in Keybindings.
P toggles marker/path/handle visibility without opening F4 or changing camera mode.
The top-center bar displays visible/hidden plus the current shortcut or disabled.
Markers and the sampled path render outside F4, regardless of the active tool tab.
Outside F4 they are passive (NoInputs), preserving mouse-look and movement.
Open F4 for picking and dragging XYZ/rotation handles. Shift+F4 Clean Preview
intentionally hides all editor indicators. No depth occlusion is implemented.

Old binding files that explicitly assigned P to another action keep that action.
The newly added visibility action takes the first unused F12–F24 key instead;
the status bar displays the actual key. Explicit conflicts still reject the file.
No existing action indices changed.

## Dolly smoothing

0–2 seconds, default 0. The shared evaluator samples five binomial-weighted
poses around ReplayTime. Its window shrinks to the closest segment endpoint,
preserving exact authored key poses, endpoints and Step cuts. Quaternion blending
uses SLERP, never Euler interpolation. Both the runtime path and editor curves
use this evaluator. No accumulated frame history or second camera clock exists.
Smoothing can alter the path between keys and its velocity; constant-speed lookup
still describes the underlying path before this optional smoothing filter.

## Sequencer spline editor

An optional Dolly curve region in the resizable sequencer displays X, Y, Z or FOV
against the same pan/zoom and master ReplayTime as the timeline. It evaluates the
existing centripetal Catmull–Rom / Bezier / timing curves, rather than a separate
visual approximation of the data. Select a node and choose outgoing interpolation
Linear / Smooth / Bezier / Curve / Spline / Step. Drag vertically to change its
channel value; Ctrl-drag horizontally retimes the node; Escape restores the
original node. Duplicate timestamps and invalid edits remain rejected. Selection
is shared with timeline diamonds, viewport gizmos and the key inspector.
Graph/path sampling is bounded (192 / 128 steps); spline/arc tables rebuild on
project changes only. Key edits are throttled to 30 Hz with a final release edit.
The camera inspector retains the detailed Bezier handle and easing editors.

## Shake

Existing position and rotation amplitudes now have an explicit Apply shake to
Dolly cameras checkbox (default enabled). Below frequency: speed multiplier
0–10 (default 1), then smoothing 0–2 seconds (default 0). Frequency times speed
controls phase advancement at ReplayTime. Smoothing is an analytic Gaussian
low-pass of the procedural sinusoidal signal: exp(-0.5 * omega^2 * seconds^2).
It reduces high-frequency shake, including amplitude, without introducing seek
history or latency. Camera nodes store the unshaken base pose.
New sliders retain right/middle-click defaults and direct numeric entry.

## Storage

Visibility and effects persist as application preferences in TheaterOverlay.ini;
slider changes are debounced instead of writing each drag frame. They currently
apply globally, not per camera key or per .ercam track. Existing .ercam v1 remains
unchanged/backward compatible; it stores authored nodes and interpolation data.

## Verification

Release C++ host/native backend and Rust DLL built successfully.
14/14 CTest suites PASS. Rust: 49 PASS, 1 optional real-recording test ignored.
Added deterministic smoothing, exact endpoints/Step, no-op default, shake speed,
low-pass attenuation, legacy P conflict and hidden-overlay visibility checks.
UI construction covers EN/RU, seven resolutions and visibility/tool combinations.
It is CPU construction testing, not GPU rendering or native camera validation.
An initial UI test saved language/tool preferences; the previously observed values
were restored. Subsequent UI tests are isolated from preference/layout storage.

## Manual test

1. Stop recordings and close the old host and game (DLL changes require restart).
2. Launch EldenRingTheaterMode.exe from this package. Use its existing YAFSML
   launcher with TheaterMode.dll from the same package; load an offline world.
3. Load a replay and use F3 to select Dolly. Capture two or more separated keys
   with K at different replay times. Existing native-write arming guards remain.
4. Hide F4: markers/path remain visible. P hides/shows them; the mode bar reports
   the state. Disable the shortcut checkbox and confirm P no longer toggles.
   Rebind it and confirm the status label updates. Clean Preview hides everything.
5. Open F4 to select/drag viewport handles and curve nodes. Switch X/Y/Z/FOV,
   choose Spline/Bezier/Step, drag vertically, Ctrl-retime, Escape cancel and scrub.
   Check the graph fits; the sequencer is movable/resizable for more curve space.
6. Set Dolly smoothing from 0 to 0.2–0.5 seconds. Scrub the same timestamp repeatedly:
   the pose must be reproducible; exact key timestamps retain their authored pose.
7. Enable Dolly shake, set small amplitudes, then vary frequency, speed and smoothing.
   Pause/scrub and check reproducibility; disabling Apply shake to Dolly removes
   shake from Dolly without modifying keys. Right/middle reset each slider.
8. F6 and Default release camera writes. Confirm native camera and gameplay return.

Send %TEMP%/TheaterModeGame.log and the matched package manifest on failure.
No in-game or visually verified success is claimed for these new controls.
