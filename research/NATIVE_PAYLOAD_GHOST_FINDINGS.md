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
