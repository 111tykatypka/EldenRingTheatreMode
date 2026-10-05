ELDEN RING THEATER MODE — PHASE 3 RECORDER + REPLAY PLAYER

Run EldenRingTheaterMode.exe for the native Windows application. It preserves
Phase 2 recording controls and adds an offline replay browser and player.

Recorder:
  Click START RECORDING / PAUSE / RESUME / STOP
  F5 Start, F6 Stop, F7 Pause, F8 Resume (registered global hotkeys)
  OPEN REPLAY FOLDER opens the replay directory.

Replay player:
  Select a validated .erplay in the browser and click OPEN (or double-click).
  PLAY / PAUSE / STOP / RESTART control the independent replay clock.
  The timeline seeks by replay timestamp; << and >> step to adjacent samples.
  Playback speeds: 0.1x, 0.25x, 0.5x, 1x, 2x, 4x.
  Space toggles play/pause, arrows step while paused, Home/End seek to bounds.
  Add Bookmark saves timestamps next to the replay in a .bookmarks sidecar;
  double-click a bookmark to seek to it. The trajectory preview shows X/Z
  projection, current player position and orientation; its trail can be toggled
  and its visible past span adjusted.

Replay directory:
  %LOCALAPPDATA%\EldenRingTheaterMode\replays
Log:
  %LOCALAPPDATA%\EldenRingTheaterMode\logs\TheaterModeRecorder.log

The versioned ERPLAY v2 reader validates the complete file before constructing
ReplayData. It keeps a binary-searchable timestamp/file-offset index and caches
one chunk at a time. Playback interpolates position linearly and orientation by
normalized quaternion SLERP. See ERPLAY_FORMAT.md and REPLAY_PLAYER.md.

Current replay data is player position and quaternion only. The preview is a
trajectory visualization, not Elden Ring world reconstruction. NPCs/events,
replay injection, timeline camera tracks and cinematic cameras are not included.
The Phase 2 recorder and the existing game DLL/IPC protocol remain unchanged.
