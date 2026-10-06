# Native payload checkpoint

Branch: codex/native-bloodstain-replay-research. Prior commit c047350. See research/NATIVE_PAYLOAD_GHOST_FINDINGS.md for exact-build findings and research/NATIVE_PAYLOAD_RUNTIME_TEST.md for new controlled experiment.

Implemented: bounded independent primary/secondary node decoder; sparse state/event presence grammar; quantized position/time/BlockId extraction; opaque behavior/event preservation; callback-level explicitly activated read-only capture with F10/F11; complete first active-list snapshot; live action/transform context; byte-diff and marker/cadence analyzer. Existing profile/task/pipe/read path and write guards unchanged. New feature native-payload-capture depends on native-bloodstain-readonly. Cargo.lock unchanged.

Verified locally: Release AMD64 EXE and DLL built; CTest14/14; Rust37/37; Python payload9/9; existing Python research/capture/probe tests passed; real-device DX12 smoke passed. Earlier sandbox-only Python temp-file test attempt failed due filesystem access, then package tests reran successfully outside that restriction. These tests do not prove game behavior.

Genuine prior native journal:413 node snapshots decoded, zero errors;137 tail comparisons; max position quantization errors .01985054/.03572998/.01965149; exact duration and BlockId agreement. Control angle mismatch up to2.37017rad intentionally retained. New action-labeled capture has NOT run in-game.

Output: C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\NativeReplayPayloadResearch. SHA256 EXE63413F0750C55C10B8BAF66BE3D0615377009762250984B63D014CAF6589AD1F; DLLE9F657BB8497EE0C6A5A28D81021B13180B0F1CEB4F083A13B220F743205CB46. Build manifest hashes protected baseline files; Phase5 unchanged.

Ghost: static local creation/copy/refcount/insertion/removal chain identified. No ghost constructor called. Runtime manipulator type3, spawn phase, lifecycle cleanup and native playback remain unverified. Need fully valid metadata and native scheduling/teardown contract before enabling creation. Next human test answers new payload/action/cadence questions and does not repeat pool-population test.
