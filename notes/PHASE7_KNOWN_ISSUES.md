# Phase7 known issues

- User reported two startup crashes of original Phase7_Runtime_UI on Oct6, after
  DX12 initialization and before PLAYER_FOUND. Exact cause is unresolved.
  Hotfix1 is a diagnostic mitigation, not a verified fix. See PHASE7_CRASH_DIAGNOSTIC.md.

This is a buildable runtime-validation checkpoint, **not a completed Phase7 release**.

- FULL XYZ replay previously lost grounding/fell/died. No verified final fix yet.
- XZ-only is diagnostic; slopes, jumps and vertical recorded motion are not reproduced.
- UNKNOWN map start guard reduces relocation; it does not verify loaded collision.
- NPC noMove/noAttack probes have new static evidence, but native AI/render/animation
  effects and restore require user testing. Continuous NPC replay still lacks ownership.
- noUpdate blocked. Native WALK/HKS/hkb event scheduling not implemented.
- In-game DX12 hooks/resize/input have not been tested in Elden Ring. CPU UI tests
  do not verify hook interoperability with other mods or swapchain formats/HDR.
- Hooks must initialize before game swapchain creation. If only DX12_HOOKS_INSTALLED
  appears, without DX12_QUEUE_BOUND, restart the game; no speculative queue fallback.
- ImGui cursor/input routing does not establish suppression of every raw input,
  XInput or engine polling path. Existing normalized replay input remains experimental.
- In-game list contains recorded identities; resolved/applied/AI status is UNKNOWN.
- Seek/step stops transform replay and changes the host playhead only. In-game reverse
  scrubbing/world reconstruction is not implemented.
- Loop/work range, camera system/freecam/Dolly, `.eredit`, automatic world loading missing.
- DLL callbacks/hooks are process-lifetime. No hot unload/reload via FreeLibrary;
  close Elden Ring before replacing DLL. Shutdown path is compiled, not runtime verified.
- Native old SDK tail starts at wrong debug offset; adjacent late fields need auditing.
- No performance/FPS overhead numbers were measured in game.
