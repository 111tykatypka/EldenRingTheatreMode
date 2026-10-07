# P2d — full-fidelity core checkpoint and roadmap evidence

Date: 2026-10-07. Independent branch `codex/p2b-continuation`.
Original Claude source, TheaterMode-Current, Phase5 and earlier test packages are preserved.

**Overall status: full roadmap NOT COMPLETE. New changes are COMPILE_VERIFIED;
in-game and visual acceptance are UNKNOWN.** A passing unit test is not a game test.
This report supersedes inherited P2b summaries wherever they claim automatic location
conversion, old private replay-clock smoothing, quantized storage, or optional flag writes.

## Implemented in this checkpoint

- ERWORLD v2 bit-exact float-word pose storage, with v1 and legacy `.bones` reading.
- Every available importer bone is copied using its validated dynamic count, rather
  than assuming the player's historical 150-bone stride. Fixed strides remain only
  in the legacy file reader. Actual parent indices, model ID and hierarchy fingerprint
  are captured; current identity is validated before applying a new-file pose.
- Player and accepted actors retain full local/model hkQsTransform arrays. Missing
  identity/topology changes are reported capture gaps, not fabricated compatible data.
- One authoritative host ReplayTime: protocol 11 carries source time and its Windows
  monotonic anchor. DLL interpolation extrapolates that anchor (bounded to 250 ms),
  never integrates a second clock. Paused samples and backward seeks take effect directly.
- Main Play icon toggles Play/Pause through the same host function as Space. Pause
  retains ReplayTime. Explicit host application state separates loaded, paused and
  stopped ownership. Overlay visibility alone no longer starts writes after Stop.
- Safety errors latch instead of repeatedly reacquiring the body and teleporting it.
  Stop then Play is an explicit retry. Missing player/IPC disables ownership; no stale
  return transform remains armed after anti-cheat refusal.
- Known EAC-process presence or process-inspection failure refuses record/play. DLL
  polls on its worker once a second, not on game callbacks. This is a process-name
  safeguard, not proof of absence of every driver or anti-cheat implementation.
- Save-relevant event-flag and world-clock replay writes are disabled, including old persisted options.
  Restore-after-use alone cannot prove autosave isolation. Flag and clock observation remain.
- Hardcoded ten-minute capture cutoff removed. Writer creates files and performs
  all encoding/I/O off game tasks; Stop closes the producer without blocking on disk.
  Periodic durability flushes added; recordings with no player poses do not finalize.
- Radius is configurable via `THEATER_ACTOR_RADIUS` (finite metres, default 100;
  0 = every discovered loaded body). No forced far-actor sampling-rate reduction.
- Reader rejects malformed lengths/counts, invalid metadata, orphan tracks, ordering
  failures and invalid events. Pose semantic validation precedes native writes.
- Before final rename, the writer streams saved chunks back to validate integrity,
  decoding, ordering and identity/count agreement without holding the whole replay.
  Failure retains the partial file and reports a save error.

## Evidence labels

STATIC_VERIFIED means directly inspected source/structures/fixture bytes, not native
behavior. COMPILE_VERIFIED means compiled/tested host or pure data component.
RUNTIME_VERIFIED and VISUALLY_VERIFIED require the actual game. The user's existing
successful on-foot skeletal replay belongs to the inherited baseline; it does not
verify P2d, Torrent, new metadata, world restoration or new timing behavior.

## Category-by-category feasibility

