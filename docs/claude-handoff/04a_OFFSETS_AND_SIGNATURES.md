# Exact-target offsets and signatures

Snapshot: 2026-10-06, source3d97070, target2.7.0.0 SHA D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134. Preferred base0x140000000. Runtime=loadedbase+(VA-preferredbase). Tables are static evidence unless live status explicitly stated. No successful ghost mutation verified.

|Object|Identified layout/global|
|---|---|
|WorldChrMan|global143d69ff8; mainplayer+1e508; transition+1e524; ghostset+10f38; activevector+1e630/+1e638|
|ChrSet|capacity+10; entries+18; stride10; entryactor0/word8/flagsa|
|ChrIns|handle+8; entry+10; ctrl+58; modules+190|
|PlayerIns|owned replay manip+588; alternate+590; recorder+5c8; ghost ReplayData+740|
|ChrCtrl|owner+10; primarymanip+18; overridemanip+3b0; e8 mode condition `(b&0x60)==0x60 && b<0x80`|
|ReplayRecorder|size860; vt142a4aa40; allocator8/owner10; pool18/head20/tail28/free30; capacity40/count44|
|Recorder node|size248; primarylength0/payload4..103; secondarylength104/payload108..207; float208; convertedXYZ220/yaw22c; unknownhash238; next240|
|ReplayData|size238; vt142a8b640; refcount8; primarycountd0/ptrd8/cursor220; secondarycount224/ptr228/cursor230; flag21c; blockstride104|
|ReplayManipulator|vt142a2eda0; ownera8; data100; remaining108/end10c/elapsed110/total114; gate132; decoder138/140; selector14c/first14e; literaltype3|
|CSDelayDeleteManImp|global14458d728; queue node20: next0/deleter8/payload10|
|TestNetStep|descriptor143d74f60; callback140b08250; call140b0841d/return140b08422; child+d8, context+b8, aux+c0|
|Native deleter|object143b4cee0; initializedvt142bfc0d8 slot8→140e775e0; initializer1400b5890|

Rust bridge derives SDK physics position/orientation and related offsets with offset_of!, not guessed constants. Public binding references include position+70/orientation+50, but use exact generated binding offsets, not this table for writes.

## Installed prototype hooks (Microsoft x64 ABI)

|VA|Purpose|
|---|---|
|140703e30|Original TestNetStep context; guarded post-original queued create|
|1407048e0|Periodic builder observation; return140703f3c|
|140507e60|Native actor creation/insertion result|
|14050efa0|Native world removal drain, queued REMOVE|
|14050b340|Native removal/retirement|
|1404f1c10|Activation|
|1403deec0|Manipulator disable|
|140e78ca0|DelayDelete enqueue|
|1404f1ab0|Ghost destructor|
|1403dec70|Manipulator destructor|
|141ebc000|Reference release/final transition|
|140e775e0|Actor deleter call/return|

Not separately hooked: data destructor/free, allocator free. NativeGhostFingerprints.h has26 guards; older25-guard note superseded.

|Other native function|Static role/ABI|
|---|---|
|1406514f0/14025f810/14025f7e0|Metadata ctor/current fill/alternate lookup|
|1406f1ec0/1406f2410|Serialized size/serializer; fifth alternate metadata arg required|
|141ebbcd0/141ebbfc0|Allocator(three args, allocatorR8)/retain|
|1406f1bb0/1406f1f20/1406f27f0|ReplayData ctor/decode/factory|
|140493a80/140404570/1404f1840/14065db40|Native actor/manip constructor chain, not manual API|
|1403deaa0/1403df010|Replay manip creation/attach|
|1403df190/1403df650|Replay updater/decoder|
|1403f75b0/1403f6180/1403cdc20|Initial placement/rotation/control movement|
|1404f1d30/140655390/1403ed710|Disable/unregister chain|
|140494fb0/1403c3830|Set erase/actual entry clear|
|140e79d80/140e799f0|Delay promotion/drain; deleter CALL140e79b01|
|1404f1bd0/1406515f0/14065e6c0|Deleting ghost virtual/base cleanup/owned manip cleanup|
|14041a920/145b59eda|Behavior descendant/string history deep copy|

Task registration pelite capture in adapter/src/lib.rs:
`e8 ? ? ? ? 48 8b 0d ? ? ? ? 4c 8b c7 8b d3 e8 $ { ' }`.
Unique resolution was user verified. Preserve SDK/task logic.

Render COM hooks: Present slot8, ResizeBuffers13, IDXGIFactory CreateSwapChain10, IDXGIFactory2 CreateSwapChainForHwnd15; Win32 SetCursorPos detour. They are COM indices/API targets, not game AOBs.

Audit item: prototype readiness checks recorder+40 (capacity), also headnonnull. Actual nonempty count is+44. This is a candidate validation refinement, not a proven live bug until count/head measured.

The lifecycle appendix below includes detailed task masks, callback descendants, teardown order and exact confidence classifications. It is local uncommitted research evidence, not a new runtime acceptance.

---

## Evidence appendix: `research/NATIVE_GHOST_LIFECYCLE_BLOCKER.md` (local uncommitted evidence)

Historical statuses retain their original scope. Latest Step placement/26 guards/observed timeouts in the opening sections supersede older builder-only and untested-callsite wording. Static proof does not mean successful native creation.

# Native ghost lifecycle — static proof checkpoint

Current result: **SAFE GHOST CREATE/REMOVE LIFECYCLE STATICALLY COMPLETE** (section 9, 2026-10-06). Earlier blocker decisions below are retained as historical checkpoints, superseded by section 9. No spawn toggle, build or runtime test added. Runtime source/binaries unchanged. Branch `codex/native-bloodstain-replay-research`, implementation baseline `5e3ba5f`.

Exact target independently rehashed by Image: Elden Ring1.17/2.7.0.0 AMD64, SHA256 D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134. Evidence windows outside Git: `../research/ghidra-eldenring/targeted/native_lifecycle/*.json`. Only four lifecycle contracts investigated. Preferred VAs below; no native functions called.

## 1. Local/debug execution context

[CONFIRMED static] Exact-image targeted CALL/JMP/LEA search found CALL at140703f37 ->1407048e0 and LEA at1400a47e0 ->140b08250. Indexed caller chain is140b08250 ->140703e30 ->1407048e0. Search hits were checked against instruction windows; no other direct call to these two endpoints found in executable sections. This does not exclude indirect calls.

[CONFIRMED static] Initializer fragment1400a47cf..1400a4818 stores140b08250 at143d74f60 and UTF16 name pointer142b642f8, string `TestNetStep::STEP_Update`. **It follows the RET of1400a47b0; do not attribute it to that function.** Constructor140b070d0 passes this descriptor to140b06f30, sets vtable142b649a8; COL/type descriptor confirms `.?AVTestNetStep@CS@@`. It creates child object through140703980 and stores at+ d8. STEP_Update140b08250 passes this child, context+ b8, dt from task argument+8 and context+ c0 to140703e30.

[CONFIRMED static]140703e30 requires WorldChrMan global143d69ff8, updates child entries, runs140704340, decrements countdown+38. Only when countdown<=0 does it reset that timer and call1407048e0. Debug predicates1401dee10/1401dedc0 affect interval; their field semantics remain UNKNOWN. Timer reset precedes the call; surrounding manager update continues afterwards.

[HIGH CONFIDENCE static]1407048e0 requires current player/recorder, obtains assembly/player metadata through14025f810 and alternate-stream predicate14025f7e0. Size1406f1ec0 -> native buffer ->1406f2410(buffer,size,recorder,metadata). This serialization mutates recorder. Eligibility1404e5440 and debug predicates gate creation. It constructs0x238 data through1406f1bb0, retains it, deserializes1406f1f20, then calls1406f27f0(out_handle,context,data,false) only on successful decode and the creation debug predicate. Immediately afterwards it releases the temporary reference (old count1 invokes deleting virtual), frees the serialized buffer, and destroys temporary metadata. No proven global playback-start write follows the spawn call.

