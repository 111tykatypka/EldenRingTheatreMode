# CameraTools v1.0.18 Р Р†Р вЂљРІР‚Сњ deep analysis and independent Theater port

Date: 2026-10-07. Active checkout: EldenRingTheatreMode-P2b-independent; branch codex/cinematic-editor-pass. Reference folder remains read-only. No original game files or stable Phase5/C4 packages changed.

## Evidence and status

- **STATIC_VERIFIED:** PE instruction bytes, managed IL decompilation corroborated by native dispatch, RIP calculations and RTTI below.
- **COMPILE_VERIFIED:** assigned only after the checkpoint build and tests; see BUILD_MANIFEST.txt.
- **RUNTIME_VERIFIED / VISUALLY_VERIFIED:** no new Theater camera/timing tests were performed in Elden Ring during this pass. Prior reference logs are archival evidence, not proof of the new port.
- **UNKNOWN:** subsystem coverage, lifecycle behavior and features explicitly identified below.

Decompiler names such as FUN_18021e350 are research labels, not original symbols. Partial game Ghidra output has incorrect function boundaries (notably near the second timing site). Instruction bytes override pseudocode when they disagree. No proprietary implementation was pasted into the new adapters.

## 1. Complete folder inventory

| Relative file | Bytes | Architecture | Purpose |
|---|---:|---|---|
| `EldenRingCameraTools.dll` | 2,871,008 | AMD64 native | Native in-process hooks, camera, input, timing, paths, D3D overlay |
| `EldenRingCameraTools.dll.log` | 4,811 | data | Archived discovery/hook/shutdown evidence; not a new Theater test |
| `igcs.config` | 627 | data | .NET 4.5.2 startup, process/DLL selection, DPI configuration |
| `IGCSClient.exe` | 640,224 | CLR IL (PE I386, no 32BITREQUIRED) | Managed external WPF client, injection, settings and pipe commands |
| `IGCSClientSettings.ini` | 3,325 | data | User settings and [VK, Alt, Ctrl, Shift] binding arrays |
| `ModernWpf.Controls.dll` | 382,464 | CLR IL (PE I386, no 32BITREQUIRED) | Managed WPF controls |
| `ModernWpf.dll` | 1,008,640 | CLR IL (PE I386, no 32BITREQUIRED) | Managed WPF theme/resources |
| `Readme.txt` | 9,803 | data | Version history and attribution |
| `steam_appid.txt` | 7 | data | Distribution launch metadata; not copied into the game |
| `System.ValueTuple.dll` | 78,992 | CLR IL (PE I386, no 32BITREQUIRED) | Managed compatibility library |
| `ToastNotifications.dll` | 109,568 | CLR IL (PE I386, no 32BITREQUIRED) | Managed client notifications |

