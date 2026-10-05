# Current limitations — Modern

- **RUNTIME VERIFIED historically:** Phase5 player transform sampling/replay, full
  duration, smooth movement/rotation. Golden binaries and source remain intact.
- **IMPLEMENTED / UNIT TESTED:** modern host, typed character serialization,
  registry/presence validation, optional-v3 compatibility, recovery, timeline math,
  dedicated replay worker, existing controller/IPC guards.
- **OFFLINE VERIFIED:** real `replay_2026-10-05_105119.erplay`: 1069 samples,
  17.8149115 seconds, 2 player chunks, 63 action events; first five seconds and full
  endpoint via mock IPC. This was NOT an Elden Ring test.
- **EXPERIMENTAL, RUNTIME VALIDATION REQUIRED:** nearby actor enumeration/capture,
  normalized local input suppression at PreBehaviorSafe, new grounding diagnostics.
- **FAILED AT RUNTIME previously:** debug-flags-only input lock. The new candidate
  is not declared fixed. Floating player and native WALK/RUN remain unresolved.
- **UNKNOWN:** actual NPC capture overhead, frame ordering, map origin compatibility,
  reliable spawn generations, native AI ownership, correct locomotion graph inputs.
- No native NPC replay, spawning, destruction, camera, VFX, world reconstruction,
  anti-cheat bypass, custom injector or modifications to game files.

Character preview is decimated and presence-driven; it is an offline data view.
Original .erplay data keeps every received sample; dropped frames are counted.
No collision/world/AI state is reconstructed. Raw NPC animation IDs are not named.

Character latest snapshot and host queue are bounded; count/rate/budget/drops are
visible. No duration or total replay-size limit. Reader indexes and registry memory
grow with recording length. Optional track metadata is not authenticated.

Library scans/load/rename/delete run between rendered frames on the UI thread.
Large files can temporarily stall UI input; playback clock still runs separately.
Graphics device loss stops replay and requests an editor restart. Device recreation
without restart and multi-viewport are not implemented.

Headless ImGui checks are not visual QA or real Windows DPI transitions. Modern UI,
global hotkeys with the game focused, and in-game restoration must be checked by
the user with the matching new EXE/DLL pair.
