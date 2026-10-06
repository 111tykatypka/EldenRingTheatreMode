# Engine research

Snapshot: 2026-10-06, implementation `3d97070`. Target only Elden Ring 1.17 / 2.7.0.0 / AMD64, SHA-256 D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134. Native addresses are preferred-image VAs at base0x140000000, not ASLR runtime pointers. See04a tables.

## Evidence method

Use SQLite candidate search → exact disassembly/xrefs/callers/callees/RTTI/vtables → pinned SDK correlation → read-only runtime instrumentation → controlled experiment → binding → feature. Ghidra export is partial: about349625functions,134900pseudocode lines,3446364references,42455warning entries, one-hour analysis timeout. Pseudocode is not ground truth. Linear disassembly may include neighboring functions/padding; missing xrefs and indirect calls are real limitations.

## Established player access

Preserve CSTaskImp → ChrIns_PostPhysics → WorldChrMan → main_player → chr_ins.modules.physics → position/quaternion. Actual task signature resolved uniquely and recurring callbacks/read sampling were user verified at approximately60Hz. Every callback reacquires the player. Mutable access uses public PlayerIns::local_player_mut()/WorldChrMan::instance_mut(), not stale pointers, const casts or guessed offsets.

Shared GameProfile validates actual executable path/file version/product version/PE architecture/on-disk SHA before access. Static probe and DLL share the validator. Disk hash is not mapped PE/text hash; image base is separately identified. Other builds fail closed.

## Native recorder and ghost pipeline

ReplayRecorder uses linked active/free nodes, not a proven conventional ring buffer. Capacity+40=60; live count+44 ranged0..59. Nodes contain primary/secondary length-delimited256-byte payloads and time/reference/conversion fields. They encode more than transforms, but full bone/equipment/items/damage/actions remain unresolved. Native ReplayData0x238 holds reference count, block counts/pointers and decoder cursors. ReplayManipulator drives native control/events through native APIs rather than merely copying XYZ.

Creation must follow native metadata/serialization/allocator/decoder/factory/world insertion/activation/task registration. Removal must disable/unregister, erase active vector, clear ChrSetEntry, enqueue DelayDelete, then native destructors/reference releases/allocator free. Never construct a ghost/entry manually, set manipulator enabled gate directly or call raw destructors for cleanup.

Scheduler/lifetime static work supports examined synchronous task descendants and deferred deletion ordering. Examined event string history deep-copies its data rather than retaining actor/temp pointers. This is bounded static evidence, not universal proof or successful live spawn.

## Latest prototype and observed failure

- `ccc3d1b`: one-shot create/remove feature.
- `843c482`: persistent status HUD.
- `3d97070`: guarded native-step completion consumption.

PID33112: hooks installed, bridge initialized, original local native callsite observed, recorder ready, F10 requested, F11 cancelled. Subsequent F10 refused one-attempt guard. PID24580: context_seen1 and recorder ready, F10 requested then60s timeout. **No successful owned ghost, activation or teardown has been observed.** User screenshot confirms visible red TIMEOUT HUD.

Original consumption waited for703f37→7048e0 builder branch. Disassembly gates this on child+38 countdown<=0. Older logs did not capture recurring step rate/countdown, so exact runtime values remain Unverified. The error text saying context not observed was misleading: startup context was observed; later command was not consumed.

Latest fix also consumes after original703e30 returns, only genuine TestNetStep return140b08422, previously observed builder invocation, finite nonnegative native dt, callback-local context. Original builder path remains; compare/exchange permits one consumer. No timer/debug flag changes. New counters: steps_since_request, periodic_build_calls, last_step_age_ms, native_timer, context_seen. If genuine callbacks stop, prototype still fails closed. **This execution placement requires live validation.** Do not bypass it with PostPhysics/IPC spawning.

## Safety, ABI and ownership

Default OFF, one attempt per process. F10 creates, F11 removes or cancels pending create; game foreground required. Feature native-replay-ghost-create-remove includes read-only mode that blocks legacy writes. Do not combine payload-capture feature with it.

Exact shared identity guard plus26 SHA-derived16-byte fingerprints. Microsoft x64 ABI; third float dt argument inXMM2. Serializer1406f2410 has fifth alternate-metadata stack argument; native allocator141ebbcd0 requires allocator inR8. Native metadata ctor/fill and serializer/allocator/decoder produce genuine ReplayData; no ERPLAY parser or ERPLAY→native codec in DLL.

Preconditions: world/transition/player, recorder owner/vtable/pool/head/capacity, finite pose, player/recorder same block, empty ghost set. Reject alternate metadata/decoded secondary stream because factory can create paired mount actor. Serialization mutates recorder, as native code does; no competing consumers.

Owned runtime record includes epoch/world/entry/slot/handle/actor/data/manipulator. Retirement zeros all pointers and handle before native removal. Tombstones are numeric correlation identities only, never dereferenced/re-resolved; cleared on destructor/final release against ABA confusion. World change/ownership mismatch retire without guessed destruction. Unexpected active-object destruction retires before calling original destructor.

Hooks observe activation/disable/enqueue/ghost/manip destructors/native release/actor deleter. Data destructor and allocator free are not independently detoured. Deleter return supports a statically synchronous sequence returning, not measured leak freedom.

## Other engine subsystems

|Subsystem|Evidence and limitations|
|---|---|
|Entity enumeration|SDK/world character sets support read-only nearby identity/transform/presence capture; existing-actor driving experimental. No complete spawn lifecycle replay|
|Locomotion/animation|TAE ID/time observations, animation request prototype and phase-wrap cycle retrigger. User transform replay slid without animation; full HKS/native ownership unresolved|
|Grounding/proxy|Legacy sync/root motion work; flat indoor user test had no falls/jerks. BlockPosition/origin/Havok rebasing still unresolved for arbitrary scenes|
|Input ownership|Guarded local flags restore on stop/lease/player loss. Not universal isolation proof. Editor input focus is separate; no permanent HKS patch|
|Camera position/matrix/FOV/roll|IGCS/FreecamMod references demonstrate feasibility; current Theater camera feature not verified|
|DOF/focus/aperture/shake/motion blur|No verified backend. Never add fake postprocessing controls|
|Time control|Offline ReplayClock speeds and stepping implemented. Native global speed probes/research are not verified cinematic timestop with moving camera|
|World streaming|No verified independent far-camera streaming anchor or arbitrary cross-map restore|
|HUD|Overlay/editor/native status implemented; arbitrary game HUD hiding not verified|
|Offline ghosts|Owner reports no naturally available bloodstain ghost. Recorder/local native context can be observed; local creation still Unverified|

## Dead ends and corrections

