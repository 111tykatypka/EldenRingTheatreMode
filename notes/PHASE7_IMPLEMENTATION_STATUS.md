# Phase7 Runtime/UI — experimental checkpoint

**Oct6 update:** user reported startup crashes of Phase7_Runtime_UI. Exact cause
is unresolved. Use the separate Hotfix1 diagnostic build and
PHASE7_CRASH_DIAGNOSTIC.md before any replay test. Hotfix1 starts in Clean mode.

Status: **IMPLEMENTED — RUNTIME VALIDATION REQUIRED**. The requested final milestone
(grounding stable + one NPC owned + visible WALK + freecam/editor) is **not achieved**.
Do not infer game success from compilation or automated tests.

## Source and preserved baseline

Active repository: `C:\Users\user\Documents\ChatGPT\elden ring theater mode\EldenRingTheatreMode-phase4`.
Started at `eb11f66d52450b56917f17797fc73c3b743abb24`, clean working tree, branch
`codex/nightly-research-integration`. Changes are on `codex/phase7-runtime-ui`.
Phase5, TesterBuild and Nightly_ResearchIntegration are protected by the new packaging
script; original game files, loader and reference folders are unchanged.
Exact 2.7.0.0 version/product/AMD64/full disk SHA-256 guard and pinned SDK/Cargo.lock remain.

## FIXED / implemented safety behavior

- Removed normal "Play anyway". UNKNOWN map permits player BEGIN only within
  **1.0 horizontal unit and 0.25 vertical unit** of sample zero, in both host and
  DLL. DLL checks fresh native state before any replay input/transform mutation.
  No developer relocation override was added. Proximity is not proof of collision loading.
- Paused Resume keeps the existing playhead; Restart explicitly begins at zero.
- F6 and in-game Stop disable DLL writes directly as well as dispatching host Stop.
- Malformed/stale IPC, lost player and disconnect retain existing fail-open behavior.

## EXPERIMENTAL: grounding comparison

Developer XZ-only mode (v3 session flag 4, capability 128, default OFF) writes X/Z
and quaternion, **does not assign Y**. Native Y remains under the current engine
controller. It is player-only, cannot be combined with raw animation or selected
NPC-only mode. Session mode cannot change while active.
Trace records `transform_mode=0/4`, the original XYZ target, actual native Y,
physics/model positions, last update position, contact/fall/proxy fields.
This is a diagnostic, not final replay or a grounding fix. No fixed Y adjustment,
raycast snap, guessed proxy pointer or blind position-sync flag write was added.

## ChrIns layout — exact new evidence

**STATIC_VERIFIED:** exact-target ChrIns constructor `RVA 0x3e6e30`, tied to
RTTI/vtable `RVA 0x2a310c8`:

- `RVA 0x3e73e3`: callback object at `this+0x508`.
- `RVA 0x3e73fe/0x3e7405`: callback function `0x3f8fd0` stored at callback+0x28,
  hence **ChrIns+0x530 is a function pointer**.
- `RVA 0x3e7409`: qword initialized at **ChrIns+0x538** to `0x06400000`.
- `RVA 0x3cc231/0x3cc235`: load ChrCtrl owner+0x10, test owner+0x538, mask 0x20.
- `RVA 0x3d0008/0x3d0141`: test owner-derived pointer+0x538, mask 0x10.
- Additional mask 0x08 tests at RVAs 0x38b340 and 0x44aeb9 support the noHit reference.

**CROSS_CHECKED:** FreecamMod and SDK bit semantics noMove=5/noAttack=4 agree;
the SDK's enclosing `debug_flags` offset 0x530 is wrong for this exact binary.
Do not change the verified pre-0x530 transform access path or silently update the SDK.
Other SDK fields near/after this tail require a separate audit.

New version-specific `native_debug_flags.rs` uses an explicit small typed prefix.
Diagnostics no longer read the SDK field. Short noMove/noAttack/both probes require
native callback==loaded base+0x3f8fd0 AND ChrCtrl.owner==current ChrIns. All changes
occur after reacquisition, in the native callback. Restore modifies only owned bits,
on the exact current identity. No saved native reference survives a callback.
Probe duration two seconds; Stop/disconnect/timeout restore. **No continuous NPC
ownership was enabled in replay.** These flags may not suppress all AI/navigation.
noUpdate remains blocked: update lifecycle and animation consequences are unresolved.

## Controller synchronization / WALK / world loading

