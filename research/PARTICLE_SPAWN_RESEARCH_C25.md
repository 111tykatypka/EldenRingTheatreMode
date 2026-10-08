# Particle Spawn Research — C25

Date: 2026-10-08  
Target: Elden Ring PC 2.7.0.0 / patch 1.17  
Scope: local SDKs, debug-tool exports, cheat tables, SFX resources, and the pinned `fromsoftware-rs` bindings.  
Status: static research; no new native particle-spawn write has been runtime-validated.

## Executive conclusion

### C26 follow-up — exact debug dispatcher found

The earlier cheat-table assessment below was incomplete: CT entry **VFX DEBUG by Pavuk**, ID `1337199684`, does contain a direct native creation lead. Its signature `85 D2 74 0A 83 FA 01 75 0A E9` matches exactly once in the SHA-verified 2.7.0.0 research image. `tools/particle_spawn_trace.py` reproduces this check.

- Dispatcher: VA `0x140D980B0`, RVA `0xD980B0` — **STATIC_VERIFIED**.
- Mode 0 branch: VA `0x140D978A0`, a jump thunk to `0x1454EA58E` — **STATIC_VERIFIED**.
- Mode 1 branch: VA `0x140D91FD0` — **STATIC_VERIFIED**. Pseudocode shows shared CSSfx vector/resource cleanup and scene-controller calls; per-emitter ownership is not established. It must not be used as the editor's delete/stop handler.
- CT ABI lead: `RCX = [VFXDbg]`, `EDX = 0` for create / `1` for despawn, `R8 = address of a zeroed 64-byte buffer with 0xFFFFFFFF at +0x18`. This is observed CT code, not a proven runtime ABI for our caller.
- Create body at `0x1454EA58E` jumps through an indirect protected-code path to `0x145C5A8DB`; decompiler output omits the meaningful create flow. Capstone disassembly confirms this control-flow transfer. Exact request semantics, returned ownership, age/seed, transform updates and safe removal remain **UNKNOWN**.

The current DLL adds an opt-in read-only inspection using the pinned `CSSfxImp::instance()` on the existing game callback thread. It checks the exact dispatcher bytes and reads `scene_ctrl`, debug effect ID and camera distance. The overlay exposes **Inspect native VFX**. This does not invoke either dispatcher mode or modify effect state. All RVAs/offsets used by this inspection are in GameProfile.

The corrected scheduler emits lifecycle transitions rather than spawning every frame. Sequences increase independently of ReplayTime; seek/rewind rebuilds the owned emitter set; duration expiration emits remove; repeat delay follows each duration; paused evaluation does not respawn. `clear` in the command contract means editor-owned emitters only, never the native global clear branch.

There is not yet a proven, safe, arbitrary world-particle emitter API in the inspected material. The strongest immediate route is to use a known `SpEffect` whose `SP_EFFECT_VFX_PARAM_ST` entry creates the desired visual effect, applied to a deliberately selected carrier actor. This is a real game-side effect path, but the effect follows the carrier and may have gameplay semantics.

The strongest native research target for a true world emitter is Elden Ring's `CSSfxImp`/`GXFfxSceneCtrl` path. The SDK exposes fields named `debug_spawn_ffx_id` and `debug_spawn_distance_from_camera`, which strongly suggest an internal debug effect-spawn facility, but the inspected revision does not expose the consuming function or request structure. Writing those fields blindly is unsafe and is not claimed to work.

Geometry spawning and bullet spawning are confirmed native creation paths, but they create the wrong object class for a general particle editor. SFX-bank loading confirms resource acquisition, not playback at an arbitrary transform.

## Evidence labels

- **[CONFIRMED]** directly demonstrated by source, a binding, an RVA, or a reproducible local artifact.
- **[HIGH CONFIDENCE]** strong structural evidence, but the complete runtime call chain is not demonstrated.
- **[LIKELY]** plausible interpretation requiring a controlled runtime experiment.
- **[UNKNOWN]** insufficient evidence; do not implement as if proven.

