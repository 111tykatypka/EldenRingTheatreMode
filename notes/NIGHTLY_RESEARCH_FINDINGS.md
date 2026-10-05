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
