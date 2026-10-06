# Native ghost lifecycle — precise blocker checkpoint

Result **B: PRECISE BLOCKER IDENTIFIED**. No spawn toggle, build or runtime test added. Runtime source/binaries unchanged. Branch `codex/native-bloodstain-replay-research`, implementation baseline `5e3ba5f`.

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

**Unresolved:** concrete ChrCtrl call site invoking virtual+50, its task registration and phase. ChrIns ctor callbacks1403f8ff0/1403f8700/1403f9390/1403f9470 dispatch actor virtuals, but none is yet proven to reach this consumer. Do not assign the recorder task to manipulator update.

## 3. Actual start gate

[CONFIRMED static] Constructor1403deaa0 initializes+132=0 (u32 zero at+130), elapsed+110=0, first-frame+14e=1. Replay-data constructor initializes both cursors0. Attach1403df010 retains data/calculates duration; it does NOT set+132. Vtable slot3/+18 ->1403dee00 is exactly `mov byte [rcx+132],1; ret`; slot4/+20 ->1403deec0 clears it. Thus valid data plus an enabled+132 gate plus scheduled update is necessary.1403df000/1404f2340 changes stream selection, not this gate.

**Unresolved:** native caller of the enable virtual, actor initialization readiness, and whether ghost initialization automatically enables it. The update gate is found; the complete start mechanism is NOT proven. Calling the setter manually would bypass the missing lifecycle contract.

## 4. Removal and reference release

[CONFIRMED static]14050b340 notifies actor virtual+58, removes matching active-vector entry+1e630/+1e638, finds actor in sets including+10f38, sets actor+1c5 bit20, calls140e78ca0 at14050b55e. Queue insertion allocates/reuses0x20 entries, stores actor payload and links into the native queue. It does not immediately invoke the deleting destructor.13 indexed callers include14050efa0 (world request-queue drain),1406515f0 and140b02ef0. The latter's actor class is unproven, so it is NOT accepted as a genuine replay-removal example.

[CONFIRMED static] Ghost destructor1404f1ab0 releases replay-data+740 BEFORE PlayerIns destructor1406515f0. PlayerIns destroys ChrCtrl+58 and then invokes14065e6c0, which destroys owned manipulators+590 and+588 through virtual+8/allocator. ReplayManipulator destructor1403dec70 frees decoder allocations+140/+138, then releases replay-data+100. Ref-release helper141ebc000 is checked for old count1 before deleting virtual. For one primary ghost with no extra owners: temporary creator retain plus ghost/manipulator retains imply3 ->2 after creator release ->1 after ghost release ->0 after manipulator release. This is a conditional static ownership model, NOT a measured runtime refcount/leak result; additional actors/owners change counts.

**Correction to previous cleanup candidate:**140ade693 ->1403ee5c9 calls140e55380 (constant return0), then145c4dc98. Exact branch window testsAL and CMOVE-selects1403ee672, which jumps to1428f37bd, a stack-unwind/return epilogue. This examined route contains NO proven store clearing ChrSetEntry. Calling it “slot cleanup” is unsupported. Do not use it as an unregister API.

**Decisive missing contract:** native queue processor/final actor-destruction executor and its task phase; exact store nulling ghost_chr_set entry (and when other actor tasks cease); genuine ghost's removal request trigger. Current request function does not prove these guarantees. ChrIns destructor unregisters task objects, but completion/barriers and slot clear ordering are not established.

## Decision

No safe spawn implementation. Do not call constructors/setters/removal experimentally from PostPhysics. Next narrowly scoped static work: resolve TestNetStep scheduler phase, ChrCtrl enable/update call sites, native removal queue executor and exact slot-clear store. No new human test requested because no build exists that can safely test those contracts. Native playback remains unverified.
