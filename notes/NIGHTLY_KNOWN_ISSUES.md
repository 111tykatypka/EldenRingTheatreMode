# Known issues — developer experimental nightly

- User-reported grounding, NPC in-game playback and native animation failures remain unresolved. Do not use this build as a production cinematic replay.
- Debug flag offset conflict: SDK 0x530 vs Freecam 0x538. noMove/noAttack/both/noUpdate experiments are blocked. No raw offset fallback exists.
- NPC lookup uses the pinned SDK's `chr_ins_by_handle_mut`, which invokes the ChrSet virtual lookup. The existing distance-vector capture is a different path. New trace distinguishes lookup/identity rejection; no claim that native lookup is runtime verified.
- Same live actor/native handle/entity/NPC param/type required; no resolver across reloads, respawns or maps. Reused addresses are rejected by identity/lifecycle checks. No actor spawning/resurrection.
- NPC acquisition remains within 20 units of the recorded target. After acquisition target-step guard and 250ms transport lease remain active. AI can oppose transform-only playback; that is measurable, not solved.
- Selected-only session still requires a live valid player for module readiness/session validation, but performs no player transform or input writes.
- Player replay still masks normalized actions experimentally. Removing conflicting debug flag writes can alter input-conflict behavior; compare against preserved Phase5. Grounding is not fixed by this removal.
- Existing game callback paths still format some rate-limited transition/debug logs and capture structures. Only the new structured trace path avoids per-frame formatting/I/O; whole-callback allocation-free operation is not claimed.
- Proxy coordinates/ground height remain unavailable. Behavior root motion is a separate SDK vector, not proven physics/Havok root motion. Debug flags are labeled unverified in JSONL.
- The speed experiment writes once, observes, then restores. Engine may overwrite it during the two-second window. If the exact character/module disappears, no stale restore is attempted. A stopped task cannot restore until the next callback; restore after destruction is impossible.
- No live UI, game FPS, NPC grounding, speed restore or transport performance verification has occurred for this nightly.
- FieldArea/W_Event/LuaWarp unique signatures are static candidates only. CSLuaEventManager legacy pattern is ambiguous. No guessed native ABI calls.
- TGA clone completed object transfer but Windows checkout failed on long paths. Relevant source was inspected with `git show HEAD:path`, without altering the reference.

- Actor finish uses the existing player-session finish gate; this diagnostic build does not guarantee a final NPC sample write/hold at the exact last timestamp. It disables NPC writes when the session finishes.
