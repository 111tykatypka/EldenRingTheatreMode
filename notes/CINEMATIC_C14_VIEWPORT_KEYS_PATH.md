# C14: viewport mode controls, camera key selection and path preview

## Changes

- F4 camera mode controls are now an inline group inside the Game viewport,
  rather than an independently positioned top-level window. The hidden-overlay
  badge remains available over the full-screen picture; clean view hides it.
- Camera keys support plain-click selection, Ctrl-click toggling and Shift-click
  inclusive timestamp ranges (Ctrl+Shift adds the range). Selected keys are
  highlighted in the timeline, curve editor and viewport. Gizmos edit the active
  key; multiselect is for selection/deletion, not bulk transform manipulation.
- Sequencer exposes Remove selected camera keys [Del]. Delete also works while
  F4 is shown, game focused and no text input, active widget or rebinding capture
  owns input. Deletion is a single mutex-protected track replacement; pointers
  are not shared with the UI. Replay changes and removed keys prune selection.
- Play path is available in the viewport's Dolly controls and Camera panel. It
  selects Dolly, arms existing native camera integration, enables path preview,
  seeks to the first authored key and sends Play through the existing master
  replay transport. At least two current-replay keys are required.
- Author keys returns to manual movement. Space/K authoring remains independent
  of automatic path evaluation as fixed in C13c. Preview evaluates position,
  quaternion and FOV at master ReplayTime; pause/seek use the same evaluator.

## Preview diagnosis

C13c intentionally removed automatic preview on Play to permit manual camera
authoring during gameplay. Ordinary Play therefore did not follow a path unless
explicit preview was selected. The new Play path action makes that choice clear
and starts inside the authored time range instead of at a held endpoint.

The previous 20-unit camera-start guard could additionally reject an explicit
preview after moving away from its first key. Explicit preview now permits that
camera-only jump, using validated authored poses. Automatic cut starts retain
the displacement guard. Native pointer/owner, exact profile, finite transform,
offline, player/host heartbeat, focus and emergency-stop checks remain intact.
This does not relax player replay teleport guards or move the player by camera code.

## Build / manual validation

Release AMD64 host/native library/DLL built. Tests were not added or run in this
task. In-game layout, deletion and camera path following remain unverified.

Package: `outputs/Cinematic-C14-viewport-keys-path`.
Close old game and host. Launch this package's EXE with its adjacent DLL through
the existing YAFSML workflow.

1. F4: mode controls must remain inside the Game viewport as it is resized.
2. Dolly/Author keys: capture two or more distinct camera poses with K at different
   replay times, using Space to play/pause. Manual movement must stay available.
3. F4: click Play path. Replay should seek to the first key and camera should
   traverse the track, including orientation/FOV. Pause/seek while previewing;
   then Author keys to resume manual camera movement without switching modes.
4. Timeline: click one diamond, Ctrl-click another, Shift-click a third. Inspect
   highlights, remove with the button, then repeat with Delete. Typing into text
   and numeric fields must not delete camera keys. Save/reload the .ercam path.
5. Delete all remaining keys: camera override must safely release. Check Alt-Tab
   focus gating and the C13b launcher fix remain operational.

Logs: `%TEMP%/TheaterModeGame.log`, `TheaterModeRender.log` and
`%LOCALAPPDATA%/EldenRingTheaterMode/logs/TheaterModeRecorder.log`.
