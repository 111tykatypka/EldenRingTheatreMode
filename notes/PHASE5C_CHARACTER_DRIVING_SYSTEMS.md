# Phase5C — character driving systems

2026-10-05; branch `phase4-in-game-replay-prototype`; starting code `dbcc315`.

## Evidence rules

[CONFIRMED] is source or observed log evidence. It does **not** mean a newly inspected field is runtime validated. [HIGH CONFIDENCE] is a strong interpretation of that evidence. [LIKELY] is a hypothesis. [UNKNOWN] is an unresolved boundary. User feedback now confirms smooth in-game transform movement, rotation and full playback, but **sliding instead of correct locomotion**. WALK/RUN/SPRINT driving remains unverified.

Runtime target remains AMD64 EldenRing_1_17 / 2.7.0.0, original profile/hash guard and task-registration signature. No new offsets, casts of manipulator addresses, dependency changes, camera, NPC playback or event-script modification.

## Exact sources inspected

Pinned fromsoftware-rs **3c8c1d7633a99309fb004c9f894ea10b7967d0e0**, verified from the checkout HEAD. Source prefix `crates/eldenring/src/`:

| Source | What it establishes |
|---|---|
| `cs/chr_ins.rs:145,231,322,492,567,697,876,989` | ChrCtrl, network flags, recorder task, movement limit/modifier, PlayerIns recorder, partial recorder layout |
| `cs/chr_manipulator.rs:8,42` | Separate Pad/Network/Replay/NetAi/Com/Ride/Follow manipulator kinds; vectors have private and explicitly uncertain meanings |
| `cs/chr_ins/module/action_request.rs:9,95` | PreBehavior input/cancel processing; raw movement and dash flags |
| `cs/chr_ins/module/behavior.rs` | Root motion, ground touch and animation rate; behavior graph internals opaque |
| `cs/chr_ins/module/behavior_data.rs` | HKS root-motion/rate multipliers and turn speed |
| `cs/chr_ins/module/physics.rs` | Position, quaternion, ground/fall state and motion multiplier; Havok proxy/internal velocity opaque |
| `cs/chr_ins/module/event.rs` | Next-frame animation override request and idle animation ID |
| `cs/net_chr_sync.rs` | Per-ChrSet incoming placement/health buffers; no exposed locomotion packet |
| `cs/world_chr_man.rs:42,412` | Ghost ChrSet; Local=0/Remote=4 update types, three other kinds unnamed |
| `cs/net_man.rs` | Ghost/bloodstain databases opaque; update-task comment describes spawning ghost replays |
| `cs/event_man.rs`, `cs/lua_event_man.rs` | Private bloodstain/cutscene-warp controllers; typed Lua event manager is an event dispatcher, not a public player HKS VM |
| `cs/task.rs:294,347,350,389` | MovieStep, PreBehavior, HavokBehavior and PostPhysics task groups |

