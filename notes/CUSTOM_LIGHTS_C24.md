# Native illumination C24

## Implemented

Native point and spot creation is connected to the existing light definitions and viewport handles. Create Point / Create Spot arms the backend. Render lights toggles all editor lights. Enable/disable, deletion, move-to-camera, XYZ/quaternion handles, radius, relative intensity, RGB, source radius and specular RGB feed owned native lights. Setup files remain ERTLIGHTS 2; no game/save files are edited. Day/night code and UI are unchanged.

Status: implementation is an experimental native backend; runtime/visual illumination is NOT VERIFIED. No unit or in-game tests were requested/run in this task.

## Binding evidence

Exact target: EldenRing_1_17, file/product 2.7.0.0, x64, SHA D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134.

STATIC_VERIFIED Win64 instruction arguments:
- Point factory 141A2AA10: RCX manager, RDX float4 position/range, R8B internal-lock flag, XMM3 fade duration. Returns a registered light, but no caller-owned reference.
- Spot factory 141A2AC40: RCX manager, DL internal-lock flag, XMM2 fade duration. Registered return, no caller-owned reference.
- Retain 141EBBFC0 uses atomic xadd on light+8. Theater retains its own strong reference exactly as native wrappers 141CCB8D0 / 141CCB980 do.
- Removal/release wrapper 141CCBEE0 takes the owned light in RDX; RCX unused. It obtains the current graphics light manager, passes fade 0 in XMM2 to 141A2AD40 and decrements the caller's reference using the native release path. Theater never invokes deleting destructors.
- Nonblocking lock 141F0B0C0 attempts bit 0 at lock+8 and returns 0 on acquisition, -2 on contention. Unlock 141F0B0E0 clears it. Manager lock object is +58. Creation uses factory internal-lock=false ONLY while Theater holds this lock. True would deadlock under the held nonrecursive lock. Removal wrapper owns the lock itself, so Theater releases its lock first.
- Point setter 141A453F0 copies float4 to +190. Spot matrix setter 141A45F80 updates current and previous matrix blocks. Spot shape 141A46150 uses XMM1 near, XMM2 far, XMM3 vertical angle and stack argument horizontal angle; 141A466F0 computes a perspective frustum with half-angle trig. Both full angles use the editor cone in radians.
- Point/spot native updates 141A45490 / 141A46320 refresh derived world-space data and resource state. Called with zero delta after changes to avoid advancing fade/modifier time twice. Engine's normal updates remain intact.

Bindings/offsets/byte guards live in shared/GameProfile.h. Prefix guards cover factory, setters, retain, remove/release, try-lock, unlock and per-light updates. Rust's existing full file-hash/version guard still precedes the Draw_Pre task.

The direct E8 scanner candidate 141A94094 was REJECTED: it was padding between thunk stubs, not a valid native caller. light_update_caller_c24.json is retained as a rejected-candidate instruction record. No hook was installed there or at the manager update. The partially analyzed database still does not establish the manager update dispatcher.

## Data flow and ownership

LightEditor UI -> nonblocking copied snapshot -> existing Rust Draw_Pre task -> NativeLightBackend -> native create/set/update/remove APIs. UI/IPC never mutate native bodies. Render-backend host heartbeat and offline/loaded-player/focus gates are required. No new injector, worker, replay protocol or actor changes.

A native object is created once per enabled editor ID, retained, then updated at up to 30 Hz. No fixed light-count cap; the renderer and available resources determine capacity. Snapshot contention skips a frame. Update lock contention skips a frame; callback reentry skips. Disabled/deleted objects use native deferred removal plus caller-reference release. Native removal may briefly wait for its internal lock; its API is not nonblocking.

The renderer drain releases its registered reference separately. Actual collection membership is checked under the native lock, rather than trusting a potentially reused manager address as a generation token. On root replacement, old objects still have Theater's strong reference and are removed/released using the native wrapper, which searches the current manager before releasing. Unavailable renderer context defers cleanup. Loading/player loss/disconnect disarms rendering; no automatic creation in a new world. Focus loss releases temporary lights; returning focus may reconcile the same definitions if still armed. Emergency Stop disarms creation and queues game-task cleanup.

Native manager lifetime is owned by the engine, not retained by Theater. Draw_Pre is reused because it is the existing renderer-preparation task. Creation/removal task suitability and render-descendant lifetimes must still be validated live: static ABI/refcount evidence does NOT prove all runtime thread interactions. No claim of safe hot-unload.

## Properties and limitations

Diffuse/specular native float4 are +70/+80. Relative intensity scales RGB; W=1. This is not calibrated lumen/candela output. Float units and actual exposure/attenuation need visual checks. Source radius is +90, not intensity/color. Point range is float4 W; exact attenuation and culling relation should be verified live.

Spot matrix uses Theater's existing native camera-to-world matrix convention (+Z forward); native shape calculation corroborates a perspective cone. Visual beam direction still requires a check.

Shadow allocation/quality/bias, cone softness and scattering controls are disabled. Old definitions may retain those values but the backend does not claim to apply them. Shadow flag is forced off on Theater-created lights. No baked lights, save edits, environment-param overrides, sun changes or fake screen-space lighting.

Faults quarantine further native calls until game restart. SEH is a diagnostic failsafe, NOT guaranteed rollback or memory safety. A fault can leave native/owned resources allocated until exit; never retry an uncertain native call automatically. General hardware/game crashes cannot be ruled out before runtime testing.

## First manual check

1. Close Elden Ring and the old host before changing DLLs; never hot-unload.
2. Start outputs/Cinematic-C24-native-lights/EldenRingTheaterMode.exe. Ensure YAFSML loads that folder's TheaterMode.dll, then launch using the existing launcher.
3. Load a safe, dim indoor area. No replay is required. Open F4 -> Lights.
4. Create ONE point light. Native lights should show 1 and status should report submitted. Move it toward a nearby wall; try radius 5 and intensity 5-10, then change RGB to red. Observe illumination on nearby geometry, not only the icon.
5. Toggle Enabled or Render lights off; illumination should disappear. Reenable, then delete it.
6. Create ONE spot, aim toward a wall with the viewport rotation handle, change cone/radius/color and delete.
7. Check focus loss, Stop and a normal load transition only after the basic check succeeds.
8. If anything fails, send %TEMP%/TheaterModeGame.log (NATIVE_LIGHTS lines) and a screenshot. Do not call illumination verified from a native count or compilation.

## Build checkpoint

Release x64 C++ backend/host and Rust adapter compiled successfully. Runtime/visual verification remains pending. The previous packages are preserved.
