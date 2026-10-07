# C7 camera enable FPS regression

User reports FPS loss on Enable experimental Free / Dolly writes. Running game module confirmed at outputs/Cinematic-C6-reference-backends/TheaterMode.dll (on-disk SHA399CC9A3BFD89556412973E9C84C4A2C8C7DDF6E538A7640313B093CC02BA081). It has not been replaced or unloaded.

## Evidence

Recent CAMERA_RUNTIME active rows: callbacks1135 mean_hook_us12428.84 ->1147/13879.50 ->1159/15310.24; later1258/25996.07. Differencing cumulative totals gives approximately151ms spent per additional callback in the first pair. About12 callbacks per2-second logging interval; lock_skips0. Disabled rows later advance120 callbacks per interval with a falling cumulative mean. This is severe camera callback elapsed-time evidence, not a measured GPU FPS counter. The relevant stretch logs world timing OFF. Other later world-rate changes cannot establish the root cause of that stretch.

The two per-copy WriteProcessMemory calls and ReadProcessMemory use were the first concrete hot-path target. They are a suspected expensive cause, not independently isolated by pre-fix phase instrumentation. Foreground/input/math work could also contribute; live new metrics are required.

## Changes

NativeCameraMemory.cpp performs direct local memcpy/stores within small leaf SEH guards. No virtual protection changes, memory API syscalls, allocation or pointer caching. It uses only the current interception's source/destination, after existing transform validation. Memory fault returns false and restores native copying. Initialization still validates the entire profile opcode block using checked read APIs. No version/profile guards removed.

CinematicCameraRuntime removes per-copy process-memory APIs and repeated active-status string updates. Diagnostics include C7_local_store, interval callback count/mean hook cost, maximum hook cost and read/write means. These are elapsed times, not CPU cycles or GPU FPS. Existing real-time Free Camera and timeline-driven Dolly behavior retained.

## Validation

Release AMD64 EXE/DLL built.14/14 CTest passed,49 Rust passed,1 optional Rust ignored. Tests verify local store/read equality, read-only write rejection, inaccessible-page failure and null pointers, plus the existing assembler pass-through test. No live game writes or synthetic gameplay claims.

Runtime performance improvement UNVERIFIED. New package outputs/Cinematic-C7-camera-performance. Close game and host, use matching package EXE/DLL through existing YAFSML. Compare enable/disable at1x in same scene; inspect CAMERA_RUNTIME backend=C7_local_store window_mean_hook_us and mean_local_write_us. If FPS still falls, new timings distinguish copy cost from input/focus/evaluation/render behavior. Keep old packages for comparison.
