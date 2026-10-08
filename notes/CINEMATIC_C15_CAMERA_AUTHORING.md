# C15: picture overlay, edit recovery, curve box selection and J

## Camera controls / spacing

The mode selector floats at top-center inside the rendered game picture, rather
than consuming rows of the viewport window's content. Mode descriptions are in
tooltips and the Camera panel. Short mode/visibility/path status remains in the
badge. The picture again uses the available viewport area; its size is independent
of the selected camera mode. Hidden F4 keeps the small badge; clean view hides it.

## Edit recovery

Alt+Z restores the prior camera edit/deletion (Undo). Alt+Shift+Z reapplies it
(Redo). Both actions also have sequencer buttons. The wording clarifies the user's
request to recover accidental transforms/deletions rather than repeat a deletion.

History stores camera keys/cuts and replay identity, never native pointers or game
objects. It covers captures, key property/gizmo/curve edits, selected-key deletion,
clear-all and path loading. A drag is one history transaction. Undo/redo cancels
active editing and disables path-preview/cut playback so restored state cannot
unexpectedly start controlling the camera. Restoring keys does not automatically
arm camera writes after clear-all. New edits discard redo; changing replay clears
history. History is in memory only and retains 64 snapshots as a resource bound;
it does not constrain replay duration or key count.

Alt shortcuts require game focus, are suppressed during text input/rebinding,
ignore repeat messages and do not accidentally roll the Free/Dolly camera with Z.

## Curve selection

Drag empty space with left mouse in the curve plot to draw a selection rectangle.
Keys inside it are selected and highlighted in graph/timeline/viewport. Ctrl or
Shift when starting adds to the existing selection; no modifier replaces it.
Escape cancels. Dragging directly on a node remains a single-node value edit;
Ctrl/Shift clicks retain toggle/range selection. Delete and Remove operate on the
selection. Multiselect does not imply bulk transform editing. Plot vertical bounds
stay fixed during the rectangle drag.

## Play path shortcut

New persisted action `play_dolly_path`, default J, appears in Settings/Keybindings.
It shares the Play path button flow: validated current track with at least two
keys, Dolly mode, explicit arming, preview, seek first camera key, Play on existing
master ReplayTime. It works outside F4 and does not open the Camera panel. Existing
bindings retain their action indices; new action is appended. If a legacy custom
binding already uses J, migration chooses an unused function key to preserve it.
Space/K manual authoring behavior is unchanged.

## Build and runtime status

Package: `outputs/Cinematic-C15-camera-authoring`.
Release AMD64 host/native library/game DLL compiled. No automated tests were added
or run for this task. New behavior is IMPLEMENTED; in-game/visual checks remain.

Close old game and host, launch this package's EXE with its adjacent DLL via the
existing YAFSML workflow. Check:

1. F4: mode badge overlays the game picture and descriptions no longer shrink it.
2. Load a replay/track, press J outside F4: preview starts from first authored key.
3. Author keys: Space/K remain manual. Return to preview with J.
4. Curve plot: box-select several keys, Delete, Alt+Z restore, Alt+Shift+Z delete
   again. Repeat with Ctrl/Shift additive rectangle and Escape cancel.
5. Drag a viewport rotation/translation gizmo, release, Alt+Z once: restore the
   whole drag. Alt+Shift+Z reapplies it. Numeric field typing must not trigger it.
6. Alt-Tab: no editor shortcuts/sounds; no camera roll from Alt+Z on return.

Logs: `%TEMP%/TheaterModeGame.log`, `TheaterModeRender.log`, and
`%LOCALAPPDATA%/EldenRingTheaterMode/logs/TheaterModeRecorder.log`.
