# Phase8 — Player capture fidelity 1

Status: IMPLEMENTED — RUNTIME VALIDATION REQUIRED. No new real gameplay recording has been obtained with this DLL yet. Synthetic fixtures only prove storage/reader behavior. Expanded recording does NOT yet reproduce animations: existing playback remains transform-only unless an earlier explicit experimental mode is chosen.

## Implemented

242 named fields, nine continuous raw-state tracks, in addition to existing transform, action events, character and visual tracks:

| ID | Track | Retained data |
|---|---|---|
| 5 | PhysicsTrack v1 | Physics position/last position/interpolated orientation, Euler/additional rotation, gravity/motion modifiers, contact/fall/sync flags; full controller physics/model/squared matrices, additional quaternion, offsets, scale, ragdoll/IK/undulation state |
| 6 | AnimationTrack v1 | All 10 public TAE queue entries (ID, play time, length); read/write indices; speed, behavior root motion, ground touch, ankle limits; requested/default animation IDs, ladder/event flags |
| 7 | ActionTrackRaw v1 | Current/previous/pressed/released/queued/disabled/possible/cancel/readback native bits; 16 held-action timers; queue current TAE, overrides/mode; native gesture/action IDs |
| 8 | LocomotionBehaviorTrack v1 | HKS animation/root multipliers, turn speed, twist flags, modifier action/HKS flags and movement limit/root-motion reduction; movement duration; ground normal/material/slope/slide state |
| 9 | EquipmentTrack v1 | All 22 native equipment param IDs; six weapon/arrow/bolt slot selectors; arm style |
| 10 | AppearanceTrack v1 | Full public 288-byte face buffer, gender/archetype; semantic decoding UNKNOWN |
| 11 | GameplayStateTrack v1 | HP/FP/stamina current/base/max, recoverable HP/time, poise durability/toughness and recovery, broken/update flags, stamina regen remainder/modifier, draw/init/block metadata |
| 12 | EffectSignalsTrack v1 | Item cast/fire/effect SFX IDs and queued native item ID. These are signals, not the full live SpEffect/VFX inventory |
| 13 | CombatStateTrack v1 | Lock flag/target position; animation/action/modifier flags; damage/guard IDs; aim/turn/root-motion/attack movement multipliers and weapon model override flags |

Detailed source paths and compiler-derived offsets: PHASE8_CAPTURE_FIELDS.md and capture-sdk-layout.log. Field semantics in the SDK are source claims. Position already RUNTIME_READ_VERIFIED on this target; most newly read fields remain REFERENCE. No new field is called PRODUCTION_CAPTURE before a genuine replay is inspected and correlated with user actions.

## Pipeline and safety

Exact existing 2.7.0.0 file/product/AMD64/full disk SHA guard and pinned SDK/Cargo.lock/task registration unchanged. No new game-memory writes, engine calls, patches, camera hooks or animation driver were added. Existing approved experimental replay features are retained; use recording controls only for this test.

Current local player is reacquired on the verified game callback. New field bytes are copied with Windows ReadProcessMemory; owner pointers must match current ChrIns for owned modules and ChrCtrl. Modifier additionally must own current controller. Unreadable/mismatched fields are marked unavailable. Raw enum/boolean/float bytes are not dereferenced as assumed valid enum values; raw NaN/Inf are retained with readable masks for investigation. This reduces fault risk but does NOT establish semantic/layout correctness or atomic coherence with Havok worker threads. Equipment/appearance reuse the existing typed PlayerIns paths; their complete current layout and appearance semantics require live checks.

The callback copies one coherent transform/action/fidelity packet into a bounded FIFO, using try_send. Pipe worker owns pipe I/O and serializes complete frames. This replaces the player's latest-value/coalescing delivery for capture. Public latest-sample API still uses the existing paired seqlock. Queue default 4096 callbacks (about 8 MiB data, not a recording duration cap), configurable using THEATER_CAPTURE_QUEUE_FRAMES before launching the game. Overflow is counted, carried in file records, logged, displayed and reported; perfect losslessness is NOT promised. Disk writer still streams chunks of 600 samples and does not keep the entire recording in RAM. No compression, delta reduction or reduced fidelity sampling added.

## Format and compatibility

ERPLAY02/03 readers retained. ERPLAY03 optional typed tracks IDs5..13 have immutable v1 layouts defined in capture_schema.json. Old ERPLAY03 readers can checksum and skip them; old files produce UNAVAILABLE in new inspector. Future incompatible layouts must use new track IDs or a new format version, never silently reinterpret these IDs.

Existing TRAK header: u32 marker, u32 type, u32 flags=0, u32 count, u64 payload_bytes, u32 CRC32. A capture record: u64 replay_timestamp_ns, u64 source_timestamp_ns, u64 native sample sequence, u64 cumulative DLL FIFO drop counter; followed by local availability u32 masks and raw u32 words for that track. Little endian. Record sizes and field offsets are in capture_schema.json. One state per source callback, all nine tracks share time/source/sequence. Floats are IEEE754 bits, u64 requests occupy low/high words, u8/i16 are zero-padded native bytes. No raw pointers.

Main sample IPC v3: existing 104-byte v2 prefix, reserved=1896 followed by capture schema1 header (u32 schema=1, u32 words=455, u64 drops), 15 availability words, 455 raw words. Total 2000 bytes. Host also accepts v1/v2 samples. New DLL requires the matching host; old host rejects v3. This fidelity build refuses new recording on old v1/v2 sample streams instead of silently saving transform-only data. Replay control/editor IPC is unchanged, as are ReplayPlayer/interpolation/playback worker/YAFSML.