Input-only determinism rejected due AI/physics/RNG. Old SDK layouts not preferred over exact pinned2.7.0.0. Manipulator owner+a0 was wrong, corrected+a8. Wrong polymorphic get_type ABI avoided in favor of static literal getter evidence. Serializer/allocator arguments corrected from instructions. Disk-initialized purevirtual deleter is not runtime initialized vtable. Guessed slot-cleanup chain replaced by actual3c3830. No raw gate/debug flags/fallback spawn to fabricate success.

Detailed authored evidence appendices below preserve exact scope and uncertainty. Older statuses are historical and superseded by latest findings above.

---

## Evidence appendix: `research/REPLAY_RECORDER_LAYOUT.md` (repository authored evidence snapshot)

Historical statuses retain their original scope. Latest Step placement/26 guards/observed timeouts in the opening sections supersede older builder-only and untested-callsite wording. Static proof does not mean successful native creation.

# ReplayRecorder layout — exact-build research

[STATIC_VERIFIED] Player initialization VA140657840 allocates0x860, constructs via1404e4f70 and stores at PlayerIns+5c8. Native object exceeds pinned SDK's0x70 prefix. Destructor1404e5310 frees0x860. Vtable RVA2a4aa40.

| Offset | Interpretation | Evidence |
|---|---|---|
|10|owning PlayerIns pointer|constructor/update + SDK|
|18|pool allocation|pool initializer1404e4da0; subobject mapping high confidence|
|20,28|active head/tail candidates|insert/evict code; runtime required|
|30,38|free head/tail candidates|pop/recycle code; runtime required|
|40|pool capacity candidate|allocation/eviction; SDK name max_frame_rate misleading|
|44|active node count candidate|increment/eviction; runtime required|
|48|float accumulator|ADDSS/load/store in1404e5af0; SDK u32 frame_duration misleading|
|4c..58|oldest/reference XYZ + yaw|node-copy code; coordinate semantics unresolved|
|5c|block identity candidate|copied native field; not decoded map id|
|60..6c|unknown counters/state|raw capture only|
|70,2c0|primary serializer pair candidates|1404e5af0 call sites|
|460,6b0|secondary serializer pair candidates|same call sites|
|850,851|flags/reset candidates|constructor/update bytes|

[STATIC_VERIFIED]140660ac0 loads player+5c8 and calls1404e5af0 with task-data float delta. It checks player+69c bit0, subtype predicate and virtual query first. This is an update entry, not yet proof of precise task registration. Native ChrIns constructor1403e6e30 initializes task at+4d8 and callback at+500 to VA1403f92b0; pinned SDK tasks differ by8 in this tail. Follow callback before assigning task-group semantics.

[STATIC_VERIFIED]1404e5af0 tests accumulator against approximately1/6s plus state-change conditions; this is not proof of fixed60Hz frames. Capacity getter1406f1e30 computes a duration-derived count; exact NETWORK_PARAM mapping and rolling retention must be observed.

Read-only probe captures full object only after verified profile, follows nodes only if recorder vtable+owner match. No call to serializer1404e5530: it evicts a node and is not read-only.

## Evidence rules

Exact target disk SHA256: D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134; version2.7.0.0, AMD64, patch1.17. VAs assume preferred image base0x140000000; runtime addresses are ASLR base+RVA.

STATIC_VERIFIED means exact image bytes support the stated operation; REFERENCE means SDK/source only. At the initial checkpoint runtime measurements were absent; subsequent read-only results are in NATIVE_REPLAY_RUNTIME_RESULT.md. Decompiler names/argument reconstruction are hypotheses. Linear extraction may include padding/adjacent functions after RET; these bytes do not belong to the preceding function automatically.

Evidence artifacts (kept outside repository): `../research/ghidra-eldenring/targeted/native_bloodstain/verified_bytes/<VA>.json` include exact bytes, indexed callers/callees, pseudocode and disassembly. Partial Ghidra analysis does not guarantee complete xrefs.

## Update task chain cross-check

[STATIC_VERIFIED] ChrIns ctor1403e6e30 initializes task+4d8, owner+4f8, callback+500=1403f92b0. The callback loads dt from actor+b0, constructs task data and invokes actor vtable+c0. Exact PlayerIns vtable142a7fbb0+c0 AND ReplayGhostIns vtable142a4c558+c0 both point to140660ac0, which calls1404e5af0 through player+5c8. The pinned SDK names the corresponding task update_replay_recorder_task; actual task-group scheduling/order remains runtime-unverified. No constructor/callback invocation is performed by the probe.

## Runtime observation — 2026-10-06

[RUNTIME_VERIFIED]139 recorder snapshots over139.1788874 seconds. Vtable and owner matched all snapshots. Capacity+40 remained60; count+44 ranged0..59. All556 checked head/tail/free pointers were inside60*0x248 pool and stride-aligned. Primary tail payload sizes72..206 bytes,121 distinct payload hashes; secondary size0 throughout. Changes prove genuine native data population, not decoded actions. Observation1Hz cannot establish exact frame rate; duration0..0.18333384 and state-triggered capture are consistent with static accumulator logic.26 count decreases include near-capacity and reset/eviction cases; retention is not simply total captured count.


---

## Evidence appendix: `research/REPLAY_MANIPULATOR.md` (repository authored evidence snapshot)

Historical statuses retain their original scope. Latest Step placement/26 guards/observed timeouts in the opening sections supersede older builder-only and untested-callsite wording. Static proof does not mean successful native creation.

# ReplayManipulator

[STATIC_VERIFIED] RTTI + COL identify vtable RVA2a2eda0. Constructor VA1403deaa0; allocation0x150. Type method VA1403def60 is `mov eax,3; ret`. Pinned SDK declares a reference-return ABI: do not invoke that declaration. Probe reads literal opcode instead.

[STATIC_VERIFIED] attach VA1403df010 refcounts incoming data and stores it at+100. Mode byte+14c chooses data+220/+230, then calls1403dfd50. That function chooses count at data+d0 or+224 and sums results from1403dfdc0. Frame decoding/advance1403dfdc0 still needs full disassembly correlation.

Owner is +a8, supported by pinned aligned layout, exact native constructor and runtime snapshots. Original probe incorrectly used +a0; its owner_matches=false messages are diagnostic errors, not control ownership failures. +132 toggled by tiny methods1403dee00/1403deec0; these are enable/disable-like setters, NOT proven update methods. Do not mistake adjacent linear disassembly for their body.

Pad vtable RVA2a2e788 / constructor3d8670 / literal type1; Network RVA2a2e1b8 / constructor3d4a00 / type2 candidate; NetAI RVA2a2d858 / constructor3d2f00 / type4 candidate. Read actual actors before treating these as ownership behavior.

