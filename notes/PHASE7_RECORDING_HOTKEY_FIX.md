# Hotfix3 — global recording controls

User reported F5 has no effect with game running in Hotfix2. Current log confirms
F5/F6/F7/F8 RegisterHotKey success and PLAYER FOUND, but no recording-start entry.
There was no hotkey-delivery logging, so receipt of that physical F5 cannot be proven.

[CONFIRMED] modern_proc gated F5/F7/F8 WM_HOTKEY handling on the desktop ImGui
WantCaptureKeyboard flag. That flag represents widget focus, may be stale when the
host is minimized, and is not a policy for a Windows global hotkey. F6 was exempt.
[HIGH CONFIDENCE] This explains silent loss of recording hotkeys. Other physical
key delivery failures cannot be excluded until the new log shows receipt.

Fix: WM_HOTKEY always delegates registered IDs to handle_global_hotkey independently
of ImGui keyboard capture. F6 still uses emergency_stop. Existing recorder/replay
interlocks remain. Receipt and queueing now logged; missing-player/replay-active
refusals show a diagnostic rather than silently discarding START.

Regression added to existing modern-ui-tests: force WantCaptureKeyboard=true and
verify START/PAUSE/RESUME are queued; unknown ID refused. No simulated game samples
or new native adapter changes. The DLL source and game hooks are unchanged.

## Next user test

Close game and old host, then launch Phase7_Runtime_UI_Hotfix3/EldenRingTheaterMode.exe
and start game through its Launcher. The existing DLL sample pipe worker exits on
disconnect and does not reconnect after closing the host; a fresh game process is
needed for this host replacement. No DLL hot reload; no copying into game folder.

Load flat safe ground. Verify Recorder's Player FOUND, press F5 with game focused.
External desktop Recorder should show RECORDING and growing Samples/Duration. The
in-game UI currently displays replay transport, not the recorder's REC indicator;
verify recording state in the desktop Recorder panel. F7 pauses, F8 resumes, F6
stops and finalizes. The new replay should appear in Library.

If F5 still fails, inspect %LOCALAPPDATA%/EldenRingTheaterMode/logs/TheaterModeRecorder.log:
GLOBAL_HOTKEY received id=1 -> RECORD_COMMAND queued START -> Recording started.
Missing receipt points to key delivery; missing queue may be replay interlock;
START rejected identifies unavailable player samples. Click Recorder's Start / F5
to exercise the same recorder command without keyboard delivery.

Runtime F5 verification pending; do not mark the recorder/replay validated solely
from the dispatch unit test.