**Unresolved:** STEP scheduler thread, ordering/barriers relative to ChrIns/physics/world mutation, safe interception contract for TestNetStep::STEP_Update. This is NOT proof that our ChrIns_PostPhysics callback may insert a ghost. Keep that distinction.

## 2. ReplayManipulator consumer

[CONFIRMED static] Vtable142a2eda0 slot10/+50 ->1403df190. It reads data+100, stream selector+14c, primary count/cursor data+d0/+220 or secondary+224/+230. Active gate+132 must be nonzero. Note machine code reads selected count BEFORE its later null check: data must already be valid; do not invoke an unattached instance.

[CONFIRMED static] First-frame flag+14e causes1403df650(current_cursor,&remaining108), then clears+14e. When remaining108 is below the epsilon threshold it increments selected cursor (getter1403dfd30), checks count, and decodes until positive duration or failure/end. Float+110 is elapsed time, +114 is remaining stream duration determined by attach1403df010 ->1403dfd50(cursor+1) ->1403dfdc0. Update adds dt to+110 and subtracts dt from+108. End/count/decode failure sets+10c=1. Owner+1c4 bit4 suppresses normal progression in one branch. These conditions are not an external Stop API.

[HIGH CONFIDENCE static] Decoder1403df650 selects block getter1406f1e60/1406f1e90, parses state/events through1404e9490/1404eb3d0, validates, updates persistent decoders+138/+140. Owner's module at owner+190 -> +30 receives140423180/140423120/140423100/140423090. Normal duration processing computes target position+ c0, target control rotation+ d0 and rates+ e0/+ f0; first-frame branch uses native placement1403f75b0 and rotation1403f6180. Subsequent1403df190 applies movement through virtual+d8 and rotation-delta1403cdc20, including native fade operations. It is not simply repeated physics transform assignment. Exact behavior-to-animation task ordering remains unproven.

[CONFIRMED static] Concrete normal dispatch: actor task at+478 (callback stored at+4a0) ->1403f9390 ->actor virtual+88 ->ReplayGhostIns1404f25e0 ->PlayerIns140660f70 ->ChrIns140401a30 ->ChrCtrl1403c8db0 ->CALL1403c8e1d, manipulator virtual+50 ->ReplayManipulator1403df190. ChrCtrl selects override+3b0, otherwise primary+18; owner is ctrl+10, actor holds ctrl at+58. +50 is conditional on `(ctrl.byte_e8 & 0x60)==0x60 && ctrl.byte_e8<0x80`; the alternate call at1403c8e28 uses+40. This is a generic selected-manipulator dispatch, without a replay-type test at the call site.

[CONFIRMED static / HIGH CONFIDENCE phase name] Constructor1403e6e30 initializes task+478/owner+498/callback+4a0. Registration1403f5ca0 (param2==0) selects+478 and group0x47 at CALL1403f5ce0. Task vtable142a31090 +20 ->1403e8230 ->140eb3460; that registers through140eb3590 or replaces the previous registration. Pinned SDK3c8c1d7 labels0x47 `ChrIns_PreBehavior`, also present as exact-image string142c07aa8. Thus this dispatch belongs before HavokBehavior and PrePhysics in the SDK task ordering, rather than the recorder/PostPhysics task. Constructor task bases are+418,+448,+478,+4a8,+4d8,+508; +440,+470,+4a0,+4d0,+500 are CALLBACK fields, not task bases. Do not infer phases from the binding's task field names.

[CONFIRMED static] A second concrete dispatch exists:1403c91f0 ->1403cc5c0 ->CALL1403cc61a virtual+50, conditional on selected manipulator virtual+f8 returning true. ReplayManipulator+f8 ->1403defc0 returns true. Actor virtual+d0 ->1404014b0 ->1403c91f0, and world batch140510d70 ->1403f9dc0 ->1403c91f0 are concrete callers. Batch callback140510d70 is installed in six task objects by14050e6c0. Its exact scheduler assignment/barrier is still UNKNOWN; do not label this second route PrePhysics just from its apparent purpose, or assume both calls advance the stream on every frame.

[CONFIRMED static, dispatch comparison only] Exact-class vtable entries (exclude DLRuntimeClass metadata tables): Pad +50=1403d9f30/+f8=1403cd920(false); Network +50=1403d5aa0/+f8=1403d5a60(true); NetAI +50=1403d3b10/+f8=1403d3560(true); Replay +50=1403df190/+f8=1403defc0(true). Consequently all four can use the normal selected-manipulator route, while Pad does not pass the second route's+f8 condition. No Network behavior researched.

## 3. Actual start gate

[CONFIRMED static] Constructor1403deaa0 initializes+132=0 (u32 zero at+130), elapsed+110=0, first-frame+14e=1. Replay-data constructor initializes both cursors0. Attach1403df010 retains data/calculates duration; it does NOT set+132. Vtable slot3/+18 ->1403dee00 is exactly `mov byte [rcx+132],1; ret`; slot4/+20 ->1403deec0 clears it. Thus valid data plus an enabled+132 gate plus scheduled update is necessary.1403df000/1404f2340 changes stream selection, not this gate.

[CONFIRMED static] Native enable caller is PlayerIns initializer140651a40: MOV140651bcb loads actor+588, null-check, CALL140651bdc `[vtable+18]` ->ReplayManipulator1403dee00 ->byte+132=1. ReplayGhostIns virtual+50 points to1404f1c10, which calls140651a40. Constructor1404f1840 ->140650c90 ->14065db40(case3) allocates ReplayManipulator1403deaa0 and stores it at actor+588; ChrCtrl initialization1403de370 ->1403c76e0 stores that pointer at ctrl+18. Ghost ctor then attaches retained replay-data through1403df010. Activation is therefore the generic PlayerIns lifecycle dispatch, with a replay-specific implementation of that virtual, not TestNetStep setting+132 after creation. Whether a particular external spawn invocation reaches full actor activation still requires the factory/world activation contract; constructor alone does not enable it.

[CONFIRMED static] Native disable: ghost virtual+58 ->1404f1d30 (tail JMP) ->PlayerIns140655390. It calls1403ed710 to unregister tasks, then loads actor+588 at14065553a, null-checks and tail-JMPs `[vtable+20]` at14065555c ->ReplayManipulator1403deec0 ->byte+132=0. Other three manipulator classes use shared+18=1403cd490/+20=1403cd690 (return stubs), supporting a generic lifecycle slot rather than a replay-only start command.

[CONFIRMED exclusion / UNKNOWN extra callers]1403cc000's CALL1403cc52b +18 is on a different object obtained via ctrl virtual+30, not evidence for enabling ReplayManipulator. ChrIns1403e8240 invokes ctrl virtual+18 ->1403ddff0 ->1403c5630; its examined base path creates control/behavior resources, not a proven replay gate write. Opcode candidates1403c39ba(+18),1403c3a04(+20),1403cb5ad(+18) lie in fragmented/obfuscated control flow; receiver identity and reachability remain UNKNOWN. The bounded search is not an exhaustive proof of every indirect enable/disable caller.

## 4. Removal and reference release

[CONFIRMED static]14050b340 notifies actor virtual+58, removes matching active-vector entry+1e630/+1e638, erases actor from sets including+10f38 through140494fb0 (returns detached actor, NOT an entry lookup), sets actor+1c5 bit20, calls140e78ca0 at14050b55e. Queue insertion allocates/reuses0x20 entries, stores actor payload and links into the native queue. It does not immediately invoke the deleting destructor.13 indexed callers include14050efa0 (world request-queue drain),1406515f0 and140b02ef0. The latter's actor class is unproven, so it is NOT accepted as a genuine replay-removal example.

[CONFIRMED static] Ghost destructor1404f1ab0 releases replay-data+740 BEFORE PlayerIns destructor1406515f0. PlayerIns destroys ChrCtrl+58 and then invokes14065e6c0, which destroys owned manipulators+590 and+588 through virtual+8/allocator. ReplayManipulator destructor1403dec70 frees decoder allocations+140/+138, then releases replay-data+100. Ref-release helper141ebc000 is checked for old count1 before deleting virtual. For one primary ghost with no extra owners: temporary creator retain plus ghost/manipulator retains imply3 ->2 after creator release ->1 after ghost release ->0 after manipulator release. This is a conditional static ownership model, NOT a measured runtime refcount/leak result; additional actors/owners change counts.

