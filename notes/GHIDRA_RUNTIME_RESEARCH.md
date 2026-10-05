# Ghidra to runtime: first targeted checkpoint

2026-10-05. Branch `codex/ghidra-runtime-research`, parent `cb2c8a9`.
Target: EldenRing_1_17, AMD64, WW 2.7.0.0, SHA-256
`D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134`.
Pinned SDK: `3c8c1d7633a99309fb004c9f894ea10b7967d0e0`, unchanged.

**IMPLEMENTED — RUNTIME VALIDATION REQUIRED.** This checkpoint implements a
query tool and opt-in read-only class/collection instrumentation. Native NPC
ownership, native gait driving and native ghost playback are not implemented.
No new engine calls or actor writes are introduced. Earlier Modern experimental
input/replay code remains present; do not activate it for this diagnostic test.

## Research dataset

374,920 discovered entries, 349,625 C pseudocodes, 134,900 extracted strings,
3,446,364 xrefs. 134,900 is NOT a pseudocode line count. One-hour analysis timeout;
42,455 C entries include warnings. Generated signatures and function boundaries
remain hypotheses. FTS matches alone cannot establish class ownership or ABI.

The read-only query tool and actual schema are documented in
`tools/ghidra_query/README.md`. Local reproducible evidence:
`../research/ghidra-eldenring/targeted/` (outside Git).
`sdk_anchors.json` contains string addresses/xrefs, validated MSVC x64 COL
structures, hierarchy bytes and first candidate vtable slots. Addresses below
are preferred VAs (base 0x140000000), NOT live process pointers.

## Correlated types

All vtable rows are [HIGH CONFIDENCE] static class identity, NOT runtime validation.
COL signature=1, descriptor RVA, self RVA and pointers back to the COL were checked
directly against hash-verified PE section bytes; target slots point into executable
sections. The first slots are not automatically promoted into callable bindings.

| SDK type | Candidate vtable VA | Evidence / boundary |
|---|---|---|
| WorldChrMan | UNRESOLVED | [CONFIRMED] name strings and task strings; no exact WorldChrMan COL found by this pass |
| PlayerIns | 0x142a7fbb0 | RTTI PlayerIns@CS, COL 0x1432e7df0; known local-player access retained |
| ChrIns | 0x142a310c8 | RTTI, COL 0x1432d03a0; subclass/lifetime still needs runtime validation |
| ChrCtrl | 0x142a2c6a0 | RTTI, COL 0x1432cf428; no proxy writes enabled |
| ReplayRecorder | 0x142a4aa40 | RTTI, COL 0x1432d8870; constructor/destructor cross-checked |
| NetChrSync | UNRESOLVED | No matching literal RTTI/name string found; absence does NOT imply absence of subsystem |
| NetChrSetSync | 0x142a49c50 | RTTI, COL 0x1432d8778; SDK per-set placement/health buffers, not a locomotion API |
| ChrManipulator | 0x142a2c8e8 | RTTI and COL; vector meanings remain uncertain in SDK |
| ReplayManipulator | 0x142a2eda0 | Actual native name, not ChrReplayManipulator |
| PadManipulator | 0x142a2e788 | Actual native name, not ChrPadManipulator |
| ComManipulator | 0x142a2cd70 | Actual native name, not ChrComManipulator |
| NetworkManipulator | 0x142a2e1b8 | RTTI/COL; packet codec and ownership transfer unknown |
| CSChrPhysicsModule | 0x142a3c890 | RTTI/COL; Havok/model synchronization consumer not yet resolved |
| CSChrBehaviorModule | 0x142a35d48 | RTTI/COL; graph parameter producer not yet resolved |
| CSChrActionRequestModule | 0x142a32780 | RTTI/COL; existing normalized requests are not a verified gait driver |

### WorldChrMan string is not a singleton address

[CONFIRMED] `0x140095fa0` loads a pointer, resolves it conditionally, stores ASCII
WorldChrMan / UTF16 WorldChrMan at +0x38/+0x40 and tail-jumps to 0x141ec2860.
Evidence: `world_registration_disasm.json`, string refs at 0x140095fb5/0x140095fc0.
[HIGH CONFIDENCE] this is class/name registration infrastructure. Do NOT derive a
game WorldChrMan pointer from this string xref. Preserve the verified SDK singleton.

### Character enumeration

[CONFIRMED] pinned `cs/world_chr_man.rs` defines distance vector entries as:
NonNull<ChrIns>, f32 distance, unknown u32; 16 bytes on x64.
`0x14050f9e0` has the following independently decoded instructions:

- 0x14050faa5: mov rdi, [rbx+0x1f1e0]
- 0x14050faac: sub rdi, [rbx+0x1f1d8]
- 0x14050fc00: mov rcx, [rbx+0x1f1d8]
- 0x14050fc14: mov ecx, [rcx+r14+0xc]
- 0x14050fd4e: add r14, 0x10

[HIGH CONFIDENCE] this operates on the SDK distance-vector layout, with update
selection/omission behavior. `enumeration_candidate.json` and
`enumeration_disasm.json` retain evidence. The +0xc meaning stays UNKNOWN.
This is an update collection, NOT proof of every loaded actor or a stable lifetime.

Current `character_capture.rs` already reads this vector at PostPhysics. Do not
replace it with arbitrary traversal. New 1Hz diagnostics compare distance-list
length, priority-list length and ChrSet capacities without dereferencing extra
actor entries. Capacity is not occupancy. Read-only capture, handle resolution,
disappearance and re-entry need the first real NPC runtime test before any writes.

### Control ownership: SDK ABI discrepancy found