CRC, record length, masks, per-track timestamp/source/sequence/drop ordering and replay duration are validated. Finalizing a capture-fidelity-v1 session also requires all nine track counts to equal accepted player samples. Recovery retains complete checksum-valid capture chunks; an interrupted last batch may have fewer/missing tracks and is reported honestly through counts.

## Diagnostics and viewer

Recorder shows fidelity frames and source queue drops. Inspector -> Player capture tracks shows per-track counts, playhead state, raw fields and unavailable masks. No rendering/reconstruction inference.

%TEMP%/TheaterModeGame.log includes CAPTURE_FIDELITY, FIELD compiler offset inventory and once-per-second CAPTURE_STATS (callback count, mean field-read microseconds, available words, drops). Mean read cost is NOT measured game FPS impact. Actual engine performance, values, drop rate and real replay size remain unmeasured until live test.

`python inspect_capture.py <replay.erplay>`: counts, observed bytes/sec and changes. `--time 10`: complete raw state near 10 seconds. `--export-jsonl capture.jsonl`: every captured record and named field with hex bits, decoded primitive and confidence. Parser uses streaming chunks and bounded latest-state summaries. Exports may be large.

## Pose investigation and exact blockers

Pinned SDK ChrInsModuleContainer.bonemove and ChrCtrl.animation_ctrl/joint_modifier are opaque/private; no public bone array/pose lifetime/count/layout. CSChrBehaviorModule's intermediate context and Havok pose/blend buffers are opaque. Ghidra RTTI finds CSFD4LocationHkaPoseImporter at preferred VA 0x143cf5668 and a name at 0x142b703b8; exported name refs are empty. Presence of a name is not a live-player pointer chain. hkbCharacter RTTI and the older behavior+0x10/+0x30 reference also lack verified ownership/ABI. No bone read or bone count is invented. Next step is importer constructor/vtable/callers -> animation controller ownership -> read-only pose lifecycle/count instrumentation on exact 2.7.0.0.

Known ChrCtrl disassembly at 0x1403c8bb0 ORs [rbx+0xfc] with 3 and stores SIMD at +0x100, supporting the existing sync-flag anchor, not proving complete proxy/ground restoration. Physics root_motion fields are private; public behavior root_motion is retained and explicitly distinct. Native angular/proxy velocity is unknown; velocity may be derived offline from timestamps but must be labeled DERIVED.

Action-request DLVector stores private first/last/end pointers; direct SDK slice construction assumes valid allocation/lifetime. Full variable queue entries need checked metadata/lifetime handling before promotion. SpecialEffect's linked-list head/next are private; public iterator follows unchecked links. Full buffs/effects, cycles/lifetime, start/end semantics and attachment bones remain blocked. Model modifier list has private payloads and pointer-based names. No blind memory-page/pointer dump substitutes for those tracks.

## References inspected

Pinned KamiyamaShiki0704/fromsoftware-rs 3c8c1d7 public fields + exact compiler layouts; local vswarte/fromsoftware-rs time_act layout; FreecamMod game-data headers; EldenRingHKS c0000.hks/MoveSpeedLevel/ExecEventHalfBlend; libER README/versioned model; old NightFyre SDK and FriXeee internal headers (no trusted 2.7.0.0 pose path); prior TGA/Hexinton anchors and read-only Hexinton XML. Existing local source inventory/commits are in NIGHTLY_RESEARCH_FINDINGS.md and research/symbols_2_7_0_0.json. These references do not authorize old offsets; no SDK pin was changed or proprietary binary/source redistributed. Public sources: https://github.com/KamiyamaShiki0704/fromsoftware-rs ; https://github.com/soulsmods/EldenRingHKS ; https://github.com/Logersnamed/FreecamMod . Ghidra partial analysis is not ground truth.

## Build and live test

Build: scripts/Build-CaptureFidelity.ps1; output Phase8_PlayerCapture_Fidelity1, separate from all preserved Phase5/Phase7 builds. Release x64; automated tests passed: CTest 14/14, Rust 28/28, Python research 3/3 + capture parser 4/4; separate real-device DX12 smoke passed. Synthetic tests never count as gameplay.

1. Close Elden Ring and the old host before selecting a new DLL. Never hot-swap a loaded DLL.
2. Launch Phase8_PlayerCapture_Fidelity1/EldenRingTheaterMode.exe. Use its existing launcher/YAFSML workflow with the sibling TheaterMode.dll. If using manual YAFSML, point its DLL setting to this new DLL, not Hotfix6.
3. Load the already-used save on safe flat indoor floor. Wait CONNECTED and PLAYER FOUND.
4. Press F5 or Start. Confirm RECORDING and fidelity frame counter increasing; keep Play/Replay inactive.
5. Record 60–90 seconds: idle 5s -> walk/run -> sprint -> rotate -> roll/backstep -> jump/land if safe -> light/heavy attack -> guard -> weapon slot switch -> safe item/gesture -> idle 5s. Note approximate action times. Do not provoke death/combat just to test this checkpoint.
6. F6/Stop. Require READY, saved .erplay and valid finalization. Open the NEW file; Inspector -> Player capture tracks should report roughly one record per player sample for all nine tracks. Unavailable fields are allowed but must be investigated; NaN/Inf raw states need correlation, not silent acceptance as meaningful engine data.
7. Send replay pathname, approximate action sequence/times, %TEMP%/TheaterModeGame.log and %LOCALAPPDATA%/EldenRingTheaterMode/logs/TheaterModeRecorder.log. I will run the parser, verify track counts/order/CRC/drop/gap/rate and correlate fields. Do not call animation replay verified; this build only captures its raw inputs/state.