**Correction to previous cleanup candidate:**140ade693 ->1403ee5c9 calls140e55380 (constant return0), then145c4dc98. Exact branch window testsAL and CMOVE-selects1403ee672, which jumps to1428f37bd, a stack-unwind/return epilogue. This examined route contains NO proven store clearing ChrSetEntry. Calling it “slot cleanup” is unsupported. Do not use it as an unregister API.

[CONFIRMED static] Queue owner is CSDelayDeleteManImp, singleton14458d728. Producer140e78ca0 uses free-list head+20/tail+28, bucket head/tail arrays+10/+60, pending count+40. Node size0x20: next+0, deleter object+8, actor payload+10. Consumer140e79d80 ages/promotes buckets into ready head+30/tail+38 and ready count+48;140e799f0 pops ready nodes and CALL140e79b01 invokes deleter virtual+8 with node+10. It then decrements counts and recycles/frees the queue node; budgeted execution may leave pending work for later updates. These queue-node zero stores do NOT clear ChrSetEntry.

[CONFIRMED static] Important initializer:1400b5890, store1400b58a4 sets deleter object143b4cee0 to vtable142bfc0d8. Its+8 ->140e775e0. The initial disk contents of143b4cee0 point at base Deleter142bfc268/pure-virtual14251ec90 and are NOT its initialized actor-deletion dispatch.140e775e0 loads the actor from node payload, obtains allocator140e1c010, CALL140e77605 invokes actor virtual+8 with flags0; for ReplayGhostIns ->1404f1bd0 ->1404f1ab0. CALL140e77612 then frees actor memory through allocator+68. This establishes actual destruction and allocation release, not just removal submission.

[CONFIRMED static / HIGH CONFIDENCE phase name] Update entry140e7b240 ->CALL140e7b37b ->140e79680 ->140e79d80 ->140e799f0. Exact LEA1400b5bfc installs140e7b240 at14458d7f0 in descriptor14458d7e0. CSDelayDeleteStep constructor140e7a430 passes that descriptor through140e7a290/140e7a340 (step object+10). Step executor140e7ac20 dispatches descriptor entries.140df0be0 constructs that step and CALL140df0cdb invokes140eb73a0(owner+b8,0xa5,step).140eb73a0 delegates registration to140eb3de0. Pinned enum labels0xa5 `DelayDeleteStep` (exact-image string142c08c20), after Flip/before FrameEnd. This proves task assignment; physical thread affinity and completion barriers are not established merely by enum order.

[CONFIRMED static ordering]14050b340 FIRST calls actor+58 ->ghost1404f1d30 ->140655390 ->1403ed710 (six task-unregister virtual+28 calls1403ed858..1403ed899 ->140eb34c0 ->140eb3680), then disable14065555c ->1403deec0; only AFTER returning does it remove the active vector, erase the matching ChrSetEntry through140494fb0, set pending-removal flag and enqueue140e78ca0. Later consumer ->140e775e0 ->ghost deleting wrapper(flags0) ->ghost data+740 release ->PlayerIns destructor/ChrCtrl destruction ->owned ReplayManipulator destruction/data+100 release ->ChrIns teardown ->actor allocator free. Whether already-running tasks have joined before deletion remains UNKNOWN. ChrIns teardown also destroys task objects through140eb3110.

## 5. Exact entry invalidation — blocker located

[CONFIRMED static] Pinned SDK3c8c1d7 and exact-image accesses agree: ChrSet has capacity+10, entries+18; ChrSetEntry stride0x10, actor pointer+0, load status+8, update type+9, flags+a. Actor+10 is its entry back-reference. No generation member is established in this entry layout.

[CONFIRMED static] World removal14050b340 uses LEA14050b4ec `world+10f38`, CALL14050b4f6 ->140494fb0. This is **erase**, not lookup: it scans entries for pointer equality, resolves the occupied actor through set virtual+8, removes group/entity mappings through140495b20/140495ab0, invokes set virtual+50 with the saved actor handle, then CALL140495064 ->1403c3830(entry). Exact store1403c3834 (`48 c7 01 00 00 00 00`) sets entry.actor to null.1403c3830 also masks flags+a withf8 and zeros the word+8 at1403c383b. CALL140495079 ->1403c3810(entry,0) repeats null initialization (pointer store1403c3817). Return value is the detached actor. No slot compaction or actor destruction is performed by this helper. Base ChrSet virtual+50 ->140495aa0 is a return stub; do not infer generation invalidation from that dispatch.

[CONFIRMED static] Thus unregister/disable -> active-vector erase -> ghost entry null/status reset -> actor pending flag at14050b51a -> DelayDelete enqueue14050b55e -> later destructor/free. Entry invalidation is synchronous on this removal call path and precedes deferred destruction. Generic140494f10 and bulk140495890 also call the same reset helper; bulk destruction has a different order and is not the single-ghost removal contract. The rejected destructor candidate remains rejected; no destructor-side clear is needed to explain this path.

### Handle and slot reuse

[CONFIRMED static] FieldInsHandle is selector32 + block_id32 in the pinned binding; selector is index bits0..19, container20..27, type28..31. For ReplayGhost/type5 exact-image mask table143b37928+5*24 isfffff.14062f5d0 extracts this index;140493d00 calls set virtual+8 with that index. Base140493ce0 checks bounds then virtual+10 ->140493d40 returns entries[index].actor. This examined lookup has **no generation, full-handle equality or occupant identity comparison**.

[CONFIRMED static] Free-slot selector140496960 scans0x10-stride slots for actor==null, starting after set+20 and wrapping capacity; it updates the cursor and returns entry/index. Empty initialization1403c3810 can replace entry.actor with its second argument. Clearing makes this slot eligible for reuse; an old index-based handle can consequently select a later occupant. No generation increment or retired-handle registry is proven on the examined erase/reuse path. No claim is made about every higher-level resolver.

[HIGH CONFIDENCE design consequence] Future tooling must retire its ghost reference immediately on removal and never re-resolve a retired handle or dereference a saved actor/entry across callbacks. Pointer/full-handle comparison alone does not solve allocator/slot ABA reuse. A wrapper ownership epoch and an established engine execution/lifecycle boundary are needed; they have not been implemented or proven here. The slot clear itself prevents an entry from pointing to the detached actor until reuse, but does not prove safety for arbitrary cached references.

### Minimal task unregister semantics

[CONFIRMED static]140eb34c0 clears the task object's registration pointer after140eb3680.140eb3680 FIRST sets registration+10 (task owner) to null, then140eb3c60 ->140eb4490 ->1426578e0 ->142657810(owner,registration,0,true). Registration ctor140eb34f0 stores task at+10/group at+18; exact registration vtable142c049f8 slot+10 ->140eb36e0 loads+10, tests null and only dispatches task virtual+10 if nonnull. Subsequent dispatch that observes null returns without invoking the task.

[CONFIRMED static]142657810 branches on scheduler byte+38: zero calls erase command1401267f0 immediately; nonzero allocates0x20 command and submits through1426a3470 at1426578b9.1401267f0 removes registration via1426a1ac0 and, on successful removal with command+1c true, destroys/frees registration.1426a1ac0 acquires object+88 through141ee9410, erases matching registration from two collections, then releases through141ee9460. This is **immediate owner invalidation plus conditional deferred physical unregister**, not unconditional synchronous list removal. Exact meaning/lifetime of scheduler byte+38 is not inferred.

[UNKNOWN] A dispatcher that already loaded a nonnull task owner before the clear could still invoke it. The examined paths do not establish a join/fence between such an in-flight dispatch (or other saved actor references), deferred unregister completion, and DelayDelete destruction. Null-gating and the late DelayDeleteStep assignment alone do not prove that barrier. Do not call native removal from an arbitrary callback based on these findings.

## Decision

