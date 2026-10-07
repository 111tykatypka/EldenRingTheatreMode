# Camera reference: targeted native timing trace

## Reference identity

Read-only folder: `C:\Users\user\Documents\Codex\2026-10-04\sa\work\EldenRing_CameraTools_v1018`.

Inventory: EldenRingCameraTools.dll, IGCSClient.exe, ModernWpf.Controls.dll, ModernWpf.dll, System.ValueTuple.dll, ToastNotifications.dll, Readme.txt, igcs.config, steam_appid.txt. No source files or PDBs are present. Consequently source class/function names below are only available where strings/RTTI expose them; RVA labels are not invented source names.

- Native DLL SHA-256: `1A1DA1FDBEB9F3EB85EF29469FF9EE2A1322108D47F107C11F137D4E13A077EB`.
- Client SHA-256: `E991DD88A66A3D98B91ADE146C1EB33561B060B3499679EC4D9641758D0767CF`.
- Reference preferred native image base: `0x180000000`. Use **RVAs** below, not these preferred addresses at runtime.
- Exact game analysis copy SHA-256: `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134`.

## STATIC_VERIFIED timing chain

1. Native initialization function RVA `0x21EE30` registers the `AOB_TIME_DILATION_LOCATION_ADDRESS` pattern. Text pattern at DLL file offset `0x265C70`:

   `48 8B 05 | ?? ?? ?? ?? F3 0F 10 88 ?? ?? ?? ?? F3 0F 59 88 ?? ?? ?? ?? 48 8D`

2. Resolver RVA `0x21EDB0` obtains the AOB capture and calls relative-address helper RVA `0x2254B0`; its result is stored at DLL RVA `0x2A19C8`.
3. Getter RVA `0x218A00` reads that stored pointer location, dereferences it, and adds **0x2CC**. This is the pointer logged as `Time dilation address` by RVA `0x2124A0`.
4. Update/write function RVA `0x21E350` calls the getter. If its feature object's byte at +0x1EC is enabled, it writes the float at that object's +0x1E8 to the returned address; otherwise it writes **1.0f**. The disabled constant at reference RVA `0x26E8AC` was decoded as float 1.0.

This establishes a native float write, not an inferred QPC hook or replay-clock-only multiplier. It does not identify every downstream engine clock consumer.

The reference's PauseFeature RTTI and `AOB_PAUSE_ENGINE_ADDRESS_INTERCEPT` are separate. RVA `0x21E020` passes an enable byte and pause AOB key to its patch helper. Readme describes using the native menu-explanation pause to freeze grass/TAA. Full patch body/callback continuation and lifetime have not been traced; no such patch was ported.

## Exact 2.7.0.0 binary correlation

The broad time-dilation pattern matches **two** sites, so claiming uniqueness would be wrong:

| Game RVA | First instruction | Following float operations |
|---|---|---|
| 0xDEB30F | MOV RAX, [RIP + 0x37A2842] | MOVSS XMM1,[RAX+0x2CC]; MULSS XMM1,[RAX+0x268] |
| 0xDEBE2F | MOV RAX, [RIP + 0x37A1D22] | Same offsets and product |

Both resolve the global pointer slot at game **RVA 0x358DB58** (preferred VA 0x14358DB58). They compute the product of fields +0x2CC and +0x268. Combined with the reference's getter/write, +0x2CC is the supported speed-scalar candidate; +0x268 is a likely frame-delta field. The exact type name and units of +0x268 are not proven by these two instructions alone.

The independent controller checks the concrete opcodes/field offsets at both sites and their common target, under the existing exact executable identity guard. It does not select the first arbitrary broad signature match.

## Client/UI evidence and remaining chain

Managed client strings identify `_gameSpeedInput`, `GameSpeedIncreaseDecreaseDelta`, `Paths_OverrideGameSpeed`, `_gameSpeedToSetDuringPlayBackSlider`, `supportsGameSpeedControl`. Native request strings identify time-dilation toggle/increase/decrease actions. This supports the UI/feature relationship. The complete managed command serialization and numeric IPC message map have **not** yet been traced; they are UNKNOWN. No reference IPC protocol or binaries are incorporated into Theater Mode.

## Camera leads, not yet implementation proof

- Camera pattern file offset `0x263440`: `89 42 ?? 8B 41 ?? 89 42 ?? 8B 41 ?? 89 42 ?? 8B 41 5C 89 42 5C 0F 28 41 ?? 0F 29 42`.
- Coordinate pattern file offset `0x2634A0`: `0F 14 DA 0F 58 9F 80 00 00 00 0F 29 5D E7 E8 ?? ?? ?? ?? 0F 28 00`.
- Readme mentions path roll fixes and camera shake controls; client strings mention shake frequency/strength.
- Public documentation lists camera movement/rotation/FOV interpolation and internal game speed: [official Elden Ring camera documentation](https://opm.fransbouma.com/Cameras/eldenring.htm).

Exact camera damping, acceleration, shake equations, Catmull-Rom parameterization, client protocol and unscaled/scaled camera clock selection remain UNKNOWN from the inspected subset. These patterns are research leads, not version-approved camera write bindings.

## Independent implementation choices

Reuse the observed timing **concept**: resolve the native manager pointer, access its scalar, retain/restoring normal ownership. Reimplement it in Rust with the exact profile/opcode checks and the project's existing callback and snapshot flow. No proprietary source, assets or reference binaries are copied into the build.

Keep the host ReplayClock unscaled, multiplying elapsed time by playback speed once. Native world speed is controlled independently by the scalar. The skeletal renderer uses QueryInterruptTimePrecise for source-time extrapolation. Slow-world behavior for animation, physics, NPC AI, particles/environment, camera movement and load transitions requires live comparison; static scalar writes do not prove every subsystem shares that clock.

FreecamMod's separate MinSpeedhack/frametime-limit approach was inspected as additional context; it is **not** the IGCS scalar mechanism and was not ported.

## Runtime evidence still required

First confirm live manager pointer and scalar/delta plausibility without writes. Then enable a controlled experiment at 1.00/0.50/0.25/0.10, check world vs replay coherence and restoration. Do not load both camera tools simultaneously. Pause restores normal world speed in this prototype, not native whole-world pause. The requested reference comparison and restart acceptance tests are not yet performed.