No manipulator swapping, initialization or update virtual calls implemented. Research build captures full ReplayManipulator only when RTTI and literal type agree, and reads attached prefix only after owner match.

## Evidence rules

Exact target disk SHA256: D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134; version2.7.0.0, AMD64, patch1.17. VAs assume preferred image base0x140000000; runtime addresses are ASLR base+RVA.

STATIC_VERIFIED means exact image bytes support the stated operation; REFERENCE means SDK/source only. At the initial checkpoint runtime measurements were absent; subsequent read-only results are in NATIVE_REPLAY_RUNTIME_RESULT.md. Decompiler names/argument reconstruction are hypotheses. Linear extraction may include padding/adjacent functions after RET; these bytes do not belong to the preceding function automatically.

Evidence artifacts (kept outside repository): `../research/ghidra-eldenring/targeted/native_bloodstain/verified_bytes/<VA>.json` include exact bytes, indexed callers/callees, pseudocode and disassembly. Partial Ghidra analysis does not guarantee complete xrefs.

## Concrete native consumer path

[STATIC_VERIFIED] ReplayManipulator vtable slot10 (+50) is1403df190. It checks enable+132, reads timing+108/+110/+114, advances frames via1403df650, applies movement through virtual+d8 and orientation via1403cdc20, then subtracts dt from remaining frame time and adds dt to elapsed playback. Calling/ABI/scheduling remains unverified.

[STATIC_VERIFIED]1403df650 loads attached data+100, obtains block via1406f1e60/1406f1e90, decodes via1404e9490 and related functions; it calls actor/module routines1404236f0 and140423180..140423090 and actor virtual+220. These calls establish richer native control than XYZ writes; exact action/equipment semantics not yet proven.

[STATIC_VERIFIED] primary attached data count+d0, buffer pointer+d8; secondary count+224, buffer+228. Both getters index fixed0x104 blocks. Each block begins u32 encoded length followed by0x100 payload; consumer1403dfdc0 builds bounded input and decoder1404e9490, validity1404e9940, float duration getter1404e9840 (decoder+2c). This serialized-array representation differs from live0x248 linked recorder nodes. Never reinterpret nodes directly as imported ghost data.


---

## Evidence appendix: `research/REPLAY_GHOST_ACTOR.md` (repository authored evidence snapshot)

Historical statuses retain their original scope. Latest Step placement/26 guards/observed timeouts in the opening sections supersede older builder-only and untested-callsite wording. Static proof does not mean successful native creation.

# Replay ghost actor and world positioning

[STATIC_VERIFIED] ReplayGhostIns RTTI/vtable RVA2a4c558; native object0x760; incoming data+740 retained by refcount. Factory/constructor chain documented in BLOODSTAIN_GHOST_PIPELINE.md. Alternate factory140403d50 → ctor1404f19c0 creates different/default configuration, not interchangeable.

[REFERENCE] WorldChrMan.ghost_chr_set and ChrType::BloodstainGhost names from SDK. Runtime type must be obtained from RTTI and manipulator; set membership alone is insufficient. Read-only probe samples up to512 ghost slots and one non-player distance actor; truncation logged. These are diagnostic budgets, not replay actor limits.

[HIGH CONFIDENCE] coordinate conversion14061f0c0 participates recorder builder; block ids in nodes help native world positioning. Exact local/world map resolution, loaded-area lifecycle, model/physics grounding and collision policy remain UNKNOWN. Do not feed absolute XYZ into unidentified codec.

Recommended eventual design: retain host timeline/container; independent native replay actor with genuine replay ownership and decoded native data. This is a candidate design only. Do not replace current stable player playback until ghost allocation/lifetime, streaming and consumer API verified.

## Evidence rules

Exact target disk SHA256: D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134; version2.7.0.0, AMD64, patch1.17. VAs assume preferred image base0x140000000; runtime addresses are ASLR base+RVA.

STATIC_VERIFIED means exact image bytes support the stated operation; REFERENCE means SDK/source only. At the initial checkpoint runtime measurements were absent; subsequent read-only results are in NATIVE_REPLAY_RUNTIME_RESULT.md. Decompiler names/argument reconstruction are hypotheses. Linear extraction may include padding/adjacent functions after RET; these bytes do not belong to the preceding function automatically.

Evidence artifacts (kept outside repository): `../research/ghidra-eldenring/targeted/native_bloodstain/verified_bytes/<VA>.json` include exact bytes, indexed callers/callees, pseudocode and disassembly. Partial Ghidra analysis does not guarantee complete xrefs.


---

## Evidence appendix: `research/BLOODSTAIN_GHOST_PIPELINE.md` (repository authored evidence snapshot)

Historical statuses retain their original scope. Latest Step placement/26 guards/observed timeouts in the opening sections supersede older builder-only and untested-callsite wording. Static proof does not mean successful native creation.

# Bloodstain / ghost pipeline

[STATIC_VERIFIED] PlayerIns records through+5c8 →1404e5af0 → state/event builders1404e6630/1404e65d0 → encoders1404e9f90/1404eb680 → two bounded node payloads. Export1404e5530 writes counts and length-prefixed blocks; caller1406f2410 adds actor state. Serialization is mutating.

[STATIC_VERIFIED] factory140404570 allocates0x760 → ReplayGhostIns constructor1404f1840. Constructor prepares player config through1404035b0 with kind3, calls PlayerIns ctor140650c90, retains data at ghost+740, obtains current manipulator, checks type3 and attaches same data through1403df010. Player factory14065db40 selects ReplayManipulator for config+24==3 and stores at player+588. This proves a native replay ghost/manipulator data attachment, not complete network bloodstain provenance.

RTTI identifies BloodstainGhostDownloadJob, BloodstainUploadJob, BloodstainListDownloadJob, FNBloodstain/FNBloodstainImpl. Their codec and route to140404570 are unresolved. Ordinary own-death recoverable-rune metadata must not be conflated with downloaded other-player ghost recording.

Runtime acceptance: recorder buffer changes with gameplay; observed ReplayGhostIns has type3 manipulator, owning actor matches, ghost+740 matches manipulator+100, attached buffers are bounded and evolve during playback. Recorder buffer population is runtime-verified (see NATIVE_REPLAY_RUNTIME_RESULT.md); ghost identity and playback remain unobserved. Offline session may contain no usable bloodstain ghost: record absence; do not enable online play for this experiment.

## Evidence rules

Exact target disk SHA256: D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134; version2.7.0.0, AMD64, patch1.17. VAs assume preferred image base0x140000000; runtime addresses are ASLR base+RVA.

STATIC_VERIFIED means exact image bytes support the stated operation; REFERENCE means SDK/source only. At the initial checkpoint runtime measurements were absent; subsequent read-only results are in NATIVE_REPLAY_RUNTIME_RESULT.md. Decompiler names/argument reconstruction are hypotheses. Linear extraction may include padding/adjacent functions after RET; these bytes do not belong to the preceding function automatically.

