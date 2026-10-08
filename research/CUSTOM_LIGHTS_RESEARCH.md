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

## C19 targeted continuation — 2026-10-08

The following are **STATIC_VERIFIED instruction/RTTI observations**, with semantic interpretations labelled separately. Preferred-image addresses below use base `0x140000000`; runtime must use validated image base + RVA. No light factory is called by Theater.

| Candidate | Exact-target evidence | Interpretation |
|---|---|---|
| `0x141A44DF0` | Installs the `GXPointLight` vtable `0x142F15630` | HIGH CONFIDENCE point constructor |
| `0x141A2A8E0`, `0x141A2AA10` | Allocate `0x1F0` aligned to 16, invoke constructor, increment/decrement ownership count, call `0x141A27D20` | HIGH CONFIDENCE point creation wrappers |
| `0x141A45630` | Installs `GXSpotLight` vtable `0x142F15790`; initializes matrix-like region +0x190 and owned resources | HIGH CONFIDENCE spot constructor |
| `0x141A2AB00`, `0x141A2AC40` | Allocate `0x370` aligned to 16; constructor then common registration | HIGH CONFIDENCE spot creation wrappers |
| `0x141A27D20` | Two insertion paths through +0x18/+0x38, intrusive retain/release calls, optional +0x58 lock/unlock pair, assigns incrementing ID | HIGH CONFIDENCE renderer registration; exact manager ABI still UNKNOWN |
| `0x141A45880` | Releases resource at +0x368; frees owned string backing and delegates cleanup | HIGH CONFIDENCE spot destruction body, NOT an approved removal entry point |
| `0x141A45F80`, `0x141A46150` | Called by spot wrapper with descriptor +0x10 and four values from +0x50/+0x54/+0x58/+0x5C | Setter candidates; parameter meanings/units UNKNOWN |

Evidence: `light_registration_c19.json`, `spot_light_constructor_c19.json`, exact-target SQLite callers/vtable xrefs. The decompiler's inferred prototypes are not trusted declarations. Existing C4 point factory/registration evidence remains unchanged.

### Is a Lights tab possible?

**HIGH CONFIDENCE: native point and spot lights are technically feasible.** There are renderer types, constructors and creation/registration paths; this is more evidence than shader strings alone. **UNKNOWN: a safe callable runtime binding.** Renderer manager lookup/generation, task/lock ownership, removal/list erasure, deferred render work, shadow/resource budgets, setter units and ABI must be proven before spawning anything.

Next concrete milestone: read-only manager and existing-light enumeration on the proven renderer task, logging type, stable ID, generation and lifecycle through load/unload. Then establish a paired public factory/removal path. Only after that, expose a Lights tab with Add Point/Add Spot, transform, color/intensity/range and capability-gated cone/shadow controls. Never call a deleting destructor while an object is still registered. An editor track stores IDs/properties separately from native handles and reconciles on seek; no per-frame respawning.

No Lights tab with fake controls or unvalidated allocations was added in C19. Runtime/visual evidence for custom light creation: **UNKNOWN**.