**PRECISE BLOCKER REMAINS.** The decisive entry-pointer store is now proven. The remaining safety edge is the native execution boundary at which removal, in-flight actor/task references and deferred unregister are quiescent before DelayDelete frees the actor. Index-only handles are reusable, so cached handles/pointers cannot substitute for this lifetime boundary. No safe create/remove completeness claim, spawn, runtime writes, builds, tests or human test request. Earlier update-phase unknowns remain documented but were not broadened for this task.

## 6. Recovery continuation — bounded scheduler findings (2026-10-06)

Evidence labels in this continuation follow the new task: STATIC_VERIFIED / REFERENCE / UNKNOWN. Recovery details: ROLLBACK_RECOVERY_STATUS.md. Exact target disk hash matched again. Raw targeted scheduler extracts remain outside Git at `../research/ghidra-eldenring/targeted/native_scheduler/`.

STATIC_VERIFIED: scheduler global144861280 is initialized through140eb3770 ->1426584a0/142657de0. Outer scheduler+40 receives the CSTask implementation whose ctor14268f530 installs exact vtable14329caf0, slot+28 ->14268ede0. +18 owns deferred physical removal queue, instantiated among three queue objects by142657de0.1426a3470 locks queue+8 through141ed8010, appends command pointer to backing vector queue+10, unlocks141ed8080.1426a33a0 locks the same queue, executes each command virtual+8 (erase-command143296de0 ->1401267f0), destroys/frees command, resets vector end, unlocks. Queue ownership/drain identified, not inferred from naming.

STATIC_VERIFIED:140eb3f80 ->142657c00 ->142657b70 is frame dispatch.142657b70 drains queue+10 before dispatch, sets scheduler byte+38=1 at142657bae, invokes CSTask virtual+28 at142657bcb, clears+38 at142657bce, drains queue+18 at142657bd6. Thus +38 is an observed dispatch-in-progress/defer-mutation gate; no claim of atomic cross-thread synchronization. Queued unregister completes after this dispatch returns, not automatically before DelayDeleteStep within that frame. Separate queue+20 drains during concurrency-row advance at1426d7563; it is NOT the physical unregister queue+18.

STATIC_VERIFIED:14268ede0 dispatches runner work then CALL14268f125 ->1426a2d10 before returning.1426a2d10 waits/retries while runner+32 work-active flags remain and clears shared queue assignments afterwards. This is a completion candidate, not an OS-thread join proof.1426d8a00 clears runner+32 only after its synchronous command dispatch returns. Worker implementation and the relationship of this end-of-frame wait to an earlier0xa5 actor deletion still require exact ordering verification.

STATIC_VERIFIED: concurrency state1426d7470 transitions1 ->2 only if manager+20 mask==0,2 ->3 only if+24 mask==0; state3 advances row or completes(state4).1426d7200 populates+20 for modes2/3 and+24 for mode5.1426d7d90 processes modes2/3, returns from callback dispatch, then calls1426d73c0 to clear runner bit+20 under manager lock.1426d7d10's task virtual+10 dispatch at1426d7d72 precedes completion accounting. Mode5 waits for+20 zero via1426d72a0 before clearing+24 through1426d7340. These establish concrete accounting and conditions, but are not yet proof of the exact concurrency configuration of every actor task.

STATIC_VERIFIED: constructor140eb46b0 reads descriptor table and six-slot row table;140eb6fb0 adds six-slot rows,140eb6d00 maps group memberships to runner masks through140eb67d0. Exact row142c06750 is[47,47,47,47,47,47]; row142c06ed0 is[a5,aa,aa,aa,aa,aa] (aa unused sentinel in this construction). Descriptor142c05600 names PreBehavior47 with mask3f;142c061c0 names DelayDeletea5 with mask1. Mapping descriptor options through140eb4ed0/concurrency policy to actual runner modes and barriers is not fully verified. Enum ordering alone remains insufficient.

### Five requested answers / precise remaining edge

1. YES: already-loaded task owner can survive a subsequent owner=null store;140eb36e0 has no later cancellation check around the invoked task.
2. Candidate protection is concurrency-row completion plus native delayed destruction, not a proven actor reference retain. No complete memory protection proof yet.
3. Physical unregister queue+18 drains142657bd6 after CSTask dispatch returns; command execution is1426a33a0 ->1401267f0 ->1426a1ac0.
4. That drain is NOT established before DelayDeleteStep; the observed outer ordering places it after dispatch. Actor safety must therefore be established by earlier row/callback completion, not by insisting physical removal already happened.
5. UNKNOWN: precise quiescent boundary before0xa5 for all actor callbacks/saved references. Remaining static task is verify descriptor/concurrency-mode mapping and worker completion path for the relevant actor rows versus0xa5. No broader scheduler map required.

P0 remains; no NativeReplayGhostPrototype or game test created. Preserve native capture/read-only probes and baseline outputs. Do not manufacture native replay-data or revert to legacy transform fixes while this safety edge remains open.

## 7. Final concurrency-row proof checkpoint

STATIC_VERIFIED: descriptor words for47 at142c05600 are index47/name142c07aa8/mask3f/options1,0/registration flags100. For4e at142c056e0: mask3f/options1,0/flags6. Fora5 at142c061c0: mask1/options0,0/flags1. Do NOT call registration flags100 a runner mode.

STATIC_VERIFIED:140eb6c40 ->140eb65e0 ->140eb4960 stores descriptor mask at+50, options at+54/+58, flags at+5c.140eb4770 copies these to wrapper+48/+4c/+50;140eb4ed0 builds [first_row,last_row,runner_mask,mode_option] ranges. Wrapper+4c controls mask reduction, NOT runner mode. Its final range mode_option is zero when wrapper+50==0: store140eb50c4 zeros range+c. Each of47,4e,a5 occurs in exactly one six-slot row, so its only range is the final range.

STATIC_VERIFIED:140eb53f0 loads mask from range+18 intoR8D, mode_option from+1c intoR9D, range begin/end+10/+14 onto stack; CALL140eb548d ->14268f1c0 ->1426d7880. Exact1426d7920..1426d7926 derives mode=(row==last_row)+1 when option0; stores mode at1426d7940. Option1 instead gives4/5, option-1 gives0. Therefore the verified default final rows are **mode2**, not5:47 six runners mask3f;4e six runners mask3f;a5 runner0 mask1, other slots unused.1426d7a60 may change mode2 to3 only where that group has no matching slot on another runner; six identical47/4e slots stay2. A5 has no same-group peer and can normalize2→3; both2 and3 use completion mask+20. No arbitrary runtime configuration change is covered by this static default mapping.

STATIC_VERIFIED:1426d7200 initializes primary completion mask+20 for modes2/3, secondary+24 for5.47/4e rows therefore initialize3f, a5 initializes1.1426d7d90 mode2 drains its task iterator and synchronously invokes either task virtual+10 at1426d7fec or tasklet virtual+8 at1426d7fe0. Only after the drain/callback return does CALL1426d8016 ->1426d73c0 clear that runner's+20 bit under manager lock. Mode3 also calls the synchronous dispatch helper before this accounting.1426d7470 allows1→2 only when+20==0,2→3 only when+24==0, and only state3 advances row. Thus no still-running invocation included in these completion bits can remain after the corresponding row advances. This statement does NOT cover descendant async work submitted by a callback.

STATIC_VERIFIED: earlier rows must pass those conditions before reaching a5's table row142c06ed0. The examined scheduled47/4e invocations cannot overlap a5 dispatch. Physical unregister queue18 still drains after the outer frame, which is compatible with owner-null suppression and row completion; it is not the before-a5 barrier.

STATIC_VERIFIED: second ReplayManipulator route installation is14050e6c0 (six tasks in world vector+1f0a0, callback140510d70), registration14050b5a0 at CALL14050b77b withEDX=4e.140510d70 uses an atomic shared index and calls1403f9dc0 ->1403c91f0 ->1403cc5c0 ->ReplayManipulator virtual+50. Its default row142c067f8 is[4e,4e,4e,4e,4e,4e], governed by the same mode2/mask3f barrier. This establishes the known WORLD BATCH route; it does not prove an exhaustive caller set for the additional actor virtual+d0 route.

