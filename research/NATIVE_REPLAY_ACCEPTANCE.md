# Native replay acceptance checklist

This is a buildable research checkpoint, NOT task completion. Read-only game session completed; native recorder population verified, bloodstain ghost absent. Preserve Phase5.

|Section|Requirement|Status|
|---|---|---|
|0|TARGET BUILD|STATIC/REFERENCE DOCUMENTED|
|1|PROJECT PATHS|STATIC/REFERENCE DOCUMENTED|
|2|REQUIRED REFERENCE SOURCES|STATIC/REFERENCE DOCUMENTED|
|3|CRITICAL KNOWN ENGINE CLUES|ACTIVE — INCOMPLETE|
|4|MAIN RESEARCH QUESTION|ACTIVE — INCOMPLETE|
|5|FIRST PRIORITY: REPLAYRECORDER|ACTIVE — INCOMPLETE|
|6|FIND THE REPLAY RECORDER UPDATE TASK|ACTIVE — INCOMPLETE|
|7|BUILD A REPLAYRECORDER MEMORY MAP|ACTIVE — INCOMPLETE|
|8|RUNTIME READ-ONLY RECORDER PROBE|IMPLEMENTED — RUNTIME VALIDATION REQUIRED|
|9|CRITICAL EXPERIMENT:|ACTIVE — INCOMPLETE|
|10|REPLAYMANIPULATOR|ACTIVE — INCOMPLETE|
|11|COMPARE MANIPULATORS|ACTIVE — INCOMPLETE|
|12|NATIVE ACTOR CONTROL PROBE|IMPLEMENTED — RUNTIME VALIDATION REQUIRED|
|13|BLOODSTAIN GHOST SPAWN PIPELINE|ACTIVE — INCOMPLETE|
|14|TRACE ChrType::BloodstainGhost|ACTIVE — INCOMPLETE|
|15|FIND NATIVE BLOODSTAIN SERIALIZED DATA|ACTIVE — INCOMPLETE|
|16|NETWORK / FROMNET BLOODSTAIN PATH|ACTIVE — INCOMPLETE|
|17|BLOODSTAIN WORLD POSITIONING|ACTIVE — INCOMPLETE|
|18|FIND WHETHER BLOODSTAIN USES NATIVE WORLD/MAP IDENTITY|ACTIVE — INCOMPLETE|
|19|FULL FRAME FORMAT|ACTIVE — INCOMPLETE|
|20|IF NATIVE REPLAY FRAMES ARE COMPRESSED / DELTA-ENCODED|ACTIVE — INCOMPLETE|
|21|DETERMINE REPLAY DURATION LIMIT|ACTIVE — INCOMPLETE|
|22|INVESTIGATE POSSIBILITY OF EXTENDING NATIVE RECORDING|ACTIVE — INCOMPLETE|
|23|THEATER REPLAY ACTOR ARCHITECTURE|ACTIVE — INCOMPLETE|
|24|INPUT ISOLATION IMPLICATION|ACTIVE — INCOMPLETE|
|25|GROUNDING IMPLICATION|ACTIVE — INCOMPLETE|
|26|DO NOT MODIFY CURRENT PRODUCTION PLAYBACK YET|STATIC/REFERENCE DOCUMENTED|
|27|BUILD A NEW ISOLATED PROTOTYPE|STATIC/REFERENCE DOCUMENTED|
|28|REQUIRED DOCUMENTS|STATIC/REFERENCE DOCUMENTED|
|29|REQUIRED RUNTIME EXPERIMENT|IMPLEMENTED — RUNTIME VALIDATION REQUIRED|
|30|LOGGING|STATIC/REFERENCE DOCUMENTED|
|31|EVIDENCE LEVELS|STATIC/REFERENCE DOCUMENTED|
|32|NON-NEGOTIABLE EXECUTION RULES|ACTIVE — INCOMPLETE|
|33|PRIMARY SUCCESS CONDITION|ACTIVE — INCOMPLETE|
|34|MINIMUM ACCEPTABLE SUCCESS|ACTIVE — INCOMPLETE|
|35|FINAL REPORT|ACTIVE — INCOMPLETE|
|36|FINAL PRIORITY|ACTIVE — INCOMPLETE|

## Minimum acceptance (section34)

|Criterion|Current evidence|
|---|---|
|Recorder layout sufficiently mapped|0x860 object, prefix, node0x248; payload schema incomplete|
|Update task callback found|STATIC_VERIFIED +4d8/task+500→3f92b0→virtual+c0→660ac0→4e5af0; live scheduling pending|
|Actual recorded buffer|RUNTIME_VERIFIED pool/owner/capacity/node payload changes; full codec pending|
|ReplayManipulator concrete class|RTTI/vtable/ctor and type3 exact bytes|
|Ghost creation|Factory404570→ReplayGhostIns4f1840→ReplayManipulator data attachment; bloodstain provenance pending|
|Ghost actual manipulator runtime verified|NOT OBSERVED — offline session has no ghost|
|Data→ghost consumer|Static slot10→3df190→3df650→native decoder; runtime behavior pending|
|Clear next implementation step|Read-only recorder/node diffs + real ghost identity/data equality; then codec research. No ghost spawning before lifecycle verified|

## Tests this checkpoint

C++ CTest14/14; Rust research feature35/35; Rust default35/35; Python existing3+5 and new2 passed; DX12 real-device smoke passed. These exercise tools/build regression checks, not native replay execution.

## Open questions

Full native codec and action semantics; downloaded bloodstain local decoder/manager linkage; map/streaming conversion; recorder retention observed duration; safe extension/concatenation; native ghost lifecycle/update ownership; grounding and input isolation. No performance numbers measured in Elden Ring.

Runtime result: NATIVE_REPLAY_RUNTIME_RESULT.md / .json. +A0 owner diagnostic corrected to+A8 after exact-byte and275 real snapshot checks.

Hotfix1 build: CTest14/14, Rust research36/36, Python10/10, DX12 smoke PASS. Native recorder observations refer to initial build; corrected owner+A8 binary itself not reloaded in game yet.
