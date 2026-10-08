# Custom particles / VFX roadmap

2026-10-07. Research/design only. No spawn or native replay ghost creation added.

## Actual source evidence

[STATIC_VERIFIED] Pinned fromsoftware-rs revision 3c8c1d7633a exposes `WorldSfxMan`, area/block/grid SFX containers, `CSSfxImp`, `CSSfx`, and `GXFfxSceneCtrl`. `CSSfx` has debug-spawn ID/distance fields. `gxffx.rs` exposes FXR resource lists, wrappers and graphics-resource/scene managers. These are layouts and resource catalogs, not a validated create/update/remove API. Debug fields alone do not justify writing them.

[STATIC_VERIFIED] `CSBulletManager` exposes bullet storage and network-related fields. Projectile capture and effect capture must remain distinct: visual effects cannot substitute for hitbox, velocity, collision or combat events. SQLite contains Wwise-related `GXFfxSoundManager_Wwise` RTTI and CSSound subsystem names; neither proves audio replay.

[UNKNOWN] Effect instance ownership, scheduler task, emitter lifecycle, deterministic seed control, attachment handles, resource residency, local/global coordinate conversions, destruction completion and audio coupling. SDK `BlockPosition` documents multiple coordinate domains; equal float layout is not proof of equal world origin.

## Architecture

VfxTrack stores effect resource identity, spawn/despawn/source times, attachment identity with generation, transform space and availability flags. ParticleBackend creates only owned instances through a validated native API. Authoring effects are separate from immutable captured effects. Missing resource or unsupported property is a visible capability failure.

A transform-only particle record cannot reproduce a non-deterministic emitter. Options are native emitter-state snapshots if proven, recorded particle transforms if feasible, or explicitly approximate restarting from checkpoint. Do not label any of these exact without measuring equivalence. Looping effects need explicit lifecycle intervals, not repeated starts per frame.

## Milestones

1. Read-only enumerate active instances, resources and attachment lifetime on game tasks.
2. Correlate creation/removal callers, argument layouts, reference-counting and map teardown in disassembly.
3. User-supervised one-instance create/remove with guaranteed rollback.
4. Record lifecycle plus transform; validate seek reconciliation and checkpoint restoration.
5. Investigate seed/state access; report unsupported exact reconstruction rather than simulate gameplay AI.

Audio requires independent event/loop/parameter tracks and stop/seek semantics. Re-triggering a sound on every evaluated frame is incorrect. State when muted/scrubbing must be explicit. Runtime verification of custom particles, sound replay and exact emitter reconstruction: UNKNOWN.

## Additional reference hub (2026-10-08)

See `research/SOULSMODDING_REFERENCE_INDEX.md` for the reviewed Souls Modding hub, exact FXR component-notes sheet, resource-usage candidates and limitations. Its particle list links to the same C29 spreadsheet; no duplicate catalog import is needed. The numbering guide suggests a cutscene family for 7191190, but this is not a verified semantic name. Detailed FXR property notes are the next source for density/scale/intensity research; no native property binding follows from the hub alone.