### Out-of-row lifetime escape — precise outstanding contract

STATIC_VERIFIED exclusions: ghost1404f2500 only adjusts fade scalars;140487380 only resets scalar/flag fields;140436b90 ->1404389f0 clamps/writes a scalar. These examined helpers do not submit actor work. Callback140401a30's examined14012* calls concern temporary string/container work and are not evidence of task submission.

UNKNOWN: no exhaustive no-escape proof for callback-reachable native virtuals. Concrete unresolved receiver:140401e3c actor virtual+600 ->ReplayGhost1404f20b0 returns actor+6a0; CALL140401e48 then invokes that object's virtual+e0. Player constructor140650c90 assigns actor+6a0 from incoming context[7]. Its concrete type/implementation and retained-actor/child-job contract for local ReplayGhost creation are not resolved here. Ordinary synchronous CALL return does not establish absence of queued descendant work. ReplayManipulator's decoded module/owner virtuals likewise must not be assigned a blanket synchronous lifetime guarantee from scheduler masks alone.

**PRECISE BLOCKER REMAINS:** prove callback-descendant saved actor references/jobs are joined or retired before a5, starting with the concrete140401e48 receiver and relevant ReplayManipulator module dispatches. Row-mode mapping, registered callback quiescence and the known second world-batch route are now resolved; they are no longer the blocker. No actual asynchronous escape has been demonstrated, but its absence/cleanup barrier has not been proved. No runtime changes, builds, ghost prototype or human test. Next static step resolves those callback-local lifetime edges; the create/remove implementation plan remains gated.


## 8. Callback-descendant lifetime checkpoint — 2026-10-06

This section supersedes earlier open receiver/scheduler wording. Scope: exact-image static analysis only; no runtime edits, build, spawn, writes or human test. Registered row completion from section 7 remains accepted. Partial SQLite/pseudocode is not an exhaustive call graph; conclusions below are bounded explicitly.

### Resolved actor+0x6A0 receiver

[CONFIRMED static] Local ReplayGhost constructor `1404f1840` obtains context through `140403510` / `1404035b0(type=3)`; `1404035b0` invokes `140402ac0`. That initializer puts `singleton[143d6f820]+0x6a8` in context+0x38. Player constructor `140650c90` copies context[7] into actor[0xd4], i.e. actor+0x6a0. This is a borrowed embedded singleton member, not a ghost allocation or actor-owned refcounted object.

[CONFIRMED static] Global initialization `140df1450` allocates 0x8a0, calls `140765ef0`, then publishes singleton143d6f820. Constructor sets the embedded +0x6a8 vtable to `142a9ed38`. PE RTTI COL `1432efe10`, type descriptor name `.?AVNullPlayerMenuCtrl@CS@@`, proves **CS::NullPlayerMenuCtrl**. Parent vtable142a9eed8 / COL1432efd48 identifies **CS::CSMenuManImp**. Thus the actor receiver is the null menu controller, not a character worker queue.

[CONFIRMED static] Vtable142a9ed38 slot+0xe0 contains `140766710`; bytes `c2 00 00` are `RET 0` at function entry. There are no instructions before that return, no descendants, no stores, no captured actor arguments and no job submission. Classification: **SYNCHRONOUS_ONLY** for this concrete native local-ghost construction path. Do not extend this result to arbitrary replacement of actor+0x6a0 or another menu-controller class.

[CONFIRMED static] Vtable first entry140766d60 is `xor al,al; ret`, not a proven deleting destructor slot; do not call it as one. Embedded ownership is established by construction, not by guessing slot names. Singleton shutdown140df05b0 invokes parent destruction and frees/zeros143d6f820. Parent deleting wrapper140766480 ->140766160 unregisters its own task and tears down its members. No ghost teardown frees the borrowed NullPlayerMenuCtrl. Menu shutdown is a separate world/session lifetime, not a descendant of this empty +e0 call.

[HIGH CONFIDENCE bounded writer audit] Indexed actor-area literal+6a0 hits in6540b0,655710,655570,657c70,6594f0,65e350,661760 are reads/dispatches; the known constructor assignment uses typed index0xd4 rather than literal6a0. No replacement writer was found in that bounded indexed search. The incomplete/obfuscated database cannot establish an exhaustive all-writers theorem. Classification applies to the demonstrated factory context and vtable; arbitrary externally changed context is excluded.

### Concrete ReplayManipulator receiver and ownership

[CONFIRMED static] `1403df650` loads manipulator owner+0xa8 ->actor+0x190 ->container+0x30 for all four handlers. Pinned SDK3c8c1d7 names container+0x30 `behavior_sync`; container+0x28 is the distinct owned CSChrBehaviorModule. Exact RTTI type descriptor143c80b40/name143c80b50 is `.?AVCSChrBehaviorSyncModule@CS@@`; COL1432d1298 ->vtable142a36c78. Constructor140422950 stores this table at140422973, initializes+10 null, +18 zero, and gate bytes+1c..1f. Deleting wrapper140422cc0 resets the vtable, calls14043cfb0 and optionally frees0x20 bytes. Base teardown14043cfb0 only resets base vtables. Therefore these handlers operate on **CS::CSChrBehaviorSyncModule**, not the larger behavior module. Module+8 is the owner back-reference (`14043d250` getter); module+10 is the downstream behavior wrapper. No teardown join/barrier is established merely from this small destructor.

| Handler | Bounded descendant / persistent writes | Lifetime classification |
|---|---|---|
|140423180|Gate+1f;1404e95e0 supplies state; tail JMP1404231af ->140c09890. Four map traversals build stack-only wrappers with behavior pointer and override-map pointer. Thunks140c1e4c0/4a0/4b0/490 jump to140c1dff0/1e0c0/1e190/1e260. These find numeric indices through1414191f0 and write scalar variable slots;1414191f0 is a lookup-only hash probe with no calls.|**SYNCHRONOUS_ONLY [HIGH CONFIDENCE]** for examined scalar application; no actor/module/state pointer stored in these variable slots, no job submit demonstrated.|
|140423120|Gate+1f;140c09890(state+50), then owner getter and140c1a200(container+28+b00,state+128).140c093f0 creates caller-stack wrappers;140c1a200 iterates existing map and calls the same scalar setter140c1e260.|**SYNCHRONOUS_ONLY [HIGH CONFIDENCE]** for examined scalar application. No retained decoded-state pointer/job found.|
|140423090|Gate+1e;140c09110 returns a stack wrapper,140421480 iterates u16 IDs,140c15a90 ->140c15ba0 appends an event record to behavior-owned storage.|**SYNCHRONOUS_ONLY [CONFIRMED static for actor-pointer escape on this append path]**: persistent scalar event records exist, but the demonstrated append contains no actor/module pointer. Not a claim that all engine event consumers are synchronous.|
|140423100|Gate+1d; tail JMP140423116 ->1404231c0. Builds temporary u16 vector through1404248e0, calls14041a920 on container+28, maps IDs, then uses the same event append. Frees temporary vector before return.|**UNKNOWN**: the concrete14041a920 descendant remains incompletely resolved; no escaping child job has been demonstrated, but absence/cleanup is not proven.|

[CONFIRMED static] Event append is real deferred *data*, not automatically a deferred actor job. At140c15aa0 the downstream behavior pointer supplies its+0xd0 queue.140c15aaa sets the event ID;140c15ab3 zeros the following16 bytes with MOVDQU;140c15ab9 calls140c15ba0. Stores140c15c03/+09/+11 copy ID and those two zero qwords into the0x18 ring record. Queue growth140c15c90 allocates/copies existing records via allocator and141696d90, then replaces the backing array. These calls do not pass the actor, ChrCtrl or decoder pointer into the appended record. Do not misclassify this as RETAINS_ACTOR_WITH_REFCOUNT or QUEUES_CHILD_JOB_WITH_BARRIER without evidence. The wrapper and decoder arguments themselves remain caller-local in this examined route.