Evidence artifacts (kept outside repository): `../research/ghidra-eldenring/targeted/native_bloodstain/verified_bytes/<VA>.json` include exact bytes, indexed callers/callees, pseudocode and disassembly. Partial Ghidra analysis does not guarantee complete xrefs.


---

## Evidence appendix: `notes/NATIVE_GHOST_PROTOTYPE_IMPLEMENTATION_STATUS.md` (repository authored evidence snapshot)

Historical statuses retain their original scope. Latest Step placement/26 guards/observed timeouts in the opening sections supersede older builder-only and untested-callsite wording. Static proof does not mean successful native creation.

# Native Replay Ghost Prototype 1

Status: **IMPLEMENTED — RUNTIME VALIDATION REQUIRED**. Compilation is not lifecycle acceptance. No native create/remove has been run by this development session.

Release x64 EXE, DLL and compatibility probe build succeeded with MSVC 19.51 and the locked, offline Cargo dependencies. Existing warnings: shared GameProfile C4530; Rust unused import/dead code/crate naming. Automated tests were not run for this native lifecycle milestone. Runtime success remains entirely unverified.

## First user attempt and status HUD update

Observed log from PID33112: hooks installed, bridge initialization=1, native local callsite observed, PLAYER_RECORDER_READY, F10 REQUESTED, then F11 cancelled pending create. A subsequent F10 was refused by the one-attempt guard. No owned ghost/create/teardown is confirmed by this attempt.

Added a noninteractive native status HUD in the top-right corner, independent of Insert. Native command messages appear immediately; pending commands show a 60-second countdown. Errors/cancellation stay visible. Existing controls and lifetime guards are unchanged. The HUD does not capture mouse/keyboard when the editor is closed. Graphics/runtime appearance still requires user verification.

HUD checkpoint was staged in `../outputs/EldenRingTheaterMode/NativeReplayGhostPrototypeHUD`.

## Native-step command fix (latest package; supersedes builder-only consumption below)

PID24580 confirmed context_seen=1 and PLAYER_RECORDER_READY, then CREATE requested and timed out without consumption. The timeout text "context not observed" was misleading: context HAD been observed, but the periodic builder branch did not consume the new command within 60 seconds. Native 703e30 disassembly gates its 703f37 call on child+38 countdown. Previous logs did not measure countdown/ongoing step frequency, so their precise runtime values are not claimed.

Latest output: `../outputs/EldenRingTheaterMode/NativeReplayGhostPrototypeStep`. CREATE is now additionally consumed after original 140703e30 returns, ONLY with genuine TestNetStep caller return address 140b08422 (CALL140b0841d), prior observed 703f37 builder invocation, and finite nonnegative native dt. It uses callback-local native context, never cached context or an invented task phase. The original builder path remains; compare/exchange allows only one consumer. No timer/debug flag changes.

New timeout evidence: steps_since_request, periodic_build_calls, last_step_age_ms, native_timer and context_seen. If native steps cease after world load, this still fails closed. Post-update execution placement is experimental within the original native context and requires the controlled user test; no ghost success claimed. Added exact callsite fingerprint (26 guards total). The older builder-only description below is historical and superseded by this section.

## Scope and isolation

Branch: `codex/native-bloodstain-replay-research`. Rust adapter, pinned SDK/Cargo.lock, shared exact-build guard, host/launcher/YAFSML and recorder remain in place. New Cargo feature: `native-replay-ghost-create-remove` (includes the existing read-only feature to block legacy replay writes). Existing EXE UI is reused without changes. F10/F11 are developer commands inside the DLL, scoped to game foreground; the payload-capture feature must NOT also be enabled.

New canonical output: `../outputs/EldenRingTheaterMode/NativeReplayGhostPrototype` relative to the active checkout. Stable Phase5/Tester/Nightly are not overwritten. Isolated CMake and Cargo build directories; build script stages EXE, DLL, compatibility probe, source/binary hashes and these instructions.

## Execution context

No engine mutation in IPC, hotkey worker, Rust PostPhysics, or an invented task phase. MinHook observes the original `140703e30` entry and marks only that call's dynamic thread-local extent. CREATE is consumed ONLY in `1407048e0`, with both that context flag and the exact native return address `140703f3c` (CALL at `140703f37`). This substitutes one explicit local creation for that invocation's normal serialize/upload/debug branch; other invocations call the original unchanged. No debug flag or countdown is forced.

If that original callsite does not execute in this offline build/session, CREATE times out after 60 seconds and performs no native fallback. This is an explicit runtime uncertainty, not a claim that the offline callsite has been verified.

REMOVE is consumed at the original `14050efa0` world-removal queue drain and invokes the same `14050b340` removal routine that the drain uses. Ownership, current world, entry address, slot, actor and native handle must still match. Native lifecycle removes tasks, clears entry and queues DelayDelete. No direct ghost/manipulator destructor is called by Theater.

## Genuine native data

Reacquire WorldChrMan global and current main player in the native callsite. Check world transition state, recorder exact vtable/owner, nonempty recorder, finite transforms, recorder/player same BlockId and empty ghost set. Metadata comes from native ctor `1406514f0` and live game data fill `14025f810`; no fabricated arrays/metadata/ChrSetEntry. Secondary/mounted metadata is refused because `1406f27f0` can create a paired secondary actor. Reject a decoded secondary stream too.

Native path: `14025f7e0` → `1406f1ec0` → native allocator → `1406f2410` → `1404e5440` → native allocation → `1406f1bb0` → `141ebbfc0` → `1406f1f20` → `1406f27f0` → native `140507e60`/`140493a80`/`140404570`/`1404f1840`/`14065db40`/`1403deaa0`/`1403df010`.

**ABI corrections verified from instructions:** `1406f2410` has a fifth alternate-metadata argument in `[rsp+20]`; `141ebbcd0` has a third allocator argument in R8. The complete genuine native 0x238 object is allocated and constructed by those native functions, never assembled from ERPLAY. Temporary creator reference is retained/released with the native interlocked helpers and native deleting virtual ONLY on last-reference transition; this is temporary smart-reference cleanup, not forced ghost teardown. Native buffer is freed with its native allocator. Recorder serialization is a real mutating operation, just as in the original local path; do not simultaneously run another recorder consumer.

## Lifecycle guards and evidence

Default OFF. One create attempt per process (even a rejected attempt is not retried). One active guarded record: active, epoch, genuine native handle, actor/entry/data/manipulator/world and slot. Removal retires epoch and zeros ALL cached engine pointers/handle before entering native removal. World change and ownership mismatch invalidate state without attempting destruction.

