# Bloodstain / ghost pipeline

[STATIC_VERIFIED] PlayerIns records through+5c8 →1404e5af0 → state/event builders1404e6630/1404e65d0 → encoders1404e9f90/1404eb680 → two bounded node payloads. Export1404e5530 writes counts and length-prefixed blocks; caller1406f2410 adds actor state. Serialization is mutating.

[STATIC_VERIFIED] factory140404570 allocates0x760 → ReplayGhostIns constructor1404f1840. Constructor prepares player config through1404035b0 with kind3, calls PlayerIns ctor140650c90, retains data at ghost+740, obtains current manipulator, checks type3 and attaches same data through1403df010. Player factory14065db40 selects ReplayManipulator for config+24==3 and stores at player+588. This proves a native replay ghost/manipulator data attachment, not complete network bloodstain provenance.

RTTI identifies BloodstainGhostDownloadJob, BloodstainUploadJob, BloodstainListDownloadJob, FNBloodstain/FNBloodstainImpl. Their codec and route to140404570 are unresolved. Ordinary own-death recoverable-rune metadata must not be conflated with downloaded other-player ghost recording.

Runtime acceptance: recorder buffer changes with gameplay; observed ReplayGhostIns has type3 manipulator, owning actor matches, ghost+740 matches manipulator+100, attached buffers are bounded and evolve during playback. Recorder buffer population is runtime-verified (see NATIVE_REPLAY_RUNTIME_RESULT.md); ghost identity and playback remain unobserved. Offline session may contain no usable bloodstain ghost: record absence; do not enable online play for this experiment.

## Evidence rules

Exact target disk SHA256: D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134; version2.7.0.0, AMD64, patch1.17. VAs assume preferred image base0x140000000; runtime addresses are ASLR base+RVA.

STATIC_VERIFIED means exact image bytes support the stated operation; REFERENCE means SDK/source only. At the initial checkpoint runtime measurements were absent; subsequent read-only results are in NATIVE_REPLAY_RUNTIME_RESULT.md. Decompiler names/argument reconstruction are hypotheses. Linear extraction may include padding/adjacent functions after RET; these bytes do not belong to the preceding function automatically.

Evidence artifacts (kept outside repository): `../research/ghidra-eldenring/targeted/native_bloodstain/verified_bytes/<VA>.json` include exact bytes, indexed callers/callees, pseudocode and disassembly. Partial Ghidra analysis does not guarantee complete xrefs.