[HIGH CONFIDENCE]1404248e0/140c1a300 ->140c16fe0 collects u16 event IDs into temporary storage; actor/module back-references are read to obtain the owned behavior state, not copied as vector elements. Auxiliary string lookup/allocation paths are not a proven scheduler submission. This does not close the following independent obfuscated behavior call.

### Exact remaining descendant edge

[CONFIRMED static] CALL `14042326d` invokes **14041a920**, a JMP thunk to **145b59eda**, with RCX=actor.container+0x28 (**CSChrBehaviorModule**) and RDX pointing to the caller's temporary event vector. This is the concrete unresolved lifetime contract; it is not the scheduler row barrier or NullPlayerMenuCtrl.

[CONFIRMED static]145b59eda tests behavior byte+0x1928 at145b59f38. Its manually checked CMOVE selects14041abb8 (zero branch) versus140c0df63 (nonzero branch), then indirect-jumps at145b59fb5. Zero branch14041abb8 ->140a70a26 ->14041abbf checks the stack cookie and reaches restoration/return144e03a2c. Nonzero branch140c0df63 checks vector begin/end; empty vector reaches the same epilogue, nonempty vector jumps14041a971. The nonempty path obtains an allocator (141ebc760), constructs temporary storage, iterates IDs, and has fragmented continuations. Ordinary CALL/RET and the early epilogues do not prove the entire nonempty path has no pointer retention.

[UNKNOWN — P0] The **nonempty, behavior+0x1928!=0 branch of14041a920/145b59eda**, reached through140c0df63 ->14041a971, is not fully reconstructed. Its remaining fragments include14041a9e7 ->14011dac0 ->thunk144e135df/14011f270 and further loop continuations. No complete write-set, receiver-alias propagation, or cleanup/completion proof for this branch exists yet. Neither a stored actor pointer nor a child-job submission is positively demonstrated. The missing evidence is the no-escape (or safe-retention) contract of this concrete behavior receiver/function; do not replace it with a generic “scheduler unknown” statement.

Evidence: hash-verified exact-image extraction under outer `research/ghidra-eldenring/targeted/native_descendants/`; constructor/handler JSON includes pseudocode, indexed xrefs and raw instruction bytes. RTTI/fragment windows were additionally read directly from the same validated PE because indexed body lengths omit split blocks. Sequential bytes after RET belong to other blocks and were not treated as descendants. Pseudocode naming the thunk does not imply it is resolved.

**PRECISE BLOCKER REMAINS** — CSChrBehaviorModule receiver of14041a920 ->145b59eda, specifically its nonempty event branch when+0x1928!=0. NullPlayerMenuCtrl+e0 is closed; scalar handlers and the zero-pointer event append are bounded exclusions. No native one-ghost implementation plan is enabled until this concrete descendant contract is closed. Runtime baseline5e3ba5f and all builds remain unchanged.


## 9. Final nonempty behavior branch — lifetime proof

Target/branch/HEAD unchanged: exact SHA-verified 1.17/2.7.0.0 AMD64 image; `codex/native-bloodstain-replay-research`,697d420. This closes ONLY the remaining contract in section 8. No runtime source changes, builds, calls into the game, spawn, writes or human tests. Accepted scheduler rows are not reopened.

**Final classification: RETAINS_MODULE_DATA_WITH_SAFE_OWNERSHIP.**

[CONFIRMED static] The nonempty branch creates a **module-owned text-history entry**. It formats names selected by u16 event IDs into an aggregate UTF-16 string, deep-copies that text into a separately owned record, marks the record with a byte, and resizes the history to0x40 entries. It does not retain the caller's vector or decoded-state pointer. Calling it an asynchronous behavior-job submission was an unproven possibility; the actual observed persistent object is string history.

### Proof boundary and method

Direct PE reads supplied instruction boundaries/bytes; CMOV-selected absolute destinations and synthetic RET/JMP stack dispatch were followed manually. `RET` in an obfuscation dispatch is not counted as a function return. Final restoration blocks recover the original caller stack and jump to the saved return address. Indexed xrefs/pseudocode were supporting evidence, not control-flow authority. In particular,14011f270,14011fd30,14011e4a0,14011e820 and14011de10 have split bodies/thunks that the export does not recover adequately.

The complete actor-aware nonempty branch is mapped below. Ordinary memory copy/fill, format-specific character copying, allocator allocate/free/validation, cookie checks and nonreturning allocation/string-error routines form primitive leaves: their arguments contain only owned buffers, lengths, alignment, allocator or static diagnostic text. This is a pointer-lifetime proof for this native call contract, not a theorem about all internal allocator work or corrupt/out-of-contract vectors. No actor or input-vector argument crosses these leaf boundaries. Exception/fatal-error exits are listed separately; their full generic runtime implementation is not treated as an actor callback descendant.

### Entry aliases and object layouts

| Value | Proven origin / lifetime |
|---|---|
|M|Entry RCX=CSChrBehaviorModule. Copied to RSI at145b59f33. Remains the module until history setup; later RSI is reused as a numeric container index. No owner back-reference at M+8 is loaded in this branch.|
|V|Entry RDX=caller vector wrapper. Copied to RDI at145b59f24. Read V+8(begin),V+10(end); RBX iterates backing memory by2. RDI is later reused as scalar string length and then a history slot.|
|A|141ebc760 returns allocator wrapper; stored at[rbp-21]. Has no actor-derived origin.|
|S|Aggregate string at[rbp-21]: allocator+0,inline/heap characters+8,length+18,capacity+20,byte+28. Inline capacity7 UTF-16 characters. Heap character ownership belongs to S, not V.|
|T|Second string at[rbp+0f], independently initialized from default allocator144846dc0; deep-copies S chars/length. Byte at[rbp+37] comes from S's constant-initialized flag, not actor data.|
|D|M+1988; owned container: allocator+0,additional allocated storage+8,pointer map+10,capacity+18,index+20,count+28. Slots point to0x30 string/flag records.|
|H|New/reused owned history record obtained through map[index]; allocator from T copied atH+0,own inline/heap chars atH+8,length+18,capacity+20,flag+28.|
|Event name|140bffac0 ->140c293e0 ->140c26760: bounds-check scalar ID, return static/default or manager-owned UTF-16 name. Consumed synchronously as `%s`; pointer not put into H.|

No value is labelled actor/ChrCtrl/ReplayGhost unless it has that origin. M is actor-owned, but the persistent pointers installed in D/H are allocations and allocator references, not M. V and its begin/end live only in registers/caller-stack temporaries until the loop finishes.

### Complete nonempty branch map (normal and terminal exits)

Addresses are preferred VAs; arrow sequences include split fragments, not invented source-level functions.