[CONFIRMED] ReplayManipulator candidate vtable slot 2 is 0x1403def60:
`mov eax,3; ret`. PadManipulator slot 2 is 0x1403d9930: `mov eax,1; ret`.
The values match SDK ManipulatorType::Replay/Pad.

[CONFIRMED] pinned `cs/chr_manipulator.rs` declares
`manipulator_type(&self) -> &ManipulatorType`. [HIGH CONFIDENCE] this reference
return is incompatible with the scalar values returned by these exact functions.
DO NOT invoke this typed virtual method or dereference its result for this build.
No SDK dependency has been changed; no virtual function is called by this checkpoint.
Independent ABI validation is required before a future value-returning binding.
Replacing a manipulator pointer is NOT an ownership mechanism: construction,
destruction, actor registration, per-task consumers and restoration remain UNKNOWN.

### Native recorder / ghosts

[CONFIRMED] constructor candidate 0x1404e4f70 stores vtable at object+0 and owner
rdx at +0x10. It initializes subobjects at +0x70/+0x2c0/+0x460/+0x6b0, and writes
at +0x850. Vtable xrefs point to this constructor and destructor 0x1404e5070.
Deleting-destructor slot 0x1404e5310 calls 0x1404e5070, tests deletion flag bit 0,
then supplies 0x860 to a deallocation call. Caller index finds 0x140657840 calling
the constructor; its complete lifecycle has not been established.

[HIGH CONFIDENCE] native recorder allocation is much larger than the known SDK
0x70 prefix. It is not a flat struct to memcpy/save/restore. SDK names for counters
and oldest transform are source evidence, not a decoded frame/action codec.
New runtime instrumentation only reads that prefix after matching vtable RVA
0x2a4aa40 against the actual module base, and compares owning_player with current
main_player. Mismatch logs and skips counters. No frame buffer or native API calls.
Ghost ChrSet and ReplayManipulator coexist, but their full spawning/playback
contract remains UNKNOWN. Do not replace ERPLAY with native recorder yet.

### Locomotion and grounding

[CONFIRMED, SDK SOURCE] ChrCtrl has manipulator, modifier, model/physics matrices,
vertical offset, proxy flags. Source comments define bits 0/1 as position/rotation
sync requests. Existing `grounding.rs` and `locomotion_trace.rs` observe these
layers and root-motion/rate/action values. Their presence does not establish the
callback order or consumption ABI. Native model matrix constructor/flag consumer,
analog input producer and Walk/Run/Sprint graph driving are still UNKNOWN.
No arbitrary Y correction, proxy flags, root-motion writes or animation request
changes were added. Next static work should follow ChrCtrl/physics module call
graphs around these exact vtables, using the live differential trace as a filter.

## Executable changes and build

- Query tool: readonly SQLite, FTS, names, ranges, xrefs, calls, raw bytes,
  disassembly, pointer-table and structural RTTI candidates.
- Adapter: opt-in `research_readonly.rs`, existing PostPhysics callback, 1Hz
  summaries, compiled `offset_of!` layout evidence, recorder vtable/owner checks.
- Native profile/hash guard, task signature, pinned dependencies and IPC unchanged.
- Release Rust AMD64 build PASS (existing crate naming warning only).
- Release C++ host build PASS. Real database queries and PE disassembly executed.
- Unit tests not run in this checkpoint; no runtime success claimed.
- Separate artifacts: `../experimental/GhidraRuntimeResearch/`.
- Phase5 EXE/DLL hashes checked; no files in Phase5 overwritten.

## Next controlled runtime test

1. Stop any recording; close Theater Mode and Elden Ring. Restart is necessary
   to load this new DLL. Preserve your normal offline/modded YAFSML workflow.
2. In the separate artifact folder run `Enable-ReadOnlyResearch.ps1` (PowerShell).
   It sets `%LOCALAPPDATA%/EldenRingTheaterMode/Research.readonly.ini` enabled=1.
3. Launch that folder's EldenRingTheaterMode.exe. Select existing YAFSML in Launcher
   and click Start Elden Ring. The existing launcher selects TheaterMode.dll next
   to the EXE; no manual copy into game/Phase5. With a custom loader config, point
   only the Theater DLL entry at this research DLL and do not load two versions.
4. Load a familiar safe location with ordinary nearby NPCs. Avoid a boss/warp.
   Wait for PLAYER FOUND. Do NOT activate replay or transform probes yet.
5. Stand still ~5 seconds, walk/turn near NPCs ~15 seconds, then stop ~5 seconds.
   Optional: existing diagnostic trace UI Start/Mark Idle/Walk/Run/Stop to correlate
   locomotion. Labels are user supplied; they are not automatic gait detection.
6. If comfortable, record 20–30 seconds using existing F5/F6. Check actual actor
   tracks in the replay browser. Do not interpret zero count as no actors in world.
7. Send `%TEMP%/TheaterModeGame.log`, `%TEMP%/TheaterModeLocomotionTrace.log` if
   enabled, recorder log in LocalAppData/EldenRingTheaterMode/logs, and new .erplay.
   Look for RESEARCH_LAYOUT / RESEARCH_ENUM / RESEARCH_RECORDER and class match.
8. Disable with `Enable-ReadOnlyResearch.ps1 -Disable`, close game, then reload your
   unchanged Phase5 DLL for the stable replay experience.

Expected visual result: normal gameplay and existing capture UI; no new NPC
movement, no forced animation or grounding correction. Any crash/mismatch is a
failed diagnostic checkpoint, not a verified feature. The next implementation
decision depends on actual collection counts, class/owner checks and trace values.
