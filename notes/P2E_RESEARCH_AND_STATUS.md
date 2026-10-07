# P2e1 — actor lifecycle observations and reconstruction research

Date: 2026-10-07. Source baseline: P2d `adbe560`. Independent checkout only.

**P2e acceptance is NOT COMPLETE.** This checkpoint captures/evaluates richer actor
observations. It does not reconstruct a killed/despawned enemy, permanently defeated
boss, changed model/phase, projectile or VFX instance. No new runtime/visual verification.

## NPC/enemy discovery and identity

[STATIC_VERIFIED] Existing `actors.rs` uses current `WorldChrMan::instance()`,
`chr_inses_by_distance` plus validated buddy-set candidates, excludes main_player,
deduplicates current pointers, checks FieldInsHandle and configured radius. No far-rate
reduction. `THEATER_ACTOR_RADIUS` defaults to 100; 0 includes all discovered loaded bodies.
This is not exhaustive discovery of unloaded actors, bosses outside the radius or VFX owners.

[COMPILE_VERIFIED] Per-session increasing ReplayActorId is assigned to
(FieldInsHandle, event_entity_id, npc_param_id), with a new generation after an observation
gap exceeding 500 ms when a key reappears. Raw handles are provenance, not portable replay IDs.
Cross-session matching still uses entity+NpcParam, or a unique supported companion role.
It cannot construct absent bodies. Recording IDs are never engine pointers.

Root/orientation, origin metadata and full available local/model poses use P2d's lossless
codec and validated per-actor skeleton hierarchy/count/model fingerprint. New observation
records add character_id, npc_id, model_id, death/render flags, HP/max HP, raw backread/
cleanup state and pose availability. They use public fields of the unchanged pinned SDK,
under the existing exact file-version/product/AMD64/SHA guard.

Death is observed through `ChrIns.chr_flags1c5.death_flag()`, not HP==0. The SDK also exposes
`enable_render()`. [STATIC_VERIFIED] `chr_ins.rs` documents these at lines 449-450 and
444-445; drop-item/rune suppression flags are separate at lines 457-460. Reading a death
flag does not establish death-animation completion, corpse/ragdoll state or cleanup timing.
Those semantics remain UNKNOWN. No HP, death, reward, quest or progression writes added.

## Timeline/snapshot implementation

[COMPILE_VERIFIED] `actor_lifetime.rs` provides pointer-free Observation, per-actor Timeline
and binary-search evaluation. It records state changes plus one-second refresh snapshots.
Unobserved means enumeration/radius/validation uncertainty, not native Despawn. Invalid pose
and topology change have explicit reasons. Queue failure does not commit observation state.

Track 7/kind 9 adds these snapshots to ERWORLD v2. Catalog references, sizes, fields and
per-actor ordering are validated on save/open. Actor-local query is independent of seek
direction, with no NPC clock. The existing host ReplayTime remains authoritative.
Existing root/pose chunks remain independent and seekable. A latest observation does not
authorize interpolating over an unavailable pose interval.

The existing-body backend refuses incompatible recorded/live death or render state and logs
ACTOR_RECONSTRUCTION_REQUIRED; it never clears death flags to satisfy a rewind. This refusal
is not a resurrected visual actor. Legacy files without observations keep legacy behavior.

## Cheat Engine boss revival: exact findings

Reference file requested in the specification exists and was read without execution:
`C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\eldenring_all-in-one_Hexinton-v8.0.4.CT`.
Its SHA agrees with the previously indexed desktop copy (`b083aafa...0829e2d`); the table
targets 1.17.1/2.7.1.0, not our exact 1.17/2.7.0.0. Static corroboration is not compatibility.

[STATIC_VERIFIED] Hierarchy: `[ Enable ]/[ Scripts ]/[ Progression ]/Revive/Kill All Bosses
(Base Game only)/Revive/Kill ALL Bosses`, CE ID **1337309787**. Its parent warns about
hundreds of flags/quest progression. Enable activates records named `//skilllessaf//` and
`//skillless//`, then issues **174** four-byte writes of zero to locations based on
`[[EventFlagMan]+28]+offset`. Examples of byte offsets: 702D3, 170CC8, A0E25, 16AC1A.
Disable issues **174** writes of decimal 255 to those locations. It does not restore
saved original bytes. These offsets are not event IDs or portable RVAs.