| Node / fragment chain | Work / branch targets |
|---|---|
|14041a920 ->145b59eda|Prologue; save M/V; test M+1928. CMOVE145b59f93 selects14041abb8 if zero, otherwise140c0df63. Requested branch selects latter.|
|140c0df63|Compare V.begin/end. Equal ->14041abb8 (empty exit); unequal ->14041a971.|
|14041a971 ->140ec2f7b ->14041a987 ->144e4d330|Acquire A through141ebc760, save on stack; allocator virtual+18 validates flags. Bit5 clear ->14200453c/assert path; bit5 set ->14041a9a7. Assertion calls141ebb5a0; successful path continues.|
|14041a9a7 ->1459aadf7|Initialize S length0,capacity7,inline null,flag1. Load iterator V.begin; compare end. Equal ->14041a9f6; unequal ->14041a9d1.|
|14041a9d1 ->14041a953 ->1407ffea8 ->14041a9d4|Load u16 ID; CALL140bffac0.|
|14041a9da ->1459a0970 ->14041a9e7|Event-name pointer goes toR8; RDX=142a36388, the UTF-16 constant `(%s)`; RCX=&S. CALL14011dac0.|
|14041a9ed ->14515347d|Iterator+=2; compare V.end. CMOVNE selects14041a9d1(loop); equal selects14041a9f6. No pointer/vector store.|
|14041a9f6 ->1452b5cd6|Read S length and choose inline/heap chars. Read default allocator144846dc0. Null ->14041aa14; nonnull ->14041aa23.|
|14041aa14 ->145840bd8|CALL141ed0910 ->141f1ef90 obtains default allocator. Store only that allocator at144846dc0; join14041aa23.|
|14041aa23 ->145c45815 ->14041aa31 ->145b1b4ff|Initialize T allocator; virtual+18 validation. Bit5 clear ->140f3880d ->14041aa4c/assert141ebb5a0; successful ->14041aa51. Both valid continuations join145aad550.|
|14041aa51 ->145aad550 ->14041aa74 ->141402aae|Initialize T's own empty string. CALL14011b190(&T,S.chars,S.length), deep copy. Copy S.flag to T.flag. Set RBX=D=M+1988. If D.capacity>D.count+1 ->14041aaa2; otherwise ->14041aa95.|
|14041aa95 ->1400e7f23 ->14041aa9d|CALL14041ee40(D,1) grows pointer map; returns14041aaa2.|
|14041aaa2 ->1456d218c|D.index &= D.capacity-1. Nonzero ->14041aab7; zero ->1400cf097 sets RSI=D.capacity, then14041aab7.|
|14041aab7 ->145aa5b9a|Decrement numeric index; compute slot pointer. Nonnull ->14041aaec; null ->140a2bf54 ->14041aadf.|
|14041aadf ->1400f1453|CALL1404208b0(D,1) allocates0x30 record; store pointer in D.map[index]; join14041aaec.|
|14041aaec ->145b59382|Load H; put only on stack scratch. Null ->14041ab56; nonnull ->140d9c9fd ->14041ab0e.|
|14041ab0e ->144ce033f|CALL140117a10/140117a70/140117ab0 copies T allocator through temporary wrapper. Store allocator atH+0; initialize own H length/capacity/empty chars. Inline/heap null-terminator target selects14041ab37 or1407a6f08 ->14041ab37.|
|14041ab37 ->145bd8074 ->14041ab49|Set empty terminator. CALL140116c70(H,&T,0,-1): deep-copy substring/full T.|
|14041ab4f ->140224cd4 ->14041ab56 ->141dc8ee5|Store T.flag atH+28. Set D.index=selected index, increment D.count. T.capacity<8 ->14041ab73; otherwise14228386f ->14041ab70 frees T heap chars via allocator+68.|
|14041ab73 ->145180d93 ->14041ab8c|Reset T length/capacity/inline null. CALL140420c90(D,0x40) sets history size; over-limit records' strings are destroyed synchronously. Then14041ab92 ->1451f441e.|
|1451f441e|S.capacity<8 ->14041aba7; otherwise14041ab99 ->1428ac671 ->14041aba4 frees S heap chars.|
|14041aba7 ->1454cbfe0 ->14041abb8|Reset S length/capacity/inline null; join common exit.|
|14041abb8 ->140a70a26 ->14041abbf ->144e03a2c|Cookie check1424fd340 then actual epilogue restores frame/registers; jump to original saved return address. All normal branch exits converge here.|

No unresolved indirect destination remains in this actor-aware map. All apparent closure/function addresses pushed to scratch stack are immediate control-flow routing constants consumed by that same invocation, not registered callbacks.

### Descendant contracts and resolved thunks

| Callee / resolved chain | Arguments and lifetime effect |
|---|---|
|140bffac0 ->140c293e0 ->140c26760|u16 ID; global name-manager lookup only; no writes/calls in terminal bounds-checked accessor. Missing singleton calls assertion with static text, not M/V.|
|14011dac0 ->14011f270 ->144e135df ->14011f2a6|RCX=&S,RDX=fixed format,R8=event-name. Wrapper saves varargs on its stack.14011f550 ->145a2fe3f merely copies S.allocator into stack wrapper. No module/vector argument forwarded.|
|14011f2bc ->145bfc94e|Allocator validation; success14011f2dd ->144fbc3ae constructs local string, sets empty/flag1. Assertion path1459308f1 ->14011f2d7 uses static diagnostics.|
|14011f304/14011f333 ->141ebd830 ->141ebe3b0|Two passes of fixed UTF-16 format: count then fill. Literal characters and `%s` only; reachable conversion141ec0cb0 copies UTF-16 chars or null fallback into local output;141ec1000 updates parser stack. Numeric/%n/float branches are unreachable for constant `(%s)`; no need to analyse unrelated format semantics.|
|14011f317 ->14011fd30 ->144ea6761|Resize local output string with scalar length and fill0x78. Capacity checks ->14011e820 growth or existing buffer; inline/heap fill via1452488c7,145b7359d,140f26863(REP STOSW); set length142224700 and terminator1401dbff7. Invalid size ->1424d5020 (terminal).|
|14011f347 ->14011e4a0 ->14508edc1|Append local output substring to S with offset0/count-1. Clamp length, check overflow, grow S if needed, select source/destination inline/heap; CALL14011e577 ->14251e6a0 copies characters only. Set S.length145a98c4a and terminator140188be7. Empty append returns via145bc08ba; invalid range/overflow ->1424d5044/1424d5020.|
|14011e820 ->144c6b4e2 ->145538c9f|Scalar capacity rounding/growth,14011ee80 ->145ca4053 ->14011ee99 allocator+50(length*2,alignment2). Copy old characters14011e8d7,free old chars14011e8ed; install new owned pointer140804658; length/capacity14054afd5/+d9; terminate145a9a69c; restore frame/return. No function-pointer/actor data stored.|
|14011f355 ->140a81e53 ->14011f362|Free local formatting heap through its allocator+68 when capacity>=8.14011f367 ->1458af323 ->14011f372 cookie check ->14550ad28 actual return.|
|14011b190 and140116c70|Copy UTF-16 into independent T/H buffers. Growth140116660 ->140116810 allocator+50; character copy14251e6a0; lengths/terminators only. Wrapper equality/self-substring path exists generally but S,T,H are distinct objects here. No backing-pointer adoption.|
|140117a10 ->140117a70 ->140117ab0|Copy allocator value only; not a string move, ownership transfer, actor retain or listener registration. Earlier naming it a possible move was not a conclusion.|
|14041ee40 ->140420670/140418e50|Grow D pointer map; allocate(size*8,align8), copy existing owned-record pointers, zero empty slots,free old map; replace map/capacity. No M,V/name pointer is inserted into record map.|
|1404208b0|Allocator+50(size0x30,align8) record allocation; error routines terminal. No actor argument submitted.|
|140420c90|Set size0x40; below size creates empty independently owned records; above size frees record character buffers, resets lengths/capacity and decrements count. All synchronous. These are text records, not a scheduler/job queue.|
|141ebc760,141ed0910 ->141f1ef90|Allocator acquisition; persistent global allocator144846dc0 is independent of ghost. Allocate/free/validation virtuals receive only allocator/owned buffers/scalar size/alignment.|
|14251e6a0,14251eaf0,1424fd340|Memory copy/fill and cookie-check leaves. Destinations are derived from output string/owned map, not actor/V. No callback registration.|
|141ebb5a0,1424d5020,1424d5044|Assertion / string allocation-length-range failures with static diagnostics; exceptional/fatal exits, not delayed actor execution. No fabricated successful return on these paths.|

### Write-set and pointer-escape audit

The following classifies stores by destination origin, not guessed type names. All obfuscation PUSH/POP/MOV/XCHG routing slots in the branch map are STACK_LOCAL: saved registers, argument wrappers, cookies, immediates and selected branch addresses. They are consumed before the actual frame exit. Such a stack slot can temporarily contain M or V; it is not persistent retention. Main string stack fields at[rbp-21..+07] and[rbp+0f..+37] are STACK_LOCAL; allocated chars referenced by them are HEAP_ALLOCATION with local ownership. No INPUT_VECTOR store exists. There is no store of M to global/heap/record; M-derived D is only the container destination/receiver.

