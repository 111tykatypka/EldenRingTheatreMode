# Custom lights — evidence and independent implementation plan

2026-10-07. Research/design only. No light allocation, engine writes or hooks added.

## Evidence

[STATIC_VERIFIED] Exact-target string/RTTI index contains `GXPointLight@GXSR` (preferred VA 0x143D2D9B0), `GXSpotLight@GXSR` (0x143D2DA00), `GXLightManager@GXSR` (0x143D2D518), and `FXPointLight SFXID%d #%08x` (0x14306EAE8). These addresses are preferred-image research addresses, not runtime pointers. The presence of types is evidence for a lighting subsystem; it does not prove a callable factory, setter ABI, renderer registration or deletion protocol.

[UNKNOWN] Construction/destruction, manager ownership, task affinity, light budgets, shadow allocation, color/intensity units, transform space, resource references and unload behavior. No validated SDK creation API has been established. Ghidra warnings and incomplete analysis prevent treating pseudocode as an ABI specification.

## Proposed independent architecture

An editor LightTrack stores stable editor IDs, type, transform, color, intensity and capability-gated settings. Master ReplayTime evaluates these properties. A game-side LightBackend owns runtime handles separately from saved IDs and records their generation. It must create/register/destroy on proven engine tasks, with exception-free rollback. A capability report disables unsupported fields instead of pretending that shadows, volumetrics or aperture exist.

First inspect RTTI xrefs/vtables and paired factory/destructor callers. Follow resource ownership through map unload. Add read-only light enumeration and lifecycle logs before any allocation experiment. Only after that proof, run one user-supervised light creation/removal test in a loaded world; verify cleanup on stop, load and disconnect. Repeated idempotent create/remove cycles precede animated lighting.

## Seeking and restoration

Seek computes the desired set of editor lights and reconciles it with owned handles. No repeated spawn on every tick. Preserve original game lighting; temporary edits need prior-value capture and generation-aware restoration. Never restore into a replaced manager. Snapshot light properties alone cannot reproduce all native lighting/environment state.

Acceptance: no leaked resources, no save mutation, correct deletion, same evaluated state after seek, explicit feature availability. Runtime and visual verification: UNKNOWN.
