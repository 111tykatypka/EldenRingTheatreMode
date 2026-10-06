# Replay data format

Snapshot:2026-10-06, implementation3d97070. ERPLAY and native ReplayData are separate formats; no ERPLAY→native codec writer exists.

## Capture

Telemetry captures real timestamp, XYZ, XYZW and optional action/character/visual/fidelity observations. Legacy player wire72 bytes, current base104 plus fidelity extension. Nine raw fidelity tracks cover242 fields/455u32 words with availability masks. A7555-sample/~126s capture was audited for native animation ID/time bit fidelity. This proves storage, not accurate gameplay reconstruction.

## Header and samples

Little-endian Windows/x64. Header64 bytes:

|Offset|Field|
|---|---|
|0|8 bytes ERPLAY02 or ERPLAY03|
|8|u32 version2/3|
|12|u32 reserved|
|16|u64 Unix recording start ns|
|24|double requested Hz|
|32|double actual Hz|
|40|u64 sample count|
|48|u64 duration ns|
|56|u64 excluded paused duration ns|

Then five UTF-8 strings, each u32 length+bytes: game_version, mod_version, title, description, tags. Reader metadata guard16MiB/field. No universal map ID in fixed header; raw block observations are separate.

Transform52 bytes: indexu64, replaytimeu64, sourcetimeu64, XYZ3float, XYZW4float. Optional in-memory action/capture serialized separately. CHNK0x4b4e4843 +countu32+payloadbytesu64+CRC32u32 =20-byte header, count×52 payload.

TRAK0x4b415254 +type/flags/countu32 +bytesu64 +CRC32u32 =28-byte header. Type2 action40 bytes; type3 character112; type4 visual440; types5–13 raw fidelity records. Raw capture begins fouru64(timestamp/source/sequence/drops), then masks/words by schema. Canonical shared/capture_schema.json and generated C++/Rust definitions. Character kinds registry1/transform2/presence3; presence loss is not proven despawn/death.

Flags0/1 allowed; unknown required(flag1) rejected, optional unknown skipped. CRC polynomial0xedb88320. No compression codec implemented. FOOT0x544f4f46 +fouru64(chunks,count,duration,paused), v3 addsactioncountu64:36/44 bytes including marker.

## Finalization and storage

Default chunk600samples (~10s at60Hz), bounded streaming buffers, per-chunk flush. Finalize flushes tracks/footer, updates header actualrate=(count-1)/duration, closes, fully validates temporaryfile then renames final. Tagged fidelity tracks must match samplecount. NaN/inf transforms, quaternion squarednorm outside.25..2.25, ordering/schema/CRC/truncation fail explicitly. Raw opaque captured words retain bit patterns with validity rather than pretending all are floating transforms.

No whole-recording RAM needed by writer. Reader validation/index memory grows with samples; cache holds chunks. 64MiB payload-read guard protects corruption, not total recording duration. .tmp improves recoverability, not power-loss guarantees. Recover complete validated chunks without truncating source; historical recovery self-truncation regression must not return.

## Playback/fidelity

Host owns ReplayClock, random access, linear position interpolation and normalized shortest-arc quaternion SLERP. DLL receives current state and applies on verified callbacks. No duplicate DLL parser/clock. Discrete actions/raw words are not indiscriminately interpolated. Speeds.1/.25/.5/1/2/4; stepping uses stored samples. Bookmarks sidecar `<replay>.bookmarks`. Internal pause accounting remains despite simplified UI.

AI/RNG/physics/combat/world snapshots are not complete. Transform writes can fight Havok/input/root motion; user observed correct path without animations. Native animation requests remain experimental. Block/Havok rebasing can make raw coordinates inconsistent. Native recorder capacity60 does not establish long recording retention or concatenation/seek semantics.

Historical deleted fixture replay_2026-10-05_022417.erplay:4093samples,7chunks,~68.5s,59.737Hz,213128bytes (~3.11KB/s transform-only). Owner deleted it; do not assume available. Nine-track rates differ and require actual measurement.

---

## Evidence appendix: `research/NATIVE_REPLAY_FRAME_FORMAT.md` (repository authored evidence snapshot)

Historical statuses retain their original scope. Latest Step placement/26 guards/observed timeouts in the opening sections supersede older builder-only and untested-callsite wording. Static proof does not mean successful native creation.

# Native replay frame candidates

[STATIC_VERIFIED] pool initializes nodes of0x248 through1404e4ec0; link at+240. Update copies two encoded payloads each capacity0x100. This is a linked active/free pool; conventional ring-buffer behavior not yet proven.

|Node offset|Candidate field|Confidence|
|---|---|---|
|0|u32 primary length|STATIC_VERIFIED|
|4..103|primary payload, capacity256|STATIC_VERIFIED|
|104|u32 secondary length|STATIC_VERIFIED|
|108..207|secondary payload, capacity256|STATIC_VERIFIED|
|208|float delta/elapsed|STATIC_VERIFIED representation; time meaning runtime pending|
|20c,210|block/reference identities|HIGH CONFIDENCE, undecoded|
|214|reference Vec3 candidate|UNKNOWN semantics|
|220..22b|converted XYZ|HIGH CONFIDENCE|
|22c|yaw scalar|HIGH CONFIDENCE|
|230|source counter candidate|UNKNOWN|
|234|position validity flag|HIGH CONFIDENCE|
|238|action/state hash candidate|UNKNOWN, not animation ID|
|23c,23d,23e|secondary/boundary/event candidates|UNKNOWN exact meaning|
|240|next node pointer|STATIC_VERIFIED|

Builders read physics/control/module state, so payload contains more than transforms. Bone pose, weapon, items, damage, all actions and full encounter reproducibility NOT established. Serializer layout has count+state prefix, primary length/payload blocks and optional secondary blocks. Not a supported .erplay codec yet.

Duration getter1406f1db0 reads provider+13c with minimum fallback;1406f1e30 multiplies duration by30/5. Public NETWORK_PARAM recordDeadingGhostTotalTime/MinTime are references only until exact offsets/provider match. Rolling retention, arbitrary-duration concatenation, reset keyframes and decoder format remain unknown; changing capacity alone could corrupt ownership.

## Evidence rules

Exact target disk SHA256: D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134; version2.7.0.0, AMD64, patch1.17. VAs assume preferred image base0x140000000; runtime addresses are ASLR base+RVA.

STATIC_VERIFIED means exact image bytes support the stated operation; REFERENCE means SDK/source only. At the initial checkpoint runtime measurements were absent; subsequent read-only results are in NATIVE_REPLAY_RUNTIME_RESULT.md. Decompiler names/argument reconstruction are hypotheses. Linear extraction may include padding/adjacent functions after RET; these bytes do not belong to the preceding function automatically.

Evidence artifacts (kept outside repository): `../research/ghidra-eldenring/targeted/native_bloodstain/verified_bytes/<VA>.json` include exact bytes, indexed callers/callees, pseudocode and disassembly. Partial Ghidra analysis does not guarantee complete xrefs.

Imported/attached representation is independently verified: primary count+d0/pointer+d8 and secondary count+224/pointer+228; block stride0x104 = length4 + payload256. Getters1406f1e60/1406f1e90 only compare signed index<count and pointer nonnull; do NOT call with negative/unvalidated index. Raw JSONL attached_data_prefix permits observation of these counts/pointers without following them. Decoder output duration is float+2c, verified MOVSS in1404e9840. Actual codec bitfields/keyframe schema remain unresolved.


---

## Evidence appendix: `research/NATIVE_PAYLOAD_GHOST_FINDINGS.md` (repository authored evidence snapshot)

Historical statuses retain their original scope. Latest Step placement/26 guards/observed timeouts in the opening sections supersede older builder-only and untested-callsite wording. Static proof does not mean successful native creation.

# Native payload and local ghost checkpoint — 2026-10-06

Status: recorder proven; pool mapped; payload partially decoded; ghost creation/playback NOT runtime proven. New experimental DLL is read-only. No constructor, serializer, allocator, destructor or unknown virtual function is called by it.

Target only: Elden Ring 1.17, AMD64, file version 2.7.0.0, disk SHA256 D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134. Preferred VAs below are research identifiers; runtime addresses require verified ASLR base. Pinned SDK and Cargo.lock unchanged.