Separate numeric tombstones correlate callback-local destruction/release arguments with the retired epoch. They are never dereferenced, used for handle lookup, or turned back into objects. Each identity is cleared at its observed destructor/final release so later address reuse cannot produce false lifecycle events. Unexpected direct destruction while the runtime record is active retires the record before calling the original destructor and logs the contradiction. No post-removal actor/refcount polling. World-owned slot is read after removal only if the current world still matches. Hook logs cover native activation (`1404f1c10`), disable (`1403deec0`), enqueue (`140e78ca0`), ghost destructor (`1404f1ab0`), manipulator destructor (`1403dec70`), release (`141ebc000`), actor deleter (`140e775e0`). Deleter return is evidence that its statically confirmed synchronous destructor/allocator-free sequence returned; allocator free itself is NOT independently detoured. Final release count is observed; data destructor/free is not independently detoured. Do not report either as separately measured hooks.

At creation/activation/removal entry: actor/entry/handle/ChrType/ctrl/primary/replay manipulator/vtable/type inference/gate/data/refcount/counts/cursors/BlockId/position/quaternion/slot and entry.actor comparison. Native activation owns gate132; Theater never sets it. Full field validity must be checked in actual logs, not assumed from compilation.

Exact file version/product/AMD64/SHA validation remains shared and precedes installation. 25 SHA-derived 16-byte entrypoint fingerprints reject patched entrypoints. All prototype hooks are created before enabling; installation failure removes only prototype hooks. No injection changes, game-file modification, networking, animation decoding, camera, world restore, or legacy transform work.

## Still unverified

- Original native local callsite runs in the user's offline session and supplies usable recorder data.
- Exact native ABI behavior, factory one-actor result, native activation and visual presence.
- No crash, clean entry clear, DelayDelete, destructor/refcount events, no stale access.
- No leak: no live allocation measurement yet; deleter-return log alone is not a leak test.
- Prototype 2 locomotion/playback accuracy is NOT implemented/verified in this task.

Do not proceed to Prototype 2 until the user's single create/remove test passes. A TIMEOUT means no ghost was created by the command, and is not permission to spawn from PostPhysics.


---

## Evidence appendix: `notes/FAILED_EXPERIMENTS.md` (repository authored evidence snapshot)

Historical statuses retain their original scope. Latest Step placement/26 guards/observed timeouts in the opening sections supersede older builder-only and untested-callsite wording. Static proof does not mean successful native creation.

# Failed findings / experiments

| Finding | Evidence | Decision |
|---|---|---|
| Grounding failed | Latest user's visible game test: floating/falling through geometry | Unresolved; new differential trace, no Y offset or fake grounding correction |
| NPC application failed | Latest user's real test; trajectory capture and host display work | Selected actor only + per-stage failure trace; no fabricated success |
| Locomotion failed | User sees rigid transform sliding | Native event pipeline research only; no arbitrary ID or fake WALK success |
| Debug layout cross-check failed | Rust `offset_of!(ChrIns,debug_flags)` 0x530, expected Freecam 0x538 | Flag experiments blocked; no guessed relocation; nightly removes conflicting flag writes |
| Legacy CSLuaEventManager signature ambiguous | Two target byte matches resolve different globals | Do not use; prefer typed reflected CSLuaEventMan read-only |
| TGA full Windows checkout failed | filename-too-long errors after successful object clone | Read committed reference with git show; no source changes |

noMove, noAttack, combined flags and noUpdate: **NOT RUN in game**, not described as failed native behavior. Debug-camera control-mode experiment: NOT IMPLEMENTED. animationSpeed=0: IMPLEMENTED/UNIT TESTED, **NOT RUN in game**. NoUpdate's documented freezing of physics/AI is not claimed to have been observed here.

Existing logs also contain player `TARGET_REJECTED detail=13` (target-step guard). This establishes a rejection, not its cause. New target/callback telemetry must distinguish legitimate motion, IPC gaps and overwritten state before changing the guard.


---

## Evidence appendix: `notes/PHASE5C_CHARACTER_DRIVING_SYSTEMS.md` (repository authored evidence snapshot)

Historical statuses retain their original scope. Latest Step placement/26 guards/observed timeouts in the opening sections supersede older builder-only and untested-callsite wording. Static proof does not mean successful native creation.

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


---

## Evidence appendix: `notes/PHASE5_PLAYER_ANIMATION_RESEARCH.md` (repository authored evidence snapshot)

Historical statuses retain their original scope. Latest Step placement/26 guards/observed timeouts in the opening sections supersede older builder-only and untested-callsite wording. Static proof does not mean successful native creation.

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


## Concrete source anchors (implementation HEAD 3d97070)

Lines refer to original code, not the appended prose. Long lines are truncated for display; inspect source before edits.

### `native_ui/NativeReplayGhostPrototype.cpp`

