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

STATIC_VERIFIED means exact image bytes support the stated operation; REFERENCE means SDK/source only. Runtime measurements in this checkpoint: NONE. Decompiler names/argument reconstruction are hypotheses. Linear extraction may include padding/adjacent functions after RET; these bytes do not belong to the preceding function automatically.

Evidence artifacts (kept outside repository): `../research/ghidra-eldenring/targeted/native_bloodstain/verified_bytes/<VA>.json` include exact bytes, indexed callers/callees, pseudocode and disassembly. Partial Ghidra analysis does not guarantee complete xrefs.

## Update task chain cross-check

[STATIC_VERIFIED] ChrIns ctor1403e6e30 initializes task+4d8, owner+4f8, callback+500=1403f92b0. The callback loads dt from actor+b0, constructs task data and invokes actor vtable+c0. Exact PlayerIns vtable142a7fbb0+c0 AND ReplayGhostIns vtable142a4c558+c0 both point to140660ac0, which calls1404e5af0 through player+5c8. The pinned SDK names the corresponding task update_replay_recorder_task; actual task-group scheduling/order remains runtime-unverified. No constructor/callback invocation is performed by the probe.
