# Research: the two tools in `debug tools\`

Evidence labels: STATIC (read from the files), LOG (seen in Theater's own logs), UNKNOWN.
The executables were not run; only their resource files were read. The save manager was not opened at all.

## What is in them
- **Elden Ring Debug Tool 0.8.6.2** (Nordgaren, open source; .NET 6 single-file exe of 180 MB; needs the game offline).
  `Resources/` is the useful part (6 MB, plain text and XML):
  - `Params/Defs/*.xml`: 190 paramdef files, every field of every param row with a (mostly Japanese, partly English) description.
  - `Params/Names/*.txt`: human names of rows (3121 SpEffect rows, goods, NPCs, bonfires, ...).
  - `Params/Pointers/ParamOffsets.txt`: 195 RVA offsets of the param tables in the exe (a cross-check for our own table lookups).
  - `Events/GoodsEvents.txt`: event flag per usable item (for example `60100 -> 130 Spectral Steed Whistle`).
  - `Items/*` (weapons, armor, goods, flasks, DLC), `Systems/SitesOfGrace.xml` (continent / hub / grace tree with row ids).
- **er-save-manager 2.0.1** (frozen Python app, 130 MB): a save editor. `resources/eventflag_bst.txt` is an event-flag tree. Not needed for Theater, and
  editing saves is exactly what we avoid.

## Findings that change what we do
1. **`CAMERA_FADE_PARAM_ST` (CameraFadeParam.xml, STATIC).** Alpha is 0 at `NearMinDist`, ramps to `MiddleAlpha` at `NearMaxDist`, stays `MiddleAlpha`
   until `FarMinDist`, ramps to 1 at `FarMaxDist`. My first near-fade fix only moved the near range and left the middle band at `MiddleAlpha`, so things could stay
   translucent. Fixed in `camera_fade.rs`: near range (-1, 0) and `MiddleAlpha = 1`.
2. **`CHR_MODEL_PARAM_ST.cameraDitherFadeId` (ChrModelParam.xml, STATIC):** -1 = take it from the material, 0 = never disappears, 1.. = a camera-fade row.
   Characters / models that use a row (or the material) fade near the camera; setting every row to 0 removes it. Added to `camera_fade.rs`.
3. **`AssetGeometryParam.camNearBehaviorType`:** described only as "drawing setting when the camera approaches"; 0 = never disappears is from the SDK and the
   cinematic branch, not from this tool.
4. **`GrassTypeParam`** fields: `lodRange`, three cluster types, `flatRadius`, `dithering` (no description). No near-camera field is documented, so grass is still unchanged.
5. **`EQUIP_PARAM_GOODS_ST` (STATIC):** `modelId` (the model shown for the item), `goodsUseAnim` (animation played on use; LOG: 10 for Crimson Tears, 19
   for Cerulean Tears, 32 for the whistle), `refId_default` (LOG: the special effect, 501000 / 501050 / 81), `castSfxId / fireSfxId / effectSfxId`
   (LOG: 301050 / -1 / 301052), `isSummonHorse` (the whistle flag), `isDisableHand` (not usable with a bare right hand), `useEnableSpEffectType`,
   `isEnableFastUseItem`. So the flask model is chosen by the goods row's `modelId` and attached by the game's item-model code; we found the in-hand
   state that switches it (model-instance byte 0x2D0 and action-flag dword 0x48 = 2), not the model itself.
6. **`RIDE_PARAM` (STATIC):** `atkChrId` (rider), `defChrId` (mount), `atkChrAnimId` / `defChrAnimId` (they rewrite the variables "RideOnAnimId" and
   "RiddenOnAnimId" in the Havok behaviour "RideOn" / "RiddenOn" states), `defAdjustDmyId` (dummy polygon of the mount used to snap the rider), `rideCamParamId`.
   This confirms from the data side that mounting is a paired behaviour-graph state, not something the importer pose alone describes.
7. **`WEP_ABSORP_POS_PARAM_ST`:** per-weapon attach dummy polygons for model 0..3 in right / left / both hands plus `hangPosType`
   (storage location; "the sheathing animation changes with this value"). This is what the action-flag module's `absorp_pos_param_condition` bytes index.
8. **Special-effect names (STATIC, SpEffectParam.txt):** 81 = Spectral Steed Whistle, 501000 = Flask of Crimson Tears, 501025 = Crimson Tears [chain],
   501050 = Cerulean Tears, 100 / 101 / 106 = grace effects (restore HP / remove status / reload), 90301 = NPC: Disable Sleep. The ids 100000..100006 seen around item use have no names.
9. **`SP_EFFECT_VFX` (SpEffectVfx.xml):** `useCamouflage`, `isInvisibleWeapon` ("Invisible Weapon for Weapon Enchantment: 0 weapon shown, 1 weapon hidden"). This is the only
   documented "hide weapon" flag in the tables, and it belongs to weapon-enchant effects, not to flasks. `Hide Weapon` as a state-info value (184) has no rows in this game version (LOG).

## Not yet used / ideas
- Use `Names` to put readable names into our logs (items, effects, NPC params).
- Compare `ParamOffsets.txt` RVAs with the table lookups we make through the SDK, as an independent check on 2.7.0.0.
- `GoodsEvents.txt` could tell which event flag an item use sets (relevant to "never alter progression").