```text
native_ui/NativeReplayGhostPrototype.cpp:15: extern "C" void tm_render_native_status(const char*);
native_ui/NativeReplayGhostPrototype.cpp:21: void log(const char* fmt,...) { char text[2048];va_list a;va_start(a,fmt);vsnprintf(text,sizeof(text),fmt,a);va_end(a);logger(text);if(strncmp(text,"NATIVE_GHOST",12)==0)tm_render_native_status(text); }
native_ui/NativeReplayGhostPrototype.cpp:23: template<class T> T get(U a) { T v{};read(a,&v,sizeof(v));return v; }
native_ui/NativeReplayGhostPrototype.cpp:24: template<class F> F fn(U rva) {return reinterpret_cast<F>(base+rva);}
native_ui/NativeReplayGhostPrototype.cpp:26: struct NativeGhostRuntimeState {bool active{};uint64_t epoch{1},handle{UINT64_MAX};U actor{},entry{},data{},world{},manipulator{};uint32_t slot{};};
native_ui/NativeReplayGhostPrototype.cpp:92: log("NATIVE_GHOST_REMOVE: REQUESTED reason=%s retired_epoch=%llu next_epoch=%llu; runtime pointers invalidated; no retired-handle resolution",reason,retiredEpoch.load(),state.epoch);
native_ui/NativeReplayGhostPrototype.cpp:103: log("NATIVE_REPLAY_DATA: creator reference release old=%d new=%d",old,old-1);
native_ui/NativeReplayGhostPrototype.cpp:106: else if(old<1)log("NATIVE_GHOST_ERROR: invalid native creator refcount; no additional cleanup attempted");
native_ui/NativeReplayGhostPrototype.cpp:110: if(snapshot().active||!ready(w,player,recorder)||hasGhost(w)){log("NATIVE_GHOST_ERROR: create precondition failed (world/player/recorder/block/occupied ghost set)");return;}
native_ui/NativeReplayGhostPrototype.cpp:113: if(!assembly||!flags||!bufferAllocator||!objectAllocator){log("NATIVE_GHOST_ERROR: native metadata/allocator unavailable");return;}
native_ui/NativeReplayGhostPrototype.cpp:118: if(!fn<uint8_t(*)(U,void*)>(0x25f810)(assembly,metadata)){fn<void(*)(void*)>(0x3c12a0)(metadata);log("NATIVE_GHOST_ERROR: metadata fill failed");return;}
native_ui/NativeReplayGhostPrototype.cpp:122: if(alternate){fn<void(*)(void*)>(0x3c12a0)(metadata);log("NATIVE_GHOST_ERROR: secondary metadata present; one-ghost prototype refuses mounted/paired replay");return;}
native_ui/NativeReplayGhostPrototype.cpp:124: if(size<=0){fn<void(*)(void*)>(0x3c12a0)(metadata);log("NATIVE_GHOST_ERROR: native serialized size invalid=%d",size);return;}
native_ui/NativeReplayGhostPrototype.cpp:126: if(!buffer){fn<void(*)(void*)>(0x3c12a0)(metadata);log("NATIVE_GHOST_ERROR: native buffer allocation failed");return;}
native_ui/NativeReplayGhostPrototype.cpp:137: log("NATIVE_REPLAY_DATA: native decode=%u object=0x%llX bytes=%d primary=%u secondary=%u refcount=%d",decoded,data,written,get<uint32_t>(data+0xd0),get<uint32_t>(data+0x224),get<int32_t>(data+8));
native_ui/NativeReplayGhostPrototype.cpp:145: observe(actor,"NATIVE_GHOST_CREATE");
native_ui/NativeReplayGhostPrototype.cpp:146: log("NATIVE_GHOST: OWNED epoch=%llu; F11 REMOVE ONCE; activation is native, gate132 never written by Theater",snapshot().epoch);
native_ui/NativeReplayGhostPrototype.cpp:147: } else log("NATIVE_GHOST_ERROR: native factory returned no actor handle=0x%llX",handle);
native_ui/NativeReplayGhostPrototype.cpp:148: } else log("NATIVE_GHOST_ERROR: decoded data invalid/secondary present; factory not called");
native_ui/NativeReplayGhostPrototype.cpp:149: } else log("NATIVE_GHOST_ERROR: native replay-data allocation failed");
native_ui/NativeReplayGhostPrototype.cpp:150: } else log("NATIVE_GHOST_ERROR: native serialization/eligibility failed written=%d size=%d",written,size);
native_ui/NativeReplayGhostPrototype.cpp:164: log("NATIVE_GHOST_CREATE: one-shot consumed after original TestNetStep update; caller=140b0841d thread=%lu native_steps=%llu periodic_build_calls=%llu",GetCurrentThreadId(),stepCount.load(),buildCount.load());
native_ui/NativeReplayGhostPrototype.cpp:172: if(valid&&!contextSeen.exchange(true))log("NATIVE_GHOST: native local replay context observed at 140703f37; thread=%lu",GetCurrentThreadId());
native_ui/NativeReplayGhostPrototype.cpp:174: if(valid&&command.compare_exchange_strong(expected,0)) {log("NATIVE_GHOST_CREATE: one-shot command consumed in original local native callsite");create(context);return;}
native_ui/NativeReplayGhostPrototype.cpp:179: if(creating){factoryActor=actor;log("NATIVE_GHOST_CREATE: native world factory result=0x%llX",actor);}
native_ui/NativeReplayGhostPrototype.cpp:184: if(creating||(snapshot().active&&snapshot().actor==actor))observe(actor,"NATIVE_GHOST_ACTIVATE");
native_ui/NativeReplayGhostPrototype.cpp:189: if(ours){observe(actor,"NATIVE_GHOST_REMOVE");retire(actor,"native-world-removal");}
native_ui/NativeReplayGhostPrototype.cpp:195: log("NATIVE_GHOST_REMOVE: slot=%u world_entry_read=%u entry_actor_null=%u epoch=%llu",before.slot,readable,readable&&value==0,retiredEpoch.load());
native_ui/NativeReplayGhostPrototype.cpp:207: } else {log("NATIVE_GHOST_ERROR: remove ownership/world/entry mismatch; no raw destruction attempted");if(s.active)retire(s.actor,"ownership-mismatch");}
native_ui/NativeReplayGhostPrototype.cpp:211: U onDisable(U manip) {U result=disableOriginal(manip);if(retired(manip,retiredManipulator))log("NATIVE_GHOST_REMOVE: ReplayManipulator native disable returned epoch=%llu",retiredEpoch.load());return result;}
native_ui/NativeReplayGhostPrototype.cpp:212: void onEnqueue(U manager,U actor) {bool ours=retired(actor,retiredActor);enqueueOriginal(manager,actor);if(ours)log("NATIVE_GHOST_DELAYDELETE: native enqueue returned epoch=%llu",retiredEpoch.load());}
native_ui/NativeReplayGhostPrototype.cpp:213: U onGhostDestroy(U actor) {auto live=snapshot();if(live.active&&live.actor==actor){log("NATIVE_GHOST_ERROR: destructor reached before observed world removal; retiring epoch without cleanup fallback");retire(actor,"unexpected-direct-destruction");}bool ours=retired(actor,retiredAc
native_ui/NativeReplayGhostPrototype.cpp:214: U onManipDestroy(U manip) {auto live=snapshot();if(live.active&&live.manipulator==manip){log("NATIVE_GHOST_ERROR: manipulator destroyed while runtime record active; retiring epoch");retire(live.actor,"unexpected-manipulator-destruction");}bool ours=retired(manip,retiredManipulato
native_ui/NativeReplayGhostPrototype.cpp:215: int32_t onRelease(U counter) {U identity=retiredData.load();bool ours=identity!=0&&counter==identity+8;int32_t old=releaseOriginal(counter);if(ours){log("NATIVE_REPLAY_DATA: native release old=%d new=%d final_release=%u epoch=%llu",old,old-1,old==1,retiredEpoch.load());if(old==1)
native_ui/NativeReplayGhostPrototype.cpp:218: if(ours)log("NATIVE_GHOST_DELAYDELETE: native actor deleter entered epoch=%llu",retiredEpoch.load());
native_ui/NativeReplayGhostPrototype.cpp:220: if(ours)log("NATIVE_GHOST_DESTROY: native deleter returned; synchronous actor destructor+allocator-free path completed; no freed-pointer reads");
native_ui/NativeReplayGhostPrototype.cpp:230: if(s.active||command.load())log("NATIVE_GHOST_ERROR: create rejected: active/pending command");
native_ui/NativeReplayGhostPrototype.cpp:231: else if(used.exchange(true))log("NATIVE_GHOST_ERROR: one create attempt per process; restart before another test");
native_ui/NativeReplayGhostPrototype.cpp:232: else if(!ready(world(),player,recorder))log("NATIVE_GHOST_ERROR: CREATE refused: player/recorder/world/block not ready; restart required before retry");
native_ui/NativeReplayGhostPrototype.cpp:233: else {requestStepCount.store(stepCount.load());deadline.store(GetTickCount64()+60000);command.store(1);log("NATIVE_GHOST_CREATE: REQUESTED; waiting for native TestNetStep (maximum 60s); context_seen=%u steps=%llu periodic_build_calls=%llu",contextSeen.load(),stepCount.load(),buil
native_ui/NativeReplayGhostPrototype.cpp:237: if(command.compare_exchange_strong(pending,0))log("NATIVE_GHOST_REMOVE: pending create cancelled; no actor owned");
native_ui/NativeReplayGhostPrototype.cpp:238: else if(snapshot().active) {deadline.store(GetTickCount64()+60000);command.store(2);log("NATIVE_GHOST_REMOVE: command queued for native world removal drain");}
native_ui/NativeReplayGhostPrototype.cpp:239: else log("NATIVE_GHOST_ERROR: REMOVE refused: no active Theater-owned ghost");
native_ui/NativeReplayGhostPrototype.cpp:241: if(command.load()&&GetTickCount64()>deadline.load()) {auto kind=command.exchange(0);uint32_t raw=timerBits.load();float timer;memcpy(&timer,&raw,4);if(kind)log("NATIVE_GHOST_ERROR: command=%u TIMEOUT; steps_since_request=%llu periodic_build_calls=%llu last_step_age_ms=%llu native
native_ui/NativeReplayGhostPrototype.cpp:242: U p{},rec{};if(!readyLogged.load()&&ready(world(),p,rec)&&!readyLogged.exchange(true))log("NATIVE_GHOST: PLAYER_RECORDER_READY; F10 CREATE ONCE, F11 REMOVE ONCE (game focus); existing payload/legacy write controls disabled in this feature");
native_ui/NativeReplayGhostPrototype.cpp:252: for(const auto& p:nativeGhostFingerprints) {unsigned char bytes[16];if(!read(base+p.rva,bytes,sizeof(bytes))||memcmp(bytes,p.bytes,sizeof(bytes))){log("NATIVE_GHOST_ERROR: native entrypoint fingerprint mismatch RVA=0x%llX; feature OFF",p.rva);return 0;}}
native_ui/NativeReplayGhostPrototype.cpp:254: struct Hook{U rva;void* detour;void** original;};
native_ui/NativeReplayGhostPrototype.cpp:268: size_t created=0;for(auto& h:hooks){auto status=MH_CreateHook(reinterpret_cast<void*>(base+h.rva),h.detour,h.original);if(status!=MH_OK){log("NATIVE_GHOST_ERROR: hook create RVA=0x%llX status=%d",h.rva,status);break;}created++;}
native_ui/NativeReplayGhostPrototype.cpp:271: auto enabled=MH_ApplyQueued();if(enabled!=MH_OK){for(auto& h:hooks){MH_DisableHook(reinterpret_cast<void*>(base+h.rva));MH_RemoveHook(reinterpret_cast<void*>(base+h.rva));}log("NATIVE_GHOST_ERROR: hook enable failed=%d; feature OFF",enabled);return 0;}
native_ui/NativeReplayGhostPrototype.cpp:272: log("NATIVE_GHOST: experimental hooks installed; default OFF; exact SHA guard passed; one attempt per process; runtime validation REQUIRED");
```

