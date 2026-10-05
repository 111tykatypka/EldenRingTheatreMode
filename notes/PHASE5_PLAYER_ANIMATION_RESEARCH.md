# Player animation/action research — exact 2.7.0.0 checkpoint

Date: 2026-10-05. Pinned `KamiyamaShiki0704/fromsoftware-rs` revision **3c8c1d7633a99309fb004c9f894ea10b7967d0e0** (Cargo.toml/lock preserved).
Source root: `C:\Users\user\.cargo\git\checkouts\fromsoftware-rs-7e356ae17759562a\3c8c1d7`.
All entries below use this exact revision. Thread requirement: **YES** for every engine read/write. CONFIRMED means source/layout/name evidence, not a claim of live animation correctness.

## Source and safety matrix

| Name / structure | Source under crates/eldenring/src | Read safe | Write safe | Confidence / usefulness |
| --- | --- | --- | --- | --- |
| PlayerIns::local_player_mut / local_player | cs/chr_ins.rs | YES with lifetime/thread preconditions | YES for obtaining access, not every child field | CONFIRMED; reacquires WorldChrMan/main_player. Existing transform path was tested live. |
| ChrIns.modules.time_act | cs/chr_ins/module.rs | UNKNOWN until live new-field validation | UNKNOWN | CONFIRMED binding; no cached reference. |
| CSChrTimeActModule.anim_queue / read_idx | cs/chr_ins/module/time_act.rs | YES structurally with read_idx <10; new live observation unverified | NO for this implementation | HIGH CONFIDENCE as current/last animation: exact-revision debug UI uses anim_queue[read_idx] as Current Anim Info. |
| CSChrTimeActModuleAnim.anim_id | same | YES under bounds/lifecycle guards | UNKNOWN | CONFIRMED raw ID; good recording candidate. Not a universal semantic action enum. |
| play_time / anim_length | same | YES if finite, length>0, time>=0 | UNKNOWN | CONFIRMED raw seconds/length candidates; normalization/context require live observation. No arbitrary phase writes. |
| CSChrBehaviorModule.animation_speed | cs/chr_ins/module/behavior.rs | YES if finite and plausible | UNKNOWN | CONFIRMED field existence; multiplier/base-rate semantics not documented enough to promise sync. |
| CSChrBehaviorModule.root_motion | same | YES structurally | UNKNOWN | CONFIRMED vector; useful diagnostic, unsafe to zero blindly. |
| CSChrBehaviorDataModule.hks_root_motion_mult | cs/chr_ins/module/behavior_data.rs | YES structurally | UNKNOWN | CONFIRMED HKS-owned multiplier; no forced write. |
| hks_animation_speed_multiplier | same | YES structurally | UNKNOWN | CONFIRMED; ownership/update timing unresolved. |
| CSChrActionRequestModule.action_requests | cs/chr_ins/module/action_request.rs | YES | UNKNOWN | CONFIRMED requested inputs, NOT proof the action actually executed. Capture separately from animation ID. |
| movement_request_flags.raw_input / dash | same | YES | UNKNOWN | CONFIRMED raw move/dash request semantics. Not sufficient to label actual Walk/Run/Sprint. |
| action_request_queue.current_tae_id | same | YES structurally | UNKNOWN | CONFIRMED queue context; not generic PlayerIns current action. |
| npc_action_id | same | YES | NO for local-player action replay | CONFIRMED NPC EzState request, often -1; do not present it as player's current action. |
| CSChrEventModule.request_animation_id | cs/chr_ins/module/event.rs | YES | UNKNOWN; EXPERIMENTAL prototype only | Source documents override animation requested for next frame. Candidate exact-ID replay transition, requires namespace/loop/live verification. |
| CSChrEventModule.idle_anim_id | same | YES | UNKNOWN | CONFIRMED default idle ID; useful contextual comparison, no guessed locomotion table. |
| ChrIns.debug_flags.disabled_movement / disabled_secondary_actions | cs/chr_ins.rs | YES | EXPERIMENTAL scoped lease | CONFIRMED local input suppression semantics; save only owned bits, restore same handle on game callback. |
| ChrCtrl.chr_proxy_flags.position_sync_requested / rotation_sync_requested | cs/chr_ins.rs | YES | UNKNOWN until demonstrated need | CONFIRMED copy-physics-to-Havok request bits. Not enabled in this checkpoint. |
| CSChrPhysicsModule.is_falling / standing_on_solid_ground | cs/chr_ins/module/physics.rs | YES structurally | UNKNOWN | CONFIRMED physics observations. Falling is not a unique fall animation; no fabricated Land event from guessed timings. |
| CSChrFallModule.fall_timer | cs/chr_ins/module/fall.rs | YES if finite | UNKNOWN | CONFIRMED; fall context only, no jump/roll semantic mapping. |
| PlayerIns.replay_recorder | cs/chr_ins.rs | UNKNOWN | UNKNOWN | Existing engine type mainly documents oldest-frame transform; does not prove a usable cinematic/action API. Do not replace our replay architecture. |
| ChrCtrl.animation_ctrl / PlayerIns.chr_manipulator | cs/chr_ins.rs | NO public typed API | NO | UNKNOWN internal usize/private fields. No offsets/casts/invented vtables. |