| Category | Structures/access path | Current implementation and achievable accuracy | Missing proof / risk | Evidence |
|---|---|---|---|---|
| World/location | `WorldChrMan`, `ChrIns` block/origin/chunk fields; `arrival::Place`; map/grace catalog | Block and origin observations; raw physics root preserved in the same measured origin/anchor. Metadata is exact storage, cross-origin reconstruction unavailable | `chunk_position` is an origin anchor, not player XYZ. Correct MSB/physics conversion and streaming lifecycle must be measured before cross-region placement | STATIC_VERIFIED fixture; COMPILE_VERIFIED guard; runtime UNKNOWN |
| Time of day | SDK `WorldAreaTime`, `world_state::Clock` | Clock/date/multiplier recorded; overrides disabled pending save isolation | Save persistence of clock, sunlight/weather coupling and seek presentation not proven | COMPILE_VERIFIED; runtime UNKNOWN |
| Weather/environment | Weather params exist in SDK | UNIMPLEMENTED; no current reliable weather-state binding | Need active weather controller, transition parameters, ownership and restoration | UNKNOWN |
| Event flags/world progression | `CSEventFlagMan`, flag-group descriptors | Initial snapshot + observed changes; read-only replay track | Need verified autosave isolation; flags alone do not recreate object/boss lifetimes. No world overrides in P2d | STATIC_VERIFIED layout; COMPILE_VERIFIED codec |
| Player skeleton/root | `CSTaskImp` callback groups, `WorldChrMan.main_player`, physics module, `CSFD4LocationHkaPoseImporter`, `hkaSkeleton` | Full available importer local/model arrays and physics/root/model transform; bit-exact storage. Endpoints exact data; interpolation approximate | Not proven to include every rendered cloth/facial/attachment skeleton or final post-IK effect. Bone names/control rotation/velocity/grounding are not complete replay tracks | COMPILE_VERIFIED new codec/identity; new runtime UNKNOWN |
| Player appearance/equipment/actions | `equipment` render assembly, player-action/fidelity observations | Inherited equipment state and grip capture; pose reflects evaluated actions visually where skeleton contains them | Inventory-handle portability, effects/weapon/sheath attachments, behavior consequences and appearance recreation not proven | Inherited baseline + COMPILE_VERIFIED; 1:1 UNKNOWN |
| Enemies/NPCs/bosses | World distance list, buddy set, `ChrIns`, per-body importer, data HP; structurally checked debug flags | Independent root and local/model tracks; stable recording IDs; existing-body matching; full-rate capture inside radius | No safe puppet spawning, revive, hide-before-spawn, persistent identity across all reloads or phase/model transitions. All-active-boss inclusion outside radius not implemented. AI suspension requires live acceptance | COMPILE_VERIFIED; full scene accuracy unavailable |
| Torrent/summons | SDK buddy `ChrSet`, ride-character pair observations | Pointer-free role/ride context; Active and ReadyForActivation discovery; own body pose if validated | Related mounted candidate had null importer. Need rendered mount body/importer association; automatic mount/dismount/spawn unsupported. Model 8000/8002 guesses never authorize writes | STATIC_VERIFIED logs; mounted pose UNKNOWN |
| Dynamic props/destruction | Native map/asset entity and physics owners need identification | UNIMPLEMENTED | Need per-object identity, state setters, collision restoration and reversible lifecycle; flag changes alone are insufficient | UNKNOWN |
| Projectiles | SDK `CSBulletManager`, `CSBulletIns`, handles and creation/death callbacks | SDK source feasibility only; no replay track implemented | Safe snapshot iteration, source/target identity, lifetime-safe recreation, trajectory override, impact suppression and random seeds | STATIC_VERIFIED SDK, runtime UNKNOWN |
| Spells/VFX/particles | Native effects/attachment/particle owners unresolved | UNIMPLEMENTED | IDs alone do not restore elapsed simulation, seeds, emitted particles or active attachments on seek | UNKNOWN |
| Game SFX | UI sound system exists; native game-audio event interception unresolved | UI sounds preserved; gameplay audio replay UNIMPLEMENTED | Need native event owners and mute/seek/loop policy; visual pose does not reproduce sound | UNKNOWN for replay SFX |
| Generic gameplay events/params | Existing ERPLAY action/fidelity/character records; flag deltas | Current observation streams preserved; no claim of semantic native attack/damage replay | Need validated event-to-state reducers, availability flags and periodic snapshots for each category | COMPILE_VERIFIED current streams; full event recreation UNKNOWN |
| Master Sequencer | Host `ReplayPlayer`, source timestamps; editor protocol 11 | One host clock for current root/pose/equipment tracks; exact pause/seek transport | Complete-scene seek equivalence unproved because many categories are absent | COMPILE_VERIFIED |
| Player/Free/Dolly camera | SDK `CSCamera`, `CSPersCam`, `CSCam.matrix/fov`, `WorldChrMan.chr_cam`/`ChrCam` | Native player camera remains; new free/dolly editor UNIMPLEMENTED | Need active render-camera writer hook, restore lifetime and matrix convention. SDK fields/public reference establish candidates, not safe ownership | STATIC_VERIFIED SDK candidates; runtime UNKNOWN |
| Camera keyframes/gizmos/cuts/attachments | Future owned camera tracks + render projection/picking | UNIMPLEMENTED; no inert UI claiming to control a game camera | Need camera binding first, quaternion convention, ray/viewport projection, world-to-bone attachments and unscaled input | UNKNOWN |
| Complete native HUD toggle | SDK `CSFeManImp.hud_state` | UNIMPLEMENTED | SDK `HideAll` documents HP/FP/stamina; it does not prove hiding compass, bosses, messages and every FE layer | STATIC_VERIFIED partial field; full HUD UNKNOWN |
| Custom lights | Native deferred/DX12 light ownership unresolved | UNIMPLEMENTED | Need actual lighting integration, create/update/destroy ABI, render-thread scheduling and lifetime proof. A drawn marker is not a native light | UNKNOWN |
| Look/DOF/focus/post-processing | Native effect/param candidates require exact profile | UNIMPLEMENTED | Need active state owners and restore contract; no fake aperture/DOF sliders | UNKNOWN |
| ReShade | Existing overlay DX12 hook/present/input chain | Optional compatibility NOT VERIFIED; no ReShade event track | Hook ordering, descriptors, swapchain/resize and separate input/cursor ownership need installed-runtime tests | UNKNOWN |
| Non-destructive cinematic project | Replay + existing sidecars/library ownership | Recorded sources preserved; editor project schema and camera/light/Look overrides UNIMPLEMENTED | Provisional direction: separate versioned sidecar with replay identity, coordinate convention, track schema, unknown-track handling and atomic save | Design only |

