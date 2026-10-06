# References and local research

Snapshot2026-10-06. This handoff inventories existing evidence; it does not re-fetch upstream sources or imply compatibility beyond the exact target.

## Source policy

Game installation/reference directories are read-only. Do not redistribute original game/camera binaries, proprietary assets or large game-derived pseudocode. Use independently implemented behavior and comply with source licenses. No secrets/.env are included.

|Reference|Role|
|---|---|
|https://github.com/111tykatypka/EldenRingTheatreMode|Actual application|
|https://github.com/KamiyamaShiki0704/fromsoftware-rs|Pinned runtime bindings, revision3c8c1d7633a99309fb004c9f894ea10b7967d0e0|
|https://github.com/vswarte/fromsoftware-rs|Original bindings upstream|
|https://github.com/Dasaav-dsv/libER|Alternative native interface reference; older version evidence|
|https://github.com/NightFyre/EldenRing-SDK|Older structure/native reference, not assumed2.7-compatible|
|https://github.com/FriXeee/ELDENRING-INTERNAL|Internal integration reference|
|https://github.com/KamiyamaShiki0704/ERSoundBankLoader|Singleton/task/signature integration examples|
|https://github.com/KamiyamaShiki0704/ERGparamPreloadPatch|Native initialization/scanning examples|
|https://github.com/techiew/EldenRingModLoader|Loader reference, not actual current loader|
|https://github.com/techiew/EldenRingMods|Mod architecture reference|
|https://github.com/Logersnamed/FreecamMod|Camera/game/time/graphics research|
|https://github.com/soulsmods/EldenRingHKS|Movement/action/animation script reference|
|https://github.com/The-Grand-Archives/Elden-Ring-CT-TGA|Cheat table aliases, animation/warp anchors; wrong layout candidates rejected|
|https://github.com/micthiesen/unseamless-coop|Native replay/network source-index reference|
|https://github.com/LukeYui/EldenRingSeamlessCoopRelease|Release/docs reference, not proof of native codec|
|https://ersc-docs.github.io/how-to-install-and-update/|Installation/session reference only|
|https://github.com/NationalSecurityAgency/ghidra|Partial headless analysis/export tooling|
|https://github.com/ocornut/imgui/tree/v1.92.5-docking|Vendored UI|
|https://github.com/TsudaKageyu/minhook|Hook library, vendored release unidentified|
|https://opm.fransbouma.com/Cameras/eldenring.htm|Otis_Inf public IGCS client/injected module and camera feasibility|

IGCS reference informs unlimited camera/FOV/time/path nodes/Catmull-Rom/easing/constant-speed/player-relative/state-save concepts. It does not prove full actor replay or supply permission to copy proprietary implementation. No known consulted Paramdex/SoulsFormats/Discord/video/paper should be invented. Additional actually recorded URLs/revisions and per-source files appear in appended source index.

## Local assets

Root research: `C:\Users\user\Documents\ChatGPT\elden ring theater mode\research` with community-nightly, ghidra-eldenring, native-replay-runtime, nightly_snapshot. Actual file sizes/existence in inventory.json; nothing large copied into Git.

Ghidra database `...\ghidra-eldenring\export\research.sqlite`: **877510656 bytes**. Export from exact-SHA analysis copy `input/eldenring.exe`; one-hour headless timeout, incomplete auto-analysis, extraction/index build. README/RESEARCH_REPORT/manifest/PE metadata capture provenance. About349625functions,3446364references,42455warning entries. Partial xrefs/indirect calls, pseudocode errors and linear disassembly contamination require cross-checks.

Targeted native_bloodstain/native_payload/native_lifecycle/native_scheduler/native_descendants/native_prototype folders contain byte/disassembly/xref/caller/callee evidence, outside repository. Native runtime JSONL under `%LOCALAPPDATA%\EldenRingTheaterMode\native-replay`; logs `%TEMP%\TheaterModeGame.log`, render/crash logs and `%LOCALAPPDATA%\EldenRingTheaterMode\logs\TheaterModeRecorder.log`.

Historical read-only directories: `C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences`, sibling EldenRing_CameraTools_v1018, original Desktop camera directory. Paths can be moved/missing; only existing assets appear in measured inventory.

SDK local checkout `%USERPROFILE%\.cargo\git\checkouts\fromsoftware-rs-7e356ae17759562a\3c8c1d7`. Caches/source versions are reference material, not mutable project code.