**STATIC candidate:** ChrCtrl vtable slot 13 points at RVA 0x3c8a50; instructions
at RVA 0x3c8bb0 OR +0xfc with 3 and store SIMD data to +0x100/+0x110.
This is stronger evidence than a field-name guess, but its arguments, callers,
consumers/clearing and teleport prerequisites are UNKNOWN. No native call was added.
Raw bounded scans for slots lacking exported body sizes can include neighboring
functions; their start labels are candidates, not verified call boundaries.

**REFERENCE:** HKS `MoveStart` requires MoveSpeedLevel>0, handles stealth/blend
and calls ExecEventHalfBlend. Existing normalized input suppression clears movement
requests; raw TAE ID transition alone cannot reproduce this native pipeline.
WALK is **NOT IMPLEMENTED/NOT VISUALLY VERIFIED**. No guessed hkbCharacter call.
Earlier W_Event/warp/FieldArea static anchors remain research-only. Map identity,
streaming readiness and automatic recorded-world loading are UNKNOWN/not implemented.

## In-game editor foundation

**IMPLEMENTED, GPU/game runtime UNVERIFIED:** `TheaterRenderBackend` C++ static
component linked into the existing Rust TheaterMode.dll. Dear ImGui **1.92.5**,
matching official DX12 backend, vendored MinHook source (BSD license included).
No extra injector or client executable.

- Hooks Present/ResizeBuffers and DXGI factory CreateSwapChain/CreateSwapChainForHwnd.
- Associates DIRECT queue from the actual swapchain creation argument, rather than
  guessing the first queue. Late injection after creation cannot render; restart via YAFSML.
- Per-buffer allocator/fence; skips busy frames instead of stalling Present.
- Resize drains GPU with bounded timeout, destroys resources, recreates on Present.
- Editor/Overlay/Clean modes via Insert; clean passes normal input through.
- WndProc queues input to render thread, avoids concurrent ImGui calls; Editor
  suppresses cursor recentering via SetCursorPos hook. Existing normalized replay
  input ownership is independent. Raw input/polled controller routing still requires testing.
- Dedicated IPC worker; no pipe I/O in Present or native character callbacks.
- Host validates the client process against its connected game PID; versioned requests,
  monotonic sequence, reserved fields and speed choices checked.
- Host is sole ReplayPlayer clock. Controls: Play/Resume/Pause/Stop/Restart,
  six speeds, seek and previous/next tick. Commands reach host UI thread even minimized.
- Seek/step keep existing safe behavior: stop native writes, change offline playhead.
  **They do not reconstruct the game world at the seek position.**
- Snapshot is cached, bounded actor pages (16/page, no total actor-count restriction),
  ImGuiListClipper; no full replay decoding on the render thread.
- Actor identities/selection and live player XYZ are shown. Per-NPC resolved/applied/AI
  ownership and ground status are explicitly UNKNOWN in this UI; JSONL has native evidence.

### Remaining editor/camera requirements

Free Camera, camera manager modes, orbit/follow/bone, Dolly, replay-time camera evaluation,
camera presets/recording, cinematic keyframe tracks, loop/work range, `.eredit` persistence,
and automatic world loading are **NOT IMPLEMENTED**. Existing bookmarks/ERPLAY/browser
remain intact. No original ERPLAY is modified. No Dolly synchronization claim.

## Automated evidence

Release AMD64 builds succeeded. CTest **13/13**, Rust **24/24**, Python research tools **3/3** passed; exact binary anchor checks **6/6** passed. These are automated results only.

Build and test results are captured by `scripts/Build-Phase7.ps1` in
the package. CTest adds CPU-only ImGui construction/protocol validation and a real
local editor-pipe seek/speed round-trip into the existing Player. Rust tests cover
start guard, wire flags, typed debug view, session behavior and ownership masks.
Mock/UI tests are not GPU Present/Resize or Elden Ring validation.

## References inspected

- FreecamMod a4628aaf50d88feeda56f79573cf2842eddec54a: DX12Hook, GUI/WndProc,
  cursor ownership, timeline core/view separation, ChrIns flags.
- dx12-imgui-overlay e5087b986215d5c2092313458c68779eb4d99304:
  factory queue association, per-buffer fence and resize resource handling.
- EROverlay bb445e0ca507a43b2bd5988113b82db2c374c0c5: Present/queue/WndProc.
- Universal-WndProc-Hook e91001c114840ac13a260b3999a4837d2f00a63d:
  message forwarding and key events; not its unsafe 256-key indexing.
- MinHook 8af6b4acae5a9388fd742b56fa79ece89d96f823: source vendored and attributed.

Independent backend design; no proprietary camera tool code/assets/binaries copied.
Exact static checks can be rerun using `tools/research_integration/verify_phase7_anchors.py`.
