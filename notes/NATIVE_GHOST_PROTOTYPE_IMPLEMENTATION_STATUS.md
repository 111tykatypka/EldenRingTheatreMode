# Native Replay Ghost Prototype 1

Status: **IMPLEMENTED — RUNTIME VALIDATION REQUIRED**. Compilation is not lifecycle acceptance. No native create/remove has been run by this development session.

Release x64 EXE, DLL and compatibility probe build succeeded with MSVC 19.51 and the locked, offline Cargo dependencies. Existing warnings: shared GameProfile C4530; Rust unused import/dead code/crate naming. Automated tests were not run for this native lifecycle milestone. Runtime success remains entirely unverified.

## Scope and isolation

Branch: `codex/native-bloodstain-replay-research`. Rust adapter, pinned SDK/Cargo.lock, shared exact-build guard, host/launcher/YAFSML and recorder remain in place. New Cargo feature: `native-replay-ghost-create-remove` (includes the existing read-only feature to block legacy replay writes). Existing EXE UI is reused without changes. F10/F11 are developer commands inside the DLL, scoped to game foreground; the payload-capture feature must NOT also be enabled.

New canonical output: `../outputs/EldenRingTheaterMode/NativeReplayGhostPrototype` relative to the active checkout. Stable Phase5/Tester/Nightly are not overwritten. Isolated CMake and Cargo build directories; build script stages EXE, DLL, compatibility probe, source/binary hashes and these instructions.

## Execution context

No engine mutation in IPC, hotkey worker, Rust PostPhysics, or an invented task phase. MinHook observes the original `140703e30` entry and marks only that call's dynamic thread-local extent. CREATE is consumed ONLY in `1407048e0`, with both that context flag and the exact native return address `140703f3c` (CALL at `140703f37`). This substitutes one explicit local creation for that invocation's normal serialize/upload/debug branch; other invocations call the original unchanged. No debug flag or countdown is forced.

If that original callsite does not execute in this offline build/session, CREATE times out after 60 seconds and performs no native fallback. This is an explicit runtime uncertainty, not a claim that the offline callsite has been verified.

REMOVE is consumed at the original `14050efa0` world-removal queue drain and invokes the same `14050b340` removal routine that the drain uses. Ownership, current world, entry address, slot, actor and native handle must still match. Native lifecycle removes tasks, clears entry and queues DelayDelete. No direct ghost/manipulator destructor is called by Theater.

## Genuine native data

Reacquire WorldChrMan global and current main player in the native callsite. Check world transition state, recorder exact vtable/owner, nonempty recorder, finite transforms, recorder/player same BlockId and empty ghost set. Metadata comes from native ctor `1406514f0` and live game data fill `14025f810`; no fabricated arrays/metadata/ChrSetEntry. Secondary/mounted metadata is refused because `1406f27f0` can create a paired secondary actor. Reject a decoded secondary stream too.

Native path: `14025f7e0` → `1406f1ec0` → native allocator → `1406f2410` → `1404e5440` → native allocation → `1406f1bb0` → `141ebbfc0` → `1406f1f20` → `1406f27f0` → native `140507e60`/`140493a80`/`140404570`/`1404f1840`/`14065db40`/`1403deaa0`/`1403df010`.

**ABI corrections verified from instructions:** `1406f2410` has a fifth alternate-metadata argument in `[rsp+20]`; `141ebbcd0` has a third allocator argument in R8. The complete genuine native 0x238 object is allocated and constructed by those native functions, never assembled from ERPLAY. Temporary creator reference is retained/released with the native interlocked helpers and native deleting virtual ONLY on last-reference transition; this is temporary smart-reference cleanup, not forced ghost teardown. Native buffer is freed with its native allocator. Recorder serialization is a real mutating operation, just as in the original local path; do not simultaneously run another recorder consumer.

## Lifecycle guards and evidence

Default OFF. One create attempt per process (even a rejected attempt is not retried). One active guarded record: active, epoch, genuine native handle, actor/entry/data/manipulator/world and slot. Removal retires epoch and zeros ALL cached engine pointers/handle before entering native removal. World change and ownership mismatch invalidate state without attempting destruction.

Separate numeric tombstones correlate callback-local destruction/release arguments with the retired epoch. They are never dereferenced, used for handle lookup, or turned back into objects. Each identity is cleared at its observed destructor/final release so later address reuse cannot produce false lifecycle events. Unexpected direct destruction while the runtime record is active retires the record before calling the original destructor and logs the contradiction. No post-removal actor/refcount polling. World-owned slot is read after removal only if the current world still matches. Hook logs cover native activation (`1404f1c10`), disable (`1403deec0`), enqueue (`140e78ca0`), ghost destructor (`1404f1ab0`), manipulator destructor (`1403dec70`), release (`141ebc000`), actor deleter (`140e775e0`). Deleter return is evidence that its statically confirmed synchronous destructor/allocator-free sequence returned; allocator free itself is NOT independently detoured. Final release count is observed; data destructor/free is not independently detoured. Do not report either as separately measured hooks.

At creation/activation/removal entry: actor/entry/handle/ChrType/ctrl/primary/replay manipulator/vtable/type inference/gate/data/refcount/counts/cursors/BlockId/position/quaternion/slot and entry.actor comparison. Native activation owns gate132; Theater never sets it. Full field validity must be checked in actual logs, not assumed from compilation.

Exact file version/product/AMD64/SHA validation remains shared and precedes installation. 25 SHA-derived 16-byte entrypoint fingerprints reject patched entrypoints. All prototype hooks are created before enabling; installation failure removes only prototype hooks. No injection changes, game-file modification, networking, animation decoding, camera, world restore, or legacy transform work.

## Still unverified

- Original native local callsite runs in the user's offline session and supplies usable recorder data.
- Exact native ABI behavior, factory one-actor result, native activation and visual presence.
- No crash, clean entry clear, DelayDelete, destructor/refcount events, no stale access.
- No leak: no live allocation measurement yet; deleter-return log alone is not a leak test.
- Prototype 2 locomotion/playback accuracy is NOT implemented/verified in this task.

Do not proceed to Prototype 2 until the user's single create/remove test passes. A TIMEOUT means no ghost was created by the command, and is not permission to spawn from PostPhysics.