## Query tool already exists

`tools/ghidra_query/research_query.py`, Python3.10+, read-only SQLite(mode=ro/query_only). Commands text/rtti/functions/code/references/callers/callees/range/around/vtable/disasm/schema. Example `python tools/ghidra_query/research_query.py callers 0x140703e30`. --database/--limit/--output precede command. around radius is bytes; offset searches textual constants, not proven field accesses. Optional Capstone5.0.6, separate research dependency directory; exact SHA verified for bytes/disassembly. Schema/usage appendix below.

Inventory directory records use measured recursive regular-file sum, excluding .git/.env, not directory inode size. Giant decompiled function trees are aggregate metadata; not copied or silently claimed complete source restoration.

---

## Evidence appendix: `research/NATIVE_REPLAY_SOURCE_INDEX.md` (repository authored evidence snapshot)

Historical statuses retain their original scope. Latest Step placement/26 guards/observed timeouts in the opening sections supersede older builder-only and untested-callsite wording. Static proof does not mean successful native creation.

# Native replay local source index

References read without modifications. New clones are snapshot references (no automatic pulls). SDK dependency remains pinned at3c8c1d7. Commit SHA proves source revision, not binary compatibility.

## https://github.com/vswarte/fromsoftware-rs

- Local: `C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\NativeReplayResearch\github\fromsoftware-rs-vswarte`
- SHA: `59fbd3b3b7daaf14aca47c9f73530493dba6bc79`
- Matching files: 18
- Files: `tools\debug-darksouls3\src\display\world_chr_man.rs`, `tools\debug-eldenring\src\display\world_chr_man.rs`, `crates\eldenring\src\cs\chr_ins.rs`, `crates\darksouls3\src\sprj\world_chr_man.rs`, `tools\debug-eldenring\src\display\game_data_man.rs`, `crates\darksouls3\src\sprj\game_data_man.rs`, `crates\darksouls3\src\sprj\chr_ins.rs`, `crates\eldenring\src\cs\world_chr_man.rs`, `crates\eldenring\src\cs\game_data_man.rs`, `crates\darksouls3\mapper-profile.toml`, `crates\eldenring\src\cs\field_ins.rs`, `tools\param-generator\params\sekiro\NetworkAreaParam.xml`

## https://github.com/KamiyamaShiki0704/fromsoftware-rs

- Local: `C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\NativeReplayResearch\github\fromsoftware-rs-kamiyama`
- SHA: `7d4fdd66eda1a35284c170feb4b07b058c005f8f`
- Matching files: 16
- Files: `tools\debug-darksouls3\src\display\world_chr_man.rs`, `tools\debug-eldenring\src\display\world_chr_man.rs`, `tools\param-generator\params\darksouls3\NetworkAreaParam.xml`, `crates\eldenring\src\cs\world_chr_man.rs`, `tools\param-generator\params\darksouls3\PlayRegionParam.xml`, `crates\eldenring\src\cs\chr_ins.rs`, `crates\eldenring\src\cs\field_ins.rs`, `tools\param-generator\params\eldenring\NetworkAreaParam.xml`, `tools\param-generator\params\sekiro\NetworkAreaParam.xml`, `tools\param-generator\params\eldenring\PlayRegionParam.xml`, `tools\param-generator\params\sekiro\PlayRegionParam.xml`, `crates\darksouls3\mapper-profile.toml`

## https://github.com/Dasaav-dsv/libER

- Local: `C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\libER`
- SHA: `d8ae5c92719adca4344d8d902b63286b807f562b`
- Matching files: 3
- Files: `include\param\paramdef\NETWORK_AREA_PARAM_ST.hpp`, `include\param\paramdef\NETWORK_PARAM_ST.hpp`, `include\param\paramdef\PLAY_REGION_PARAM_ST.hpp`

## https://github.com/NightFyre/EldenRing-SDK

- Local: `C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\EldenRing-SDK`
- SHA: `37652c02bf91c845aac14c0a4e1ee0f9a965df9c`
- Matching files: 0
- Files: No direct Replay/Bloodstain source hits; architecture/loader reference only

## https://github.com/FriXeee/ELDENRING-INTERNAL

- Local: `C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\ELDENRING-INTERNAL`
- SHA: `ab62d3b64495fec46c3ff0e0c1eca1fc52dcbc10`
- Matching files: 0
- Files: No direct Replay/Bloodstain source hits; architecture/loader reference only