Public exact sources: [TimeAct](https://github.com/KamiyamaShiki0704/fromsoftware-rs/blob/3c8c1d7633a99309fb004c9f894ea10b7967d0e0/crates/eldenring/src/cs/chr_ins/module/time_act.rs), [Event](https://github.com/KamiyamaShiki0704/fromsoftware-rs/blob/3c8c1d7633a99309fb004c9f894ea10b7967d0e0/crates/eldenring/src/cs/chr_ins/module/event.rs), [ActionRequest](https://github.com/KamiyamaShiki0704/fromsoftware-rs/blob/3c8c1d7633a99309fb004c9f894ea10b7967d0e0/crates/eldenring/src/cs/chr_ins/module/action_request.rs).

## Three playback strategies

A. Exact animation override: use observed raw ID, request once per transition through the public event field. Most compact candidate; may be rejected, interrupted, or use a different ID namespace. It does not restore Havok state, blends or arbitrary phase. **Experimental**.

B. Recreate HKS/action state: public action-request bits describe requests/cancel queues, not a complete HKS state machine. No current typed, verified 2.7.0.0 HKS playback API was found. **Unknown / not implemented**. An old SDK or velocity thresholds do not establish this API.

C. Hybrid: authoritative sampled transforms + observed animation transition track + bounded per-callback transform interpolation; request exact animation only on changes. This is the recommended **experimental foundation**, retaining raw observation and provenance. Root motion remains monitored separately.

## Normalized model and evidence limits

Enum supports Unknown, Idle, Walk, Run, Sprint, Turn, Jump, Fall, Land, Roll, Backstep. A reliable universal ID table for those actions on this character/equipment/2.7.0.0 was not found. Raw animation and request bits remain separate. Do not derive recorded Walk/Run/Sprint from velocity or dash request alone. Use Unknown until a runtime trace establishes the mapping; matching the runtime-reported default idle ID may be annotated Idle, with that provenance explicitly recorded. Unknown semantics do not discard a validated raw animation ID.

New files record bounded, finite raw animation observations and input-request context. Changes are sparse events; unchanged action/ID is not retriggered every frame. Periodic time observations are separate from claiming exact seek/freeze. Old v2 files have no action track and will never gain fabricated animation.

Before claiming locomotion support: record idle/walk/run/sprint/roll/jump sequence; inspect raw IDs/times, confirm actual animation transition and no input/root-motion conflict; then annotate semantic mapping. Readable fields and successful compilation do not complete that live experiment.
