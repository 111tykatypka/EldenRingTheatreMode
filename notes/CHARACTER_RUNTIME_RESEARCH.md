# Native character evidence

Target EldenRing_1_17 / WW 2.7.0.0, AMD64. Pinned fromsoftware-rs revision:
`3c8c1d7633a99309fb004c9f894ea10b7967d0e0`. Cargo.toml and Cargo.lock unchanged.

| Finding | Evidence / confidence |
|---|---|
| Distance-sorted character collection | CONFIRMED public `cs/world_chr_man.rs`, `WorldChrMan::chr_inses_by_distance: DLVector<ChrInsDistanceEntry>` |
| Entry points to ChrIns | CONFIRMED typed NonNull; lifetime safety still requires game-thread execution |
| Native lookup | CONFIRMED `chr_ins_by_handle`; main_player path retained |
| Character position/rotation | CONFIRMED shared `ChrIns.modules.physics`, HavokPosition/Quaternion |
| Metadata | CONFIRMED `field_ins_handle`, `event_entity_id`, `npc_param_id`, `block_id`, `chr_type` |
| Shared raw animation observation | CONFIRMED bounded `time_act.anim_queue[read_idx]`, behavior speed, action request bits |
| IDs imply action names | UNKNOWN; all NPC semantic values explicitly Unknown |
| Collection valid at our PostPhysics callback | HIGH CONFIDENCE source-based candidate; NOT RUNTIME VERIFIED |
| Native lifetime generation | UNKNOWN; internal address/handle identity is conservative and cannot prove unobserved respawn |
| Spawn/death from missing radius entry | NOT VALID; only observational presence is emitted |

Capture defaults: radius 150, requested 20 Hz, 1024 buffer slots. Saved settings in
LocalAppData/EldenRingTheaterMode/Modern.capture.ini are read on DLL initialization.
Capacity 1..16384 is a configurable memory/work budget, not a gameplay actor count.
Truncation and mailbox/host drops are exposed. Very large budgets have quadratic
identity search cost; game impact must be measured before increasing them.
Enumeration sanity ceiling 65536 prevents unbounded work on a corrupt collection.

The callback reacquires WorldChrMan/main_player each pass. It reads NPCs only,
allocates capture vectors before registration, and never writes actor state.
No game pointers survive for dereference on another callback/thread.

## Input ownership candidate

Previous two ChrDebugFlags approach: **FAILED AT RUNTIME** by user observation.
Pinned `CSChrActionRequestModule` documents update at ChrIns_PreBehavior. The new
experimental PreBehaviorSafe callback clears normalized current/queued/readback
actions, movement request bits, and documented queue input/cancel bits only while
the matching local player has an active fresh replay lease. PostPhysics owns/restores
the two existing debug bits and lower 35 `disabled_action_inputs` bits. Normal OS
keyboard/mouse/gamepad and external UI input are untouched. Consumed transient
requests are not restored as stale attacks. Queue lengths >256 stop replay rather
than traversing unbounded memory. Analog manipulator fields are private: UNKNOWN.
Exact consumer ordering and complete input suppression require live confirmation.

## Grounding

CONFIRMED typed fields: physics position/last position, ChrCtrl model and physics
model matrices (translation row 3), vertical_position_offset, ground/fall booleans,
proxy synchronization flags. The matrix source explicitly applies a vertical model
offset. Havok coordinates are not a persistent map-independent world frame.

Grounding logging is read-only at PostPhysics, once per second, with source time.
Compare its physics/model Y with the REPLAY_APPLY target. Proxy Y / ground height
are UNAVAILABLE. No arbitrary Y offset, gravity/root-motion zero, matrix writes or
proxy flags have been applied. A runtime comparison is needed to decide whether
the documented position_sync_requested/rotation_sync_requested flags are required.

## NPC ownership options

Existing actor with scoped AI ownership: LIKELY best first experiment after capture.
Ghost/replay manipulators and debug spawning exist as architectural clues; a safe
callable 2.7.0.0 ownership/spawn lifecycle is UNKNOWN. Current cutscene/event-script
research does not expose a verified native ABI. No selected NPC is written yet.
See PHASE5C_CHARACTER_DRIVING_SYSTEMS.md for previous pinned-source investigation.