## Sources inspected

### Pinned `fromsoftware-rs`

`C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRingReferences\fromsoftware-rs`  
Pinned revision used by the project: `3c8c1d7633a99309fb004c9f894ea10b7967d0e0`.

Relevant files:

- `examples/spawn-asset/src/lib.rs`
- `examples/apply-speffect/src/lib.rs`
- `crates/eldenring/src/cs/chr_ins.rs`
- `crates/eldenring/src/cs/world_chr_man.rs`
- `crates/eldenring/src/cs/world_geom_man.rs`
- `crates/eldenring/src/cs/bullet_manager.rs`
- `crates/eldenring/src/cs/sfx.rs`
- `crates/eldenring/src/cs/gxffx.rs`
- `crates/eldenring/src/cs/world_sfx_man.rs`
- `crates/eldenring/src/rva/rva_ww.rs`
- `crates/eldenring/src/param.rs`

### Other local references

- `EldenRingReferences/ERSoundBankLoader`
- `EldenRingReferences/libER`
- `EldenRingReferences/EldenRing-SDK`
- `EldenRingReferences/ELDENRING-INTERNAL`
- `C:\Users\user\Documents\ChatGPT\elden ring theater mode\debug tools`
- `C:\Users\user\Desktop\eldenring_all-in-one_Hexinton-v8.0.4.CT`
- `C:\Users\user\Documents\Codex\2026-10-04\sa\work\eldenringforcedynamicshadows`

## Candidate creation paths

### A. Apply a known `SpEffect` to a carrier actor — best first prototype

**[CONFIRMED]** The SDK example `examples/apply-speffect/src/lib.rs` obtains mutable `WorldChrMan`, acquires `main_player`, and calls `main_player.apply_speffect(id, true)`. `ChrIns::apply_speffect` resolves the native function through the pinned RVA table.

Relevant WW RVAs in `crates/eldenring/src/rva/rva_ww.rs`:

- `chr_ins_apply_speffect = 0x3e8dc0`
- `chr_ins_remove_speffect = 0x3ee2e0`

**[CONFIRMED]** Parameter schemas in `fromsoftware-rs` and `libER` include `SP_EFFECT_VFX_PARAM_ST`, so a SpEffect can reference visual-effect resources. The effect is attached to a game character/effect owner, not a freely positioned emitter.

**Advantages**

- Existing mutable API and current game-thread callback model can be reused.
- Replay event payload can identify a known SpEffect ID without passing pointers.
- Removal and lifecycle can be explicit at replay stop/seek.

**Risks**

- Some SpEffects modify gameplay, status, collision, or damage as well as visuals.
- The visual origin may be the carrier's bone or character effect socket.
- Applying it to the player is not a neutral visual operation.

**Recommendation:** implement only as a gated development probe after selecting a verified visual-only SpEffect. Prefer a temporary/debug carrier if a safe actor lifecycle is available. Do not use arbitrary IDs from the cheat table without checking the corresponding parameter rows.

### B. `CSSfxImp` / `GXFfxSceneCtrl` debug spawn path — strongest true-emitter target

`crates/eldenring/src/cs/sfx.rs` defines `CSSfxImp` with:

- `scene_ctrl: OwnedPtr<GXFfxSceneCtrl>`
- `debug_spawn_use_distance_from_camera: bool`
- `debug_spawn_ffx_id: u32`
- `debug_spawn_distance_from_camera: f32`

The binding reports `CSSfxImp` size `0x2b0`.

`crates/eldenring/src/cs/gxffx.rs` defines `GXFfxSceneCtrl`, the FXR wrapper/list types, and the graphics resource manager/container. The resource container has FXR definitions indexed by IDs.