## Concrete skeletal/source evidence

Pinned SDK revision stays `3c8c1d7633a99309fb004c9f894ea10b7967d0e0`.
Exact game profile remains EldenRing_1_17 / 2.7.0.0 / SHA-256
`D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134`.

All additional importer/skeleton offsets are read from GameProfile:
ChrIns +0x398 importer; importer +0x48 hkaSkeleton, +0x50 local pose,
+0x60 model pose; hkaSkeleton +0x20 parent array, +0x28 parent count,
+0x38 bone count, +0x48 reference-pose count. Counts must agree. Current capture
uses the inherited technical corruption guard of 1..1024 bones; decoder/hierarchy
guards are 4096. These are structure sanity guards, not intentional reduced-bone
sampling. A validated skeleton exceeding the current capture guard needs separate
profile evidence; do not silently say it was completely captured.

Reads/writes stay on the existing task callbacks: PostPhysics ownership/root,
PrePhysicsSafe pose/root, Draw_Pre capture/retention diagnostics. The three-colour
hierarchy check detects cycles in linear time. No native object pointers cross the
world writer queue. Every player callback reacquires the live player.

## Root motion / Torrent evidence retained

Real legacy file `Torrent test.erplay.world`: 685 frames; physics root spans
X [-12.258703,11.007728], Y [2.8891125,9.670378], Z [-0.3543443,27.906324].
Stored chunk anchor stays [-16,-104,-80]. This directly explains why replacing
physics XYZ with that field cancelled motion. The correction keeps the raw root
trajectory and only uses origin/anchor as a guard. New live acceptance is still needed.

