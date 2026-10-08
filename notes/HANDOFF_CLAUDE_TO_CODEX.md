# Claude -> Codex handoff (branch `claude/integrate-c19`, packages up to `TheaterMode-Integration-C19-14`)

Repo: `EldenRingTheatreMode-claude`, branch `claude/integrate-c19` (= Codex C19 merged into `claude/npc-lifecycle`, pushed to origin).
Target: Elden Ring 2.7.0.0, offline only. Standing rules from the user: offsets live in `shared/GameProfile.h`; log errors; never edit live death state / HP / progression; never delete or alter inventory items; no success claims without in-game confirmation. Everything below marked UNVERIFIED has only been built and unit-tested (72 `cargo test --lib` tests pass), not confirmed in the game.

Build: `build_release.bat "<outdir>"` from the repo dir (run through a .cmd wrapper; paths contain spaces). A package folder cannot be overwritten while the game has the DLL loaded, so every build goes to a new `TheaterMode-Integration-C19-N` folder; copy Codex's `sounds` folder in afterwards. Logs: `%TEMP%\TheaterModeGame.log`.
Known pre-existing failure (also on a clean Codex C19 clone): `render-backend-tests.exe` returns 7 (camera bar overlaps the viewport in the 1280x720 test). Not touched.

## 1. Delivered and confirmed by the user in game
- Merge of Codex C19 (camera, timescale, weather, light editor) over the NPC-lifecycle work. Rule from the user: Codex features replace mine, except the core recording of player / enemies / horse and the camera-fade (near-clipping) work.
- Dynamic destructible objects no longer fade out near the camera (`adapter/src/camera_fade.rs`, AssetEnvironmentGeometryParam `cam_near_behavior_type`, ChrModelParam `camera_dither_fade_id`).
- Torrent: driven with 123/123 bones in an earlier test; whistle via SpEffect 81, stand-in horse after 3 s (see section 5 for the "appears then disappears" report).
- Weapon hides while a flask is drunk (in-hand state replay through `weapon_loc.rs`).

## 2. Features added this stretch (all UNVERIFIED in game unless stated)
### 2.1 Effect (particle) capture and replay -- the main open problem
Files: `native_ui/EldenRingEffectAdapter.cpp` (new), `adapter/src/effects.rs` (new), `adapter/src/world_file.rs` (track 10 / kind 12 `EffectEvent{time:u64,id:u32,pos:[f32;3]}`), `adapter/src/bone_replay.rs` (capture drain + replay), `shared/GameProfile.h`, `CMakeLists.txt`, `native_ui/theater/TheaterOverlayUI.cpp` + `TheaterStrings.h` (Settings checkbox, option bit 256 = replay effects DISABLED; on by default).
- Hook: MinHook on RVA `0xDA93E0` (`FUN_140da93e0`), guarded by its first 24 bytes (`TM_EFFECT_SPAWN_BYTES`). Signature as used: `(manager, effect_id, tag, float* position, void** out)`; manager pointer is the global at RVA `0x3D87D48`. The only static caller is wrapper `FUN_140b86e30(ctx, id, p3, p4, out, pos)`, which zeroes `*out` and calls `DA93E0(manager,id,p3,pos)` with four arguments.
- Capture: while recording, every call queues `{interrupt-time ns, id, pos}` (max 8192 pending, SEH-guarded read); the Rust side drains it each frame into `Message::Effects` chunks. Replay-made calls are flagged by a thread_local so they are never re-captured.
- Replay: in the WRITE group after the actors, effects due at the replay time are created through the same function; the position gets the player-anchor translation (zero when there are no origin shifts, same rule as actors). After a seek or a jump > 0.5 s the cursor is only moved (no burst); playing on re-creates them, which gives the "reappear after rewind" behaviour.
- Time base: frames and effects both use the game's absolute monotonic clock (replay times like 319137 s). An intermediate build (C19-8) wrongly stored times relative to the recording start; files saved that way are detected on load (`last effect time < first frame time`) and shifted.
- Save bug fixed: effects arrive from several game threads, so capture order is not strictly time order; the decoder used to reject unordered chunks ("invalid effect payload", recording failed to save). Now sorted on drain and on load; non-finite positions dropped.
- RESULT SO FAR: the log shows the replay calling the function at the right moments, at positions within ~1.5 m of the replayed player, `called=true` every time, and the user hears sounds -- but **sees no particles**. Not yet determined whether (a) the game creates the effects but they are not drawn, or (b) they are dropped immediately. C19-14 logs the live effect count (`item_probe::sfx_total()`, WorldSfxMan block list at +0x30, count at +0x28, stride 0x78, per-block total at +0x5C) before/after each created effect (`EFFECT_REPLAY ... effects-alive A->B`). If A->B rises, it is a rendering/visibility problem; if flat, the call is dropped (likely the way arguments / the out handle / the unknown 3rd argument are used).
- Recorded ids (BOBA FETT test): 450282, 450284, 101141, 101142, 480310, 480320, 800025, 6210, 900080000. By the soulsmodding id scheme (1xxxxx footsteps/materials, 2xxxxx attack sparks, 4xxxxx weapons, 8xxxxx assets/environment) these are mostly weapon / footstep / environment effects; no clear blood. Blood and many hit effects are probably attached to characters (a different creation path) and are not seen by this hook.
- Ideas for Codex: confirm what `DA93E0` really needs (decompiled in `research/ghidra-eldenring/export/decompiled/functions_00177.c`; it builds `Sfx_s%09d` instance and registers it with the manager at `param_1+0xb0`/`+0x238`); try the wrapper `FUN_140b86e30`; look for the attached-effect path (dummy-poly FFX from TAE events), and the SpEffect `vfx[]` ids visible in the `SPEFFECT_PROBE` log lines.
- Rejected approach: a file-triggered "spawn test effect" hook was refused by the safety review and deleted. Any test trigger must be an explicit user action (overlay button).