**[HIGH CONFIDENCE]** These fields are intentional debug-spawn state and the scene controller is the relevant native subsystem.  
**[UNKNOWN]** The exact function that consumes the fields, the trigger/request structure, transform representation, ownership, lifetime, and cleanup path are not exposed by the inspected bindings.

Do not write `debug_spawn_ffx_id` or call an inferred vtable slot as a first implementation. The next safe step is static discovery of all references to the field and its nearby debug-input/update code in the exact 2.7.0.0 binary, followed by a read-only runtime trace.

### C. SFX bank loading — resource prerequisite, not emission

`EldenRingReferences/ERSoundBankLoader` documents the `CSSfx`/`CSFile` resource path and SFX bank mapping. The documented path format is `sfxbnd:/SfxBnd_c%04d.ffxbnd`; the project also records the `CSS::CSSfxImp` RTTI/vtable evidence and asynchronous bank acquisition work.

**[CONFIRMED]** This can locate/acquire SFX/FXR resource banks.  
**[UNKNOWN]** The loader does not demonstrate a call that instantiates an effect at a world transform. Loading a bank alone cannot make a particle visible.

Use this only after an emitter request path has been found, to ensure the required resource is resident.

### D. Geometry spawning — confirmed, wrong class

`examples/spawn-asset/src/lib.rs` uses mutable `CSWorldGeomMan`, the player's `block_id`, and `block_geom_data.spawn_geometry("AEG099_831", &GeometrySpawnParameters { ... })` from a `CSTaskGroupIndex::ChrIns_PostPhysics` callback.

**[CONFIRMED]** This is a real game-thread world creation path.  
WW RVAs include:

- `initialize_spawn_geometry_request = 0x1db5f0`
- `spawn_geometry = 0x6a5ed0`

It creates geometry/assets, not FXR/particle emitters. It is useful as an architectural example for command marshalling and game-thread ownership, but must not be presented as particle spawning.

### E. Bullet spawning — confirmed projectile path, not generic particles

`crates/eldenring/src/cs/bullet_manager.rs` exposes `CSBulletManager::spawn_bullet` using `BulletSpawnData` and a native function pointer. The WW RVA table gives `cs_bullet_manager_spawn_bullet = 0x3a2cb0`.

**[CONFIRMED]** A native projectile/action creation path exists.  
**[LIKELY]** Some spells or bullet definitions can produce visible effects that resemble particles.  
**[UNKNOWN]** This is not a safe general-purpose decorative FXR emitter and requires valid gameplay parameter data.

Do not use it for the particle editor unless the user explicitly wants projectile/spell behavior and the data is validated.

### F. Debug character spawning as an effect carrier

`WorldChrMan` exposes `spawn_debug_character(&ChrDebugSpawnRequest)` in the SDK.

**[CONFIRMED]** A debug-character creation request exists in the binding.  
**[LIKELY]** A temporary carrier could host a visual-only SpEffect.  
**[UNKNOWN]** Safe lifecycle, collision, AI, despawn, map unload, and cleanup behavior are not proven.

This is a later experiment, not a first particle implementation.

## What the cheat table and debug tools prove

The supplied Cheat Engine table contains labels for `SpEffect.add`, `SpEffect.erase/remove`, `BulletSpawn.create`, `ApplyEffect`, and action/spell helpers such as effects spawned above a target. These are useful leads to existing game actions.

**[CONFIRMED]** The table identifies effect IDs/actions and existing gameplay-oriented commands.  
**[UNKNOWN]** A table label is not evidence of a neutral arbitrary emitter API; many entries activate spells, bullets, statuses, or target-dependent effects.

The debug-tool source/export includes fields such as `NpcParam.SfxResBankId`, `autoFootEffectSfxId`, `spEffectID*`, `materialSfx*`, and `sfxSize`.

**[CONFIRMED]** These are parameter references used by native content.  
**[UNKNOWN]** They do not by themselves show a runtime call that creates a free world particle at `(position, rotation)`.