Referenced helper records also clear flag storage and write 255 on disable; examples
159BAB and 15741A. Duplicate helper descriptions exist, so CE description-based lookup
is ambiguous. No actor factory/reset, model/skeleton initialization or HP reset call
appears in these inspected revival scripts. Creation on a subsequent world/map refresh
is LIKELY controlled by game scripts, not statically proven here; reload requirements
and actual revived AI/HP correctness are UNKNOWN. This is unsuitable for Theater save safety.

[STATIC_VERIFIED] Separate `1: All Character Respawn` (ID 1337304778) is a 4-byte entry
at WorldChrMan with offset text `18484`, not an assembler revival implementation.
Its semantics, version compatibility and persistence are unverified; it is not used.

## Native construction and isolated puppets

[STATIC_VERIFIED] Exact pinned SDK `world_chr_man.rs:136` exposes
`WorldChrMan::spawn_debug_character(&ChrDebugSpawnRequest)`. It populates CSDebugChrCreator
model name, character/NPC/think params, entity/talk IDs, is_player and position, then
sets `spawn=true`. It is an asynchronous native request, not a constructor returning
an owned actor. `last_created_chr` alone does not prove ownership of a request.

CE `Spawn Debug Character`, ID **1987705439**, independently sets
`[[WorldChrMan]+1E648]+44` to 1, runs its write from `createthread`, and modifies selected
debug-entry bytes. Its parent ID 1987705402 polls a debug ChrSet through +1E268 at 150 ms.
This corroborates the conceptual creator/request path, not offsets for our version,
safe threading, teardown or save isolation. No CE script was executed or transplanted.

Prior native ghost report identifies a native factory/activation and removal queue:
creation starts near preferred VA 14025F7E0; removal drain 14050EFA0 invokes 14050B340.
Its prototype required the original native callback context, and user history includes
TIMEOUT without that context. Prior static proof is not runtime acceptance or proof that
arbitrary NPC debug-spawn requests can use a ghost destructor/removal path. No fallback
create calls or direct destructors were introduced.

Ghidra index confirms CSDebugChrCreator STEP_Idle string at 142A50B00 and RTTI at
143C86808; ReplayGhostIns RTTI at 143C85DC0. Targeted index queries produced no string
xrefs for selected creator/TAE anchors. This is a partial-analysis gap, not absence of
runtime code. No new native RVA/ABI was promoted to a callable binding.

| Approach | Evidence and decision |
| --- | --- |
| Native revival | Flag-based CE route changes progression; rejected for normal replay |
| Native spawn | Public request exists; ownership, no-save behavior and removal still unproven |
| Visual puppet | Best conceptual isolation; model/pose/resource/lifetime constructor not verified |
| Hybrid native body + Theater tracks | Promising provisional direction; do not enable until isolation/cleanup proved |

Required proof before creation: exact creator execution/task phase, request-to-entry
correlation, collision/AI/attack/TAE isolation, absence of rewards/quest/autosave effects,
owned entry removal including deferred tasks, resource release, world-transition cancellation.
ThinkParam/CharaInitParam/appearance construction metadata is not fully captured yet.
Do not use real MSB boss entity IDs for clones without proving script isolation.

## AI isolation and boss phases

Existing backend writes validated debug no-move/no-attack flags, disables gravity and
overrides pose/root. This is not proven isolation of AI targeting, TAE damage/SFX, quest
scripts, reward callbacks or boss state machines. No new AI patch added.

Bosses need representation epochs for model/skeleton/equipment changes, phase events,
multiple constituent actors, fog/arena/script independence and persistent death isolation.
P2d currently rejects incompatible topology; P2e1 records that capture gap. Phase rewind
and defeated-boss representation remain UNKNOWN, not solved by the observation track.

## Particles/VFX: sources and actual evidence