| Store instruction VA(s) / operation | Destination class / base+offset | Source / alias conclusion |
|---|---|---|
|145b59f05/+f0d/+f11/+f1f;145b59ee2..145b59fb0 dispatch stores|STACK_LOCAL: prologue/register/cookie/routing slots|Saved M/V may occur only here; no escape.|
|140ec2f7d;1459aadfa/+afe/+b02/+b0a/+b0e/+b13|STACK_LOCAL: S allocator,length,capacity,terminator,flag|Allocator A,zero,7,1. Not actor/owner/vector.|
|145c45815;145aad550/+554/+558/+560/+564;141402ab2|STACK_LOCAL: T allocator,length,capacity,terminator,flag|Default allocator,zero,7,constant flag copied from S.|
|145840bec|GLOBAL:144846dc0|Default allocator RAX only; no actor/module/vector provenance.|
|14011b264/+26d/+29c/+2a7;140116cc4/+ccf/+d3a/+d43/+d62/+d6d|STACK_LOCAL or HEAP_ALLOCATION / H owned string: length+18,terminator;14251e6a0 copy at14011b292/140116d58|Scalar lengths and copied UTF-16 bytes; never S/T/V begin/end as stored pointers.|
|140804658;14054afd5/+d9;145a9a69c|HEAP_ALLOCATION string control: pointer+8,capacity+20,length+18,terminator|Fresh allocator resultR14,scalar sizes,zero. Pointer is owned character memory; not actor-owned module pointer.|
|14011e8d7;14011e577|HEAP_ALLOCATION / inline output: copied character ranges|14251e6a0 arguments proven string buffer/length; temporary event backing not source.|
|145a98c4a;140188be7;14532ed75;145656479|STACK_LOCAL S/local formatter string or owned chars: length/terminator|Scalar size/zero only.|
|141ec0d20/+d29/+d2c/+d5b/+d5e/+d64/+d90/+d99/+d9c|STACK_LOCAL format parser or HEAP_ALLOCATION output: remaining count,cursor,UTF-16 chars|Parser/output pointer advances and char values only; no persistent name pointer.|
|142224700;1401dbff7;1452488c9;145b735a0;140f26863 REP STOSW|STACK_LOCAL formatter string / its owned output: length,terminator,fill|Scalar format-count /0x78 fill /zero.|
|1456d2193;141dc8ee5/+ee9|CSChrBehaviorModule: D+20(index),D+28(count)|Masked/decremented numeric index and count+1; no pointer source.|
|1400f1457|QUEUE/RING: D.map[index]|Fresh0x30 history-record allocation RAX; not M,V or wrapper.|
|144ce0342|MODULE_OWNED_SUBOBJECT: H+0|Allocator copied from T via140117a10; not T address.|
|144ce0349/+34d/+355;145bd8074;140224cd8|MODULE_OWNED_SUBOBJECT: H capacity,length,inline zero,flag+28|Zero,7,constant flag; no actor/module/temp pointer.|
|14041efcd/+efd1;140418e50 stores;14251eaf0 fills|CSChrBehaviorModule D+10(new map),D+18(capacity); HEAP_ALLOCATION map slots|Fresh map and numeric capacity; existing owned H pointers only, empty slots zero.|
|140420ce1/+d1b/+d47/+d4e/+d87/+d8b/+d93/+da1/+da4/+da8 (size-growth path)|D index/map/count,GLOBAL allocator,H allocator/length/capacity/flag|Same owned records/default allocator/zero/7/1; no actor retention.|
|140420df9/+e05/+e13/+e16/+e1d (trim path)|MODULE_OWNED_SUBOBJECT and D counters|Reset7/zero, count-1/index0 after synchronous string free140420df6.|
|145180d93/+d9b/+d9f;1454cbfe0/+fe8/+fec|STACK_LOCAL: T/S reset|7,zero after allocator frees14041ab70/14041aba4.|
|All remaining RSP/RBP scratch stores in listed routing fragments and helper frames|STACK_LOCAL|Register saves/absolute branch constants/stack argument copies. No long-lived destination.|

No store category UNKNOWN remains for the actor-aware main branch. Allocator implementation metadata writes are owned allocator state, not replay callback pointer retention; no actor-bearing argument reaches that API. Generic memory intrinsics may use many instruction paths internally, but their destination regions are established above.

### +0x1928 and teardown cross-check

[CONFIRMED static] Constructor140418ef0 zeroes EBP at140418f1d; store140419047 clears the word M+1928/+1929. Specific writer14549de62 sets byte M+1928=1 then restores registers/returns via14041eaac. Other exact-byte reads include14564960c and141f8b92b; broad unrelated objects with the same numeric offset are excluded. For THIS branch its role is **enable flag for accumulating event-name text history** [HIGH CONFIDENCE meaning from use]; it is not a pointer, job handle, completion bit or refcount. No separate reset-to-zero API was established; construction clears it, and destruction frees the history regardless of its value. This absence of an identified convenience reset does not leave a retained pointer alive.

[CONFIRMED static] Module deleting wrapper1404196e0 ->140419410 before optionally freeing0x19c0. Destructor LEA14041948e computes D=M+1988; CALL14041949d ->14011de10 JMP145a8d880. The latter drains records:145576450 selects record; heap chars ->14014cd41 ->CALL14011de6d allocator+68;1455b3b5d resets length/capacity;1406ff1f8 null terminates;145c08f9d decrements count,1400c3037 resets index atzero,145c2160b loops until count0. It then frees all record allocations through14011dec6, map through14011dedd, resets D.capacity/map at145947b08/+b0c and restores its frame. Returning destructor frees additional D+8 storage at1404194ad and nulls it at1404194b3. This is synchronous teardown of independent text data, not a deferred module callback or Havok job.

### Result and stop condition

[CONFIRMED static within the bounded native contract] V is consumed as u16 IDs only; no begin/end/backing/wrapper pointer stored or ownership transferred. Event-name pointers are copied into text and not retained. Persistent data consists of scalar characters/flag/counters plus independently owned allocation/allocator pointers. No actor/ChrCtrl/ReplayGhost pointer retain, refcount increment, task creation, callback registration, scheduler/job/threadpool submission or deferred command carrying those pointers is present on this complete branch. The owned text container is synchronously destroyed with its module.

NO DESCENDANT ASYNC ESCAPE FOUND IN COMPLETE NONEMPTY BRANCH

SAFE GHOST CREATE/REMOVE LIFECYCLE STATICALLY COMPLETE

This is the requested static gate under the previously accepted native lifecycle/row contracts. It is NOT runtime ghost acceptance, an arbitrary-thread spawn guarantee, or a claim that the full saved replay works. Stop static lifecycle research here; do not add hypothetical blockers. Historical sections 5–8 and their UNKNOWN decisions are superseded for this edge.

### Minimum next implementation plan — NativeReplayGhostPrototype (not implemented)

1. Separate experimental output; preserve Phase5/runtime baseline. Retain exact executable guard and existing verified access; use the proven native execution/lifecycle context, never pipe-thread world mutation.
2. Obtain genuine native recorder data through the existing native serialize/decode path mapped in section1 (`1406f2410` / `1406f1f20`, native0x238 object). Mirror native reference ownership; do not fabricate a0x238 object or convert ERPLAY yet. If no valid native data, leave prototype inactive.
3. Explicit one-shot command, default OFF: call native factory path1406f27f0 in its valid context for ONE ghost; allow normal activation. Record actor/entry/ctrl/manipulator/type,gate+132 and data/refcount observations while objects are valid. Do not enable the manipulator manually or assume ctor equals activation.
4. Request native removal through the mapped world lifecycle path14050b340; retire ALL Theater-side pointers/handles immediately. For logging after retirement, observe valid native removal/destructor/deletion hooks with callback-local arguments/identity epochs; never poll the old object after removal.
5. Trace entry.actor null,gate disabled,DelayDelete invocation,destructor and data-ref release. Read refcounts only before the corresponding release/free boundary. Additional native owners can change counts; never demand a guessed fixed value. Acceptance requires real no-crash/no-stale-access observation; this task performs none.
6. Only after Prototype1 create→inspect→remove is user/runtime verified, Prototype2 may use real native Idle→Walk→Stop to observe movement,rotation,animation,grounding,cursor/end and live input isolation. No fidelity claim now.

