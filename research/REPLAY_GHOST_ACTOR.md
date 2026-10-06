# Replay ghost actor and world positioning

[STATIC_VERIFIED] ReplayGhostIns RTTI/vtable RVA2a4c558; native object0x760; incoming data+740 retained by refcount. Factory/constructor chain documented in BLOODSTAIN_GHOST_PIPELINE.md. Alternate factory140403d50 → ctor1404f19c0 creates different/default configuration, not interchangeable.

[REFERENCE] WorldChrMan.ghost_chr_set and ChrType::BloodstainGhost names from SDK. Runtime type must be obtained from RTTI and manipulator; set membership alone is insufficient. Read-only probe samples up to512 ghost slots and one non-player distance actor; truncation logged. These are diagnostic budgets, not replay actor limits.

[HIGH CONFIDENCE] coordinate conversion14061f0c0 participates recorder builder; block ids in nodes help native world positioning. Exact local/world map resolution, loaded-area lifecycle, model/physics grounding and collision policy remain UNKNOWN. Do not feed absolute XYZ into unidentified codec.

Recommended eventual design: retain host timeline/container; independent native replay actor with genuine replay ownership and decoded native data. This is a candidate design only. Do not replace current stable player playback until ghost allocation/lifetime, streaming and consumer API verified.

## Evidence rules

Exact target disk SHA256: D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134; version2.7.0.0, AMD64, patch1.17. VAs assume preferred image base0x140000000; runtime addresses are ASLR base+RVA.

STATIC_VERIFIED means exact image bytes support the stated operation; REFERENCE means SDK/source only. At the initial checkpoint runtime measurements were absent; subsequent read-only results are in NATIVE_REPLAY_RUNTIME_RESULT.md. Decompiler names/argument reconstruction are hypotheses. Linear extraction may include padding/adjacent functions after RET; these bytes do not belong to the preceding function automatically.

Evidence artifacts (kept outside repository): `../research/ghidra-eldenring/targeted/native_bloodstain/verified_bytes/<VA>.json` include exact bytes, indexed callers/callees, pseudocode and disassembly. Partial Ghidra analysis does not guarantee complete xrefs.