These are directly inspectable in the [pinned upstream tree](https://github.com/KamiyamaShiki0704/fromsoftware-rs/tree/3c8c1d7633a99309fb004c9f894ea10b7967d0e0/crates/eldenring/src).

Read-only references searched: libER, fromsoftware-rs, EldenRing-SDK, ELDENRING-INTERNAL, EldenRingMods, ModLoader, ERSoundBankLoader and ERGparamPreloadPatch. No safe compatible graph-variable setter or replay-frame decoder was found there. libER's task/symbol infrastructure is useful but does not supply this missing character driver. The old SDK names CSPadManipulator, but its class is effectively only a vtable stub; its old offsets are not adopted. Existing camera-reference notes/binaries establish camera ownership separation, not gait playback or access to HKS graph variables.

Public [c0000.hks](https://github.com/soulsmods/EldenRingHKS/blob/main/c0000.hks), inspected on this date: changelog names 1.17, while an earlier header still names 1.15.1. This is a maintained decompile, **not verified identical to installed game data**. Its `MoveStart` checks MoveSpeedLevel; `SpeedUpdate` chooses MoveSpeedIndex using analog level, stamina, equipment and effects. Normal branches distinguish low input, input above about 0.6, and sprint above about 1.1, with hysteresis/overrides. Directional movement and upper/lower graph blending are separate. HKS fires behavior events, updates smoothed speed and applies motion controls. Event-animation requests have a separate conditional path. **Inference:** a gait needs graph state and parameters, not a universal clip ID.

[Vanilla event-script examples](https://github.com/Grimrukh/soulstruct-vanilla/blob/main/eldenring/events/m41_01_00_00.evs.py) demonstrate ForceAnimation/AI/character control instructions. These are offline script descriptions; they do not expose a callable 2.7.0.0 C ABI. No functions are invoked from those descriptions.

## LOCAL PLAYER

**[CONFIRMED]** PlayerIns is reacquired through WorldChrMan. ChrCtrl and the action-request module are present. Requests are pad input/queues, not executed-action state. Debug disabled_movement suppresses movement inputs; disabled_secondary_actions suppresses other actions. TheaterMode currently owns those two flags during transform replay and restores its saved bits.

**[HIGH CONFIDENCE]** Normal driving uses manipulator inputs → action/locomotion state → HKS/behavior graph → animation/root motion → physics/model matrices. Transform override at PostPhysics does not create earlier graph inputs. ChrCtrl physics/model matrices describe placement, not gait selectors.

Read: selected public typed fields on the game callback under binding/version/lifetime preconditions; new fields need live validation. Drive: verified transform write only. Neither public movement-limit enum nor animation rate is a documented command to enter WALK. Changing a limit caps movement; it does not supply a movement target. Direct root-motion writes cannot be assumed to select a walk clip.

## REMOTE PLAYER

**[CONFIRMED]** Network manipulator kind and Remote update type exist. NetChrSync has placement/health readback arrays and update flags. Placement is position plus Euler rotation, distinct from TheaterMode quaternion transport. A ChrIns comment attributes received-position state to NetAIManipulator. This does not mean Network and NetAI are interchangeable.

**[HIGH CONFIDENCE]** Replicated control is consumed by character controller/behavior layers. **[UNKNOWN]** Exact action, analog, gait and blend payloads; which task consumes each; interpolation/root-motion ownership. Public placement updates alone cannot explain walking.

Read: local public buffers only if capacity/lifecycle validated; this phase does not dereference remote arrays. Drive: no safe remote-driver adapter yet. Do not switch local ChrUpdateType or repurpose networking just to animate it.

## GHOST / REPLAY PLAYER

**[CONFIRMED]** Replay manipulator type=3, ghost ChrSet (bloodmessage/bloodstain/replay ghosts), replay-related FieldIns types and PlayerIns recorder are named. Character types distinguish bloodstain/bonfire ghosts. SEND_GHOST_INFO is a TAE-owned request flag. Ghost-spawning tasks are described by the binding.

**[HIGH CONFIDENCE]** There is a distinct native recording/playback pipeline. **[UNKNOWN]** Serialized frames, action codec, equipment payload, interpolation, replay manipulator input construction and how it drives HKS/behavior. Ghost playback is evidence that a character can animate without local keyboard input; it is not proof we can safely call it.

### Native ReplayRecorder: exact known boundary

Exposed fields: vtable, owning PlayerIns, max_frame_rate, frame_counter, frame_duration, oldest-frame position, oldest-frame scalar rotation, block ID. Five `usize` fields before the counters and four trailing integers remain private/unknown. The update task is private; main-player recorder-enabled flag is public.

No typed frame array, action state, animation state or playback function is exposed. Therefore **we cannot answer that it records only transforms**, nor that it records animation. Counters' units/storage capacity are not sufficiently documented. Relationship to bloodstains/wandering ghosts is [LIKELY], supported by surrounding types, not a demonstrated data-flow trace. Relationship to real-time online synchronization is [UNKNOWN].

The new diagnostic copies recorder presence, counters and oldest transform only. It neither edits recorder flags nor dereferences unknown payloads. Next native-replay research needs the recorder update/consumer implementations or a verified frame schema, not a raw memory dump presented as a format.

## CUTSCENE CHARACTER

**[CONFIRMED, USER OBSERVATION]** Actors/choreography continue in-world while an external camera detaches. **[CONFIRMED, SOURCE]** MovieStep and private event cutscene-warp control exist; event scripts contain character-animation control. LuaEventMan has event-proxy/control machinery but does not expose the player HKS behavior instance.

**[LIKELY]** Script/movie controllers own choreography and coordinate animation/placement through normal character layers. **[UNKNOWN]** Exact controller handoff, global/local input gates, animation ownership and restoration sequence for this executable. Camera detachment cannot establish those details. Native cutscene input lock is not proven equivalent to debug movement suppression. No random cutscene activation is implemented.

Read: public typed event state can be investigated separately; private controller pointers remain opaque. Drive: no safe cutscene character ABI available here.

## NPC / SCRIPTED CHARACTER

**[CONFIRMED]** Com/NetAi/Follow manipulator kinds, EnemyIns controller placeholders, NPC action ID from AI/EzState, and HKS motion/turn controls exist. These are different from local player pad requests.

**[HIGH CONFIDENCE]** AI or scripts supply desired action/movement; shared character behavior/physics layers animate and place models. **[UNKNOWN]** Typed analog/desired-velocity producer and universal action injection API. NPC action_id is not a local-player locomotion enum. This phase does not touch NPCs.

## Comparison

| System | Transform owner | Animation/action owner | Locomotion inputs | Root motion | Safe driver now |
|---|---|---|---|---|---|
| Local | Physics/controller; our PostPhysics replay verified | HKS + graph [HIGH CONFIDENCE] | Pad/action flags known; analog/graph API missing | Public behavior output; private Havok integration | Transform YES; gait UNKNOWN |
| Remote | Network placement → controller [HIGH CONFIDENCE] | Replicated requests → graph [LIKELY] | Placement/health known; action/gait codec UNKNOWN | Shared character path [LIKELY] | NO |
| Ghost | Replay manipulator [HIGH CONFIDENCE] | Native replay → graph [LIKELY] | Frame payload UNKNOWN | Shared path [LIKELY] | NO |
| Cutscene | Movie/script choreography [LIKELY] | Event/movie controller [LIKELY] | Script semantics known, runtime ABI UNKNOWN | Authored motion + controller [LIKELY] | NO |
| NPC | AI/Com/NetAi [HIGH CONFIDENCE] | AI action → HKS/graph [HIGH CONFIDENCE] | Request IDs/limits known, desired movement opaque | Shared output fields [CONFIRMED] | NO |

## Why previous raw ID playback did not establish locomotion

**[CONFIRMED]** Code writes event.request_animation_id once per observed ID change. It does not set MoveSpeedLevel/MoveSpeedIndex, graph event/state, analog direction/magnitude or root-motion timing. Input lock is still active. User sees sliding.

The current local game log contains transform playback at roughly 120 Hz IPC/60 Hz callbacks and raw_requests=0 while transforms change. It contains **no REPLAY_ANIMATION_REQUEST entries** for this session. Thus it does not even prove the opt-in handler ran; checkbox/loaded-file/action-track state must not be guessed. The previous observation is a failed visible milestone, not a proven engine rejection of a particular ID.

Ranked hypotheses:

1. **[HIGH CONFIDENCE]** Graph locomotion inputs are absent; transform movement alone supplies none. Test: normal Idle/Walk/Run/Sprint trace comparing movement flags, root motion and observed animation.
2. **[LIKELY]** Movement debug lock suppresses a necessary producer/transition. Test after identifying producer: suppress real pad input upstream while independently feeding documented graph inputs. Do not remove lock permanently.
3. **[LIKELY]** PostPhysics request is too late or gets overwritten by PreBehavior. Task ordering supports this risk; actual consumption point needs validation.
4. **[UNKNOWN]** Raw TAE ID has the wrong event-ID namespace or lacks transition context. No namespace mismatch is asserted as fact.
5. **[LIKELY]** Weapon/upper-lower graph context changes selection. Avoid universal walk IDs.

## Selected approach and prepared experiment

Keep the verified transform path authoritative. First identify/read the real locomotion producer/graph parameters, then feed that layer in its documented task phase, allowing native equipment-aware blends. This is a normalized gait/analog driver alongside transform playback, not a second replay clock.

**Hard boundary:** the pinned ChrCtrl.manipulator is `usize`; PlayerIns.chr_manipulator and the ChrManipulator vectors are private, partly marked TODO/fact-check. Behavior internals/HKS variable setters are not typed. No compatible signatures for those setters were found. Casting the address, inventing their offsets or invoking arbitrary HKS/cutscene functions would violate the requested implementation rules.

Prepared next experiment specification: gated WALK, 2 seconds, low forward analog value, native Move transition, saved actor handle/owned input suppression, bounded displacement <=2 units, grounded-only, emergency F6, player reacquisition, disconnect timeout and restoration. Compare requested gait with observed graph/TAE/root motion. Start with WALK, then RUN and SPRINT only through that same proven producer. Register any new before-behavior callback separately; preserve verified PostPhysics sampling/correction. This experiment is **not exposed as a fake TEST WALK button** before the typed producer or unique signature/ABI is known.

Current implemented checkpoint: read-only multi-layer differential tracer and timeline/IPC tests. No new synthetic input, no animation rate/root-motion write, no native recorder write, no normalized labels inferred during capture. The user-required visible milestone remains pending; diagnostic session is the next concrete runtime step.

## Answers to the 15 required questions

1. **Why raw TAE failed?** No proven locomotion inputs/graph transition were supplied; exact engine cause UNKNOWN and current log lacks opt-in request evidence.
2. **Actual Walk driver?** HKS Move transition plus nonzero graph movement input [HIGH CONFIDENCE]; callable 2.7.0.0 producer UNKNOWN.
3. **Walk vs Run?** Analog/smoothed speed and graph gait selection, modified by equipment/effects; universal ID is not justified.
4. **Run vs Sprint?** Higher movement input/dash path, stamina/effect restrictions; precise runtime producer unresolved.
5. **HKS/graph/parameters?** Combination; controller feeds parameters, HKS controls transitions, graph blends, root motion/physics place the model [HIGH CONFIDENCE].
6. **Remote animation?** Network manipulator/remote update path exists; action/gait replication format UNKNOWN.
7. **Ghost animation?** Replay manipulator/ghost infrastructure exists; recorded-frame-to-graph path UNKNOWN.
8. **Native recorder contents?** Known counters, owner, oldest placement/rotation/block; opaque frame/action payloads and codec UNKNOWN.
9. **Cutscene ownership?** Native movie/event/script infrastructure and user-observed live choreography; precise handoff/input restoration UNKNOWN.
10. **Reusable native mechanism?** Shared controller/graph concept likely useful; no ghost/network/cutscene callable API proven reusable yet.
11. **Selected mechanism?** Existing transform replay + independently driven native locomotion inputs after diagnostic/ABI validation.
12. **WALK visibly works?** NO confirmed success; driver not implemented at this checkpoint.
13. **RUN visibly works?** NO confirmed success.
14. **SPRINT visibly works?** NO confirmed success.
15. **Runtime test required?** One marked natural gameplay trace with matching Phase5C EXE/DLL; exact procedure in PHASE5C_MANUAL_TEST.md. Then implement/test the bounded native WALK driver before recording normalized gait events.