### 2.2 Player weapon swap replay (UNVERIFIED)
`adapter/src/equipment.rs::write_selection` (default on; option bit 128 disables; bit 16 = old full write, opt-in). Earlier it wrote only arm style and selected slots, which only works if the swapped-to weapon is in the live loadout. Now it also writes the recorded handles and param ids of slots 0..11 (weapon, arrow, bolt slots); armor / talismans (12..21) stay live. The user's own assembly is restored when the replay ends. Codex's `equipment.rs` writes the entire record every frame and the user reports that this shows the right weapon; if write_selection still shows the old model, switch to Codex's full write (note: the user once reported armor loss with a full write).

### 2.3 Near-fade removal (confirmed for destructible objects)
`adapter/src/camera_fade.rs`: clears camera near-fade on asset geometry, model dither fade, CameraFadeParam rows, grass dithering, and sets SpeedTree min fade (experimental). Applied when the Theater is linked and WorldChrMan exists, retried every 3 s, per-table `catch_unwind`, restored exactly on exit. `adapter/src/foliage.rs` (Codex) was replaced by an inert stub. Foliage (trees, grass, bushes) was still reported fading; the speedtree guess is untested. Logs: `CAMERA_FADE:` lines.

### 2.4 Other
- Corpse hiding, companion handling (Torrent never hidden), AI freeze of non-replay characters, puppets via debug creator, update-LOD (omission) override, all in `adapter/src/actors.rs` / `omission.rs`.
- Documentation: `notes/INTEGRATION_C19.md`, `notes/DEBUG_TOOLS_RESEARCH.md`, `notes/UPSTREAM_SDK_RESEARCH.md`.

## 3. Open problems and what I need / tried
| Problem | State |
|---|---|
| Particles invisible | See 2.1. Needs the C19-14 effect-count log. |
| Hit / blood / attached effects | Not captured at all (different creation path). |
| Hit sounds | Not captured. The sounds the user hears come from the game's own animation events. No Wwise stop/play function found. |
| Sound loops forever | Keeps playing after unloading the replay, so the loop is inside the game's audio, not replay state. A "restart sound" button was requested; no safe game call known. Need the trigger (spell? roar? animation?). |
| Enemy weapon swap | Enemies keep the same weapon model while playing the swap animation (bow in hand with sword animation). Where the NPC's current weapon is stored is unknown. C19-9 logs `NPC_MODEL_WATCH` / `NPC_MODEL_DIFF` (heap objects hanging off each enemy's `chr_model_ins`, two levels, 4 Hz, up to 10 enemies within 25 m) in `item_probe::model_pointers` / `actors.rs`. Needs a recording where an enemy visibly swaps weapons. |
| Enemy corpses frozen (should ragdoll) | The replay only drives the animation pose of a living body (death state is never written), so no ragdoll. Plan agreed with the user: record the real ragdoll bone movement. C19-11 logs `RAGDOLL_WATCH`, `RAGDOLL_STATE`, `RAGDOLL_POSE_MOVING` using `ChrIns.chr_ctrl` -> `ChrCtrl.chr_ragdoll_state` (+0x128) and `ragdoll_ins` (+0x28). Next step depends on whether the recorded pose array keeps changing after death. Needs a recording of an enemy dying. |
| Flask model not visible when drinking | Hand-model attachment not found. `ITEM_DIFF` / `ITEM_PTR` probes in `item_probe.rs` need a fresh flask-sip recording. |
| Enemies without weapons in hand | Cause unknown; `ACTOR_MASK` probe (16 bytes at ChrIns+0x2C0) logs per actor. |
| Foliage still fades | See 2.3. |
| Horse appeared, then vanished | Companion logic reworked (never hidden, whistle once, stand-in); untested since. |

## 4. Log keywords to grep
`EFFECT_HOOK`, `EFFECTS:`, `EFFECTS_IN_FILE`, `EFFECT_REPLAY`, `WORLD_FILE_ERROR`, `RAGDOLL_`, `NPC_MODEL_`, `ITEM_DIFF`, `ITEM_PTR`, `ITEM_PROBE`, `SFX_COUNT`, `CAMERA_FADE`, `ACTOR_LIFETIME`, `CORPSE_HIDDEN`, `EQUIPMENT:`.

## 5. Where the code is
- Effects: `native_ui/EldenRingEffectAdapter.cpp`, `adapter/src/effects.rs`, `adapter/src/world_file.rs` (EffectEvent, tests `effect_events_roundtrip_and_reject_garbage`), `adapter/src/bone_replay.rs` (search `Recorded one-shot effects`, `fx_relative`).
- Research probes: `adapter/src/item_probe.rs` (`model_pointers`, `ragdoll_info`, `sfx_total`), `adapter/src/actors.rs` (search `NPC_MODEL_DIFF`, `RAGDOLL_`).
- Equipment: `adapter/src/equipment.rs`. Fade: `adapter/src/camera_fade.rs`.
- UI: `native_ui/theater/TheaterOverlayUI.cpp` Settings section, strings in `TheaterStrings.h` (enum order must match table order; Russian strings need real UTF-8).
- Ghidra export: `research/ghidra-eldenring/export` (decompiled/*.c, calls.jsonl, functions.jsonl). Debug tools: `debug tools/`. Upstream SDK clone: scratchpad `fsrs` (vswarte/fromsoftware-rs).
- Soulsmodding wiki (`er-refmat`) was reviewed: mostly spreadsheet links and empty stubs; only the FFX id numbering scheme was useful.