## Pool map

[CONFIRMED — exact disassembly plus previous runtime] Recorder size 0x860, owner+10, pool+18, active head+20/tail+28, free head+30/tail+38, capacity+40, active count+44, float elapsed accumulator+48. Allocation in 1404e4da0 uses allocator virtual+50, count*0x248+0x10 bytes, requested alignment 0x10. Array header precedes nodes by 0x10. Each node is 0x248 bytes, therefore successive node addresses alternate modulo16; observed stride supports 8-byte alignment, not 16-byte alignment for every node.

The pool uses two null-terminated singly linked lists, linked at node+240. Insert 1404e5350 pops the free head (1404e6420) and appends to active tail. Eviction 1404e6520 recycles active nodes until a segment boundary. This is recycling storage, not a circular linked list. +44 is active count, not a cumulative frame number. Previously observed capacity60 is a native setting, not a new recorder limit.

| Node offset | Meaning / confidence |
|---|---|
|0 / 4|primary length u32 / payload up to256 bytes — CONFIRMED|
|104 / 108|secondary length / payload up to256 — CONFIRMED|
|208|float elapsed duration — CONFIRMED|
|20c|BlockId — CONFIRMED by payload correlation|
|210,214..21c|reference block/vector candidates — LIKELY; not decoded|
|220..228|converted position vector — CONFIRMED correlation with payload; not automatically physics-local coordinates|
|22c|scalar angle — UNKNOWN relation to control angle|
|230|source counter — UNKNOWN semantics|
|234,238,23c,23d,23e|position-valid flag, state hash candidate, secondary-present, segment-boundary, unknown u16 — static evidence; no animation ID assigned|
|240|next node pointer — CONFIRMED|

Pool ownership stays with recorder; nodes are recycled, not independently refcounted. Never retain them beyond a bounded snapshot. Serializer 1404e5530 performs eviction and must NOT be called as a read-only accessor.

## Cadence

[CONFIRMED — static] ChrIns callback1403f92b0 loads actor dt and calls virtual+c0; PlayerIns/ReplayGhostIns slots resolve to140660ac0, which conditionally invokes recorder1404e5af0. Accumulator threshold approximately1/6 second plus state-change conditions can cause additional nodes. This is not a fixed60Hz node stream. Previous1Hz diagnostics measured the observer, not native recording. Exact task scheduling frequency and effective node frequency remain UNKNOWN pending the new per-callback probe. Pool retention can reset at segment boundaries; it is not necessarily exactly10 seconds.

## Payload grammar, reader and real-byte verification

[CONFIRMED — exact reader disassembly] Generic reader140423310 reads a one-byte presence bitmap, then enabled fields in registration order. State constructor1404e8e30 registers six groups: fixed20,16,28,16 bytes; nested behavior; fixed24 bytes. Readers respectively1404e9450,1404e93e0,1404e9360,1404e93b0,nested140422990/1404243d0..730,1404e9410. Nested behavior has its own bitmap and four optional count-prefixed arrays of3/4/6/6-byte entries. Numeric keys and values remain opaque.

Events follow state in the SAME primary payload: constructor1404eaee0 registers optional u8 count + u32 array, u8 count + u16 array, nested behavior, and fixedu32. The secondary node payload is a separate actor stream, not this event stream.

Observed state mask0x34 enables groups2,4,5. Group2 layout:

| Relative offset | Field |
|---|---|
|0|u32 flags, bit semantics unresolved|
|4|f32 duration|
|8|i32 BlockId|
|c,10|two packed u32 values A/B|
|14|unknown u32; observed2048|
|18|unknown i32; observed-1|

Reader1404eaae0 decodes X=sign20(A)*0.02, Y=sign17(((A>>20)<<5)\|(B&31))*0.04, Z=sign20(B>>5)*0.02. Control heading=((B>>25)-64)*pi/64. Native conversion14061ef70 handles block coordinates; anchor state can add a reference vector. Independent Python reader reports MSB coordinates and does not reconstruct arbitrary world coordinates or inherited omitted fields.

[CONFIRMED — previous genuine journal decoded offline] 413 node snapshots parsed with zero errors; includes repeated active-head/tail/free-head observations, NOT 413 distinct replay frames. In137 active-tail snapshots: BlockId matches all; duration difference exactly0; maximum absolute coordinate differences against node+220 are X0.01985054/Y0.03572998/Z0.01965149, consistent with quantization. Max wrapped control-heading difference against node+22c is2.37017rad. Do not equate them or physical quaternion yaw. ChrCtrl getter1403de280 and writer1404ea520/1404ea8f0 support a control-angle source distinct from node scalar. Group0 retains two raw orientation components pending semantic proof.

Idle72-byte payload decomposes into66-byte state and6-byte events. All bytes can be structurally consumed, but group5/action IDs, behavior keys and event meanings are not fully decoded. No walk/roll/attack animation classification is claimed. New probe captures these numeric values alongside existing live action observations and user annotations; velocity/root-motion/TAE are not silently inferred.

## Native consumer and ownership

[HIGH CONFIDENCE — exact constructors/deserializer/refcount operations] Separate replay-data object size0x238, ctor1406f1bb0, vtable142a8b640, reference count+8. Primary count+d0/array+d8; primary cursor+220; secondary count+224/array+228/cursor+230. Each imported block0x104 is length4+payload256. Metadata+10..cf and secondary metadata+fc..21b are required; complete field semantics UNKNOWN. Decoder1406f1f20/validator1406f2580 allocate and copy these arrays rather than borrow recorder nodes. Destructor1406f1c50 releases arrays via native allocator; deleting destructor1406f1d00 frees0x238. Array ownership survives the source recorder in this static model, but runtime lifecycle not verified.

ReplayManipulator attach1403df010 retains data, stores pointer+100. ReplayGhostIns ctor1404f1840 also retains it at+740. Both references must be released through native destruction; manual free risks double-free. Node bytes alone are NOT a valid replay-data object.

## Local spawn, insertion and cleanup

[HIGH CONFIDENCE — static chain] Local/debug caller1407048e0 obtains player recorder, serializes via1406f2410 (mutating serializer), builds complete data, then calls1406f27f0 with the local branch. This demonstrates a candidate local acquisition path independent of incoming network data; it does not prove safe public invocation from our PostPhysics callback.

1406f27f0 validates/decodes first state/events and selects ChrType through1406f2e30 (literal10 bloodstain) or1406f2e40 (literal3). World wrapper140507e60 uses ghost set at world+10f38; set factory140493a80 finds native empty entry, derives handle, calls140404570. That allocates0x760 and constructs ReplayGhostIns1404f1840. Base ChrIns ctor1403e6e30 stores entry pointer and populates the entry; native actor initialization follows. Player factory14065db40 mode3 constructs ReplayManipulator1403deaa0, attaches data through1403df010, and builds ChrCtrl through1403ddf10. Native placement uses decoded block/position/control state, not arbitrary teleport writes.

1404f2340/1403df000 switches stream selector; it is NOT yet proven to be a public start-playback command. Actual ghost type3 manipulator, rendered movement, scheduling and completion remain runtime UNKNOWN.

World removal14050b340 performs actor notification, active-vector/set handling and native queued teardown. Ghost deleting destructor1404f1bd0 ->1404f1ab0 ->PlayerIns destructor1406515f0 ->ChrIns destructor1403e7970. Slot cleanup includes thunk140ade693 ->1403ee5c9; this edge is not a fully reconstructed cleanup contract. Do not call a deleting destructor to substitute for native unregistering.

## Why creation is not enabled yet

Exact spawning phase/taskgroup, complete valid appearance/game metadata, native start/expiry contract, and scheduled deletion/slot clearing must be resolved before controlled creation. The native constructor dereferences non-null replay-data metadata; a dummy/null object is unsafe. There is no safe empty ghost constructor demonstrated. User explicitly requires ownership and cleanup before writes. Thus this checkpoint implements payload parsing and NEW read-only action/cadence instrumentation, not an unsafe creation toggle. No claim of native replay solved.

