# Development queue

## Priority: native custom light illumination

Requested: point/spot definitions must illuminate the scene, not only show editor icons.
Status: QUEUED — not implemented. C22 light handles and serialization are authoring only.

Reuse the existing LightEditor definitions and viewport manipulation. Keep the user-verified day/night slider unchanged.

Required implementation gates:
1. Resolve the task/owner that updates GXLightManager (native 141A2B070).
2. Prove factory ABI, including vector/XMM parameters, for point/spot constructors.
3. Schedule creation outside the manager's nonrecursive spin lock (141F0B0A0); avoid reentrant deadlock.
4. Prove retain/register/remove/fade/render-worker lifetime and safe cleanup on scene changes.
5. Verify transform, radius/cone, RGB, intensity and shadow units from consumers; source radius +90 is NOT color.
6. Implement bounded create/update/delete on the proper game task; copy UI requests, never native pointers.
7. Release through native ownership APIs; never call deleting destructors directly.
8. Validate one light, then movement/color/radius, deletion, loading and disconnect.

Evidence and unresolved items: research/CUSTOM_LIGHTS_RESEARCH.md.

## Camera quality / offscreen animation updates

C23 adds an opt-in Look-tab transient normal-character-update request. Exact executable consumer verified statically; implementation compiled separately. Visual benefit and callback timing still require an in-game check.

Separate follow-up: render/model LOD override and camera-based relevance/streaming. Do not represent grass LOD params as animation controls. CameraTools' higher-LOD hook is a render-quality mechanism, not proof of NPC update quality.
