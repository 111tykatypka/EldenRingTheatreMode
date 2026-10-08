# Weather editor C19 — native weather requests

2026-10-08. Target: EldenRing_1_17, executable 2.7.0.0 AMD64, SHA256
`D1A84083C6C7C7902162FF098F7D86812839AA6B3575959398857E539C488134`.

## Evidence and semantics

**STATIC_VERIFIED**: public debug-tool weather access correlates with the exact executable. Source reference: [ErdHook at pinned revision](https://github.com/Nordgaren/Erd-Tools/blob/b878125782f45705c65202f8a13ea6142882bf53/src/Erd-Tools/Hook/ErdHook.cs). Independently implemented adapter; no imported reference implementation/library/assets.

| Binding | RVA / offset | Instruction evidence |
|---|---:|---|
| Singleton pointer slot | 0x3D6D3F0 | At RVA 0x582990: `48 8B 15 59 AA 7E 03`, RIP-relative load resolves this slot |
| Request ID | object +0x02, signed int16 | Constructor RVA 0x646798: `66 44 89 61 02` after r12d=-1 |
| Active weather ID | object +0x2A, signed int16 | Constructor RVA 0x6467CC: `66 44 89 61 2A` |
| Mailbox consumer | RVA 0x64B1FD | `66 83 7E 02 FF`: compare request to -1 |

Disassembly exports: `weather_accessors_c19.json` is a known-signature instruction slice, not a claimed function entry; `weather_constructor_c19.json` begins at the constructor; `weather_native_functions_c19.json` begins at consumer function `0x14064AF50`. Earlier misaligned exploratory exports were overwritten and are not evidence.

**HIGH CONFIDENCE** native flow: singleton creation at `0x14061F650` -> constructor `0x140646770`; weather update `0x1406489D0` -> `0x14064AF50` consumes +2 request and clears it to -1 -> `0x14064A440` resolves region*100 + requested ID if a row exists, otherwise falls back to requested ID -> WeatherParam row lookup `0x140D54A30` -> transition `0x140649D10` and visual/environment updates. Null row guard exists in the consumer. No direct call into inferred function prototypes is added.

**Important:** +2 is a transient request mailbox, not a persistent weather scalar. A forever-every-frame write would restart transitions or contend with native updates. C19 sends at most once per real second while waiting for a requested active ID; once matched it stops sending. A 15-second non-observation timeout releases the request and reports failure. These are request safety bounds, not recording limits. Very low world timescale may delay transitions beyond that guard.

## Implementation and control flow

`Weather rail tab -> selection / Apply -> thread-safe request state -> verified ChrIns_PostPhysics callback -> fresh WorldChrMan/player and loading/offline gate -> EldenRingWeatherAdapter -> native request mailbox`.

All weather game-memory writes occur in the game callback. UI/IPC only change atomic requests. Host connection, loaded player, foreground game and exact-profile instruction validation are required. No pointer survives as a dereference target across callbacks; the stored ownership identity is compared to a freshly read singleton.

Default OFF; never enabled by saved UI settings. Selector changes are inert until Apply. Includes 18 named families and 41 numerically identified regional variants (59 choices). Regional effect descriptions are deliberately not guessed. This is a reference preset catalog, **not proof of every WeatherParam row or of effects in every region**. No arbitrary unvalidated ID input. No fake precipitation intensity, lightning or wind-vector settings.

Restoration captures previous active weather, submits that ID once through the native transition path, then releases editor control. A native pending request owned by someone else prevents replacement/restoration. On loading/invalid context, cancel only an owned pending request, never restore an old scene into a new one. Disable on focus loss, host disconnect/stale heartbeat (2 real seconds), loading, manager replacement or emergency Stop. Transition completion and subsequent native automatic progression still require live validation. No save files, event flags, params or game executable are directly changed.

Diagnostics: active ID, pending request ID, waiting/matched/failed status in panel; deduplicated transition entries prefixed WEATHER in `%TEMP%/TheaterModeGame.log`. Applied ID is a readback condition, **not proof of visible precipitation**.

Weather remains an editor override independent of camera and actor replay. ERPLAY/world formats unchanged; no weather recording/seek restoration yet. New Tool enum value appended so persisted existing tool IDs remain stable; overlay wire protocol unchanged.

## Validation status

STATIC_VERIFIED: singleton/signature, constructor and request consumer instructions.
COMPILE_VERIFIED: see checkpoint manifest after Release build.
RUNTIME_VERIFIED / VISUALLY_VERIFIED: **NOT YET**. No Elden Ring session was launched or manipulated in this task. No automated tests added or run for C19.

## Manual validation order

1. Use the C19 package's existing launcher/YAFSML workflow in offline Elden Ring 1.17. Close old host and game before loading the new DLL; DLLs are not hot-swapped into a running process.
2. Load a safe outdoor location where native weather is visible. F4 -> Weather. Confirm manager diagnostics and native ID appear; Apply is enabled only with a connected host and loaded player.
3. At 1x world speed: select Rain, Apply; observe native ID and rain. Try Sunny and Fog, then snow/blizzard in a snow-capable region. Effects may blend or be unavailable in a particular area.
4. Restore automatic weather; verify previous weather transition and later automatic behavior. Check normal camera, controls and replay still work.
5. Apply again, alt-tab and return: override should be off. Repeat with Stop and with a safe travel/loading transition. Do not expect the editor to re-enable itself.
6. If nothing appears or game fails, supply WEATHER lines from `%TEMP%/TheaterModeGame.log` and `%TEMP%/TheaterModeRender.log`, selected ID, loaded area and whether native current ID matched.

Custom point/spot light research: see `CUSTOM_LIGHTS_RESEARCH.md`; no light allocations implemented.