The downloaded Force Dynamic Shadows mod contains only data (`regulation.bin` and SFX/FFXBND resources) and no executable emitter code. It demonstrates content/resource editing, not native spawning.

## Recommended replay integration

Keep the host-owned ReplayTime and add a versioned event track rather than embedding native pointers or running a second clock.

Conceptual event:

```text
ParticleEvent {
    sequence: u64,
    replay_time_ns: u64,
    effect_kind: Preset | SpEffect | FfxId,
    effect_id: u32,
    position: f32[3],
    rotation_xyzw: f32[4],
    scale: f32[3],
    intensity: f32,
    duration_ns: u64,
    repeat_interval_ns: u64,
    carrier_policy: None | Player | DebugActor,
}
```

The host sends immutable command data through the existing IPC. The DLL copies the latest command/event state to a synchronized buffer. Only the verified game callback thread may touch `WorldChrMan`, `CSSfx`, bullet, or geometry objects. On `REPLAY_STOP`, seek, rewind, map change, player loss, or IPC disconnect, the DLL must clear/retire active effects using the proven cleanup path for that effect class.

## Safe implementation sequence

1. Build a static index of 2.7.0.0 references to `debug_spawn_ffx_id`, `debug_spawn_distance_from_camera`, `CSSfxImp`, `GXFfxSceneCtrl`, and their callers/callees.
2. Resolve a version/hash-guarded signature for the consumer function; do not use a guessed vtable slot.
3. Add read-only runtime logging for CSSfx pointer validity, FXR ID, resource lookup result, and game-thread phase.
4. Use one known harmless FFX/SpEffect ID and a development-only command with an explicit off switch.
5. Verify appearance, transform, lifetime, replay seek, stop, map transition, and cleanup in game.
6. Only then connect the Particle Spawner tab to the native command queue.

## Current C25 UI status

The independent project has a categorized Particle Spawner editor with emitter definitions, transforms, loop/repeat settings, camera placement, viewport markers, and persistence. This is currently an editor/data layer; it intentionally does not claim native particle creation.

Source and notes:

- `EldenRingTheatreMode-P2b-independent/native_ui/ParticleEditor.cpp`
- `EldenRingTheatreMode-P2b-independent/native_ui/ParticleEditor.h`
- `EldenRingTheatreMode-P2b-independent/notes/PARTICLE_SPAWNER_C25.md`

## Final assessment

- **Best immediately reproducible visual path:** a verified visual-only SpEffect on a controlled carrier — **[CONFIRMED API, runtime visual result still UNKNOWN]**.
- **Best long-term true world-particle path:** CSSfx debug-spawn/FXR scene-controller consumer — **[HIGH CONFIDENCE target, exact call chain UNKNOWN]**.
- **Useful but not particle creation:** SFX-bank loading, parameter schemas, geometry spawning, bullet spawning, and cheat-table action labels.
- **Not yet justified:** raw writes to CSSfx debug fields, guessed FXR vtable calls, arbitrary bullet payloads, or treating CT IDs as emitter IDs.

No native particle-spawn feature should be marked runtime-verified until the exact 2.7.0.0 call path has been tested in the offline game and its cleanup behavior has been observed.

## C27 follow-up (supersedes earlier unresolved creation assessment)

Static disassembly now reaches scene-create RVA 0x1CA0CB0 and owned stop/release RVAs 0x20B81C0/0x20B7A40. A guarded, opt-in two-second native FXR preview is implemented and Release-built. The runtime resident catalog supplies actual effect IDs; editor preset numbers remain placeholders. See `notes/PARTICLE_C27_NATIVE_PREVIEW.md` for ABI, ownership evidence, limitations and the exact test. Native visibility, transform convention, callback-phase compatibility and cleanup remain RUNTIME_VALIDATION_REQUIRED; earlier C25/C26 packages still have no native spawning.
