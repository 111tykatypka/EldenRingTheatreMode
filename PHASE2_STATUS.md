# Phase 2 verification status

**Status: BLOCKED; no recorder build or real replay produced.** This note updates the earlier Phase 1 environment finding for the exact paths in the Phase 2 brief. No game files or reference-tool files were modified.

## Environment checked on 2026-10-04

| Item | Finding |
|---|---|
| Target executable | Present at `C:\Users\user\Downloads\ELDEN RING\Game\eldenring.exe`. FileVersion and ProductVersion are `2.7.0.0`, matching the requested target identifier `EldenRing_1_17`. SHA-256: `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134`. This is a candidate binary identity; the game title-screen/regulation version was not independently checked. |
| Game process | Not running at the time of this check; no in-game validation could be performed. |
| Native toolchain | Visual Studio 18 Community, MSVC toolset directory `14.51.36231`, and bundled CMake are present. This supersedes the earlier “no C++ toolchain” result. No Release build was attempted. |
| Project/output paths | The requested project and output directories exist but were empty at the time of this check. |
| Reference files | The supplied `EldenRing_CameraTools_v1018` folder is present and was read only. Its README describes external `IGCSClient.exe` plus `EldenRingCameraTools.dll`; the client injects the DLL after the user disables anti-cheat and directly launches the game. The changelog explicitly mentions a game fix through patch 1.12, not 1.17. This reference therefore does not establish 1.17 compatibility and its binaries are not being reused. |

## Why a real recording is not yet safe to claim

Phase 1 did not produce a validated TheaterMode game module, IPC connection, or 1.17 player adapter. The present workspace has no verified game-side source for player transform sampling, actor enumeration, or event capture. The matching executable version string alone does not establish memory layouts, offsets, hook sites, or thread safety. Recording guessed addresses or fabricated samples would violate the requirements.

The public `fromsoftware-rs` release history currently documents an Elden Ring 1.16.2 update, not a validated 1.17 player-state integration. See [fromsoftware-rs releases](https://github.com/vswarte/fromsoftware-rs/releases). This is evidence of a version-coverage gap, not proof that no private or newer adapter exists.

The IGCS README’s offline instructions are evidence about that tool’s launch assumptions, not an instruction to modify this game installation. The game's directory remains read-only, and no anti-cheat manipulation or online use is performed here.

## Acceptance status

- Native x64 Release host: **NOT BUILT**
- `TheaterMode.dll`: **NOT BUILT**
- Version detection against the executable file: **PARTIAL** (`2.7.0.0` matches; title screen/regulation and runtime identity are unverified)
- Game process / DLL / IPC: **NOT VERIFIED**
- Player state: **NOT AVAILABLE**
- Actor state and gameplay events: **NOT AVAILABLE**
- Chunked replay writer / `.erplay` validation: **NOT IMPLEMENTED IN THIS PROJECT PATH**
- Replay browser / recording controls / in-game indicator: **NOT IMPLEMENTED**
- Real 60-second recording and replay artifact: **NOT PERFORMED**
- CPU, memory, throughput, game-FPS impact: **NOT MEASURED**
- Output artifacts: **NONE**

## Safe next gate

1. Establish and document a 1.17-specific integration source: exact binary fingerprint, validated player structure/signatures, actor enumeration method, reliable event sources, and a game-thread sampling point. Fail closed on any mismatch.
2. Implement the independent chunked `.erplay` writer and metadata/validation tests, while clearly tagging fields not supplied by the game adapter as unavailable.
3. Build and test the host, DLL, and IPC using the installed x64 toolchain.
4. Run the exact target build in the user's explicitly offline setup and observe real changing player samples, overlay, recording, finalization, and browser discovery before claiming PASS.

Until gate 1 is evidenced, a launcher or replay file generated from synthetic samples would not satisfy this phase's real-game acceptance criteria.
