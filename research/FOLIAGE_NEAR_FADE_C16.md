# Foliage close-up research, EldenRing_1_17 / 2.7.0.0

## STATIC_VERIFIED / compile layout verification

The pinned fromsoftware-rs 3c8c1d7633a exposes CSCam.near_plane (+0x58), public
SoloParamRepository rows_mut/get_mut, AssetEnvironmentGeometryParam (index 69)
and GrassTypeParam (63, plus Lv1/Lv2 variants 64/65). Typed asset row accessor
cam_near_behavior_type and grass dithering accessor exist. We use public APIs;
the old CT agrees on asset field +0xAF and grass dithering +0x80 but those raw
row offsets are not used in implementation. Native copy-site byte validation and
the exact executable profile remain mandatory.

## STATIC_REFERENCE: separate shader fade

Read-only `research/references/eldenring-shaders` export:
`gxflvershader.shaderbnd.dcx/GXFlver_ColDifSpcBumpEmiIblGlow_DptA_ppo.hlsl`,
frag_main checks cbMatDynParam_FC_CameraFadeParam.IsEnable and computes near/far
gradients/intercepts before setting discard_state against a dither texture.
This is distance-based material fading, not just near-plane geometry clipping.
The export is a reference, not proof of this exact target's draw/grass dispatch.
No shader/constant-buffer hook or invented GPU address was added.

## Primary ER metadata

Saved focused evidence: C16_AssetCameraNearEnum.json / C16_GrassDitheringEnum.json.

- [Smithbox ER asset camera-near enum](https://github.com/vawser/Smithbox/blob/main/src/Smithbox.Data/Assets/PARAM/ER/Param%20Enums/ASSET_CAM_NEAR_BEHAVIOR_TYPE.json):
  0 Never disappear; 1/2 asset height categories. This supports a reversible
  in-memory value 0 override for known 1/2 entries.
- [Smithbox ER grass dithering enum](https://github.com/vawser/Smithbox/blob/main/src/Smithbox.Data/Assets/PARAM/ER/Param%20Enums/GRASS_DITHERING_TYPE_ENUM.json):
  only 0 Type 0; no documented disable-fade meaning. Do not guess another value.
- [Paramdex ER GrassTypeParam definition](https://github.com/soulsmods/Paramdex/blob/master/ER/Defs/GrassTypeParam.xml)
  confirms dithering is a separate byte/enum from LOD/wind/density parameters.
- [Paramdex ER asset definition](https://github.com/soulsmods/Paramdex/blob/master/ER/Defs/AssetGeometryParam.xml)
  documents camNearBehaviorType as drawing settings on camera approach.

## UNKNOWN / next evidence

Whether current loaded assets cache proximity behavior; grass render path and
the exact alpha-fade producer; visual coverage across models and foliage quality
levels; near-Z depth precision artifacts. Runtime logs report overridden asset
rows and a grass dithering histogram, not a fabricated successful grass fix.

Changes are camera-only in-memory settings. No original game files, param files,
shader binaries, game saves or third-party camera binaries were altered.