### `native_ui/NativeGhostFingerprints.h`

```text
native_ui/NativeGhostFingerprints.h:4: struct NativeGhostFingerprint { uintptr_t rva; unsigned char bytes[16]; };
```

### `adapter/src/native_ghost_prototype.rs`

```text
adapter/src/native_ghost_prototype.rs:4: pub fn initialize() {
adapter/src/native_ghost_prototype.rs:7: unsafe extern "C" { fn tm_native_ghost_start(log:extern "C" fn(*const c_char),layout:*const usize)->i32; }
adapter/src/native_ghost_prototype.rs:8: extern "C" fn log(message:*const c_char) {
adapter/src/native_ghost_prototype.rs:16: crate::log_game(&format!("NATIVE_GHOST: bridge initialization={result}; feature=native-replay-ghost-create-remove; runtime UNVERIFIED"));
adapter/src/native_ghost_prototype.rs:19: pub fn initialize() {}
```

### `adapter/src/native_bloodstain.rs`

```text
adapter/src/native_bloodstain.rs:13: fn GetCurrentProcess()->*mut c_void;
adapter/src/native_bloodstain.rs:14: fn GetCurrentProcessId()->u32;
adapter/src/native_bloodstain.rs:15: fn ReadProcessMemory(process:*mut c_void,address:*const c_void,output:*mut c_void,size:usize,read:*mut usize)->i32;
adapter/src/native_bloodstain.rs:17: fn read(address:usize,size:usize)->Option<Vec<u8>>{
adapter/src/native_bloodstain.rs:22: fn u64_at(b:&[u8],i:usize)->Option<u64>{Some(u64::from_le_bytes(b.get(i..i.checked_add(8)?)?.try_into().ok()?))}
adapter/src/native_bloodstain.rs:23: fn u32_at(b:&[u8],i:usize)->Option<u32>{Some(u32::from_le_bytes(b.get(i..i.checked_add(4)?)?.try_into().ok()?))}
adapter/src/native_bloodstain.rs:24: fn pointer(b:&[u8],i:usize)->usize{u64_at(b,i).unwrap_or(0) as usize}
adapter/src/native_bloodstain.rs:25: fn hex(b:&[u8])->String{use std::fmt::Write;let mut s=String::with_capacity(b.len()*2);for v in b{let _=write!(s,"{v:02x}");}s}
adapter/src/native_bloodstain.rs:26: fn json_quote(s:&str)->String{format!("\"{}\"",s.replace('\\',"\\\\").replace('"',"\\\"").replace('\n',"\\n").replace('\r',"\\r"))}
adapter/src/native_bloodstain.rs:27: fn class_name(address:usize,base:usize,image_size:usize)->String{
adapter/src/native_bloodstain.rs:42: fn manipulator_kind(address:usize,base:usize,image_size:usize)->Option<u32>{
adapter/src/native_bloodstain.rs:48: fn literal_kind(code:&[u8])->Option<u32>{if code.len()!=6||code[0]!=0xb8||code[5]!=0xc3{return None;}let n=u32_at(code,1)?;(n<=7).then_some(n)}
adapter/src/native_bloodstain.rs:49: pub fn write_command(kind:u16)->bool{matches!(kind,3|5|6|7|11|14|15)}
adapter/src/native_bloodstain.rs:50: struct Row{prefix:&'static str,message:String,address:usize,bytes:Vec<u8>}
adapter/src/native_bloodstain.rs:51: struct Batch{timestamp:u64,rows:Vec<Row>,dropped:u64}
adapter/src/native_bloodstain.rs:52: #[derive(Default)]struct PayloadCapture{active:bool,start:u64,marker:usize,keys:[bool;2],callbacks:u64,observed_nodes:u64,last_tail:usize,last_player:usize,last_accum:Option<u32>,accum_changes:u64}
adapter/src/native_bloodstain.rs:55: fn GetAsyncKeyState(key:i32)->i16;
adapter/src/native_bloodstain.rs:56: fn GetForegroundWindow()->*mut c_void;
adapter/src/native_bloodstain.rs:57: fn GetWindowThreadProcessId(window:*mut c_void,pid:*mut u32)->u32;
adapter/src/native_bloodstain.rs:59: fn pool_node_valid(address:usize,pool:usize,capacity:u32)->bool{capacity>0&&capacity<=512&&address>=pool&&address.checked_sub(pool).is_some_and(|d|d%NODE_BYTES==0&&d/NODE_BYTES<(capacity as usize))}
adapter/src/native_bloodstain.rs:60: pub struct Capture{tx:Option<SyncSender<Batch>>,next:u64,base:usize,image_size:usize,dropped:u64,payload:PayloadCapture}
adapter/src/native_bloodstain.rs:62: pub fn new()->Self{
adapter/src/native_bloodstain.rs:82: pub fn tick(&mut self,now:u64){
adapter/src/native_bloodstain.rs:94: rows.push(Row{prefix:"REPLAY_RECORDER",address:recorder,message:format!("player=0x{address:X} recorder=0x{recorder:X} vtable_rva={:?} owner_matches={} prefix_names=REFERENCE raw40={} frame44={} raw48=0x{:08X} elapsed48_as_f32={} oldest={floats:?} yaw58={} block5c={} unknown=[8:{:
adapter/src/native_bloodstain.rs:96: let node=pointer(&raw,offset);if let Some(b)=read(node,NODE_BYTES){rows.push(Row{prefix:"REPLAY_FRAME",address:node,message:format!("role={role} len0={} len104={} elapsed208={} next240=0x{:X}; raw static node stride=0x248; payload interpretation UNKNOWN",u32_at(&b,0).unwrap_or(0)
adapter/src/native_bloodstain.rs:98: }else{rows.push(Row{prefix:"REPLAY_RECORDER",address:recorder,message:format!("player=0x{address:X} recorder=0x{recorder:X} absent_or_unreadable; NO buffer dereference"),bytes:Vec::new()});}
adapter/src/native_bloodstain.rs:110: fn payload_tick(&mut self,now:u64){
adapter/src/native_bloodstain.rs:143: rows.push(Row{prefix:"REPLAY_FRAME",address:tail,message:format!("role=observed_write user_marker={} observed_node={} callback={} start_ns={} first_is_existing={} native payload raw, no game calls",PAYLOAD_MARKERS[self.payload.marker],self.payload.observed_nodes,self.payload.call
adapter/src/native_bloodstain.rs:153: rows.push(Row{prefix:"REPLAY_FRAME",address:at,message:format!("role=pool_active index={index} user_marker={} start_ns={}; linked active pool snapshot, NOT time sequence from old test",PAYLOAD_MARKERS[self.payload.marker],self.payload.start),bytes:node});at=next;
adapter/src/native_bloodstain.rs:159: fn actor(&self,rows:&mut Vec<Row>,address:usize,source:&str,set:i32,slot:usize){
adapter/src/native_bloodstain.rs:172: rows.push(Row{prefix:"REPLAY_MANIPULATOR",address:manip,message:format!("class={manip_class} type_literal={kind:?} ownerA8=0x{:X} owner_matches={owned} data100=0x{data:X}; no engine call",pointer(&m,MANIPULATOR_OWNER)),bytes:m});
adapter/src/native_bloodstain.rs:173: if owned&&data!=0{if let Some(bytes)=read(data,0x240){rows.push(Row{prefix:"REPLAY_FRAME",address:data,message:"kind=attached_data_prefix size=0x240; consumer at RVA3df010 references +220/+230; NOT a decoded frame; no followed buffer pointers".into(),bytes});}}
adapter/src/native_bloodstain.rs:179: #[test]fn verified_pool_bounds(){assert!(pool_node_valid(0x10000+59*NODE_BYTES,0x10000,60));assert!(!pool_node_valid(0x10000+60*NODE_BYTES,0x10000,60));assert!(!pool_node_valid(0x10001,0x10000,60));assert!(!pool_node_valid(0x10000,0x10000,0));}
adapter/src/native_bloodstain.rs:180: #[test]fn known_sdk_prefix(){assert_eq!(size_of::<ReplayRecorder>(),0x70);assert_eq!(offset_of!(ReplayRecorder,owning_player),0x10);assert_eq!(offset_of!(ReplayRecorder,frame_counter),0x44);assert_eq!(offset_of!(PlayerIns,replay_recorder),0x5c8);assert_eq!(offset_of!(ChrCtrl,mani
adapter/src/native_bloodstain.rs:181: #[test]fn owner_pointer_uses_exact_a8(){let mut b=[0u8;0xc0];b[0xa0..0xa8].copy_from_slice(&0x7ff100000000u64.to_le_bytes());b[0xa8..0xb0].copy_from_slice(&0x7ff177d09800u64.to_le_bytes());assert_eq!(pointer(&b,MANIPULATOR_OWNER),0x7ff177d09800);assert_ne!(pointer(&b,0xa0),pointe
adapter/src/native_bloodstain.rs:182: #[test]fn scalar_kind_without_virtual_call(){assert_eq!(literal_kind(&[0xb8,3,0,0,0,0xc3]),Some(3));assert_eq!(literal_kind(&[0xb8,1,0,0,0,0xc3]),Some(1));assert_eq!(literal_kind(&[0x48,0,0,0,0,0xc3]),None);assert_eq!(literal_kind(&[0xb8,9,0,0,0,0xc3]),None);}
adapter/src/native_bloodstain.rs:183: #[test]fn bounded_decode_and_readonly_command_policy(){assert_eq!(u64_at(&[0;7],0),None);assert_eq!(u32_at(&[0;4],usize::MAX),None);for k in [3,5,6,7,11,14,15]{assert!(write_command(k));}for k in [1,2,4,8,9,10,12,13]{assert!(!write_command(k));}}
```