All 11 files were inventoried with SHA-256, imports, sections and CLR flags in CAMERATOOLS_INVENTORY.json. There are no source files or PDBs in this folder. The native DLL has .pdata/unwind information and RTTI, but no matching source symbols were supplied. Managed client and dependency inspection uses the existing IL export in ../research/igcs-camera-v1018 (178 client C# files). PE I386 with CLR flags 0x1 is IL-only and does not prove an exclusively x86 process. ValueTuple/Toast flags 0x9 include strong-name signing. WPF resources belong to the reference and are not repackaged.

Native imports include XINPUT9_1_0, Kernel32, User32, Gdi32, Advapi32, Oleaut32, Imm32, D3DCompiler_47, Dwmapi, DXGI, D3D11 and D3D12. This proves use of APIs, not an exact private hooking library. Managed assembly references demonstrate ModernWpf, notifications and framework dependencies. MinHook is our implementation dependency; its use by the reference is not established.

Native DLL SHA-256: 1A1DA1FDBEB9F3EB85EF29469FF9EE2A1322108D47F107C11F137D4E13A077EB.
Client SHA-256: E991DD88A66A3D98B91ADE146C1EB33561B060B3499679EC4D9641758D0767CF.

## 2. Actual architecture

```text
IGCSClient.exe (.NET/WPF external client)
  settings/keybindings/path-control UI + injector
  -> IgcsClientToDll named pipe -> native command dispatcher
  <- IgcsDllToClient named pipe <- feature/path state
EldenRingCameraTools.dll (inside game)
  AOB discovery + camera copy interception
  input message/raw-input/XInput hooks
  native feature writes + internal path controller
  independent QPC frame controller (DXGI Present loop per archived log)
Elden Ring engine
```

MessageHandler.cs and ConstantsEnums.cs establish the two pipe names. IGCS messages start with type byte then ID byte; setting messages type 1, binding messages type 2, actions type 7. Float payloads use BitConverter/IEEE binary32 on Windows, not strings or pointers. CameraPathsState reads four little-endian int32 values at payload offsets 2/6/10/14 and a playback-state byte at 18; it marshals UI updates through SynchronizationContext. Framing and native dispatcher must not be confused with Theater's existing versioned IPC. Our port retains YAFSML and Theater IPC; it does not add the IGCS injector or pipes.

## 3. Timescale Р Р†Р вЂљРІР‚Сњ complete demonstrated chain

**STATIC_VERIFIED:** ImageAdjustmentsPage -> AppState settings -> Setting<T>.SendValueAsMessage -> MessageHandler.SendSettingMessage -> type 1/id 12 enabled boolean or id 13 float -> native RVA 0x21E260 -> feature fields +0x1EC (enabled), +0x1E8 (value) -> RVA 0x21E350 -> getter RVA 0x218A00 -> dereference global DLL RVA 0x2A19C8 -> native object +0x2CC float.

Native DLL addresses here are RVAs at preferred base 0x180000000, never game RVAs. The setter writes enabled value or exactly 1.0f. Native standard-setting clamp constants are 0x3A83126F (0.001f) and 0x40400000 (3.0f). The client setting advertises 0..5; these are DIFFERENT limits. Action 21/path override at RVA 0x21E570 saves previous override state, enables the requested float and calls the same writer. RVA 0x21E680 restores that saved state; path override bypasses the ordinary setting clamp. RVA 0x21ED40 disables override and restores 1.0 during shutdown.

Four discovered direct callers of the writer are setting change, path override, path restoration and shutdown. This supports event-driven writes, not an unconditional generic clock hook. Indirect/virtual callers were not exhaustively excluded. No QPC manipulation, PlayerIns speed or separate Havok/animation speed write appears in this demonstrated chain.

The resolver AOB registered by RVA 0x21EE30 is:
`48 8B 05 | ?? ?? ?? ?? F3 0F 10 88 ?? ?? ?? ?? F3 0F 59 88 ?? ?? ?? ?? 48 8D`

The pipe keybindings for override/increment/decrement are actions 43/44/45; the managed increment is 0.05f. Both UI and path controller therefore reach the same native scalar.

### Exact target port and native owner

Target executable 2.7.0.0, patch 1.17, AMD64; file SHA-256 D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134.

Two raw instruction sites, game RVAs 0xDEB30F and 0xDEBE2F, independently resolve the same pointer slot RVA **0x458DB58**. The obsolete 0x358DB58 calculation is wrong. Each site loads +0x2CC and multiplies it by +0x268. This is direct machine-code evidence of a shared multiplier, not proof that every subsystem shares that clock.

Constructor candidate game RVA 0xE843E0 has a vtable write at VA 0x140E843EA. RIP calculation gives vtable VA 0x142BFF358; complete object locator VA 0x143372B08 resolves type descriptor **.?AVCSFlipperImp@CS@@**. This identifies the timing owner as **CS::CSFlipperImp**, rather than an unnamed timing manager. Constructor instruction VA 0x140E844E3 initializes +0x2CC to 0x3F800000 (1.0f); VA 0x140E8447E initializes the +0x268 region with 0x3D088889. The exact semantic units and all downstream task consumers remain unresolved. Pseudocode candidate FUN_140DEB230 passes the multiplied scalar onward, but a full scheduler/Havok coverage proof is not complete.

Evidence: CAMERATOOLS_NATIVE_TRACE.json and CAMERATOOLS_GAME_TIMING_TRACE.json. Archived reference log reports AOB time site RVA DEB30F; it does not record the game's hash, so matching RVA alone is not an identity check.

### Menu behavior and range Р Р†Р вЂљРІР‚Сњ limits of the evidence

No verified menu-specific clock, inventory/map exclusion or menu restoration branch was found in the reference path. Feature-valid gating at RVA 0x04C1F0 is not evidence of a gameplay-only check. Documentation describes engine speed, but per-system coverage and menu response require live observation. Smoothness comes from a native multiplier while the reference camera uses a separate real-time clock; a guarantee of normal render FPS cannot be derived from the scalar alone.

Only the setting's .001..3 clamp is statically established. 0.001/0.005/0.01/0.025/0.05/0.1/0.25/0.5/1/2/5/10 stability is **UNKNOWN** until tested. No test result was invented.

## 4. Camera interception and ownership

**STATIC_VERIFIED:** initialization RVA 0x20A1C0 registers AOB_CAMERA_ADDRESS_INTERCEPT with a 0x35-byte displaced-code interceptor targeting DLL stub RVA 0x6C110. AOB:
`89 42 ?? 8B 41 ?? 89 42 ?? 8B 41 ?? 89 42 ?? 8B 41 5C 89 42 5C 0F 28 41 ?? 0F 29 42`

The stub captures RDX in DLL global RVA 0x2A19B0 and tests camera-enabled byte RVA 0x2A1979. Disabled: copies FOV +0x50, fields +0x54/+0x58/+0x5C, and four matrix blocks +0x10/+0x20/+0x30/+0x40. Enabled: retains the other three fields but skips matrix and FOV stores. Thus free-camera ownership is achieved by suppressing the game's overwrite at a camera-copy site. It does not require guessing a singleton named CSCamera. The archived log resolves game interception RVA 0x3BB458.

Names CSCamera/CSPersCam/CSCam/WorldChrMan.chr_cam are SDK candidates. Exact captured pointer identity is not proven merely from those names. +0x54/+0x58/+0x5C are consistent with aspect/clipping parameters, but semantic labels need SDK/runtime correlation.

Theater uses its previously investigated, separately guarded copy-function RVA 0x681970. Original function runs first, then the final output at render-object +0x20 is rewritten. This is an independently implemented equivalent at a DIFFERENT site, not a claim of identical IGCS interception. It avoids introducing an unproven mid-function assembly detour. Actual render coverage and flicker at lock skips are still runtime questions; diagnostics report callback count/lock skips/cost.

### Direct camera writer and state save/restore

Reference functions RVA 0x209B20 and 0x209B70 pass the captured pointer's matrix +0x10 and FOV +0x50 to helpers 0x04C350 / 0x04C3B0 (capture/restore candidates). RVA 0x209D40 writes FOV directly. RVA 0x209D80 writes three basis triples at +0x10/+0x20/+0x30 and converts the supplied double XYZ to floats at +0x40/+0x44/+0x48. This confirms the full input/path output reaches the captured native camera, beyond merely locating the hook. Helpers around 0x008C60/0x008CF0/0x008BD0 participate in orientation conversion; Ghidra's mixed SSE/register signatures are unreliable, so no Euler order is claimed from those prototypes. RVA 0x209FC0 removes camera/aspect/player intercepts and clears the captured pointer; 0x20CDC0 clears enabled and captured state after its virtual cleanup. Map-transition safeguards within the reference remain unproven.

### Matrix and rotation

Our checked representation stores basis vectors at indices 0..2, 4..6, 8..10, translation at 12..14, homogeneous last entry 1. The conversion uses quaternion XYZW and radians-to-degrees for native FOV. Column-vector algebra treats these contiguous basis vectors as columns; row-vector notation is the transpose. A byte layout alone is not enough to assert the engine's algebraic convention/handedness. Unit basis/orthogonality/determinant +1 and quaternion round-trip tests cover conversion; visual forward/up/roll alignment still needs TEST 1/2.

The reference interceptor proves the four copied matrix blocks, not its entire Euler order or quaternion math. No guessed Euler order is imported. Theater composes local quaternion rotations and never Euler-lerps Dolly orientation.

### Unscaled timing and restoration

RVA 0x218470 initializes QueryPerformanceFrequency and QueryPerformanceCounter. RVA 0x2188C0 returns elapsed milliseconds. Frame controller RVA 0x210F40 derives elapsed/16.6666667 and clamps it to .001..2.0 before evaluating movement/path output. This prefix has no native +0x2CC dependency. Archived log states the frame loop runs on DXGI Present. Camera-copy interception itself runs at the native copy, not Present.

Theater uses QueryInterruptTimePrecise real dt for Free Camera, regardless of replay timescale. Dolly/shake use the host's authoritative ReplayTime anchor, rate and playing flag; there is no independent path clock. Switching to Player or Stop releases output overrides: next original native copy restores the game's current matrix/FOV. It does not force an old matrix onto a newly loaded owner. Fresh camera, player, offline context, host heartbeat and focus are required. Generation change disables writes. Unloading a DLL while its hook is installed is unsupported; exit the game before replacing it.

## 5. Input and controls

**STATIC_VERIFIED:** imports and archived hook log show Get/PeekMessage A/W, GetRawInputBuffer, XInputGetState and Present hooks. GetRawInputData, RegisterRawInputDevices and GetAsyncKeyState are imported. RVA 0x225860 tests the high bit of GetAsyncKeyState (held state). RVA 0x21B5B0 processes WM_INPUT and keyboard messages, converting client coordinates. RVA 0x21BA30 decodes mouse button make/break flags and keyboard virtual keys. RVA 0x217710 updates a key-state collection and resets Ctrl/Shift/Alt variants after a reset flag. Its exact repeat/edge policy is not fully proven; decompiler return types disagree between callers.

IGCS settings persist VK plus Alt/Ctrl/Shift flags. Reference mouse modes include rotate, button-modified pan/dolly/roll and wheel FOV; keyboard and XInput controls are documented externally. Theater adapts independent movement, quaternion look/roll, adjustable speed, smoothing, FOV keys, held fast/precision movement and event-debounced speed changes. It keeps keyboard/mouse suppression in the existing input hooks when camera owns input. Overlay-visible state stops camera movement; numeric/text editing and binding capture suppress action hotkeys. F6 retains emergency behavior.

New Settings key capture persists all active single-VK actions to %LOCALAPPDATA%/EldenRingTheaterMode/keybinds.ini; validates schema, duplicate rows, VK range and conflicts; invalid files preserve the working map. Host refreshes global start/stop registrations on its timer; DLL reloads on IPC worker, never on the native camera hook. Windows registration failures are logged with fallback. Modifier CHORDS and gamepad rebinding/blocking are not implemented. Single held Shift/Ctrl movement modifiers are implemented. UTF-16 key names are converted to UTF-8 for UI.

## 6. Paths, nodes and Sequencer

Managed CameraPathControlWindow proves duration, player-relative option, ease-in/out, constant-speed option, delayed start, temporary speed override and movement/rotation shake payloads. Public documentation describes path nodes with transform/FOV. Native path state/math lives in the DLL, not in the WPF client's CameraPathsState DTO. Exact private file-layout and all spline internals are not proven; do not call our .ercam IGCS-compatible.

Independent Theater Track uses position Linear/Smooth/cubic Bezier/Curve/centripetal Catmull-Rom/Step; shortest-path quaternion SLERP and FOV interpolation. Ease Curve uses cubic ordinates at fixed X positions. Constant position speed uses an edit-time 128-subdivision arc-length table per segment; it is approximate, not exact for tightly curved paths. Keys have stable ID, time_ns, position, XYZW quaternion, FOV, outgoing interpolation, relative Bezier handles, ease coefficients and constant-speed flag. .ercam ERTCAM 1 retains UTF-8 replay identity and 17-digit doubles.

K captures a node at ReplayTime; L confirms deletion. Existing individual key numeric editing retains other keys. The new CameraCutTrack uses validated non-overlapping half-open segments, exact hard-cut boundaries and native Player fallback in gaps. Runtime integrates the cut choice at the same ReplayTime. Current cut UI supports Player or the CURRENT single Dolly asset, is session-only and must be explicitly armed. Replay changes clear cuts/ownership. Multiple paths, persistent cut serialization, SQUAD, selectable 3D gizmos, target-relative paths and viewport node picking remain incomplete. These are not hidden behind false success labels.

## 7. Theater integration and safeguards

See TIMESCALE_IMPLEMENTATION.md and CAMERA_SYSTEM.md for source responsibilities. The new world adapter is OFF by default, opt-in for an owned Playing skeletal replay only. It requires fresh IPC, present player, offline allowance and foreground game. It validates both timing sites and the common pointer root, reacquires root each callback, requires an initially normal scalar, writes only changed values, restores its saved scalar if same root and last value still owned. It never restores to a stale root or overwrites another tool's change. Paused/Stopped/loading/context loss releases world timing rather than inventing a native pause hook.

GameProfile holds timing RVAs/offset and camera RVA/layout constants. Exact executable acceptance remains upstream in the Rust adapter. Hook/input/controller/editor layers remain separate. NPC replay files were not changed except the small shared timing tick bridge in bone_replay.rs; recording/format/actor lifetime are not refactored.

## 8. Feature matrix

| Feature | CameraTools evidence | Theater C4 before | Theater C5 after | Status / gap |
|---|---|---|---|---|
| Native speed scalar | Exact setter/getter/AOB | No active native adapter | Guarded CSFlipperImp +2CC | Static proof; new runtime UNKNOWN |
| Continuous .001..10 | Standard native clamp .001..3; path differs | Replay .01..4 | Replay + opt-in world .001..10 | Range tests; stability UNKNOWN |
| FreeCam real-time dt | QPC controller | Implemented real dt | Retained | Visual UNKNOWN |
| Native camera ownership | Matrix/FOV interception | Original-copy then override | Retained guarded output hook | Different site; visual UNKNOWN |
| FOV / roll | Stub + documentation | Numeric FOV / roll keys | Rebindable keys + roll reset | Visual UNKNOWN |
| Movement damping | Settings/interpolation | Movement exponential damping | Retained | Rotation/FOV damping not equivalent |
| Camera speed | Settings and modifiers | Fixed keys | Rebindable speed +/- and modifiers | Multipliers not yet configurable |
| Shake | UI/path payload | Timeline deterministic shake | Retained | Not the identical reference noise model |
| Paths / spline / ease | Client flags/public docs | .ercam and pure evaluator | Retained | Full native spline formula unresolved |
| Player-relative paths | Client playback flag | No | No | Planned |
| Camera state slots | Documentation | No | No | Planned |
| Cut track | Not established | No | Session Player/single-Dolly cuts | No multi-assets/persistence |
| Key rebinding | VK + modifier arrays | Defaults only | Single-VK persistence/capture | Modifier chords missing |
| Controller | XInput hooks | Not blocked | Not blocked | Not implemented |
| World pause / frameskip | Separate AOB patch/docs | No | No | Native pause patch not ported |
| HUD / stealth / LOD / sun / aspect fixes | Docs/config/AOB | No corresponding new adapter | No | Separate verified hooks required |
| Replay recording/world reconstruction | Not established in camera tool | Existing Theater systems | Preserved | Reference is not proof of replay determinism |
| XYZ/rotation gizmos | Not established from current trace | Numeric editing | Numeric editing | Required viewport editor not implemented |

## 9. Runtime evidence and remaining work

No runtime observation for the new build exists. Current evidence is file/disassembly plus offline compile/tests. The prior menu-slowdown regression's exact old cause cannot be proved from current source; the new design avoids startup/menu writes by replay gating, but inventory/map during an active replay still needs explicit menu-state research. No claim that CSFlipperImp ignores all UI is made.

Remaining proof: real camera object/FOV axes, render ownership and restoration, slow-motion coverage of NPC/physics/UI, renderer cost, .001..10 stability, replay/world time drift, keyboard/gamepad conflicts and menu/loading transitions. Whole-world pause and high-range stability cannot be declared finished by passing unit tests.

Next safe runtime order is read -> two-second +.25 X probe -> free camera/restore -> FOV -> world rates 1/.5/.25/.1/.05 -> .01/.005/.001 -> 2/5/10 -> dedicated native pause research. See CAMERA_SYSTEM.md for exact package and log procedure. Full requested parity is NOT yet runtime-verified or feature-complete.

## Sources

Local DLL bytes/IL/config/readme/archived log are primary evidence. Public feature cross-check: https://opm.fransbouma.com/Cameras/eldenring.htm . Decompiler output stays research-only; no original DLLs/assets are distributed with the package.

## Offline checkpoint result

C5 first package: Release AMD64 built; 14/14 CTest, 49 Rust passed, 1 optional Rust ignored. Additional tests cover keybinding malformed/duplicate/conflicting rows and exact cut boundaries/overlaps/IDs. Native adapter test checks default OFF and rejection of the test executable. It never writes Elden Ring memory. Final package is outputs/Cinematic-C5-final; see its manifest for hashes and commit.
