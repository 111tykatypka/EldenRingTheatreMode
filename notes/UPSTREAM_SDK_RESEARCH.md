# Research: vswarte/fromsoftware-rs (main, commit 59fbd3b) versus our pinned SDK

Evidence labels: STATIC (read in the SDK source), LOG (seen in Theater's own logs), CE (Cheat Engine table text), UNKNOWN.
Nothing here is RUNTIME- or VISUALLY-verified in the game unless it says LOG.

Compared: `crates/eldenring/src` of upstream main with the pinned checkout (`KamiyamaShiki0704/fromsoftware-rs` rev 3c8c1d7).
Upstream has 143 source files; the pin is behind by a handful of structural changes and some new singletons (pad / key assign / mouse / msg repository).

## 1. Layout fix that explains an old surprise (STATIC + LOG)
Upstream inserts 8 bytes (`unk3b8: usize`) into `ChrIns` before `unk3f0`, so every `ChrIns` field between 0x3b8 and 0x548 moves by +8 and the struct keeps its size (inferred: the trailing `unk548[0x38]` becomes `unk550[0x30]`).
`debug_flags` therefore sits at 0x538, which is exactly what we measured earlier (the pinned SDK said 0x530). `PlayerIns` fields (0x580 and up) do not move.
Consequence: any pinned-SDK field in that window (`distance_to_player_sqr`, `update_priority`, `chr_activate_threshold`, `max_render_range`, update tasks, `debug_flags`, `stamina_recovery`, `phantom_param_override`) is 8 bytes off for this game version.
We only read `debug_flags` there, through GameProfile (correct). Fields before 0x3b8 (flags 1c4..1ca, `omission_mode`, `character_id`, `special_effect`) are unchanged and match what we read.
Recommendation: move the SDK pin to upstream main (API changes seen: `ChrDebugSpawnRequest.is_player` removed, `HandIndex` replaced by `ChrAsmHand`). Cost: small compile fixes; benefit: correct layout for all those fields.

## 2. Why a ridden Torrent has no skeleton (STATIC, matches LOG)
`CSPairAnimNode` doc: "one party is forwarding animations to a receiver. One such example is riding Torrent". `CSRideNode.ride_state`: 0 none, 3 getting on, 5 riding, 7 dismounting.
While ridden, Torrent's body is animated through the pair-animation link from the rider, which is consistent with our logs: ridden = ride flags 9 and a null pose importer; standing and summoned = `ride=None` and 123 bones.
So: a *standing* Torrent can be recorded and driven (we record 1377 horse frames with 123 bones); a *ridden* Torrent cannot be read through the importer. Mounting and dismounting animations for the horse would have to be reproduced from the rider's data or by letting the game run the pair animation itself.

## 3. The game has its own player-replay system (STATIC)
- `FieldInsType::ReplayGhost = 5` and `ReplayEnemy = 6` (ReplayGhostIns, ReplayEnemyIns).
- `WorldChrMan.ghost_chr_set`: "bloodmessage and bloodstain ghosts as well as replay ghosts".
- `ManipulatorType::Replay = 3` (also Network = 2, Ride = 6, Follow = 7) in `ChrManipulator`.
- `PlayerIns.replay_recorder: ReplayRecorder` (max_frame_rate, frame_counter, frame_duration, oldest position / rotation / block) and `NetChrSyncFlags.replay_recorder_enabled` (set on the main player).
- TAE action 20 `SEND_GHOST_INFO`; `ChrType` has BloodstainGhost, MessageGhost, BonfireGhost, Ghost.
This is the game's native "show what another player did" feature (bloodstain / message ghosts). It replays through the normal behaviour graph, so item use, weapon hiding and effects come for free there.
UNKNOWN: the recorded data format, how a ghost is created, whether it can be fed our data. A deep reverse-engineering task, but it is the only route that would reproduce flask / weapon / VFX state exactly.

## 4. Other findings that matter to us
- `CSChrTimeActModule` (the TAE player): `anim_queue[10]` of `{anim_id, play_time, anim_length}` with read / write indices. We already log the player's animation id; this is where it comes from. It is also the place an animation could be requested by id (so TAE events run). UNKNOWN whether writing the queue plays the animation (the behaviour graph normally decides).
- `ChrIns.tae_queued_use_item` (`OptionalItemId`): "Used by TAE's UseGoods to figure out what item to actually apply". LOG: set to 0x400003E9 at a Crimson Tears sip, 0x40000082 at the whistle.
- `ChrInsExt::apply_speffect(id, dont_sync)` / `remove_speffect(id)` exist (call through RVA). LOG: the whistle is special effect 81, state info 336, which matches the CE list ("336 Summon Torrent").
- State info histogram (LOG, SpEffectParam, 8384 rows): 184 ("Hide Weapon" in the CE list) does not occur in this game version, so the flask does not hide the sword with that state.
- `CSSfxImp.debug_spawn_ffx_id` / `debug_spawn_distance_from_camera`: a debug spawn of an effect by id at a distance from the camera. A possible way to play an FFX, not yet tried.
- `WorldChrMan.summon_buddy_chr_set` holds spirit ashes and Torrent; `debug_chr_set` holds debug-created characters (our stand-ins). Enumerating `debug_chr_set` directly is a cleaner way to find a freshly created stand-in than the distance list.
- `NetChrSetSync` carries placement (position, rotation) and health for remote characters only; no animation or item data there.
- `CSChrModelParamModifierModule` (a list of named modifier entries with value vectors) is a candidate home for per-character model parameters; not examined yet.
- `ChrIns.network_authority` and `NetChrSyncFlags.distance_based_network_update_authority`: how the game decides which machine runs a character's gameplay events. A replayed character that is not "authoritative" would not execute gameplay TAE events (like healing); UNKNOWN whether that can be used safely.

## 5. Ranked next experiments
1. Move the SDK pin to upstream main (layout correctness, `debug_chr_set`, new APIs). Low risk.
2. Flask: log the player's animation ids with the `ITEM_DIFF` windows, then try requesting the same animation by id through `CSChrTimeActModule` in a stand-in (not the player) to see whether TAE events (model, effects, weapon) run. If they do, decide how to keep gameplay events (healing) from applying to the real player.
3. Ghost route: create or locate a ghost in `ghost_chr_set` (a bloodstain ghost near a bloodstain) and read its manipulator and recorded data, to learn the native replay format.
4. Torrent mounting: capture the rider's pair-anim node (`ride_state`, start position) during mounting; plan to replay the mount by letting the game run it on a stand-in horse.