Evidence: exact SHA-checked bounded artifacts outside repository under ../research/ghidra-eldenring/targeted/native_payload/*.json; previous journal SHA979679b26882470e199c1ae58a107c50013f8cb2a5441f5878342af0c5e00b0c. Partial Ghidra indexing may omit callers; disassembly is cross-checked and decompiler labels are not authority.


---

## Evidence appendix: `notes/PHASE8_REAL_CAPTURE_STATUS.md` (repository authored evidence snapshot)

Historical statuses retain their original scope. Latest Step placement/26 guards/observed timeouts in the opening sections supersede older builder-only and untested-callsite wording. Static proof does not mean successful native creation.

# Phase8 — first real expanded player capture

## Evidence

User reports approximately two minutes fighting a boss, resting at a grace and using items. Automatically discovered actual file: `%LOCALAPPDATA%/EldenRingTheaterMode/replays/replay_2026-10-06_075252.erplay`.

SHA256: `6af296f656729f5463ac898d90d31f516fa149e8117c5864d9955874327acf54`.

ERPLAY03, producer `0.7.0-fidelity1`, game `2.7.0.0`. Duration **126.4597572 seconds**, **7555** player samples, **59.734418 Hz**, **13** transform chunks. File **21,030,717 bytes** (20.056 MiB), observed total file rate **166,303.63 bytes/s**. This rate includes legacy tracks, not only new player fields.

Read-only Python inspector checked header/footer, CRC, sample order, track lengths, availability masks and per-track ordering. The C++ Reader/ReplayPlayer independently opened this real file and sought to start, midpoint and end with all nine tracks present (`replay-player-tests --capture-real <file>`). This command only reads the file and never applies game transforms.

## New tracks observed

Every track has 7555 records and every declared field is available in every record. No raw float NaN/Inf found. All 242 fields were readable; **112** changed during this session. Readability is not proof of correct field interpretation.

| Track | Named fields | Changed records |
|---|---:|---:|
| PhysicsTrack | 62 | 7542 |
| AnimationTrack | 42 | 7542 |
| ActionTrackRaw | 39 | 3559 |
| LocomotionBehaviorTrack | 23 | 6981 |
| EquipmentTrack | 8 | 4 |
| AppearanceTrack | 3 | 0 |
| GameplayStateTrack | 25 | 3133 |
| EffectSignalsTrack | 4 | 4 |
| CombatStateTrack | 36 | 5295 |

Animation queue IDs and phase/time vary in all 10 public entries; each slot contains 50–58 distinct IDs. Queue indices cycle 0–9. Behavior root motion changes 6712 times. Current native action-request bits change 249 times. These are native observations, not an established attack/roll semantic mapping.

Item-use timer changes near **22.54s** and **106.45s**. Item SFX fields change near **22.54s**, **106.61s**, **109.51s**. These support item-use correlation; exact item identity and effect lifetime remain unverified. HP changes at **69.96s, 104.65s, 110.37s, 121.31s, 124.05s**, range **0–522**. FP range **38–78**; stamina raw signed value range **-16–97**. Negative stamina is retained honestly; its interpretation needs validation, not clamping. Lock flag changes near **36.89s** and **124.10s**. Arm style changes four times; equipment IDs and slot indices remain constant. Appearance stays constant, consistent with an unchanged character, but its full semantic layout is not verified.

No exact grace-rest interval or boss identity is proven by raw changes alone. User action timestamps/video observation are needed before labeling those events. Locomotion orientation matrix values range outside unit rotation bounds: do not treat this raw SDK field as a validated pure rotation matrix.

## Timing / losses / performance

Player FIFO drop counter: **0**; recorded player sequence gaps: **0**. Maximum source sample interval **70.0702ms**; therefore 60 Hz is an average, not a guarantee of every frame. All nine tracks share source timestamps/sequences.

Separate pre-existing character capture reports cumulative **313 character drops** in host logs. This is a different queue and is not claimed lossless; its per-session loss delta is not established here. Do not present zero player drops as zero losses across the whole replay.

Game log reports capture reads around **116–124 microseconds per callback** during ordinary player availability. This measures the field-read portion, not complete recording overhead or game FPS impact. Game later reports PLAYER_LOST then PLAYER_FOUND; this is not demonstrated to fall inside the recorded interval, whose samples have no sequence gaps. No claim of captured transition lifecycle completeness.

## Reader compatibility hotfix

Host logs after finalization contain `EDITOR_ERROR=incompatible replay mod version`. Exact cause: ReplayPlayer allowed only `0.2.0`, `0.3.0`, `0.6.0`, while the new recorder writes `0.7.0-fidelity1`. Added that exact producer version to the whitelist. Unknown producers remain rejected. Added synthetic accepted/rejected producer tests plus the actual-file read-only validation CLI. No format, game guard, DLL, playback writes or animation logic changed.

New host package: `Phase8_PlayerCapture_Fidelity1_Hotfix1`. Close the old host before launching the new EXE. DLL behavior is unchanged; no game restart is needed solely for this host opening fix. Open the existing file, inspect Player capture tracks and scrub. This verifies data viewing; replaying animations remains NOT IMPLEMENTED. Do not use this capture validation as authorization for uncontrolled gameplay writes.

## Remaining scope

Raw field availability and genuine persisted multi-track capture are established for this session. Full skeletal pose, behavior blend weights, full HKS VM, dynamic action queue and complete SpEffect inventory remain unresolved. Most semantic labels remain REFERENCE pending controlled correlation. This capture does not yet reproduce the boss fight, damage, items, grace state or animation in-game.

Detailed evidence: PHASE8_REAL_CAPTURE_INSPECTION.json (state near 50s) and PHASE8_REAL_CAPTURE_AUDIT.json (all-field change/availability/range summary). Original replay and original logs are left unchanged.


---

## Evidence appendix: `notes/PHASE8_CAPTURE_FIELDS.md` (repository authored evidence snapshot)

Historical statuses retain their original scope. Latest Step placement/26 guards/observed timeouts in the opening sections supersede older builder-only and untested-callsite wording. Static proof does not mean successful native creation.

# Player fidelity capture field catalog — schema 1

Offsets below are compiler-derived from the exact pinned SDK, not independent proof of the runtime game layout. New reads use ReadProcessMemory and module-owner validation; semantics require live correlation. No raw pointers are serialized. All tracks sample every available ChrIns_PostPhysics callback (target about 60 Hz, measured rate displayed). All records are continuous snapshots, not sparse semantic labels.

| Track | Field | Source structure/path | SDK offset | Representation | Confidence |
|---|---|---|---|---|---|
| PhysicsTrack | `position` | `CSChrPhysicsModule.position` | `0x70` | f32, 4 words | RUNTIME_READ_VERIFIED |
| PhysicsTrack | `last_update_position` | `CSChrPhysicsModule.last_update_position` | `0x80` | f32, 4 words | REFERENCE |
| PhysicsTrack | `interpolated_orientation` | `CSChrPhysicsModule.interpolated_orientation` | `0x60` | f32, 4 words | REFERENCE |
| PhysicsTrack | `additional_rotation` | `CSChrPhysicsModule.additional_rotation` | `0x190` | f32, 4 words | REFERENCE |
| PhysicsTrack | `orientation_euler` | `CSChrPhysicsModule.orientation_euler` | `0x2D0` | f32, 4 words | REFERENCE |
| PhysicsTrack | `rotation_multiplier` | `CSChrPhysicsModule.rotation_multiplier` | `0x1C0` | f32, 1 words | REFERENCE |
| PhysicsTrack | `motion_multiplier` | `CSChrPhysicsModule.motion_multiplier` | `0x1C4` | f32, 1 words | REFERENCE |
| PhysicsTrack | `gravity_multiplier` | `CSChrPhysicsModule.gravity_multiplier` | `0x1CC` | f32, 1 words | REFERENCE |
| PhysicsTrack | `chr_push_up_factor` | `CSChrPhysicsModule.chr_push_up_factor` | `0x104` | f32, 1 words | REFERENCE |
| PhysicsTrack | `default_max_turn_rate` | `CSChrPhysicsModule.default_max_turn_rate` | `0x314` | f32, 1 words | REFERENCE |
| PhysicsTrack | `hit_height` | `CSChrPhysicsModule.hit_height` | `0x2F0` | f32, 1 words | REFERENCE |
| PhysicsTrack | `hit_radius` | `CSChrPhysicsModule.hit_radius` | `0x2F4` | f32, 1 words | REFERENCE |
| PhysicsTrack | `weight` | `CSChrPhysicsModule.weight` | `0x300` | f32, 1 words | REFERENCE |
| PhysicsTrack | `chr_proxy_pos_update_requested` | `CSChrPhysicsModule.chr_proxy_pos_update_requested` | `0x91` | u8, 1 words | REFERENCE |
| PhysicsTrack | `standing_on_solid_ground` | `CSChrPhysicsModule.standing_on_solid_ground` | `0x92` | u8, 1 words | REFERENCE |
| PhysicsTrack | `touching_solid_ground` | `CSChrPhysicsModule.touching_solid_ground` | `0x93` | u8, 1 words | REFERENCE |
| PhysicsTrack | `is_falling` | `CSChrPhysicsModule.is_falling` | `0x1D0` | u8, 1 words | REFERENCE |
| PhysicsTrack | `is_touching_ground` | `CSChrPhysicsModule.is_touching_ground` | `0x1D1` | u8, 1 words | REFERENCE |
| PhysicsTrack | `gravity_disabled` | `CSChrPhysicsModule.gravity_disabled` | `0x1D5` | u8, 1 words | REFERENCE |
| PhysicsTrack | `fade_out_gravity_disabled` | `CSChrPhysicsModule.fade_out_gravity_disabled` | `0x1D3` | u8, 1 words | REFERENCE |
| PhysicsTrack | `flying_character_fall_requested` | `CSChrPhysicsModule.flying_character_fall_requested` | `0x1D9` | u8, 1 words | REFERENCE |
| PhysicsTrack | `use_world_y_alignment_logic` | `CSChrPhysicsModule.use_world_y_alignment_logic` | `0x1DB` | u8, 1 words | REFERENCE |
| PhysicsTrack | `is_surface_constrained` | `CSChrPhysicsModule.is_surface_constrained` | `0x1DC` | u8, 1 words | REFERENCE |
| PhysicsTrack | `adjust_to_hi_collision` | `CSChrPhysicsModule.adjust_to_hi_collision` | `0xCC` | u8, 1 words | REFERENCE |
| PhysicsTrack | `move_type_flags` | `CSChrPhysicsModule.move_type_flags` | `0x320` | u8, 1 words | REFERENCE |
| PhysicsTrack | `physics_model_matrix` | `ChrCtrl.physics_model_matrix` | `0x1B0` | f32, 16 words | REFERENCE |
| PhysicsTrack | `model_matrix` | `ChrCtrl.model_matrix` | `0x230` | f32, 16 words | REFERENCE |
| PhysicsTrack | `additional_orientation_quat` | `ChrCtrl.additional_orientation_quat` | `0x2C0` | f32, 4 words | REFERENCE |
| PhysicsTrack | `vertical_position_offset` | `ChrCtrl.vertical_position_offset` | `0x2D0` | f32, 1 words | REFERENCE |
| PhysicsTrack | `scale_size_x` | `ChrCtrl.scale_size_x` | `0x2D4` | f32, 1 words | REFERENCE |
| PhysicsTrack | `scale_size_y` | `ChrCtrl.scale_size_y` | `0x2D8` | f32, 1 words | REFERENCE |
| PhysicsTrack | `scale_size_z` | `ChrCtrl.scale_size_z` | `0x2DC` | f32, 1 words | REFERENCE |
| PhysicsTrack | `offset_y` | `ChrCtrl.offset_y` | `0x2E0` | f32, 1 words | REFERENCE |
| PhysicsTrack | `ragdoll_revive_time` | `ChrCtrl.ragdoll_revive_time` | `0x12C` | f32, 1 words | REFERENCE |
| PhysicsTrack | `foot_ik_error_height_limit` | `ChrCtrl.foot_ik_error_height_limit` | `0x308` | f32, 1 words | REFERENCE |
| PhysicsTrack | `disable_move` | `ChrCtrl.disable_move` | `0xE9` | u8, 1 words | REFERENCE |
| PhysicsTrack | `height_correction_request` | `ChrCtrl.height_correction_request` | `0x301` | u8, 1 words | REFERENCE |
| PhysicsTrack | `chr_ragdoll_state` | `ChrCtrl.chr_ragdoll_state` | `0x128` | u8, 1 words | REFERENCE |
| PhysicsTrack | `flags` | `ChrCtrl.flags` | `0xF0` | u32, 1 words | REFERENCE |
| PhysicsTrack | `flags_copy` | `ChrCtrl.flags_copy` | `0xF4` | u32, 1 words | REFERENCE |
| PhysicsTrack | `chr_proxy_flags` | `ChrCtrl.chr_proxy_flags` | `0xFC` | u32, 1 words | REFERENCE |
| PhysicsTrack | `fall_timer` | `CSChrFallModule.fall_timer` | `0x18` | f32, 1 words | REFERENCE |
| PhysicsTrack | `force_max_fall_height` | `CSChrFallModule.force_max_fall_height` | `0x1D` | u8, 1 words | REFERENCE |
| PhysicsTrack | `disable_fall_motion` | `CSChrFallModule.disable_fall_motion` | `0x1E` | u8, 1 words | REFERENCE |
| PhysicsTrack | `chr_hit_height` | `CSChrPhysicsModule.chr_hit_height` | `0x2E0` | f32, 1 words | REFERENCE |
| PhysicsTrack | `chr_hit_radius` | `CSChrPhysicsModule.chr_hit_radius` | `0x2E4` | f32, 1 words | REFERENCE |
| PhysicsTrack | `step_disp_interpolate_time` | `CSChrPhysicsModule.step_disp_interpolate_time` | `0x3E8` | f32, 1 words | REFERENCE |
| PhysicsTrack | `step_disp_interpolate_trigger_value` | `CSChrPhysicsModule.step_disp_interpolate_trigger_value` | `0x3EC` | f32, 1 words | REFERENCE |
| PhysicsTrack | `is_enable_step_disp_interpolate` | `CSChrPhysicsModule.is_enable_step_disp_interpolate` | `0x3E6` | u8, 1 words | REFERENCE |
| PhysicsTrack | `is_watcher_stones` | `CSChrPhysicsModule.is_watcher_stones` | `0x1E1` | u8, 1 words | REFERENCE |
| PhysicsTrack | `physics_transform_matrix_squared` | `ChrCtrl.physics_transform_matrix_squared` | `0x1F0` | f32, 16 words | REFERENCE |
| PhysicsTrack | `model_matrix_squared` | `ChrCtrl.model_matrix_squared` | `0x270` | f32, 16 words | REFERENCE |
| PhysicsTrack | `foot_ik_error_on_gain` | `ChrCtrl.foot_ik_error_on_gain` | `0x30C` | f32, 1 words | REFERENCE |
| PhysicsTrack | `foot_ik_error_off_gain` | `ChrCtrl.foot_ik_error_off_gain` | `0x310` | f32, 1 words | REFERENCE |
| PhysicsTrack | `forward_undulation_limit_radians` | `ChrCtrl.forward_undulation_limit_radians` | `0x32C` | f32, 1 words | REFERENCE |
| PhysicsTrack | `backward_undulation_limit_radians` | `ChrCtrl.backward_undulation_limit_radians` | `0x330` | f32, 1 words | REFERENCE |
| PhysicsTrack | `side_undulation` | `ChrCtrl.side_undulation` | `0x334` | f32, 1 words | REFERENCE |
| PhysicsTrack | `undulation_correction_gain` | `ChrCtrl.undulation_correction_gain` | `0x338` | f32, 1 words | REFERENCE |
| PhysicsTrack | `weight_type` | `ChrCtrl.weight_type` | `0x18C` | u32, 1 words | REFERENCE |
| PhysicsTrack | `is_undulation` | `ChrCtrl.is_undulation` | `0x328` | u8, 1 words | REFERENCE |
| PhysicsTrack | `use_ik_normal_by_undulation` | `ChrCtrl.use_ik_normal_by_undulation` | `0x329` | u8, 1 words | REFERENCE |
| PhysicsTrack | `hit_group_and_navimesh` | `ChrCtrl.hit_group_and_navimesh` | `0x3A9` | u8, 1 words | REFERENCE |
| AnimationTrack | `read_idx` | `CSChrTimeActModule.read_idx` | `0xC4` | u32, 1 words | REFERENCE |
| AnimationTrack | `write_idx` | `CSChrTimeActModule.write_idx` | `0xC0` | u32, 1 words | REFERENCE |
| AnimationTrack | `queue_0_anim_id` | `CSChrTimeActModule.anim_queue[0].anim_id` | `0x20` | i32, 1 words | REFERENCE |
| AnimationTrack | `queue_0_play_time` | `CSChrTimeActModule.anim_queue[0].play_time` | `0x24` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_0_anim_length` | `CSChrTimeActModule.anim_queue[0].anim_length` | `0x2C` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_1_anim_id` | `CSChrTimeActModule.anim_queue[1].anim_id` | `0x30` | i32, 1 words | REFERENCE |
| AnimationTrack | `queue_1_play_time` | `CSChrTimeActModule.anim_queue[1].play_time` | `0x34` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_1_anim_length` | `CSChrTimeActModule.anim_queue[1].anim_length` | `0x3C` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_2_anim_id` | `CSChrTimeActModule.anim_queue[2].anim_id` | `0x40` | i32, 1 words | REFERENCE |
| AnimationTrack | `queue_2_play_time` | `CSChrTimeActModule.anim_queue[2].play_time` | `0x44` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_2_anim_length` | `CSChrTimeActModule.anim_queue[2].anim_length` | `0x4C` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_3_anim_id` | `CSChrTimeActModule.anim_queue[3].anim_id` | `0x50` | i32, 1 words | REFERENCE |
| AnimationTrack | `queue_3_play_time` | `CSChrTimeActModule.anim_queue[3].play_time` | `0x54` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_3_anim_length` | `CSChrTimeActModule.anim_queue[3].anim_length` | `0x5C` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_4_anim_id` | `CSChrTimeActModule.anim_queue[4].anim_id` | `0x60` | i32, 1 words | REFERENCE |
| AnimationTrack | `queue_4_play_time` | `CSChrTimeActModule.anim_queue[4].play_time` | `0x64` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_4_anim_length` | `CSChrTimeActModule.anim_queue[4].anim_length` | `0x6C` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_5_anim_id` | `CSChrTimeActModule.anim_queue[5].anim_id` | `0x70` | i32, 1 words | REFERENCE |
| AnimationTrack | `queue_5_play_time` | `CSChrTimeActModule.anim_queue[5].play_time` | `0x74` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_5_anim_length` | `CSChrTimeActModule.anim_queue[5].anim_length` | `0x7C` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_6_anim_id` | `CSChrTimeActModule.anim_queue[6].anim_id` | `0x80` | i32, 1 words | REFERENCE |
| AnimationTrack | `queue_6_play_time` | `CSChrTimeActModule.anim_queue[6].play_time` | `0x84` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_6_anim_length` | `CSChrTimeActModule.anim_queue[6].anim_length` | `0x8C` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_7_anim_id` | `CSChrTimeActModule.anim_queue[7].anim_id` | `0x90` | i32, 1 words | REFERENCE |
| AnimationTrack | `queue_7_play_time` | `CSChrTimeActModule.anim_queue[7].play_time` | `0x94` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_7_anim_length` | `CSChrTimeActModule.anim_queue[7].anim_length` | `0x9C` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_8_anim_id` | `CSChrTimeActModule.anim_queue[8].anim_id` | `0xA0` | i32, 1 words | REFERENCE |
| AnimationTrack | `queue_8_play_time` | `CSChrTimeActModule.anim_queue[8].play_time` | `0xA4` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_8_anim_length` | `CSChrTimeActModule.anim_queue[8].anim_length` | `0xAC` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_9_anim_id` | `CSChrTimeActModule.anim_queue[9].anim_id` | `0xB0` | i32, 1 words | REFERENCE |
| AnimationTrack | `queue_9_play_time` | `CSChrTimeActModule.anim_queue[9].play_time` | `0xB4` | f32, 1 words | REFERENCE |
| AnimationTrack | `queue_9_anim_length` | `CSChrTimeActModule.anim_queue[9].anim_length` | `0xBC` | f32, 1 words | REFERENCE |
| AnimationTrack | `animation_speed` | `CSChrBehaviorModule.animation_speed` | `0x17C8` | f32, 1 words | REFERENCE |
| AnimationTrack | `max_ankle_pitch_angle_rad` | `CSChrBehaviorModule.max_ankle_pitch_angle_rad` | `0x1684` | f32, 1 words | REFERENCE |
| AnimationTrack | `max_ankle_roll_angle_rad` | `CSChrBehaviorModule.max_ankle_roll_angle_rad` | `0x1688` | f32, 1 words | REFERENCE |
| AnimationTrack | `ground_touch_state` | `CSChrBehaviorModule.ground_touch_state` | `0x1680` | u32, 1 words | REFERENCE |
| AnimationTrack | `root_motion` | `CSChrBehaviorModule.root_motion` | `0x30` | f32, 4 words | REFERENCE |
| AnimationTrack | `request_animation_id` | `CSChrEventModule.request_animation_id` | `0x18` | i32, 1 words | REFERENCE |
| AnimationTrack | `idle_anim_id` | `CSChrEventModule.idle_anim_id` | `0x1C` | i32, 1 words | REFERENCE |
| AnimationTrack | `ez_state_request_ladder` | `CSChrEventModule.ez_state_request_ladder` | `0x28` | i32, 1 words | REFERENCE |
| AnimationTrack | `ez_state_request_ladder_output` | `CSChrEventModule.ez_state_request_ladder_output` | `0x4C` | i32, 1 words | REFERENCE |
| AnimationTrack | `flags` | `CSChrEventModule.flags` | `0x40` | u8, 1 words | REFERENCE |
| ActionTrackRaw | `action_requests` | `CSChrActionRequestModule.action_requests` | `0x10` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `previous_action_requests` | `CSChrActionRequestModule.previous_action_requests` | `0x18` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `new_action_presses` | `CSChrActionRequestModule.new_action_presses` | `0x20` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `released_actions` | `CSChrActionRequestModule.released_actions` | `0x28` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `cancel_ready_actions` | `CSChrActionRequestModule.cancel_ready_actions` | `0x30` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `queued_action_inputs` | `CSChrActionRequestModule.queued_action_inputs` | `0x38` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `disabled_action_inputs` | `CSChrActionRequestModule.disabled_action_inputs` | `0x40` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `possible_action_inputs` | `CSChrActionRequestModule.possible_action_inputs` | `0x98` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `possible_action_cancels` | `CSChrActionRequestModule.possible_action_cancels` | `0xA0` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `prev_possible_action_inputs` | `CSChrActionRequestModule.prev_possible_action_inputs` | `0xA8` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `readback_new_presses` | `CSChrActionRequestModule.readback_new_presses` | `0x108` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `readback_cancel_ready` | `CSChrActionRequestModule.readback_cancel_ready` | `0x110` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `readback_queued_inputs` | `CSChrActionRequestModule.readback_queued_inputs` | `0x118` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `readback_possible_inputs` | `CSChrActionRequestModule.readback_possible_inputs` | `0x120` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `readback_possible_cancels` | `CSChrActionRequestModule.readback_possible_cancels` | `0x128` | u64, 2 words | REFERENCE |
| ActionTrackRaw | `npc_action_id` | `CSChrActionRequestModule.npc_action_id` | `0xF4` | i32, 1 words | REFERENCE |
| ActionTrackRaw | `requested_gesture` | `CSChrActionRequestModule.requested_gesture` | `0xF8` | i32, 1 words | REFERENCE |
| ActionTrackRaw | `readback_npc_action_id` | `CSChrActionRequestModule.readback_npc_action_id` | `0x130` | i32, 1 words | REFERENCE |
| ActionTrackRaw | `queue_tae_id_override` | `CSChrActionRequestModule.queue_tae_id_override` | `0x134` | i32, 1 words | REFERENCE |
| ActionTrackRaw | `queue_current_tae_id` | `CSChrActionRequestModule.action_request_queue.current_tae_id` | `0x90` | i32, 1 words | REFERENCE |
| ActionTrackRaw | `tae_cancels` | `CSChrActionRequestModule.tae_cancels` | `0x100` | u32, 1 words | REFERENCE |
| ActionTrackRaw | `movement_request_flags` | `CSChrActionRequestModule.movement_request_flags` | `0xFC` | u32, 1 words | REFERENCE |
| ActionTrackRaw | `queue_mode_enabled` | `CSChrActionRequestModule.queue_mode_enabled` | `0x138` | u8, 1 words | REFERENCE |
| ActionTrackRaw | `timer_r1` | `CSChrActionRequestModule.action_timers.r1` | `0xB0` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_r2` | `CSChrActionRequestModule.action_timers.r2` | `0xB4` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_l1` | `CSChrActionRequestModule.action_timers.l1` | `0xB8` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_l2` | `CSChrActionRequestModule.action_timers.l2` | `0xBC` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_action` | `CSChrActionRequestModule.action_timers.action` | `0xC0` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_roll` | `CSChrActionRequestModule.action_timers.roll` | `0xC4` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_jump` | `CSChrActionRequestModule.action_timers.jump` | `0xC8` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_use_item` | `CSChrActionRequestModule.action_timers.use_item` | `0xCC` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_switch_spell` | `CSChrActionRequestModule.action_timers.switch_spell` | `0xD0` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_change_weapon_r` | `CSChrActionRequestModule.action_timers.change_weapon_r` | `0xD4` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_change_weapon_l` | `CSChrActionRequestModule.action_timers.change_weapon_l` | `0xD8` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_change_item` | `CSChrActionRequestModule.action_timers.change_item` | `0xDC` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_r3` | `CSChrActionRequestModule.action_timers.r3` | `0xE0` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_l3` | `CSChrActionRequestModule.action_timers.l3` | `0xE4` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_touch_r` | `CSChrActionRequestModule.action_timers.touch_r` | `0xE8` | f32, 1 words | REFERENCE |
| ActionTrackRaw | `timer_touch_l` | `CSChrActionRequestModule.action_timers.touch_l` | `0xEC` | f32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `hks_root_motion_mult` | `CSChrBehaviorDataModule.hks_root_motion_mult` | `0x24C` | f32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `turn_speed` | `CSChrBehaviorDataModule.turn_speed` | `0x250` | f32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `hks_animation_speed_multiplier` | `CSChrBehaviorDataModule.hks_animation_speed_multiplier` | `0x310` | f32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `fixed_rotation_direction` | `CSChrBehaviorDataModule.fixed_rotation_direction` | `0x1E3` | u8, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `has_twist_modifier` | `CSChrBehaviorDataModule.has_twist_modifier` | `0x1E2` | u8, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `root_motion_reduction` | `ChrCtrlModifier.data.root_motion_reduction` | `0x2C` | f32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `movement_limit` | `ChrCtrlModifier.data.movement_limit` | `0x38` | u32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `action_flags` | `ChrCtrlModifier.data.action_flags` | `0x18` | u32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `hks_flags` | `ChrCtrlModifier.data.hks_flags` | `0x1C` | u32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `movement_request_duration` | `CSChrActionRequestModule.movement_request_duration` | `0xF0` | f32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `normal_vector` | `CSChrPhysicsModule.material_info.normal_vector` | `0x230` | f32, 4 words | REFERENCE |
| LocomotionBehaviorTrack | `orientation_matrix` | `CSChrPhysicsModule.material_info.orientation_matrix` | `0x1F0` | f32, 16 words | REFERENCE |
| LocomotionBehaviorTrack | `hit_material` | `CSChrPhysicsModule.material_info.hit_material` | `0x250` | i32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `is_slippery_surface` | `CSChrPhysicsModule.material_info.is_slippery_surface` | `0x255` | u8, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `is_non_slippery_surface` | `CSChrPhysicsModule.material_info.is_non_slippery_surface` | `0x254` | u8, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `slide_vector` | `CSChrPhysicsModule.slide_info.slide_vector` | `0x260` | f32, 4 words | REFERENCE |
| LocomotionBehaviorTrack | `normal_angle` | `CSChrPhysicsModule.slide_info.normal_angle` | `0x278` | f32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `normal_angle_deg` | `CSChrPhysicsModule.slide_info.normal_angle_deg` | `0x280` | f32, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `is_sliding` | `CSChrPhysicsModule.slide_info.is_sliding` | `0x27C` | u8, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `enable_angle_check` | `CSChrPhysicsModule.slide_info.enable_angle_check` | `0x27D` | u8, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `enabled` | `CSChrPhysicsModule.slide_info.enabled` | `0x284` | u8, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `enable_slide_interpolation` | `CSChrPhysicsModule.slide_info.enable_slide_interpolation` | `0x285` | u8, 1 words | REFERENCE |
| LocomotionBehaviorTrack | `min_twist_rank` | `CSChrBehaviorDataModule.min_twist_rank` | `0x1E0` | i16, 1 words | REFERENCE |
| EquipmentTrack | `equipment_param_ids` | `ChrAsm.equipment_param_ids` | `0x7C` | i32, 22 words | REFERENCE |
| EquipmentTrack | `left_weapon_slot` | `ChrAsm.equipment.selected_slots.left_weapon_slot` | `0xC` | u32, 1 words | REFERENCE |
| EquipmentTrack | `right_weapon_slot` | `ChrAsm.equipment.selected_slots.right_weapon_slot` | `0x10` | u32, 1 words | REFERENCE |
| EquipmentTrack | `left_arrow_slot` | `ChrAsm.equipment.selected_slots.left_arrow_slot` | `0x14` | u32, 1 words | REFERENCE |
| EquipmentTrack | `right_arrow_slot` | `ChrAsm.equipment.selected_slots.right_arrow_slot` | `0x18` | u32, 1 words | REFERENCE |
| EquipmentTrack | `left_bolt_slot` | `ChrAsm.equipment.selected_slots.left_bolt_slot` | `0x1C` | u32, 1 words | REFERENCE |
| EquipmentTrack | `right_bolt_slot` | `ChrAsm.equipment.selected_slots.right_bolt_slot` | `0x20` | u32, 1 words | REFERENCE |
| EquipmentTrack | `arm_style` | `ChrAsm.equipment.arm_style` | `0x8` | u32, 1 words | REFERENCE |
| AppearanceTrack | `face_buffer` | `PlayerGameData.face_data.face_data_buffer` | `0x768` | bytes, 72 words | REFERENCE |
| AppearanceTrack | `gender` | `PlayerGameData.gender` | `0xBE` | u8, 1 words | REFERENCE |
| AppearanceTrack | `archetype` | `PlayerGameData.archetype` | `0xBF` | u8, 1 words | REFERENCE |
| GameplayStateTrack | `hp` | `CSChrDataModule.hp` | `0x138` | i32, 1 words | REFERENCE |
| GameplayStateTrack | `max_hp` | `CSChrDataModule.max_hp` | `0x13C` | i32, 1 words | REFERENCE |
| GameplayStateTrack | `max_uncapped_hp` | `CSChrDataModule.max_uncapped_hp` | `0x140` | i32, 1 words | REFERENCE |
| GameplayStateTrack | `base_hp` | `CSChrDataModule.base_hp` | `0x144` | i32, 1 words | REFERENCE |
| GameplayStateTrack | `fp` | `CSChrDataModule.fp` | `0x148` | i32, 1 words | REFERENCE |
| GameplayStateTrack | `max_fp` | `CSChrDataModule.max_fp` | `0x14C` | i32, 1 words | REFERENCE |
| GameplayStateTrack | `base_fp` | `CSChrDataModule.base_fp` | `0x150` | i32, 1 words | REFERENCE |
| GameplayStateTrack | `stamina` | `CSChrDataModule.stamina` | `0x154` | i32, 1 words | REFERENCE |
| GameplayStateTrack | `max_stamina` | `CSChrDataModule.max_stamina` | `0x158` | i32, 1 words | REFERENCE |
| GameplayStateTrack | `base_stamina` | `CSChrDataModule.base_stamina` | `0x15C` | i32, 1 words | REFERENCE |
| GameplayStateTrack | `chara_init_param_id` | `CSChrDataModule.chara_init_param_id` | `0xC4` | i32, 1 words | REFERENCE |
| GameplayStateTrack | `recoverable_hp` | `CSChrDataModule.recoverable_hp` | `0x160` | f32, 1 words | REFERENCE |
| GameplayStateTrack | `recoverable_hp_time` | `CSChrDataModule.recoverable_hp_time` | `0x168` | f32, 1 words | REFERENCE |
| GameplayStateTrack | `sa_durability` | `CSChrSuperArmorModule.sa_durability` | `0x10` | f32, 1 words | REFERENCE |
| GameplayStateTrack | `sa_durability_max` | `CSChrSuperArmorModule.sa_durability_max` | `0x14` | f32, 1 words | REFERENCE |
| GameplayStateTrack | `armor_recover_time` | `CSChrSuperArmorModule.recover_time` | `0x1C` | f32, 1 words | REFERENCE |
| GameplayStateTrack | `poise_broken_state` | `CSChrSuperArmorModule.poise_broken_state` | `0x22` | u8, 1 words | REFERENCE |
| GameplayStateTrack | `toughness` | `CSChrToughnessModule.toughness` | `0x10` | f32, 1 words | REFERENCE |
| GameplayStateTrack | `toughness_max` | `CSChrToughnessModule.toughness_max` | `0x18` | f32, 1 words | REFERENCE |
| GameplayStateTrack | `toughness_recover_time` | `CSChrToughnessModule.recover_time` | `0x1C` | f32, 1 words | REFERENCE |
| GameplayStateTrack | `stamina_recovery_remainder` | `ChrIns.stamina_recovery_remainder` | `0xE4` | f32, 1 words | REFERENCE |
| GameplayStateTrack | `stamina_recovery_modifier` | `ChrIns.stamina_recovery_modifier` | `0xE8` | f32, 1 words | REFERENCE |
| GameplayStateTrack | `draw_params` | `CSChrDataModule.draw_params` | `0xC0` | u32, 1 words | REFERENCE |
| GameplayStateTrack | `block_id_origin` | `CSChrDataModule.block_id_origin` | `0x7C` | u32, 1 words | REFERENCE |
| GameplayStateTrack | `trigger_max_toughness_update` | `CSChrToughnessModule.trigger_max_toughness_update` | `0x2D` | u8, 1 words | REFERENCE |
| EffectSignalsTrack | `item_use_cast_sfx_id` | `ChrIns.item_use_cast_sfx_id` | `0x16C` | i32, 1 words | REFERENCE |
| EffectSignalsTrack | `item_use_fire_sfx_id` | `ChrIns.item_use_fire_sfx_id` | `0x170` | i32, 1 words | REFERENCE |
| EffectSignalsTrack | `item_use_effect_sfx_id` | `ChrIns.item_use_effect_sfx_id` | `0x174` | i32, 1 words | REFERENCE |
| EffectSignalsTrack | `tae_queued_use_item` | `ChrIns.tae_queued_use_item` | `0x160` | u32, 1 words | REFERENCE |
| CombatStateTrack | `is_locked_on` | `ChrIns.is_locked_on` | `0xC9` | u8, 1 words | REFERENCE |
| CombatStateTrack | `lock_on_target_position` | `ChrIns.lock_on_target_position` | `0xD0` | f32, 4 words | REFERENCE |
| CombatStateTrack | `animation_action_flags` | `CSChrActionFlagModule.animation_action_flags` | `0x10` | u32, 1 words | REFERENCE |
| CombatStateTrack | `action_modifiers_flags` | `CSChrActionFlagModule.action_modifiers_flags` | `0x40` | u64, 2 words | REFERENCE |
| CombatStateTrack | `damage_level` | `CSChrActionFlagModule.damage_level` | `0x1C` | u8, 1 words | REFERENCE |
| CombatStateTrack | `guard_level` | `CSChrActionFlagModule.guard_level` | `0x20` | u32, 1 words | REFERENCE |
| CombatStateTrack | `received_damage_type` | `CSChrActionFlagModule.received_damage_type` | `0x34` | u32, 1 words | REFERENCE |
| CombatStateTrack | `turn_speed` | `CSChrActionFlagModule.turn_speed` | `0x84` | f32, 1 words | REFERENCE |
| CombatStateTrack | `lock_on_turn_speed` | `CSChrActionFlagModule.lock_on_turn_speed` | `0x88` | f32, 1 words | REFERENCE |
| CombatStateTrack | `joint_turn_speed` | `CSChrActionFlagModule.joint_turn_speed` | `0x8C` | f32, 1 words | REFERENCE |
| CombatStateTrack | `facing_angle_correction_rad` | `CSChrActionFlagModule.facing_angle_correction_rad` | `0xA8` | f32, 1 words | REFERENCE |
| CombatStateTrack | `root_motion_div` | `CSChrActionFlagModule.root_motion_div` | `0xAC` | f32, 1 words | REFERENCE |
| CombatStateTrack | `root_motion_mult_min_dist` | `CSChrActionFlagModule.root_motion_mult_min_dist` | `0xB0` | f32, 1 words | REFERENCE |
| CombatStateTrack | `speed_default` | `CSChrActionFlagModule.speed_default` | `0x94` | f32, 1 words | REFERENCE |
| CombatStateTrack | `speed_extra` | `CSChrActionFlagModule.speed_extra` | `0x98` | f32, 1 words | REFERENCE |
| CombatStateTrack | `speed_boost` | `CSChrActionFlagModule.speed_boost` | `0x9C` | f32, 1 words | REFERENCE |
| CombatStateTrack | `root_motion_mult_target_radius` | `CSChrActionFlagModule.root_motion_mult_target_radius` | `0xBC` | f32, 1 words | REFERENCE |
| CombatStateTrack | `disable_lock_on_angle` | `CSChrActionFlagModule.disable_lock_on_angle` | `0x1E0` | f32, 1 words | REFERENCE |
| CombatStateTrack | `mov_dist_multiplier` | `CSChrActionFlagModule.mov_dist_multiplier` | `0x1F8` | f32, 1 words | REFERENCE |
| CombatStateTrack | `cam_turn_dist_multiplier` | `CSChrActionFlagModule.cam_turn_dist_multiplier` | `0x1FC` | f32, 1 words | REFERENCE |
| CombatStateTrack | `ladder_dist_multiplier` | `CSChrActionFlagModule.ladder_dist_multiplier` | `0x200` | f32, 1 words | REFERENCE |
| CombatStateTrack | `sa_durability_multiplier` | `CSChrActionFlagModule.sa_durability_multiplier` | `0x208` | f32, 1 words | REFERENCE |
| CombatStateTrack | `knockback_value` | `CSChrActionFlagModule.knockback_value` | `0x218` | f32, 1 words | REFERENCE |
| CombatStateTrack | `camera_lock_on_param_id` | `CSChrActionFlagModule.camera_lock_on_param_id` | `0x1E4` | i32, 1 words | REFERENCE |
| CombatStateTrack | `guard_behavior_judge_id` | `CSChrActionFlagModule.guard_behavior_judge_id` | `0x204` | u32, 1 words | REFERENCE |
| CombatStateTrack | `action_flags` | `CSChrActionFlagModule.action_flags` | `0x21C` | u32, 1 words | REFERENCE |
| CombatStateTrack | `weapon_model_location_overridden` | `CSChrActionFlagModule.weapon_model_location_overridden` | `0x78` | u8, 1 words | REFERENCE |
| CombatStateTrack | `sp_effect_wet_condition_depth` | `CSChrActionFlagModule.sp_effect_wet_condition_depth` | `0x20C` | u8, 1 words | REFERENCE |
| CombatStateTrack | `bullet_aim_angle_up_limit` | `CSChrActionFlagModule.bullet_aim_angle_up_limit` | `0x238` | i16, 1 words | REFERENCE |
| CombatStateTrack | `bullet_aim_angle_down_limit` | `CSChrActionFlagModule.bullet_aim_angle_down_limit` | `0x23A` | i16, 1 words | REFERENCE |
| CombatStateTrack | `bullet_aim_angle_right_limit` | `CSChrActionFlagModule.bullet_aim_angle_right_limit` | `0x23C` | i16, 1 words | REFERENCE |
| CombatStateTrack | `bullet_aim_angle_left_limit` | `CSChrActionFlagModule.bullet_aim_angle_left_limit` | `0x23E` | i16, 1 words | REFERENCE |
| CombatStateTrack | `bullet_aim_angle_up_dead_zone` | `CSChrActionFlagModule.bullet_aim_angle_up_dead_zone` | `0x240` | i16, 1 words | REFERENCE |
| CombatStateTrack | `bullet_aim_angle_down_dead_zone` | `CSChrActionFlagModule.bullet_aim_angle_down_dead_zone` | `0x242` | i16, 1 words | REFERENCE |
| CombatStateTrack | `bullet_aim_angle_right_dead_zone` | `CSChrActionFlagModule.bullet_aim_angle_right_dead_zone` | `0x244` | i16, 1 words | REFERENCE |
| CombatStateTrack | `bullet_aim_angle_left_dead_zone` | `CSChrActionFlagModule.bullet_aim_angle_left_dead_zone` | `0x246` | i16, 1 words | REFERENCE |

242 fields / 455 raw words; masks distinguish unavailable reads from actual zero. Appearance stores the native bounded 288-byte face buffer without decoding hair/colors/body semantics. No opaque memory-page dump, pointer-containing game object or executable content is saved. Nonfinite float bit patterns are preserved in raw tracks and reported as suspicious, not substituted. TransformTrack retains existing finite/quaternion validation.

Missing: PoseTrack; actual native proxy velocity/angular velocity; controller/proxy full internal state; HKS VM variables and behavior node/layer/blend weights; full per-state action vector contents; full active SpEffect list and bone attachments; independent animation transitions within a single callback interval. These are UNAVAILABLE or NOT YET IMPLEMENTED, not zero-valued synthetic states.

## SDK root access offsets (REFERENCE only)

```text
ROOT PlayerIns.chr_ins offset=0x0 confidence=REFERENCE
ROOT PlayerIns.chr_asm offset=0x638 confidence=REFERENCE
ROOT PlayerIns.player_game_data offset=0x580 confidence=REFERENCE
ROOT ChrIns.chr_ctrl offset=0x58 confidence=REFERENCE
ROOT ChrIns.modules offset=0x190 confidence=REFERENCE
ROOT ChrInsModuleContainer.physics offset=0x68 confidence=REFERENCE
ROOT ChrInsModuleContainer.behavior offset=0x28 confidence=REFERENCE
ROOT ChrInsModuleContainer.time_act offset=0x18 confidence=REFERENCE
ROOT ChrInsModuleContainer.event offset=0x58 confidence=REFERENCE
ROOT ChrInsModuleContainer.action_request offset=0x80 confidence=REFERENCE
ROOT ChrInsModuleContainer.behavior_data offset=0xC0 confidence=REFERENCE
ROOT ChrInsModuleContainer.data offset=0x0 confidence=REFERENCE
ROOT ChrInsModuleContainer.fall offset=0x70 confidence=REFERENCE
ROOT ChrInsModuleContainer.super_armor offset=0x40 confidence=REFERENCE
ROOT ChrInsModuleContainer.toughness offset=0x48 confidence=REFERENCE
ROOT ChrInsModuleContainer.action_flag offset=0x8 confidence=REFERENCE
```

These are source layout anchors. Existing production transform path is retained; additional module/equipment/appearance correctness still requires runtime correlation.


## Concrete source anchors (implementation HEAD 3d97070)

Lines refer to original code, not the appended prose. Long lines are truncated for display; inspect source before edits.

### `src/erplay.hpp`

```text
src/erplay.hpp:17: struct Quaternion { float x{}, y{}, z{}, w{1.0f}; };
src/erplay.hpp:18: struct Vec3 { float x{}, y{}, z{}; };
src/erplay.hpp:19: struct Sample {
src/erplay.hpp:29: struct Metadata {
src/erplay.hpp:39: struct Summary {
src/erplay.hpp:53: enum class RecordingState { idle, recording, paused, saving, ready, error };
src/erplay.hpp:55: // Incremental .tmp writer. finalize() validates the completed stream and atomically
src/erplay.hpp:57: class Writer {
src/erplay.hpp:67: [[nodiscard]] Summary finalize(std::uint64_t paused_duration_ns = 0);
src/erplay.hpp:70: struct Impl;
src/erplay.hpp:76: class RecordingSession {
src/erplay.hpp:98: class Reader {
src/erplay.hpp:118: struct Impl;
```

### `src/erplay.cpp`

```text
src/erplay.cpp:21: template<class T> void put(std::ostream& o, T v) {
src/erplay.cpp:26: template<class T> T get(std::istream& i) {
src/erplay.cpp:68: struct TypedChunk{std::uint32_t type{},flags{},count{};std::uint64_t bytes{};std::uint32_t crc{};};
src/erplay.cpp:92: struct Writer::Impl {
src/erplay.cpp:99: struct Captured{std::uint64_t time,source,sequence;CaptureFrame frame;};
src/erplay.cpp:127: void flush_chunk() {
src/erplay.cpp:147: auto& x=*impl_; if(x.done) throw std::logic_error("replay writer is finalized");
src/erplay.cpp:163: x.pending.push_back(s); ++x.count; if(x.pending.size()>=x.chunk_limit) x.flush_chunk();
src/erplay.cpp:165: void Writer::append_character(CharacterRecord r){auto&x=*impl_;if(x.done||x.metadata.format_version!=3||!x.has_sample||r.timestamp_ns>x.duration)throw std::runtime_error("character record outside active player timeline");x.characters.accept(r);x.pending_characters.push_back(r);if
src/erplay.cpp:166: void Writer::append_visual(VisualState s){auto&x=*impl_;if(x.done||x.metadata.format_version!=3||!x.has_sample||s.timestamp_ns>x.duration||!s.valid()||(s.id&&!x.characters.actors.contains(s.id)))throw std::runtime_error("invalid visual timeline/identity/schema");const auto old=x.
src/erplay.cpp:197: try { auto result=writer_.finalize(paused_total_ns_); state_=RecordingState::ready; return result; }
src/erplay.cpp:200: Summary Writer::finalize(std::uint64_t paused_duration_ns) {
src/erplay.cpp:201: auto& x=*impl_; if(x.done) throw std::logic_error("replay writer already finalized");
src/erplay.cpp:202: x.flush_chunk();
src/erplay.cpp:250: struct Reader::Impl {
src/erplay.cpp:251: struct Entry { std::uint64_t time{}, offset{}; std::uint32_t chunk{}; };
src/erplay.cpp:252: struct Chunk { std::uint64_t offset{}; std::uint32_t count{}; };
src/erplay.cpp:258: CharacterValidator characters;struct CharEntry{std::uint64_t time,offset;CharacterKind kind;std::uint32_t flags;};std::map<std::uint64_t,std::vector<CharEntry>> character_index;
src/erplay.cpp:341: if(summary.sample_count||summary.duration_ns) throw std::runtime_error("temporary file already has finalized header data");
```

### `src/capture_track.hpp`

```text
src/capture_track.hpp:8: struct CaptureRecord {
src/capture_track.hpp:14: template<class T>inline void capture_put(std::ostream&o,T value){o.write(reinterpret_cast<const char*>(&value),sizeof(value));if(!o)throw std::runtime_error("capture write failed");}
src/capture_track.hpp:15: template<class T>inline T capture_get(std::istream&i){T value{};i.read(reinterpret_cast<char*>(&value),sizeof(value));if(!i)throw std::runtime_error("capture truncated");return value;}
```

### `src/character_visual.hpp`

```text
src/character_visual.hpp:11: struct VisualState {
src/character_visual.hpp:20: template<class T>inline void visual_put(std::ostream&o,T v){for(unsigned j=0;j<sizeof(T);++j)o.put(char((std::uint64_t(v)>>(j*8))&255));if(!o)throw std::runtime_error("visual write failed");}
src/character_visual.hpp:21: template<class T>inline T visual_get(std::istream&i){std::uint64_t v=0;for(unsigned j=0;j<sizeof(T);++j){auto c=i.get();if(c==EOF)throw std::runtime_error("truncated visual record");v|=std::uint64_t(static_cast<unsigned char>(c))<<(j*8);}return static_cast<T>(v);}
```
