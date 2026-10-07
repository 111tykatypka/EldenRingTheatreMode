# CameraTools reference — static findings and reuse boundaries

Canonical full export: `../../research/igcs-camera-v1018/README.md`, REFERENCE_MEMORY.md and research.sqlite. Read-only original camera folder and SHA provenance are there. No source/PDB was supplied; C#/C exports are inferred code. Important native conclusions were cross-checked against the earlier disassembly trace in `../notes/REFERENCE_CAMERA_TIMING.md`. No reference binary or copied proprietary implementation is linked into our application.

## Timescale chain (STATIC_VERIFIED)

1. Client `IGCSClient.Classes/Setting.cs`: bound control ValueChanged -> SendValueAsMessage -> `GeneralUtils.ConvertToByteArray` -> `MessageHandler.SendSettingMessage`.
2. `IGCSMessage.cs`: `[type=1][setting ID][payload]`; `NamedPipeSubSystem` sends on `IgcsClientToDll`. Incoming updates/logs use `IgcsDllToClient`. SettingType IDs 12/13 are enable/scalar.
3. Native DLL RVA **0x21E260** handles IDs 12/13 at message+1 and data+2. ID12 sets feature+0x1EC. ID13 calls decoder RVA **0x225520**, clamps through **0xF4E0** with float constants 0x3A83126F (.001) and 0x40400000 (3.0), then sets feature+0x1E8.
4. Writer **0x21E350** calls getter **0x218A00**, which returns dereferenced resolved manager+0x2CC. Enabled writes feature+0x1E8; disabled writes 1.0 (0x3F800000). RVA **0x21ED40** turns off the feature and restores 1.0 during its reset path.
5. Path override action ID21 at **0x21E570** saves old enable/value, installs temporary path speed and writes it; **0x21E680** restores saved state. Its path override path is distinct from the standard clamped setting path.
6. Scanner registration **0x21EE30**, resolution **0x21EDB0**, DLL slot **0x2A19C8**. Exact target 2.7.0.0 consumers both agree on game global RVA 0x358DB58 and multiply +0x2CC by +0x268. +0x268 is a delta candidate, not a proven complete clock-domain contract.

RVA labels are not recovered original source function names. Signature-wide uniqueness is false: the old broad pattern has two matching consumers. We check both concrete sites under the unchanged exact game identity guard. Ghidra game export did not index a pseudocode consumer under the searched global identifier; that absence is not evidence the disassembly consumers do not exist.

## Camera architecture

Managed AppState registers movement/rotation/FOV interpolation parameters (1–350, defaults 8/8/1), shake frequencies/strengths, FOV and input-speed settings. CameraPathControlWindow exposes timing/relative-player options and speed override; CameraPathsState receives path/node/player state. These are confirmed UI/protocol controls, not proof that spline evaluation or shake occurs in managed code.

Native AOBs and PauseFeature RTTI are identified in the prior notes. Exact native smoothing/noise equations, Catmull-Rom parameterization, constant-speed arc-length mapping and camera clock domains remain **UNKNOWN**. No camera hook was enabled this session. The real menu-explanation pause patch is separate from timescale and requires callback/resume lifetime proof before integration.

Independent Theater design: reuse the resolved scalar mechanism with explicit ownership/restore. Keep a GameCameraAdapter inside DLL; FreeCamera input integrates unscaled real delta, keyframed camera evaluates Sequencer ReplayTime, camera state snapshot/restore is owned by a controller. Native camera writes need exact-target signatures and competing game-writer suppression validated separately. Path data belongs to editor tracks, independent from recorded gameplay. Translation interpolation, quaternion SLERP/SQUAD and FOV channels must share the master evaluation time; constant-speed paths require an arc-length table and inverse mapping, not constant increments of spline parameter.

Implement generic pure math/schema independently where useful; do not copy proprietary resources or assume the decompiled class layouts are safe ABIs. Camera smoothing/shake controls can inspire UI, but equations and native placement still need proof. No whole camera rewrite is justified while skeletal replay is the stable foundation.

## Runtime coverage (UNKNOWN)

Player, NPC AI, animation, Havok, projectiles, environment, particles, audio and gameplay timers may use multiple clock domains. A scalar product and a working reference do not establish all domains for our callback/ownership conditions. Unscaled QueryInterruptTimePrecise/steady_clock keep our editor/time transport responsive, but no FreeCamera responsiveness test exists yet. Runtime comparisons at 1/.5/.25/.1 and pause/reset/load transitions remain required.
