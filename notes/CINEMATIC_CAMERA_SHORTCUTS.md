# Camera shortcuts — C2

2026-10-07. Release AMD64 host and DLL compiled. Runtime and visual validation required.

- F3 cycles editor selection Default → Free → Dolly → Default.
- K requests a Dolly keyframe. Currently rejected with an explicit diagnostic: no verified active camera transform/FOV backend exists. No invented camera data is inserted.
- L opens a destructive-action warning, with Cancel/Escape and Delete all. Confirm clears the editor's dolly track only; original replay files remain unchanged. The current track is empty because key capture remains unavailable.
- Each shortcut reveals the Camera panel, including when F4 had hidden it, so a request cannot silently appear to do nothing.
- Auto-repeat is ignored, text-field typing takes precedence, and queued actions are processed on the render thread. The window callback does not mutate the UI or game camera.
- Native camera remains Default in all three selections; unavailable modes are labeled. This is input plumbing, not a delivered Free/Dolly backend.

Central defaults live in `shared/TheaterHotkeys.h`. Existing F4/F5/F6/Space bindings are preserved. No new remapping UI or keybind persistence was added.

Package: `outputs/Cinematic-C2-camera-shortcuts`. Use its matching EXE/DLL; close game/host before replacement through the established YAFSML configuration. With the game loaded, press F3 three times and check labels; press K and check the unavailable diagnostic; press L, cancel, then reopen and confirm. No actual camera movement/key capture is expected yet. Return to the preserved C1/P2e package if there is a regression.

No test suite was run for this shortcut-only revision; build success is not evidence of in-game keyboard handling. Next dependency is the C1 read-only active-camera identification test described in `CINEMATIC_EDITOR_CHECKPOINT_1.md`.
