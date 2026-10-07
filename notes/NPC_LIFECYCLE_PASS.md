# NPC / enemy / boss lifecycle pass (branch `claude/npc-lifecycle`, base `codex/p2e-lifecycle-research` 194d68f)

Evidence labels: STATIC_VERIFIED, COMPILE_VERIFIED, RUNTIME_VERIFIED, VISUALLY_VERIFIED, FAILED, UNKNOWN.
**Nothing in this pass is RUNTIME or VISUALLY verified. NPC replay is NOT complete** until the acceptance tests below
pass in the game. Package: `outputs\EldenRingTheaterMode\TheaterMode-NpcLifecycle1`.

## 1. Current NPC pipeline (audited, with code references)
| Stage | What happens | Where |
|---|---|---|
| Discovery | `WorldChrMan.chr_inses_by_distance` plus buddy-set bodies (`companions::buddies`), minus the main player, deduplicated; radius `THEATER_ACTOR_RADIUS` (default 100 m, 0 = all). Only loaded bodies are visible to it. | `actors.rs live_snapshot`, `Recorder::sample` |
| Acceptance | Needs a readable skeleton (three bone counts agree), pose arrays and a root; otherwise skipped with one warning. | `actors.rs bone_count/pose_arrays` |
| Stable id | Per-recording counter keyed by (FieldInsHandle, event_entity_id, npc_param_id); a gap of more than 500 ms issues a new id. Raw handles are provenance only. | `Recorder::sample` |
| Root + skeleton | Root transform, origin/anchor metadata, full local + model pose, HP; lossless float words; a skeleton identity record (model, parents, fingerprint). | `world_file.rs` ERWORLD v2, `skeleton.rs` |
| Lifetime observations | One-second refresh plus state changes: death flag (SDK `chr_flags1c5` bit 7), render flag, HP, backread/cleanup, pose availability. | `actor_lifetime.rs`, track 7 kind 9 |
| File read | Chunked, CRC-checked; ordering and identity validated on save and on open. | `world_file.rs open / validate_saved_world` |
| Playback lookup | Every 0.5 s: match a recorded actor to a live body by entity id + NpcParam, or by handle; a unique companion role as fallback. | `Player::find` |
| Pose application | PrePhysicsSafe, interpolated, hierarchy-correct, no-move/no-attack flags, gravity off. | `Player::write` |
| Missing live body | **Before this pass:** log `ACTOR_RECONSTRUCTION_REQUIRED` and skip; nothing shown. | |
| Enemy dies | **Before:** a recorded death flag that differed from the live flag released the body, so a body that was dead in the game could never show the earlier recorded state. | |
| After despawn | **Before:** no live body, so nothing. | |
| Backward seek | **Before:** poses seeked correctly (one chunk decode) but existence did not: a dead or despawned live enemy stayed dead or absent. | |

## 2. Death lifecycle (what is actually known)
STATIC_VERIFIED (SDK): `ChrIns.chr_flags1c5` bit 7 = death_flag, bit 3 = enable_render, bit 4 = is_invincible;
`chr_flags1c6` bits 0/1 = has_dropped_item/runes (prevent double rewards); `CSChrDataModule.hp`; `ChrIns.backread_state` and
`chr_set_cleanup`; `ChrCtrl.chr_ragdoll_state`. In Codex's "Enemy test" recording, 5 of 51 actors went from positive HP to 0;
all kept the death flag and render flag and had pose samples after HP 0.
UNKNOWN: when the death animation ends, when ragdoll starts, when the corpse is cleaned up. The recording therefore derives
only: first observation (spawn), first death-flagged observation (DeathStart), and the terminal "not observed" record
(vanish). A vanish after a death flag is classified `Gone`; a vanish while alive is `Left` (radius or unload), not a despawn.

## 3. Cheat Engine boss revive (exact)
STATIC_VERIFIED, read from `eldenring_all-in-one_Hexinton-v8.0.4.CT` without executing it:
`[ Enable ]/[ Scripts ]/[ Progression ]/Revive/Kill All Bosses (Base Game only)/Revive/Kill ALL Bosses`, CE ID 1337309787,
author "Zei". Its parent shows a warning that it sets hundreds of flags and changes progression. Enable switches on two
helper records, then performs 174 four-byte writes of 0 at `[[EventFlagMan]+28]+<offset>`; Disable writes 255 to the same
places. It only changes event-flag storage: no actor factory call, no HP/AI/skeleton restore and no forced map reload in the
script itself. The game's own map scripts recreate bosses when their area is next loaded (LIKELY, not proven). It changes save
progression and autosave would persist it. `Spawn Debug Character` (ID 1987705439) sets `[[WorldChrMan]+1E648]+44` = 1, the
same creator request as the SDK's `spawn_debug_character`. Conclusion: the flag route is rejected for Theater (save risk); the
creator request is the only native construction path with public evidence. The table targets 2.7.1.0, not our 2.7.0.0.

## 4. Reconstruction strategies
| Strategy | Finding | Decision |
|---|---|---|
| A. Native revive | No safe way known to clear death state/HP/behavior; the flag route changes progression. UNKNOWN for bosses. | Not used. A live body's death state or HP is never edited. |
| B. Native respawn | Public path: debug character creator request (asynchronous; the new body is found through `last_created_chr`). No constructor or destructor is called by Theater. | Used only through that request, opt-in. |
| C. Replay puppet | Cleanest for dead bosses and new sessions; needs a body that renders with the recorded model. | Realised through B + D. |
| D. Hybrid | Native body from the creator + Theater-owned pose, root, visibility and lifetime + AI held. | **Recommended and implemented (experimental, off by default).** |

