# C13a — focused input, clean view and camera editor layout

Status: COMPILE_VERIFIED. 14 CTest suites and 49 Rust tests PASS (one optional
recording check ignored); standalone DX12 visibility/copy/resize smoke PASS.
Elden Ring HUD coverage, Space behavior and gizmo feel are RUNTIME UNVERIFIED.
Final matched package: outputs/Cinematic-C13a-focus-clean-view. C13 is intermediate.
Previous builds and unrelated actor research are preserved.

## Camera-bar overlap

Shown F4 now draws/measures the mode bar before solving the game viewport's bounds.
Its actual bottom reserves a strip above the viewport, including Dolly capture/
clear/status rows. The sequencer's minimum top also accounts for that strip.
The hidden bar remains compact and top-center; O clean view hides it completely.
CPU texture-layout tests assert the picture is below the bar and above the sequencer
at 1280x720, 1920x1080 and 3840x2160. Floating windows remain movable; arbitrary
manual window-to-window overlaps outside these bounds are not collision-managed.

## Middle-click transforms

Removed Move XYZ / Rotate XYZ radio/check controls in the viewport and Camera tab.
With F4 open, middle-click inside the actual game picture toggles translation/
rotation; header text reports current gizmo. This hit test excludes the header,
letterboxing, other panels and sliders. Select a Dolly node and drag its axes/rings.
Changing mode clears an active drag. Middle-click slider reset remains unchanged.

## Scalable Dolly curve timeline

Drag the separator between tracks and curves to change their height allocation.
The ratio persists. The entire sequencer is still movable/resizable. Hover curve:
wheel zooms the vertical value range; Shift-wheel pans values; Ctrl-wheel zooms
master time around the pointer. Fit curve resets vertical range. Timeline and
curves share horizontal pan/zoom; no second camera clock exists.

## Space outside F4

The configured playback key (Space default) now belongs to Theater when a replay
is loaded and the game is focused, including hidden/clean views. Key repeats are
ignored. Text input and keybinding capture prevent activation. The key's DirectInput
state/buffered events are filtered so Space does not also jump/interact in game.
Without a loaded replay it remains a native game key outside F4.

## Focus bug and correction

Root cause: blocking/queueing in WndProc and both DirectInput hooks had no foreground
check; virtual cursor and ImGui Win32 polling could therefore deliver background
mouse/modifier activity. The rebind editor also polled global GetAsyncKeyState.

Now the native backend checks the game HWND's foreground root before handling
hotkeys, wheel, blocking input or capturing DirectInput deltas. Background input is
forwarded unchanged. Each background render frame clears pending ImGui input,
mouse/key state and virtual cursor/buttons and sends a focus-loss event. UI command
emission and global-key rebinding are gated by the frame focus flag. Active gizmo,
curve and scrub interactions stop on focus loss.

Sound queuing and playback check foreground process; active voices/queued cues are
cleared on the sound worker within its 25-ms poll interval when focus is lost.
User sound enable/volume preferences remain untouched.

Host F5/F6 registration is now scoped to the connected game foreground, refreshed
at 100 ms, and queued WM_HOTKEY messages re-check focus before acting. No recording
or stop command is emitted from browser/other-app focus. There can be a <=100-ms
registration-release interval; stale messages are still rejected immediately.
Existing direct UI buttons and in-game emergency polling remain available.

## O clean view and native HUD

O is a new rebindable action appended without changing existing action indices.
Settings > Clean view / HUD offers Hide native game HUD, plus the combined clean
view action. O hides native HUD and all Theater indicators, remembering previous
F4 visibility; O again restores it and native HUD. F4 reopening releases native
HUD hiding. Old binding files assigning O elsewhere retain that explicit binding;
the new action migrates to an unused F12–F24 key and the label follows the binding.
HUD hiding is session-only, not persisted and does not touch game settings/saves.

STATIC_VERIFIED evidence: the reference's AOB_HUD_OPACITY_READ_ADDRESS string uses
41 0F 28 00 48 8B C2 41 0F 28 48 ?? 0F 29 02 0F 29 4A. Reference log resolves
RVA 0x11624E0. Exact SHA-verified 2.7.0.0 has one occurrence of the complete 20-byte
leaf there. Disassembly: copy [r8] first 16 bytes to [rdx], copy [r8+0x10] to
[rdx+0x10], return rdx in rax. The reference carries the zero-vector patch
0F 57 C0 90 for this first copy. Evidence: research/HUD_OPACITY_C13_EVIDENCE.json.

Independent implementation: EldenRingHudAdapter.cpp uses a MinHook trampoline,
calls the original copy, then zeroes only the first output vector when requested,
focused and a verified offline loaded-player callback is fresh (<500 ms). Second
vector/return value preserved. Original game files are untouched. Profile/RVA/20
opcode guard lives in GameProfile.h; initialization is only after exact runtime
profile acceptance, outside the recurring callback. An invalid signature disables
the feature. The callback supplies only context/heartbeat, never retained pointers.
Default is native pass-through. No borrowed pointer is saved, no per-HUD-call
allocation or process-memory syscall; writes use the current call's output under SEH.
A fault disables the filter. Process-lifetime hooks match the current adapter;
hot-unloading the DLL is unsupported.

The reference and static flow support a HUD-opacity interpretation, but exact
coverage (boss bars, prompts, subtitles, menus, other mods) remains UNKNOWN until
in-game testing. Do not claim that every possible engine UI layer is hidden.
The feature does not suppress other mods' own overlays or Windows/Steam overlays.

## Tests and limitations

Legacy P/O binding migration and strict explicit conflict rejection pass.
CPU tests reject F4, F3, Space and O messages when no game window is foreground,
and check mode-bar/picture/timeline geometry. Standalone GPU fixture changes
visibility explicitly so it does not bypass or depend on foreground restrictions;
it is not a game/keyboard runtime test. Native HUD binding/filter is untested in
Elden Ring. There is no new HUD patching on unknown versions.

## Manual validation

Close old game/host, launch this package's EXE and use its same-folder DLL through
existing YAFSML. Load an offline world and replay.

1. F4: verify mode bar clears viewport header/image. Resize timeline and viewport.
2. Select a Dolly node. Middle-click in picture: Move/Rotate status changes; drag
   rings/axes. Middle-click a slider still resets it rather than changing gizmo.
3. Drag curve separator, wheel/Shift-wheel/Ctrl-wheel and Fit curve. Scrub and edit.
4. Hide F4: Space starts/pauses loaded replay. K captures as before. With no replay
   loaded, Space should remain the native game action.
5. Settings reports HUD opacity READY. O hides native HUD and all Theater UI.
   O restores, or F4 restores UI/HUD. Check boss bars, prompts and subtitles and
   report any layer remaining; those have not been visually verified.
6. Alt-Tab to browser: scroll, middle/right click, type, press F3/F4/Space/O/F5/F6.
   There must be no UI sounds, setting changes, queued playback or recording starts.
   Return: no stuck mouse buttons, scrolling, key capture or resumed accidental drag.

Logs: %TEMP%/TheaterModeGame.log (HUD hook READY/rejection), TheaterModeRender.log,
%LOCALAPPDATA%/EldenRingTheaterMode/logs/TheaterModeRecorder.log and build manifest.
