# CSTask 2.7.0.0 Runtime Diagnostic

Date: 2026-10-05  
Target: Elden Ring 1.17, `eldenring.exe` 2.7.0.0, AMD64  
Profile: `EldenRing_1_17` / WW 2.7.0.0

## Result

The initialization failure is fixed in the diagnostic DLL and the live runtime test passed through `TASK_RUNTIME_READY`, `WORLDCHR_READY`, `PLAYER_FOUND`, and `READY`. This was verified twice: once with YAFSML's normal external-DLL timing and once with `|data_ready`.

The prior host error `0x201` identifies a timeout returned by `CSTaskImp::wait_for_instance(Duration::from_secs(120))`. The old DLL logged only that wrapper-level timeout. The wrapper has **two** timeout sites, so the historical log cannot establish which internal predicate timed out:

1. `wait_for_system_init` waits for the version-specific `global_hinstance` slot to become nonzero.
2. After that, `CSTaskImp::wait_for_instance` loops until reflected `CSTaskImp::instance()` returns an instance rather than `Null`.

The prior binary did not log which site failed. It would be inaccurate to claim that the old run proves one of these two conditions. The repair follows ERSoundBankLoader's observed approach: resolve and log task registration, then poll `CSTaskImp::instance()` directly while preserving `NotFound` versus `Null` diagnostics. In both live runs the reflected singleton became available promptly. This establishes that the task singleton itself was accessible and that the former 120-second wait path was the problem area; it does not retroactively identify the old wrapper's exact timed-out predicate.

## Reference implementation findings

### `fromsoftware-rs`

At pinned revision `3c8c1d7633a99309fb004c9f894ea10b7967d0e0`, `CSTaskImp::wait_for_instance` in `crates/eldenring/src/cs/task.rs` first calls `wait_for_system_init`, then repeatedly calls `CSTaskImp::instance()`:

- `wait_for_system_init` in `crates/eldenring/src/util/system.rs` resolves `rva::get().global_hinstance`, reads that slot until it is nonzero, and returns `SystemInitError::Timeout` if it remains zero.
- For this WW 2.7.0.0 profile, `crates/eldenring/src/rva/rva_ww.rs` lists `global_hinstance = 0x3D89708` and `register_task = 0xEB3DE0`.
- After the startup gate, `InstanceError::NotFound` becomes `InvalidRva`; `InstanceError::Null` is retried until the timeout; `Ok` returns the singleton.
- Its `CSTaskImp::register_task_internal` uses a version-selected RVA. The checked-in WW 2.7.0.0 value is `eldenring.exe+0xEB3DE0`.

The wrapper therefore combines a game-start gate and reflected singleton lookup into one timeout result. The 0x201 packet from the earlier build did not preserve which timeout cause occurred.

### ERSoundBankLoader

The checked-in source uses `REGISTER_TASK_PATTERN` in `src/lib.rs` and resolves a **unique** match from the current executable. Its compatibility probe rejects missing or ambiguous matches. Its startup path then polls `CSTaskImp::instance()` directly every 10 ms, returning immediately on `Ok`, and retaining the last `NotFound` or `Null` description for timeout diagnostics. It does not call `wait_for_system_init`.

Its README and checked-in compatibility materials explicitly cover WW 2.7.0.0; its runtime function resolution reports `register_task` as an executable-relative RVA. Its 2.7.0.0 registration result matches the pinned binding's value `0xEB3DE0`. The DLL still depends on the shared binding's task and singleton layouts; runtime signature resolution does not prove every game structure is compatible.

### YAFSML timing

The local YAFSML 0.10.4 README documents that normal external DLLs load after `SteamAPI_Init`, whereas `data_ready` loads them after all game params finish loading.

- Normal run: YAFSML logged external DLL load before `AFTER_DATA_READY`. `CSTaskImp` first returned `NotFound`, then resolved on poll 10 after 115 ms.
- `data_ready` run: YAFSML logged deferred loading, reached `AFTER_DATA_READY`, then loaded the DLL. `CSTaskImp` resolved on poll 1 after 15 ms.
- Both runs reached `TASK_RUNTIME_READY`, `WORLDCHR_READY`, `PLAYER_FOUND`, and `READY`.

