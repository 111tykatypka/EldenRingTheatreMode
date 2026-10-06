# ReplayManipulator

[STATIC_VERIFIED] RTTI + COL identify vtable RVA2a2eda0. Constructor VA1403deaa0; allocation0x150. Type method VA1403def60 is `mov eax,3; ret`. Pinned SDK declares a reference-return ABI: do not invoke that declaration. Probe reads literal opcode instead.

[STATIC_VERIFIED] attach VA1403df010 refcounts incoming data and stores it at+100. Mode byte+14c chooses data+220/+230, then calls1403dfd50. That function chooses count at data+d0 or+224 and sums results from1403dfdc0. Frame decoding/advance1403dfdc0 still needs full disassembly correlation.

Owner reference+ a0 is REFERENCE from pinned manipulator prefix; runtime checks compare to current actor. +132 toggled by tiny methods1403dee00/1403deec0; these are enable/disable-like setters, NOT proven update methods. Do not mistake adjacent linear disassembly for their body.

Pad vtable RVA2a2e788 / constructor3d8670 / literal type1; Network RVA2a2e1b8 / constructor3d4a00 / type2 candidate; NetAI RVA2a2d858 / constructor3d2f00 / type4 candidate. Read actual actors before treating these as ownership behavior.

No manipulator swapping, initialization or update virtual calls implemented. Research build captures full ReplayManipulator only when RTTI and literal type agree, and reads attached prefix only after owner match.

## Evidence rules

Exact target disk SHA256: D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134; version2.7.0.0, AMD64, patch1.17. VAs assume preferred image base0x140000000; runtime addresses are ASLR base+RVA.

STATIC_VERIFIED means exact image bytes support the stated operation; REFERENCE means SDK/source only. Runtime measurements in this checkpoint: NONE. Decompiler names/argument reconstruction are hypotheses. Linear extraction may include padding/adjacent functions after RET; these bytes do not belong to the preceding function automatically.

Evidence artifacts (kept outside repository): `../research/ghidra-eldenring/targeted/native_bloodstain/verified_bytes/<VA>.json` include exact bytes, indexed callers/callees, pseudocode and disassembly. Partial Ghidra analysis does not guarantee complete xrefs.

## Concrete native consumer path

[STATIC_VERIFIED] ReplayManipulator vtable slot10 (+50) is1403df190. It checks enable+132, reads timing+108/+110/+114, advances frames via1403df650, applies movement through virtual+d8 and orientation via1403cdc20, then subtracts dt from remaining frame time and adds dt to elapsed playback. Calling/ABI/scheduling remains unverified.

[STATIC_VERIFIED]1403df650 loads attached data+100, obtains block via1406f1e60/1406f1e90, decodes via1404e9490 and related functions; it calls actor/module routines1404236f0 and140423180..140423090 and actor virtual+220. These calls establish richer native control than XYZ writes; exact action/equipment semantics not yet proven.

[STATIC_VERIFIED] primary attached data count+d0, buffer pointer+d8; secondary count+224, buffer+228. Both getters index fixed0x104 blocks. Each block begins u32 encoded length followed by0x100 payload; consumer1403dfdc0 builds bounded input and decoder1404e9490, validity1404e9940, float duration getter1404e9840 (decoder+2c). This serialized-array representation differs from live0x248 linked recorder nodes. Never reinterpret nodes directly as imported ghost data.