Puppets use entity id 0, so no map script, reward or progression is tied to them. They are made invincible and pre-marked as
having dropped items and runes; Theater never kills them. Unload uses the SDK's documented `force_unloaded` debug flag;
whether that fully removes a debug-created body is UNVERIFIED.

## 5. AI isolation
COMPILE_VERIFIED mechanism: debug no-move + no-attack flags (offset +0x538, written only after the +0x530 callback check),
gravity off, pose and root overwritten every frame at PrePhysicsSafe. New: held live bodies and puppets are invincible (the
live body's own bit is restored afterwards). Remaining risks: targeting, TAE-driven damage/SFX, boss state machines and scripts
are not proven isolated; a puppet may act for a frame or two before its first hold.

## 6. Actor lifetime system
`Timeline::existence(id, T)` (pure, unit-tested): `NotYet / Alive / Dead / Gone / Left / Unknown`, derived from observations
only, independent of seek direction (binary search, no cursor). `plan(existence, live body)` decides `Drive | Puppet |
Absent | Unavailable`; the live game and the save never decide existence. The unit test reproduces the owner's timeline (alive
0-12 s, death 12 s, corpse, gone 30 s): 5 s Alive, 13 s Dead, 22 s Dead, 22 -> 8 Alive again, 8 -> 13 Dead again, repeated
three times. Diagnostics: `ACTOR_LIFETIME: id -> state at T` on every transition; `ACTOR_DIAG` every 5 s with representation
(Live/Puppet), existence and AI-isolation status; `PUPPET_*` and `ACTOR_RECONSTRUCTION_UNAVAILABLE` lines.
Not implemented yet: periodic world/actor snapshots with event deltas (seeks decode one chunk instead), boss phase and model
epochs, hiding a live body for an actor that does not exist yet.

## 7. File changes (ERWORLD v2, backward compatible)
New track 8 / kind 10 `ActorMeta` (character id, npc id, NpcParam, think param, type, category) per actor: pointer-free,
skippable, validated against the catalog. Older files open as before; puppets are then unavailable for them.
See docs/ERWORLD_V2_FORMAT.md.

## 8. Tests
| Test | Result |
|---|---|
| Lifetime derivation, existence at T, repeated backward/forward seek (data level) | COMPILE_VERIFIED (6 new tests) |
| Planner: recorded alive + dead/missing live body -> puppet, never revive; dead -> drive; absent states | COMPILE_VERIFIED |
| Whole suite | 56 Rust tests pass (1 ignored fixture); 12/12 host tests pass |
| Ordinary enemy in game: alive, moves, attacks, hit, dies; seek back -> alive; forward -> dead; repeat | UNKNOWN (needs the checklist below) |
| Killed enemy, then seek to before death (second acceptance) | UNKNOWN |
| New session (third acceptance) | UNKNOWN; needs same-area arrival, still blocked by origin conversion |
| Boss | Not attempted (spec order) |
| Missing live actor does not crash | Designed (log and continue); UNKNOWN in game |

## 9. Git
Branch `claude/npc-lifecycle` from `codex/p2e-lifecycle-research` 194d68f. Files: actor_lifetime.rs, actors.rs, world_file.rs,
bone_replay.rs (wiring), native_ui theater settings (puppet checkbox), docs/ERWORLD_V2_FORMAT.md, this note, unit-test runner.

## In-game test (ordinary enemy)
1. Start from the NpcLifecycle1 package. Settings > Replay world: tick "Replay puppets (experimental)".
2. Pick ONE ordinary enemy. Press F5, let it move, hit it, kill it, F6. Stand at one spot near it while recording; the
   coordinate-origin check still blocks playback from a different map tile.
3. Stay in the game (the enemy is now dead), load the replay, press Play: expect the enemy alive, then dying at the recorded time.
4. Seek to just after the death, then back before it: expect it alive again, repeatedly.
5. Send `%TEMP%\TheaterModeGame.log`: the `ACTOR_LIFETIME`, `PUPPET_*`, `ACTOR_DIAG` and `ACTOR_RECONSTRUCTION_UNAVAILABLE`
   lines show exactly what happened. After stopping, tell me if a stand-in enemy is left standing (unload is UNVERIFIED).

## Coordinate origin fix (REPLAY BLOCKED: different coordinate origin)
Cause: playback demanded that the recording's tile anchor (`ChrIns.chunk_position`) equal the live one to 1 cm, but the anchor
changes every time the game re-bases the physics origin (every ~32 m of running), so a replay could only play from its exact
starting tile. Measured on a real recording (`tools/dump_player_root.py`, `213.erplay.world`): at the frame where the player's
anchor x went -48 -> -80, the player's physics x went 32.09 -> 0.15 (a jump of -31.94 ~ the anchor change), and
`physics - anchor` stayed continuous (80.09 -> 80.15). Every recorded enemy's physics x jumped by exactly -32.00 at the same frame.
Therefore: `live_physics = recorded_physics + (live_anchor - recorded_anchor)` (STATIC_VERIFIED on recorded data; tests encode the
real frame). Playback now rebases player and enemy positions this way (enemies with the PLAYER's anchor at the sample time,
because their own chunk fields are unrelated per-character values), interpolates in the live frame so a re-base between two
samples is not a 32 m jump, and verifies arrival by comparing live physics with the converted target. Only a different
origin id still blocks (no known conversion). Grace travel is now chosen by map area only; anchors are not distances.
RUNTIME: UNKNOWN until you play a replay from a different tile.
