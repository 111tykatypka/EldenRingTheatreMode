# Read-only native replay runtime test

Status: IMPLEMENTED — RUNTIME VALIDATION REQUIRED. No native ghost replay feature claimed.

1. Close Elden Ring and all previous Theater Mode hosts before replacing/loading a DLL. Phase5 remains untouched.
2. Use the EXE and DLL together from NativeBloodstainReplayResearch. Launch its EldenRingTheaterMode.exe first.
3. Use its existing launcher/YAFSML workflow. Inspect the selected DLL path: it must be this build's TheaterMode.dll; if the loader configuration still points to another output, change that configuration via existing host settings. No new injector or game installation changes.
4. Load a normal offline save in a safe loaded area. No particular replay/start location required; this experiment does not play ERPLAY.
5. Leave stationary5s, then walk/run/roll/jump and perform a safe attack for20–30s. No death required. Do not press Play/ownership/write-probe controls; DLL rejects write commands anyway.
6. If a normal bloodstain interaction is available in that offline session, activate it normally and observe the ghost. If none exists, report unavailable; do not go online.
7. Tell developer when world loaded and when sequence completed. Visible player control should remain normal. Diagnostics run automatically at1Hz; no address entry/console required.
8. Return `%TEMP%\TheaterModeGame.log` and newest `%LOCALAPPDATA%\EldenRingTheaterMode\native-replay\native_replay_*.jsonl`. Host log from normal logs folder helps startup diagnosis.

Analyze journal: `python analyze_probe.py <journal.jsonl> --output native_analysis.json`. JSON reports malformed/torn records, changed words, node lengths/hashes, control identities and source drops. Hash changes prove data changed, not decoded semantics.

Research feature removes write capabilities and rejects write commands before they reach probes. Game callback skips transform, actor, ownership and early input mutation paths. All evidence snapshots use bounded own-process ReadProcessMemory; no unknown engine virtual calls. File/log I/O is on worker; bounded queue drops rather than blocking callback. Existing verified profile/task/capture/IPC/launcher retained.

Runtime acceptance still outstanding: actual recorder buffer population; actual bloodstain ghost manipulator/data matching; native playback consumer behavior; world position/grounding. Compilation/unit tests do not satisfy these.

## Evidence rules

Exact target disk SHA256: D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134; version2.7.0.0, AMD64, patch1.17. VAs assume preferred image base0x140000000; runtime addresses are ASLR base+RVA.

STATIC_VERIFIED means exact image bytes support the stated operation; REFERENCE means SDK/source only. At the initial checkpoint runtime measurements were absent; subsequent read-only results are in NATIVE_REPLAY_RUNTIME_RESULT.md. Decompiler names/argument reconstruction are hypotheses. Linear extraction may include padding/adjacent functions after RET; these bytes do not belong to the preceding function automatically.

Evidence artifacts (kept outside repository): `../research/ghidra-eldenring/targeted/native_bloodstain/verified_bytes/<VA>.json` include exact bytes, indexed callers/callees, pseudocode and disassembly. Partial Ghidra analysis does not guarantee complete xrefs.
