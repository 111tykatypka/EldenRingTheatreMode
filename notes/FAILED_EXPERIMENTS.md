# Failed findings / experiments

| Finding | Evidence | Decision |
|---|---|---|
| Grounding failed | Latest user's visible game test: floating/falling through geometry | Unresolved; new differential trace, no Y offset or fake grounding correction |
| NPC application failed | Latest user's real test; trajectory capture and host display work | Selected actor only + per-stage failure trace; no fabricated success |
| Locomotion failed | User sees rigid transform sliding | Native event pipeline research only; no arbitrary ID or fake WALK success |
| Debug layout cross-check failed | Rust `offset_of!(ChrIns,debug_flags)` 0x530, expected Freecam 0x538 | Flag experiments blocked; no guessed relocation; nightly removes conflicting flag writes |
| Legacy CSLuaEventManager signature ambiguous | Two target byte matches resolve different globals | Do not use; prefer typed reflected CSLuaEventMan read-only |
| TGA full Windows checkout failed | filename-too-long errors after successful object clone | Read committed reference with git show; no source changes |

noMove, noAttack, combined flags and noUpdate: **NOT RUN in game**, not described as failed native behavior. Debug-camera control-mode experiment: NOT IMPLEMENTED. animationSpeed=0: IMPLEMENTED/UNIT TESTED, **NOT RUN in game**. NoUpdate's documented freezing of physics/AI is not claimed to have been observed here.

Existing logs also contain player `TARGET_REJECTED detail=13` (target-step guard). This establishes a rejection, not its cause. New target/callback telemetry must distinguish legitimate motion, IPC gaps and overwritten state before changing the guard.