## https://github.com/soulsmods/EldenRingHKS

- Local: `C:\Users\user\Documents\ChatGPT\elden ring theater mode\research\community-nightly\EldenRingHKS`
- SHA: `d88d6441f5fccfdd6a5fd10d493309b680181897`
- Matching files: 0
- Files: No direct Replay/Bloodstain source hits; architecture/loader reference only

## https://github.com/The-Grand-Archives/Elden-Ring-CT-TGA

- Local: `C:\Users\user\Documents\ChatGPT\elden ring theater mode\research\community-nightly\Elden-Ring-CT-TGA`
- SHA: `7926205c5a2ed236dd31278c4f5579c964ceec35`
- Matching files: 1
- Files: `CheatTable\CheatEntries\Open - The Grand Archives - Elden Ring\Coordinates & Teleport\Teleport to Map-Relative Coordinates\ Teleport to Bloodstain.xml`

## https://github.com/Logersnamed/FreecamMod

- Local: `C:\Users\user\Documents\ChatGPT\elden ring theater mode\research\community-nightly\FreecamMod`
- SHA: `a4628aaf50d88feeda56f79573cf2842eddec54a`
- Matching files: 0
- Files: No direct Replay/Bloodstain source hits; architecture/loader reference only

## https://github.com/micthiesen/unseamless-coop

- Local: `C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\NativeReplayResearch\github\unseamless-coop`
- SHA: `8faed9e1efb1e76e36d253e296daa051f4581253`
- Matching files: 0
- Files: No direct Replay/Bloodstain source hits; architecture/loader reference only

## https://github.com/LukeYui/EldenRingSeamlessCoopRelease

- Local: `C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\NativeReplayResearch\github\EldenRingSeamlessCoopRelease`
- SHA: `27ea9d7e5f50b35097d53f0ed8c5d28cba121bab`
- Matching files: 0
- Files: No direct Replay/Bloodstain source hits; architecture/loader reference only

## Interpretation

vswarte current SDK has BloodstainData/GameDataMan fields absent from the pinned fork; those newer offsets are REFERENCE only. libER/older SDK/internal sources are cross-checks, not offset authorities. HKS provides action/behavior context. CT tables provide candidate labels/scripts, never auto-executed. FreecamMod is an integration/input reference. unseamless-coop is a local clean-room session architecture reference; this snapshot has no direct bloodstain replay matches. Seamless release repository is a release reference, not proprietary source access. No binaries or game pseudocode redistributed.

Official docs: https://ersc-docs.github.io/how-to-install-and-update/ (read2026-10-06); installation/session documentation does not prove native replay codec.

Exact current working checkout is Documents/ChatGPT/elden ring theater mode/EldenRingTheatreMode-phase4 at parent6432477. Spec historical primary Documents/Codex/.../EldenRingTheaterMode is backup commit a81efe4; not overwritten.


---

## Evidence appendix: `notes/NIGHTLY_RESEARCH_FINDINGS.md` (repository authored evidence snapshot)

Historical statuses retain their original scope. Latest Step placement/26 guards/observed timeouts in the opening sections supersede older builder-only and untested-callsite wording. Static proof does not mean successful native creation.

# Targeted native research — 2.7.0.0

## Sources and reproducibility

Reference clones outside the project are read-only research material:

| Source | Inspected revision | Relevant source |
|---|---|---|
| [FreecamMod](https://github.com/Logersnamed/FreecamMod) | a4628aaf50d88feeda56f79573cf2842eddec54a | `src/core/game_data_manager.h`, `game_data/field_area.h`, `game_data/world_chr_man.h`, `features/game_state_manager.cpp`, `free_camera.cpp` |
| [EldenRingHKS](https://github.com/soulsmods/EldenRingHKS) | d88d6441f5fccfdd6a5fd10d493309b680181897 | `c0000.hks`, movement variables/turn speed/animation events |
| [TGA](https://github.com/The-Grand-Archives/Elden-Ring-CT-TGA) | 7926205c5a2ed236dd31278c4f5579c964ceec35 | table `.cea` base aliases; `MiscWIP/Dependencies/Global Functions/{PlayAnimation_code,Warp_code}.cea` |
| pinned fromsoftware-rs | 3c8c1d7633a99309fb004c9f894ea10b7967d0e0 | `cs/chr_ins.rs`, `chr_ins/module/{behavior,physics,fall}.rs`, `cs/lua_event_man.rs`, task groups |
| existing Hexinton v8.0.4 CT | local file | PlayAnimation and Fast Travel/Warp entries parsed as XML; never executed |

The reference camera-tool folder and original game files were not changed. No reference implementation or game pseudocode is bundled. The TGA Windows checkout encountered long filenames; relevant committed objects were inspected directly. Exact source hashes and scan output are in `research/symbols_2_7_0_0.json`.

Run `python tools/research_integration/integrate_sources.py` to regenerate anchor metadata. It verifies the research executable's mandatory SHA before scanning executable PE sections. It does not call the game. `research_query.py` still provides bounded SQLite/xref/disassembly searches.

## Concrete findings

### 1. Debug ownership layout conflict [CONFIRMED / writes BLOCKED]

Freecam describes noHit/noAttack/noMove at `ChrIns+0x538`, noUpdate at `+0x539` bit0. Pinned SDK names equivalent debug bits 3/4/5/8, but compiled `offset_of!(ChrIns,debug_flags)` is **0x530**. Source names/bit semantics alone do not prove member location. Typed cross-check initially failed; no 8-byte adjustment is guessed. The nightly blocks all debug flag writes, including the old player lock's writes, and removes NPC action masking/neutralization from transform-only diagnostics. This may change behavior relative to the preserved baseline; runtime comparison is required.

Next: verify ChrIns-derived native consumers against RTTI/vtables/callers, isolate accesses at 0x530 versus 0x538, then correlate read-only live values. A generic pseudocode occurrence of either number is not proof. Initial SQLite offset searches contained many unrelated vector/stack offsets and are deliberately not promoted to bindings.

### 2. animationSpeed reference agrees [CROSS_CHECKED / runtime UNVERIFIED]

Freecam's per-entity freeze supports either noUpdate or behavior animationSpeed=0, but also changes noHit/noDamage/noDead. We independently implement only the speed float on one exact actor, never the additional damage/death flags. Both Freecam and compiled SDK put `CSChrBehaviorModule.animation_speed` at **0x17c8**. Native behavior `owner` must match the current ChrIns; character and module identity are checked again before restore. This remains a developer experiment, not replay locomotion or final AI ownership.

### 3. Camera anchor [CROSS_CHECKED / runtime UNKNOWN]

Freecam FieldArea RIP pattern uniquely matches **VA 0x14061ef93**, resolves global slot **0x143d6d248 / RVA 0x3d6d248** on the exact image. Nearby disassembly confirms a RIP load. Source camera chain: FieldArea+0x20 GameRend, GameRend+0xD0 debug camera; mode at +0xC8; matrix +0x10 and FOV +0x50 inside camera. Mode 2 is selected when reference disables player controls, mode 3 otherwise. All those structure offsets/control meanings are reference claims until independently verified live. No camera dereference/write/hook is added in this nightly.

Freecam WorldChrMan pattern is unique at **0x1403ffbf6**, resolves **0x143d69ff8 / RVA 0x3d69ff8**. The existing reflected SDK WorldChrMan/task path remains in production; a second singleton resolver is not introduced.

### 4. Named native animation path [CROSS_CHECKED / ABI UNKNOWN]

Hexinton and current TGA identify an event dispatcher by the same unique pattern, subtract 0xD: **VA 0x140c15a40 / RVA 0xc15a40**. Exact disassembly has a normal function entry, tests the first argument's first pointer, checks the second argument, calls **0x140c15610** and then **0x140c15ba0**. SQLite lists five direct callers, including 0x14041c0a0 and 0x1404784a0; partial analysis can miss others. RTTI contains `hkbCharacter` at **0x143d16728** with related symbol-linker/controller/string-data descriptors.

Reference chain: behavior module +0x10 unknown context, then +0x30 purported hkbCharacter wrapper; event name is written as Unicode. The SDK leaves the intermediate context opaque. The dispatcher checks `[rcx]`, so treating rcx as a fully understood hkbCharacter object is premature. Call convention, lifetime, string ownership, native update scheduling and event-name compatibility remain unresolved. No arbitrary event call, TAE ID overwrite or fake WALK driver is introduced. HKS shows MoveSpeedLevel/MoveSpeedLevelReal/MoveAngle/TurnAngleReal and SetTurnSpeed participation; setting a float or event alone does not reproduce the locomotion graph.

### 5. Warp pipeline [CROSS_CHECKED / read-only instrumentation implemented]

Legacy Hexinton CSLuaEventManager pattern matches **two** sites: 0x14065ba0d → 0x143d6beb8, 0x140bbcfbb → 0x143d5f040. Rejected as an unambiguous runtime signature.

Current TGA registers alias **CSLuaEventMan → CSLuaEventManager**. Pinned SDK already provides reflected singleton `CSLuaEventManImp`, with proxy at +0x08 and optional script imitation at +0x18. Compiled layout tests confirm both. Exact Ghidra strings include CSLuaEventMan at 0x142a62208 and singleton RTTI at 0x143c8a520. This typed reflected access is preferable to the ambiguous legacy pattern.

LuaWarp candidate uniquely resolves **0x14059aa60 / RVA 0x59aa60**. Disassembly immediately writes r8d to `[rcx+0x1c]`; SDK script imitation's `lua_warp_bonfire_entity_id` is exactly +0x1c. TGA reference supplies script imitation, proxy, and bonfire ID minus 1000. The function calls 0x140592ac0, 0x140595fc0 and other engine routines. Its inferred three-argument setup is not yet a validated callable ABI or safe world-load operation. Reflection reads existence/warp/reentry/load-wait into a bounded trace; no runtime warp is called. Bonfire ID is not a complete WorldDescriptor/map coordinate identity.

### 6. Grounding synchronization [UNKNOWN / instrument first]

SDK `ChrCtrl.chr_proxy_flags` is +0xfc. Bits 0/1 are documented as position/rotation sync requests for the underlying Havok character. Physics also exposes a separate `chr_proxy_pos_update_requested`. Their exact consumers/timing and live model/proxy behavior are not verified. New trace compares native transforms, model translations, last position, vertical offset, behavior root motion, fall timer and grounded flags across callbacks. Havok proxy position, physics root motion and reliable ground height are unavailable; no vertical correction or collider/gravity bypass is used.

## Confidence policy

Source claims: REFERENCE. Unique target bytes/matching typed layouts: CROSS_CHECKED. Disassembly/call references identify candidate mechanics, not callable ownership. Runtime read/write verified status requires new real-game evidence. No new entry in the symbol matrix is marked runtime verified or production.


---

## Evidence appendix: `tools/ghidra_query/README.md` (repository authored evidence snapshot)

Historical statuses retain their original scope. Latest Step placement/26 guards/observed timeouts in the opening sections supersede older builder-only and untested-callsite wording. Static proof does not mean successful native creation.

# Targeted Ghidra queries

Python 3.10+. Standard library for SQLite/PE/RTTI; optional **Capstone 5.0.6** for
`disasm`. Never loads or executes the game. SQLite is opened `mode=ro` plus
`query_only=ON`. Binary commands verify the analysis copy's exact SHA-256 before
reading section bytes. No original game access is needed.

Default database: `../../../research/ghidra-eldenring/export/research.sqlite`
relative to this tool. `--database`, `--limit`, `--output` precede the command.

```powershell
python tools/ghidra_query/research_query.py text WorldChrMan
python tools/ghidra_query/research_query.py rtti ReplayRecorder
python tools/ghidra_query/research_query.py references PlayerIns
python tools/ghidra_query/research_query.py callers 0x1404e4f70
python tools/ghidra_query/research_query.py callees 0x1404e5310
python tools/ghidra_query/research_query.py function 0x14050f9e0
python tools/ghidra_query/research_query.py around 0x1404e4f86 --radius 80
python tools/ghidra_query/research_query.py range 0x1404e4f00 0x1404e5400
python tools/ghidra_query/research_query.py offset 0x1f1d8
python tools/ghidra_query/research_query.py functions-containing root_motion
python tools/ghidra_query/research_query.py vtable 0x142a4aa40 --count 4
python tools/ghidra_query/research_query.py disasm 0x1404e4f70 --count 180
python tools/ghidra_query/research_query.py schema
python tools/ghidra_query/correlate_sdk.py --output ../research/ghidra-eldenring/targeted/sdk_anchors.json
```

`text` searches literal FTS phrases in strings and C, plus symbol substrings.
`name` searches function-name substrings. `rtti` uses a bounded substring search
in decorated class-name strings because FTS does not split `AVReplayRecorder`.
All addresses are **preferred image VAs**, hexadecimal, optional `0x`; RVAs must
be added to image base 0x140000000. They are NOT live ASLR addresses.
`around` uses a byte radius, not an instruction count; it lists function entries,
not containing function bodies. `offset` searches textual hex constants only:
results can be sizes, absolute addresses, unrelated structures or false positives.
`functions-containing` is an alias for `code`. Matches are candidates.

## Actual schema

`schema` prints sqlite_master definitions, including FTS internal tables.

| Table | Columns |
|---|---|
| functions | entry (PK), rva, name, signature, body_bytes, status, c_file |
| symbols | address, name, type, source, external |
| calls | caller, callee, callee_name |
| refs | source, target, type, source_type, operand, primary_ref |
| string_refs | string_address, source, function, type |
| strings_fts | address, rva, file_offset, encoding, value (FTS) |
| code_fts | entry, c_file, body (FTS) |

Indexes exist on functions.entry, call endpoints, ref endpoints, symbols.address
and string_refs.string_address. Numeric range queries split address-width bands
and use the function PK. FTS is used for C searches; per-entry C retrieval and
decorated RTTI substring queries scan their smaller tables. Indirect calls and
references absent from partial Ghidra analysis will NOT be magically recovered.

## Disassembly and structural RTTI verification

Optional installation (separate research directory, never global Python):

```powershell
python -m pip install --target ../research/ghidra-eldenring/tools/python-deps capstone==5.0.6
```

The tool discovers that directory alongside the database. If a sandbox cannot
read packages installed under another identity, execute the query in the same
authorized environment or install in a readable dedicated environment.
`disasm` decodes a caller-specified byte range, not a proven CFG. Bytes after a
RET can be padding, embedded data, another function or obfuscation; do not treat
all decoded instructions as belonging to the requested function.

`vtable` only decodes pointer slots, labels executable-section targets and checks
indexed function entries. It does NOT prove a vtable or function ABI.
`correlate_sdk.py` separately validates x64 MSVC COL signature, self RVA, descriptor
reference, hierarchy bytes and pointers back to COL followed by executable slots.
It records candidates, exact bytes and xrefs. RTTI identity/layout/live lifetime
must still be checked. No output is automatically converted into an engine call.

Research exports remain outside the repository; do not commit or distribute
game-derived pseudocode or original binaries. Tool source and evidence notes are
independently implemented. No SDK or Ghidra source is copied into these tools.


## Complete recorded URL index

These URLs were found in authored repository evidence, not freshly validated on the web. Source files supply context; a URL alone is not a compatibility claim.

|URL|Evidence file(s)|
|---|---|
|https://docs.rs/eldenring/latest/eldenring/cs/struct.CSChrPhysicsModule.html|notes/PLAYER_STATE_1_17_RESEARCH.md|
|https://docs.rs/eldenring/latest/eldenring/cs/struct.WorldChrMan.html|notes/PLAYER_STATE_1_17_RESEARCH.md|
|https://docs.rs/eldenring/latest/src/eldenring/cs/chr_ins.rs.html|notes/PLAYER_STATE_1_17_RESEARCH.md|
|https://ersc-docs.github.io/how-to-install-and-update/|research/NATIVE_REPLAY_SOURCE_INDEX.md|
|https://github.com/111tykatypka/EldenRingTheatreMode|notes/DEVELOPER_TRANSFER_BRIEF_2026_10_06.md|
|https://github.com/Dasaav-dsv/libER|research/NATIVE_REPLAY_SOURCE_INDEX.md|
|https://github.com/FriXeee/ELDENRING-INTERNAL|research/NATIVE_REPLAY_SOURCE_INDEX.md|
|https://github.com/Grimrukh/soulstruct-vanilla/blob/main/eldenring/events/m41_01_00_00.evs.py|notes/PHASE5C_CHARACTER_DRIVING_SYSTEMS.md|
|https://github.com/KamiyamaShiki0704/ERGparamPreloadPatch|notes/PLAYER_STATE_1_17_RESEARCH.md|
|https://github.com/KamiyamaShiki0704/ERSoundBankLoader|notes/PLAYER_STATE_1_17_RESEARCH.md|
|https://github.com/KamiyamaShiki0704/fromsoftware-rs|notes/PHASE8_CAPTURE_FIDELITY.md, notes/PLAYER_STATE_1_17_IMPLEMENTATION.md, notes/PLAYER_STATE_1_17_RESEARCH.md, research/NATIVE_REPLAY_SOURCE_INDEX.md|
|https://github.com/KamiyamaShiki0704/fromsoftware-rs/blob/3c8c1d7633a99309fb004c9f894ea10b7967d0e0/crates/eldenring/src/cs/chr_ins/module/action_request.rs|notes/PHASE5_PLAYER_ANIMATION_RESEARCH.md|
|https://github.com/KamiyamaShiki0704/fromsoftware-rs/blob/3c8c1d7633a99309fb004c9f894ea10b7967d0e0/crates/eldenring/src/cs/chr_ins/module/event.rs|notes/PHASE5_PLAYER_ANIMATION_RESEARCH.md|
|https://github.com/KamiyamaShiki0704/fromsoftware-rs/blob/3c8c1d7633a99309fb004c9f894ea10b7967d0e0/crates/eldenring/src/cs/chr_ins/module/time_act.rs|notes/PHASE5_PLAYER_ANIMATION_RESEARCH.md|
|https://github.com/KamiyamaShiki0704/fromsoftware-rs/tree/3c8c1d7633a99309fb004c9f894ea10b7967d0e0/crates/eldenring/src|notes/PHASE5C_CHARACTER_DRIVING_SYSTEMS.md|
|https://github.com/KamiyamaShiki0704/nightreign_style_hud|notes/PLAYER_STATE_1_17_RESEARCH.md|
|https://github.com/Logersnamed/FreecamMod|notes/NIGHTLY_RESEARCH_FINDINGS.md, notes/PHASE8_CAPTURE_FIDELITY.md, research/NATIVE_REPLAY_SOURCE_INDEX.md|
|https://github.com/LukeYui/EldenRingSeamlessCoopRelease|research/NATIVE_REPLAY_SOURCE_INDEX.md|
|https://github.com/NightFyre/EldenRing-SDK|research/NATIVE_REPLAY_SOURCE_INDEX.md|
|https://github.com/The-Grand-Archives/Elden-Ring-CT-TGA|notes/NIGHTLY_RESEARCH_FINDINGS.md, research/NATIVE_REPLAY_SOURCE_INDEX.md|
|https://github.com/micthiesen/unseamless-coop|research/NATIVE_REPLAY_SOURCE_INDEX.md|
|https://github.com/ocornut/imgui/tree/v1.92.5-docking|notes/IMGUI_UI_ARCHITECTURE.md|
|https://github.com/soulsmods/EldenRingHKS|notes/NIGHTLY_RESEARCH_FINDINGS.md, notes/PHASE8_CAPTURE_FIDELITY.md, research/NATIVE_REPLAY_SOURCE_INDEX.md|
|https://github.com/soulsmods/EldenRingHKS/blob/main/c0000.hks|notes/PHASE5C_CHARACTER_DRIVING_SYSTEMS.md|
|https://github.com/vswarte/fromsoftware-rs|notes/PLAYER_STATE_1_17_RESEARCH.md, research/NATIVE_REPLAY_SOURCE_INDEX.md|
|https://github.com/vswarte/fromsoftware-rs/blob/main/examples/debug-line/src/lib.rs|notes/PLAYER_STATE_1_17_RESEARCH.md|
|https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-queryinterrupttimeprecise|notes/PHASE4C_STATUS.md|

## Actual local reference checkouts

Unknown branch remains Unverified; revisions are snapshot observations.

|Path|Origin|Branch|HEAD|
|---|---|---|---|
|C:\Users\user\Documents\ChatGPT\elden ring theater mode\research\community-nightly\dx12-imgui-overlay|https://github.com/kacejot/dx12-imgui-overlay.git|master|e5087b986215d5c2092313458c68779eb4d99304|
|C:\Users\user\Documents\ChatGPT\elden ring theater mode\research\community-nightly\Elden-Ring-CT-TGA|https://github.com/The-Grand-Archives/Elden-Ring-CT-TGA.git|master|7926205c5a2ed236dd31278c4f5579c964ceec35|
|C:\Users\user\Documents\ChatGPT\elden ring theater mode\research\community-nightly\EldenRingHKS|https://github.com/soulsmods/EldenRingHKS.git|main|d88d6441f5fccfdd6a5fd10d493309b680181897|
|C:\Users\user\Documents\ChatGPT\elden ring theater mode\research\community-nightly\EROverlay|https://github.com/koalabear420/EROverlay.git|main|bb445e0ca507a43b2bd5988113b82db2c374c0c5|
|C:\Users\user\Documents\ChatGPT\elden ring theater mode\research\community-nightly\FreecamMod|https://github.com/Logersnamed/FreecamMod.git|master|a4628aaf50d88feeda56f79573cf2842eddec54a|
|C:\Users\user\Documents\ChatGPT\elden ring theater mode\research\community-nightly\Universal-WndProc-Hook|https://github.com/M0rtale/Universal-WndProc-Hook.git|master|e91001c114840ac13a260b3999a4837d2f00a63d|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\ELDENRING-INTERNAL|https://github.com/FriXeee/ELDENRING-INTERNAL.git|main|ab62d3b64495fec46c3ff0e0c1eca1fc52dcbc10|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\EldenRing-SDK|https://github.com/NightFyre/EldenRing-SDK.git|main|37652c02bf91c845aac14c0a4e1ee0f9a965df9c|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\EldenRingModLoader|https://github.com/techiew/EldenRingModLoader.git|master|d5c05cb4b6f5e18151355fa170b4ce5b85202165|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\EldenRingMods|https://github.com/techiew/EldenRingMods.git|master|c36d44ffb2226c12b98ea552726e56e1f223bd7d|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\ERGparamPreloadPatch|https://github.com/KamiyamaShiki0704/ERGparamPreloadPatch.git|main|928b1237c47d492733fb7bec77c26c82cffc6c70|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\ERSoundBankLoader|https://github.com/KamiyamaShiki0704/ERSoundBankLoader.git|main|86bb27cc6f01cc4497d3192e1ca5646efd6418d6|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\fromsoftware-rs|https://github.com/vswarte/fromsoftware-rs.git|main|59fbd3b3b7daaf14aca47c9f73530493dba6bc79|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\libER|https://github.com/Dasaav-dsv/libER.git|main|d8ae5c92719adca4344d8d902b63286b807f562b|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\nightreign_style_hud|https://github.com/KamiyamaShiki0704/nightreign_style_hud.git|main|215a2158bf32e5027dc9167368ca68bcee705368|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\NativeReplayResearch\github\EldenRingSeamlessCoopRelease|https://github.com/LukeYui/EldenRingSeamlessCoopRelease|main|27ea9d7e5f50b35097d53f0ed8c5d28cba121bab|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\NativeReplayResearch\github\fromsoftware-rs-kamiyama|https://github.com/KamiyamaShiki0704/fromsoftware-rs|main|7d4fdd66eda1a35284c170feb4b07b058c005f8f|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\NativeReplayResearch\github\fromsoftware-rs-vswarte|https://github.com/vswarte/fromsoftware-rs|main|59fbd3b3b7daaf14aca47c9f73530493dba6bc79|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\NativeReplayResearch\github\unseamless-coop|https://github.com/micthiesen/unseamless-coop|main|8faed9e1efb1e76e36d253e296daa051f4581253|
|C:\Users\user\.cargo\git\checkouts\fromsoftware-rs-7e356ae17759562a\3c8c1d7|file:///C:/Users/user/.cargo/git/db/fromsoftware-rs-7e356ae17759562a|master|3c8c1d7633a99309fb004c9f894ea10b7967d0e0|
|C:\Users\user\.cargo\git\checkouts\fromsoftware-rs-7e356ae17759562a\3c8c1d7|file:///C:/Users/user/.cargo/git/db/fromsoftware-rs-7e356ae17759562a|master|3c8c1d7633a99309fb004c9f894ea10b7967d0e0|

## Measured major local asset sizes

|Path|Bytes|
|---|---|
|C:\Users\user\Documents\ChatGPT\elden ring theater mode\research\ghidra-eldenring\export\research.sqlite|877510656|
|C:\Users\user\Documents\ChatGPT\elden ring theater mode\research\ghidra-eldenring\projects\EldenRing_1_17.gpr|0|
|C:\Users\user\Documents\ChatGPT\elden ring theater mode\research|5138428229|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\eldenring_all-in-one_Hexinton-v8.0.4.CT|7931419|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences|94461100|
|C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRing_CameraTools_v1018|5101333|
|C:\Users\user\Desktop\EldenRing_CameraTools_v1018|5101333|
