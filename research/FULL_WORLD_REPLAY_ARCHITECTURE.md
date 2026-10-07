# Full-world 1:1 replay — grounded roadmap, no spawning implementation

## Reusable baseline vs evidence limits

Owner verified Claude skeletal player capture/playback. Independent changes this session are COMPILE_VERIFIED/offline-tested only. Reuse ERPLAY Writer/Reader, index/chunks/checks, host clock/seek/interpolation, launcher/YAFSML, overlay, named pipes, current game task access, bone sidecars and bookmarks. Native bloodstain/ghost research remains a candidate backend; earlier timeout/lifetime blockers and retired ghost playback must not replace the working player skeleton path without a new runtime proof.

| Subsystem | Existing code/evidence | Missing for exact reconstruction |
|---|---|---|
| Player | bone_replay.rs; saved physics transform/local/model pose; owner verified baseline | continuous pose interpolation, skeleton identity, world origin, robust ownership identity |
| Actors | character_capture.rs reads WorldChrMan.chr_inses_by_distance in PostPhysics; position/quaternion/action/identity | complete population, durable spawn lifetimes, backend suppression of AI, arbitrary reconstruction |
| Appearance | visual_capture.rs serializes equipment IDs, player slots/arm style/face, actor HP/character identity | validated application/rebuild, actor equipment and attachments, appearance restore |
| World | world_observation.rs reads CSLuaEventManImp loading/warp prerequisites; SDK position.rs documents spaces | safe map restore, origin conversion, streaming/collision readiness, restart evidence |
| Physics/root | existing proxy move/gravity ownership, recorded model-matrix diagnostics | collision/grounding consistency, props/bullet physics histories, generation identity |
| Projectiles | SDK cs/bullet_manager.rs CSBulletManager.bullets, cs/bullet_ins.rs | spawn/despawn semantic hooks, trajectory identity, safe playback lifetime |
| VFX | CSSfxImp.scene_ctrl, WorldSfxMan, GXFfx resource structures | validated spawn/stop/seek/attachment APIs and deterministic age/seeds |
| Audio | game index CSSoundImp/CSSoundBgmController and GXFfxSoundManager_Wwise strings | actual event IDs/calls, emitter lifetime, stop/seek/mixer controls |
| Props/events | desired event architecture only | native enumeration, transient state, safe override/restore |

Existing capture is radius/budget bounded (defaults 200 units, 1024 identities); actors absent from enumeration are not necessarily despawned/dead. IDs currently expire after two seconds unobserved; this is observational identity, not durable engine spawn identity. Do not label this recorder as capturing every NPC/boss. Store coverage and dropped/unavailable flags explicitly.

## ReplayActorRegistry and lifecycle

Stable replay actor IDs reference category (player/NPC/enemy/boss/summon/prop/projectile/other), native resource/entity/param identity, appearance/equipment, skeleton hash+hierarchy, map/block origin segment and birth/death interval. Keep engine handles as diagnostic identity only; never persist raw pointers. A runtime resolver reacquires engine objects per callback and validates generation/type. Different spawn instances with recycled entity IDs require distinct replay IDs.

Capture side: game task copies bounded validated data to owned buffers; workers serialize/compress without engine references. Backpressure records gaps/resource exhaustion, not invented continuous state. Current pose RAM/600-second bound is unsuitable for full-world sessions; migrate to streamed pose chunks and bracket caching before scaling to many skeletons.

Playback side: deterministic track evaluation feeds the game callback. Native animation/state/events are optional optimized backends only where reproduction is demonstrated; full final pose is the proven fallback. Unknown AI/RNG/physics must not resimulate freely. Suppression/ownership needs per-subsystem enter/apply/restore, fails toward normal gameplay, and never commits progression/inventory/quests/boss kills to the save.

## Multi-track container design

Future format version, not an in-place reinterpretation of ERPLAY/ERBONES1: header/version/profile/resource compatibility; WorldDescriptor; ActorRegistry; TrackDirectory; independently versioned data chunks; SnapshotIndex; EventIndex; checksums/footer. Each track carries type/version/actor ID/coordinate space/skeleton identity/availability. Unknown optional tracks are skippable; unknown required tracks reject exact playback or load diagnostic read-only.

WorldDescriptor records map/area/block identity, native block/MSB position, relevant block/Havok origin relationship, world segment transitions and required content. SDK position.rs differentiates block, global, Havok and map spaces; raw physics XYZ alone is insufficient across sessions. Record paired native representations/origin evidence before defining conversion. Replay restore state machine: request validated map -> await streaming -> await collision/player -> resolve origin -> apply temporary scene -> validate placement. No arbitrary Y compensation or blind model-matrix write.

Tracks: ActorTransform/State/Pose/Animation, Equipment, PropState/Physics, Projectile, VFX/Particle, Audio/Music, WorldState/Environment, Events/Markers, Camera/Lights/ReShade/CustomParticles. Snapshot anchors store actual track state and active lifetimes; event intervals reconstruct from nearest checkpoint. Local pose fallback includes translation/quaternion/scale with hierarchy/version; model pose is optional measured output. Never guess that every actor has the player's fixed 150-bone skeleton.

Original captured data remains immutable. Editor project sidecar references replay identity/hash and adds versioned overrides. `Reset to Recorded` drops overrides, not source data. Stable EditorObjectId supports cameras/lights/particles/audio/markers/props uniformly; tracks reference IDs and own timeline lifetimes. No editor object is an unmanaged dangling game pointer.

## Fidelity strategies

- Boss/NPC: recorded outcomes/state/transforms, compatible pose fallback. Capture phase/resource/equipment changes and attachments; do not rerun AI. Exact actor creation/deletion requires safe native lifetime proof.
- Projectile: event+seed/state backend only if deterministic; otherwise sampled trajectory and visual resource/impact events. Reconcile existence when seeking; no repeated damage application.
- Physics props: map entity identity and dynamic transform/state history. Do not resimulate nondeterministic collision chains or alter permanent save flags.
- VFX: start/stop/attachment/seed/age; full per-particle capture is last resort. Effects lacking age/seed control have explicit seek-fidelity limitations.
- Audio: timed native events, emitter/bone/position, loop/volume/pitch/start/stop. Music needs position seek/mute-restart policy; no promise of sample-exact native audio slowdown.
- Script/environment/params: record semantic transient changes and content identity. Param overrides must be scoped and restored; no global game-file patching or save mutation.

## Ordered phases and acceptance gates

1. Validate current player root/pose playback, continuous timescale UI and native reset.
2. Establish exact cross-session native map/origin/collision restore; explicit restart test.
3. Durable actor registry/coverage/generations and streamed per-skeleton pose schema.
4. One NPC state/pose lifecycle and seek; then many actors and boss phase/resource changes.
5. Equipment/appearance and bone attachments with restore.
6. Projectiles then gameplay effects; start/stop/reverse seek tests.
7. Props/physics and transient world/environment events without save writes.
8. Audio/music seek policy and event capture.
9. Coherent snapshots/frame commit across every implemented track; do this alongside each new subsystem, not after linear-only playback has spread.
10. Camera sequencer and environment overrides; then custom lights/particles and optional ReShade.
11. Compression/partial loading/performance, fidelity diagnostics and expanded scenes.

Standard scenes: player movement/combat/hit/death; basic enemy; boss; projectile combat; destructibles; VFX-heavy spells; multi-NPC. Compare time-aligned trajectory, quaternion, final pose, active object/event counts and visual captures. Test pause, arbitrary seek, near-end rapid scrub, unload, disconnect and full process restart. Full-world 1:1 is UNKNOWN until these measurements cover every claimed subsystem.
