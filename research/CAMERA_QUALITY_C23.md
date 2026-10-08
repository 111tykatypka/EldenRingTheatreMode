# Camera quality C23

## Scope and evidence

STATIC_VERIFIED: exact 2.7.0.0 executable SHA D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134. Pinned fromsoftware-rs 3c8c1d7633a exposes ChrInsFlags1c4.force_update (bit 1), cleared every frame, omission modes Normal/1/5/20/30 FPS, and an onscreen/frustum flag.

Disassembly at 1405100B2 tests [rdi+1c4] bit 1. Set branch zeroes edx and calls 1403F7530, then skips the distance/frustum omission selection. At 1403F7530 the zero mode is stored in +b4 if bit 0 (skip omission updates) is not set. This is omission_mode_override in the pinned SDK. This gate does not alone prove every animation subsystem will update at render FPS.

1403F9140 clears bit 1 with an AND 0xFD (see reset evidence); SDK explicitly describes per-frame reset. Evidence JSONs are instruction listings, not recovered proprietary source.

CameraTools log resolves AOB_HIGHER_LODS_ADDRESS to RVA 1A61B44. Exact target instructions there copy xmm0 to xmm6 and participate in a scalar render calculation. The public guide describes disabling lower-detail distant assets and warns of substantial performance cost. No proof that this hook fixes character animation scheduling. No hook copied or patched here.
Official reference: https://opm.fransbouma.com/Cameras/eldenring.htm

GRASS_LOD_RANGE_PARAM_ST lod*_play fields are grass parameters, NOT NPC animation update intervals. They were investigated and rejected for this issue.

## Implementation

Look checkbox: High quality LODs (character updates). Off by default; persisted in the existing settings file. It requests the public SDK force_update bit on freshly enumerated, loaded rendering-enabled nondead bodies in the existing PostPhysics callback. Explicit skip-omission ownership is respected. No persistent omission enum writes, no render-group bypass, no physics/AI changes, no bone changes, no new task or IPC protocol.

A try-lock C ABI bridge gates the option on actual enabled/writing Free or Dolly mode, existing host connection heartbeat, game focus and game context. Rust additionally requires offline guard, present player and no loading. Exact consumer and setter bytes plus typed field offset are checked before any write. On OFF no requests are submitted; final request is cleared by native code in its normal cycle. No cached body is retained or touched during cleanup.

All loaded distance-list characters are considered (no synthetic actor-count cutoff). CPU impact grows with loaded bodies. Does not load missing actors, force maximum mesh LODs, change camera frustum, or fix sparse recorded bone samples. NPC AI timing can change as a consequence of normal engine updates; this is an optional cinematic setting, not a no-impact optimization.

## Runtime unknowns

RUNTIME_VERIFIED: NO. VISUALLY_VERIFIED: NO.
PostPhysics request ordering relative to the next omission pass and bit reset needs live observation. Other native consumers may still throttle poses. Unloaded/out-of-region bodies remain absent. Frame rate overhead not measured.

## Manual check

Use the C23 package with the same offline loader. Close Elden Ring before replacing DLL. Start host then launch game through the existing launcher. In a loaded area with visible NPCs, enable Free camera, turn away from the player camera and compare NPC movement with Look quality checkbox OFF/ON. Repeat Dolly. Return to Player camera, turn checkbox off, change focus and stop replay: normal controls must remain available. Report the CAMERA_QUALITY log lines and whether character motion actually improves. Do not call the feature visually verified from build success.

## Build

COMPILE_VERIFIED: Release x64 host/native backend and Rust DLL passed. No unit or in-game tests run in this task. Package: outputs/Cinematic-C23-camera-quality. Previous C22 retained.