Thus `data_ready` changed when the DLL loaded and reduced the observed wait, but it was **not required** for successful task initialization. The failure was corrected by changing the task lookup path, not by delaying DLL loading. The YAFSML config was restored to its original normal timing after the comparison.

## Implementation changes

The adapter now:

1. Emits explicit initialization state packets to the existing host protocol: `DLL_LOADED`, `PROFILE_VALIDATING`, `PROFILE_READY`, `TASK_SIGNATURE_SCAN`, `TASK_SIGNATURE_READY`, `TASK_RUNTIME_SEARCH`, `TASK_RUNTIME_READY`, `WORLDCHR_SEARCH`, `WORLDCHR_READY`, `PLAYER_SEARCH`, `PLAYER_FOUND`, and `READY`.
2. Logs DLL load timestamp, process ID, DLL module base, runtime executable identity, selected profile, signature scan, singleton polling attempts, and discovered game objects to `%TEMP%\TheaterModeGame.log`.
3. Scans the current executable with the ERSoundBankLoader task-registration pattern and requires the unique match. The register function is called only after successful scan and `CSTaskImp` resolution.
4. Polls `CSTaskImp::instance()` on the background initializer, logging whether reflection reports the singleton missing or not yet initialized. It no longer blocks on the opaque `wait_for_instance` wrapper.
5. Registers `ChrIns_PostPhysics` sampling only after task resolution. `WorldChrMan` and `main_player` are read inside that callback; no game pointer crosses IPC.
6. Reports initialization states in the host monitor without changing the 72-byte wire message layout.

No recorder, replay format, camera, or gameplay mutation was added.

## Live-test evidence

The exact target profile passed at runtime:

- Executable path: `C:\Users\user\Downloads\ELDEN RING\Game\eldenring.exe`
- File and product version: `2.7.0.0`
- Executable architecture: AMD64
- TheaterMode DLL architecture: AMD64 (`x86_64-pc-windows-msvc`)
- Runtime file SHA-256: `D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134`
- Selected profile: `EldenRing_1_17`
- Runtime task registration signature: unique match at `eldenring.exe+0xEB3DE0`
- Normal timing: `CSTaskImp` resolved after 115 ms / 10 polls.
- `data_ready`: `CSTaskImp` resolved after 15 ms / 1 poll.
- `WorldChrMan`: resolved in both runs.
- `main_player`: found in both runs. The live callback read position `(7.707, 86.263, -57.614)` in game units and advanced the module to `READY`.

The sampled player location was the same in both launches. This confirms a finite live transform read through the `ChrIns_PostPhysics` callback. A movement-delta capture was not added to normal logging, so this test does not claim an observed change across multiple player positions.

YAFSML's log also records the second run as deferred DLL loading followed by `AFTER_DATA_READY` and successful DLL load. The host was reconnected to the successful running test after restarting between timing modes.

## Build artifact

Release x64 DLL:

`C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode\TheaterMode.dll`

SHA-256: `AF080A9577EDC0C87C4A7842BA01305340AB03DC5489299669343C6E3A1A44A8`

The external host monitor was rebuilt to display initialization states. `cargo build --release --offline --target x86_64-pc-windows-msvc` and the full Release build completed. The first build script pass correctly avoided replacing a DLL mapped by the running game; the test DLL was staged, then copied to the canonical artifact path after the game process stopped.

## Limitations and next work

- The old `0x201` DLL did not distinguish a `global_hinstance` startup-gate timeout from a reflected singleton `Null` timeout. This is an evidence limitation of that binary's diagnostics.
- The new state machine records the last `CSTaskImp` lookup result and makes subsequent timeout failures diagnosable.
- The current direct polling still uses the `fromsoftware-rs` reflection implementation. The task-registration entry point is dynamically resolved; actor/singleton structure layouts remain binding-based.
- `WorldChrMan` and player resolution were verified in a live process, but broader lifecycle cases (death, respawn, menu transitions, map transitions) remain untested.
- The original YAFSML config was restored to normal timing. The live game remains running with the tested DLL; no original game files were modified.

Next integration milestone: retain this successful state machine and validate player-transform updates over movement and map/loading transitions, then add actor-independent data sampling. Replay recording remains out of scope until those live reads are characterized.