Mounted logs showed one buddy-set status-4 entry discarded by the old Active-only
filter. A related status-2 body with character/npc ID 8002 had no +0x398 importer;
read-only RPM corroborated that field. Ghidra candidate 0x1403FF0E0 mentions
alternate render importer fields (+0x3A0/+0x3A8); this is NOT a verified offset or
approved write path. No blind alternate-pointer fallback is added.

## Storage / performance / seek limits

Exact storage is not a promise of 1:1 rendering. Local/model poses at sample endpoints
are bit-exact; between samples, quaternion SLERP/TRS and hierarchy composition are
approximations. Additional render passes/procedural effects can overwrite or extend them.

Uncompressed two-pose payload alone at 60 Hz is `bones * 96 * 3600` bytes/minute:
150 bones = 49.44 MiB/min; 500 bones = 164.79 MiB/min. A player plus ten 150-bone
actors is 543.82 MiB/min before metadata/events/compression. These are calculated
payload budgets, NOT measured boss-fight rates. XOR/zero-run encoding can shrink
unchanged words and can expand some changing words. The worker logs actual finalized
MiB/min as `WORLD_FILE: saved ... MB per minute`; no invented FPS/overhead value.

Recording is bounded-queue/disk-streamed. A one-second per-actor pending chunk is
held on the writer, with a 240-message producer queue. Batches can grow with actor
count; this is not a fixed memory budget. There are still per-frame owned-buffer
allocations and hierarchy reads. Game FPS/CPU/disk impact is unmeasured.

Playback currently retains compressed file bytes plus timestamps in RAM; each pose
track caches four decoded chunks. Load indexes/decompresses chunks once; cache misses
can decode on the game callback. Disk-backed indexing/asynchronous prefetch remain
necessary for very large sessions. No arbitrary duration cap is a claim of unlimited
physical resources.

Direct-seek tests cover encoded pose/root samples and context reducer selection.
They do not prove world state equivalence, actor revival, collision, effects, sound,
streaming or cross-map seeking. A missing actor remains unavailable rather than
being replaced by an unsafe guessed puppet.

## Why later native features cannot be marked complete

The roadmap explicitly requires lifecycle and ownership proof before writes. SDK
declarations and partial Ghidra pseudocode do not provide that proof for object
respawn, mount transition, native lighting, full HUD, weather, camera ownership or
effect rewind. Applying guessed setters would risk crashes and save changes.
These gaps are substantive engine integration work, not final-polish bugs.

The public reference camera demonstrates free-camera/path/FOV/timestop feature
feasibility: https://opm.fransbouma.com/Cameras/eldenring.htm . Its functionality
does not validate our profile hooks or permit copying proprietary implementation.
Current player camera is untouched. Global game-speed writes remain retired after
the earlier main-menu/whole-game slowdown regression.

## Next development gates

1. Test this core package in the game, including root trajectory, retained skeletal
   pose, pause/resume, explicit Stop and malformed/missing identity refusals.
2. Read-only live mounted-body enumeration: identify the rendered Torrent importer,
   correlate its hierarchy and rider root relationship. Prove lifecycle before mount writes.
3. Measure origin conversion across a controlled tile transition; do not reinstate
   the old chunk-as-player-position formula or automatic cross-region warp.
4. Establish autosave isolation before flag override/reversible object state.
5. Native camera read-only probe and writer-order proof, then bounded ownership
   experiment; only then free camera, keyframes/gizmos/dolly/editor sidecar.
6. Native projectile/prop/effect state observations, versioned reducers/snapshots,
   lifetime-safe copies; then spawning/rewind and full-scene seek acceptance.
7. Native lighting and Look/HUD hooks with restoration tests; optional ReShade
   co-install and resize/input testing. None is currently delivered as working.
