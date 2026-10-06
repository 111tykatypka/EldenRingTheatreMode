# Phase7 crash investigation — 2026-10-06

## Hotfix2 — mouse-input regression

User verified Hotfix1 loads into the world and Insert displays Overlay, then right
mouse button terminates the game. No exception-observer entry was produced.

[CONFIRMED] Added RMB down/up in both Overlay and Editor to the actual DX12 smoke
test. The pre-fix backend terminated with exit 0xC0000409 (-1073740791). Changing
only the queue draining to swap under lock and process outside it made the same
test pass. The final version also passes with direct button-event ingestion.

[CONFIRMED] Old render input drain held input_mutex while calling the Win32 backend.
Its button handling calls SetCapture/ReleaseCapture, which can re-enter our WndProc,
which also locks input_mutex. This is a reentrancy hazard. [HIGH CONFIDENCE] This
path caused the reproduced failure; exact fail-fast subcode/call stack was not
captured. Runtime causation in Elden Ring must still be verified.

Fix: drain to a local deque under a short lock; process without that lock. Deliver
button events with ImGuiIO::AddMouseButtonEvent instead of the Win32 backend's
capture calls. The game retains ownership of native window capture; injected
render thread does not SetCapture/ReleaseCapture. Queue only input/focus messages,
not arbitrary messages containing transient native pointers.

Next test: close game/old host, start **Phase7_Runtime_UI_Hotfix2** host and launch
through its Launcher (sibling DLL selected automatically). Load world, walk 15s,
Insert to Overlay; right-click and release several times. Repeat in Editor, then
Clean. Check left/right/middle click, wheel and normal gameplay after returning to
Clean. No replay needed. Report mode and action on any failure; use logs below.

Hotfix1 and original Phase7 outputs are preserved. Hotfix2 is not yet in-game verified.

## Historical Hotfix1 investigation

Status: **CRASH REPORTED; exact cause unresolved; diagnostic mitigation implemented.**
This is not a verified in-game crash fix.

## Evidence

Two user game launches (PID 29344 and 9348) accepted the exact 2.7.0.0 profile,
registered the native task callbacks, and reached DX12_IMGUI_INITIALIZED.
Neither reached WORLDCHR_READY / PLAYER_FOUND or replay playback in the latest log.
Render initialization ticks: 104459765 and 104480859.
The launcher log confirms Phase7_Runtime_UI/TheaterMode.dll was loaded.
There is no fresh Application Error event or CrashDumps entry for these PIDs.
Oct5 dumps/event fault 0x80000003 at game RVA 0xC58D46 belong to older launches;
they cannot establish the current fault. The DX12 path is a candidate, not proof.

## Changes

- Clean startup: no ImGui context/resource creation or UI drawing until explicit Insert.
  Factory/Present/window hooks remain installed. This isolates rendering from loading
  and permits player readiness to be checked before enabling experimental rendering.
- Window forwarding procedure is atomically published before installing our WndProc.
  Previously another window thread could enter it before previous_proc was assigned.
  This code-level race is confirmed; its involvement in the reported crash is unknown.
- First three rendered frames log each phase: backend NewFrame, UI NewFrame, draw,
  RenderDrawData, Execute, submission. Reinitialization resets this diagnostic count.
- A bounded exception observer logs exception RIP, module/RVA, PID and its own stack
  to %TEMP%/TheaterModeCrash.log and returns EXCEPTION_CONTINUE_SEARCH.
  It does not recover from faults. The observer stack is not an exception-context
  unwind. Some aborts, fail-fast and device failures may bypass it entirely.
- Added real-device DX12 smoke executable: actual hooked swapchain, unloaded replay
  state, Clean/Overlay/Editor, 120 Presents, ResizeBuffers and shutdown. It passed
  both before and after the mitigation. Therefore it does not reproduce the game crash.
- No changes to replay writes, SDK pin, identity validation, tasks or YAFSML.

## Exact next test

1. Close Elden Ring, the old Theater host and any completed launcher.
2. Launch Phase7_Runtime_UI_Hotfix1/EldenRingTheaterMode.exe, then use its existing
   Launcher game-start button. It selects the DLL beside this EXE. No game-file copy
   is needed. For manual YAFSML launch, point theater_mode at Hotfix1/TheaterMode.dll.
3. Load a familiar save on safe ground. **Do not press Insert or start replay yet.**
   Wait for PLAYER FOUND; walk normally for 15 seconds. No overlay is expected.
4. If stable, press Insert once (Overlay), wait 10 seconds; again (Editor), wait;
   again (Clean). Do not run replay in this diagnostic test.
5. If the game exits, send these files with the step at which it happened:
   %TEMP%/TheaterModeRender.log, %TEMP%/TheaterModeGame.log,
   %TEMP%/TheaterModeCrash.log (if created), and
   %LOCALAPPDATA%/EldenRingTheaterMode/launch/log/YAFSML.log.
6. Report separately whether Clean loading and Overlay/Editor survived. No conclusion
   about grounding, NPC ownership, animation or replay follows from this UI test.

Old Phase5, Tester, Nightly and original Phase7 outputs are preserved.

## Build evidence

Release x64 host and Rust DLL built. CTest 13/13, Rust 24/24, Python 3/3
passed; real-device DX12 smoke passed. The initial CTest attempt failed its editor
pipe test because the existing host already owned the production pipe. The test
now uses a PID-specific endpoint; production endpoint and protocol remain unchanged.
The old host was not terminated. Elden Ring runtime verification remains pending.
