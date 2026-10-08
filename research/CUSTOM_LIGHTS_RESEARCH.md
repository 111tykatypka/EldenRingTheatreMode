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

## C20 native inspector — 2026-10-08

This section supersedes C19's unknown manager lookup. It does **not** supersede the unresolved allocation/lifetime requirements.

### STATIC_VERIFIED observations

- `141CCB9AD`: RIP-relative load resolves to graphics-root slot `1447F37A8`; `141CCB9B9` reads its `+C518` light manager. Multiple factory and removal callers corroborate this path.
- Constructor `141A27880` installs manager vtable `142F116C8`. Point/spot constructors install `142F15630` / `142F15790`. RVAs and structural offsets are centralized in `shared/GameProfile.h`; runtime checks both discovery instructions and manager vtable.
- Two pointer collections have begin/end/capacity at `+20/+28/+30` and `+40/+48/+50`. Both may contain mixed light types. The selection flag is `light+E0`, not point versus spot. IDs at `+D0` are assigned by registration.
- Removal `141A2AD40` searches collections under manager `+58` lock, notifies listeners for the first collection and invokes `141AEDEB0`. It does **not** immediately erase the entry.
- `141AEDEB0` writes `+F0=0`, `+EC=-abs(fade)`, and `+F4=1` for nonzero fade. Interpretation: deferred fade-out request. Hidden floating-point argument ABI must be established before a call.
- `141A277A0` releases pointer ranges through intrusive reference counts. `141A2B2D0` reallocates vector capacity; it is not a removal drain. `141A2AFF0` shifts spatial origin; it is not a fade update.
- Point `+190` contains four spatial floats. Culling consumes XYZ and twice W. Position/radius is HIGH CONFIDENCE; coordinate convention/units remain UNKNOWN. Spot `+190` is matrix-like and its cone/range setter semantics remain UNKNOWN.

Evidence captured in `light_manager_access_c20.json`, `light_manager_constructor_c20.json`, `light_remove_c20.json`, `light_fadeout_c20.json`, and exact-target SQLite callers/vtable references. Disassembly buffers can extend past a function; do not infer extra instructions belong to the initial function. Research addresses are preferred VAs with base `140000000`.

### Implemented runtime inspection

`native_ui/EldenRingLightAdapter.{h,cpp}` copies a page of existing-light diagnostics using checked `ReadProcessMemory`. No factory, native lock, destructor, hook, or game-memory write is used. The existing Rust `Draw_Pre` callback invokes the inspector only after user opt-in, with loaded-world/offline gating. Host connection heartbeat also gates scans. UI/IPC threads only set requests or read copied snapshots; editor protocol remains v12.

Manual Refresh or opt-in 1 Hz monitoring. Maximum 32 displayed rows per page is a work/UI budget, not a limit on native collection size. Both full collection counts are reported. Root, manager/vtable and collection headers are reread after copying; inconsistent snapshots are discarded. This is **best-effort observation**, not a transaction: elements can change in place and address reuse/ABA is not detectable. Observed generation is an editor observation counter, not a native generation token. No borrowed native pointer is retained for later dereferencing.

On context loss counts/rows clear; monitor waits for return. Monitoring defaults off and is not persisted. New Lights toolbar/tab includes Refresh, monitor, A/B selection and paging. Disabled creation buttons explicitly state why allocation is not enabled. Raw spatial values are labeled unverified.

### Remaining blockers before creation

1. Identify the renderer update that consumes fade-out and erases/releases the registered reference; prove map unload and outstanding render-task references.
2. Verify factory/removal x64 ABI including XMM argument registers and descriptor layout.
3. Establish creation/removal task affinity and locking contract; Draw_Pre inspection is **not** proof that allocation is legal there.
4. Validate manager lookup/enumeration in the user's exact runtime and observe replacement across loading.
5. Establish color/intensity/range/cone units, shader participation and resource/shadow limits.

Then implement one explicitly activated point light with owned handle and idempotent deferred removal, followed by spot lights and master-ReplayTime light tracks. Never directly call deleting destructors or continuously allocate per frame.

Build: COMPILE_VERIFIED Release AMD64. Automated tests: NOT_RUN (not requested). Runtime/visual creation and inspector behavior: UNKNOWN, user testing required.