Pinned SDK provides CSSfxImp -> GXFfxSceneCtrl -> graphics resource manager -> resource
container -> FXR definitions. Those are loaded effect resources, not active emitters.
The SDK debug viewer has a `TODO: Address crashing` on definition traversal; a generic
unchecked list walk is not introduced into game callbacks.

WorldSfxMan describes area/block/grid SFX containers and counts, with undocumented instance
fields. CSChrTimeActModule has owner, animation queue/id/play_time/length; its internal
event pointer is private/undocumented. BulletIns has param ID, time_alive and opaque sfx_ctrl.
SpEffect is a gameplay status system; it is not a particle instance or particle seed.

Public primary reference inspected/cloned separately:
[FXR WebSocket Reloader](https://github.com/EvenTorset/fxr-ws-reloader), revision
`a1fe9550834ea8dbfb01c387b18bfe7701483c86`. Its `src/agent.rs:641-700` changes weapon
resident_sfx/dummy-poly params or SP_EFFECT_PARAM/VFX param references, briefly sets them
to -1, waits 100 ms and restores/replaces IDs. Its definition iterator accesses CSSfx
resources. This demonstrates resource replacement/resident-effect refresh, not an isolated
active-instance create/stop/age/seed API. It does not prove exact-target compatibility or
mid-effect seek. No library, injector or param-patching code was integrated.

[FXR editor library](https://github.com/EvenTorset/fxr) documents editing effect assets.
Asset editing is distinct from recording runtime instances. No source implementation copied.

Ghidra anchors include WorldSfxManImp RTTI 143CEF9C0, GXFfxSceneCtrl RTTI 143D37F38,
GXFfxUpdater RTTI 143D38168 and CSChrTimeActModule RTTI 143C824C0. They identify candidate
classes, not callable APIs. No verified complete chain from TAE/behavior/bullet request
through spawn to an active emitter was established in this checkpoint.

Attachment needs explicit Actor/Bone/DummyPoly/World kinds: a dummy-poly ID from the public
reloader is NOT necessarily a skeleton bone index. Resolve its binding before replay.
Effect-instance IDs, start/stop timing, source actors, binding/local offsets, elapsed age,
seed/parameters and ownership remain to be instrumented. Exact mid-particle seek is UNKNOWN.

Proposed VfxEventTrack: versioned records with stable event/instance IDs, source time,
verified effect asset ID, spawn/stop kind, stable actor/projectile reference and explicitly
typed attachment. Add age/seed/variant only when native sources are proven. Restore active
instances from snapshots then bounded resimulation only if native advancement is supported;
otherwise label approximate restoration. No unsupported fields or fabricated events stored.

## Remaining format/reconstruction work

Actor registry needs full construction metadata and representation epochs. Observation
snapshots are not full restorable native world snapshots. Genuine Spawn/Activate/Deactivate/
DeathStart/DeathComplete/Corpse/Despawn tracks, boss phase/equipment tracks, projectile tracks,
VFX instance lifetimes and active-effect snapshots remain NOT IMPLEMENTED. New kinds must
be pointer-free, ordered, chunked and skippable; required capabilities must be declared so
a reader cannot silently claim complete world reconstruction from partial tracks.

## Verification and next checkpoint

- STATIC_VERIFIED: SDK field/API declarations, CE script mechanics, selected Ghidra
  names, public FXR resource/param code. No complete spawn ABI promoted to runtime.
- COMPILE_VERIFIED: Release host/DLL; 49 Rust tests pass, 1 optional fixture ignored;
  12/12 host tests pass; 3/3 inspector tests pass (synthetic, explicitly labeled).
- RUNTIME_VERIFIED / VISUALLY_VERIFIED: no new P2e functionality.
- UNKNOWN: killed-enemy restoration, native ownership/cleanup/isolation, boss phases,
  particle capture/application and mid-effect seek.

The next controlled test is an ordinary-enemy live capture to validate observed death
flags, pose availability and native disappearance timing. See P2E_RUNTIME_TEST.md.
Then close the native spawn/removal isolation proof and implement ONE owned puppet;
only its successful create/control/death/rewind/remove test can unlock the full P2e goal.
