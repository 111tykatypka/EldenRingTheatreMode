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
