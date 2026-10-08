# Elden Ring shader reference: findings for Theater Mode

Research date: 2026-10-07. This is static research, not a runtime validation or a new game feature.

## Provenance and coverage

- Source: https://github.com/garyttierney/eldenring-shaders
- Inspected revision: `bc708baaddd18dbec350e77a92b3adeae5db62f7`, dated 2026-04-04, `feat: Add remaining shaders`.
- Local reference: `C:\Users\user\Documents\ChatGPT\elden ring theater mode\research\references\eldenring-shaders`.
- Full filename index: sibling `eldenring-shaders-files.txt`.
- README describes DXIL -> SPIR-V -> HLSL conversion using dxil-spirv and SPIRV-Cross. These are converted shader artifacts, not the original engine source.
- Full tree index: 33,532 files. Sparse checkout: six folders, 968 HLSL files plus README. Representative shader bodies and relevant declarations were inspected; not every shader body.
- No game patch/executable hash provenance was established. Compatibility with our exact 2.7.0.0 target is UNKNOWN.
- No license file was found in the full filename index. Keep this as a local research reference; no shader code was copied into our implementation or release package.

### Full tree inventory

| Folder | Files |
| --- | ---: |
| shaderbdle.shaderbdlebnd.dcx | 30,912 |
| speedtree.shaderbdlebnd.dcx | 1,593 |
| gxffxshader.shaderbnd.dcx | 317 |
| gxflvershader.shaderbnd.dcx | 242 |
| gxrenderershader.shaderbnd.dcx | 185 |
| gxposteffect.shaderbnd.dcx | 181 |
| gxdecal.shaderbnd.dcx | 48 |
| gxshader.shaderbnd.dcx | 26 |
| gxgui.shaderbnd.dcx | 17 |
| grass.shaderbnd.dcx | 10 |
| Root README | 1 |

Sparse checkout includes gxffxshader, gxflvershader, gxrenderershader, gxposteffect, gxshader and gxgui. The large shaderbdle and speedtree directories are indexed by filename only.

## Evidence labels

CONFIRMED means directly present in the inspected shader artifact. It does not mean the matching CPU object, memory address or game-version compatibility is proven. HIGH CONFIDENCE and LIKELY describe interpretations; UNKNOWN identifies missing evidence.

## 1. Skeletal rendering and root placement

**[CONFIRMED]** `gxflvershader.shaderbnd.dcx/GXFlver_Col_GBuf_Skin_vpo.hlsl`:

- Lines 1-18 declare per-instance `mWorld`, `matricesData`, `cmMatricesOffset`, `ccMatricesOffset` and `prevMatricesOffset`.
- Lines 147-155 declare POSITION, BLENDINDICES and BLENDWEIGHT vertex inputs.
- Lines 173-200 normalize four weights and select four `row_major float3x4` matrices using `matricesData + BLENDINDICES`.
- Lines 201-221 blend their coefficients and transform the vertex position, including translation.
- Lines 227-243 subtract `SC_CameraPos` from the transformed position and project with `VC_MatrixViewProj`.

**[HIGH CONFIDENCE]** This variant implements linear blend skinning with a camera-relative projection step. It is useful evidence that skeletal pose, skinning palette translation and camera origin all participate in the final rendered placement.

**[UNKNOWN]** The exact CPU conversion from our captured local/model hkQsTransform arrays to this skinning palette, inverse-bind handling, the CPU meaning of each matrix offset, and the root-to-renderer ownership path. Do not directly upload hkaSkeleton model transforms assuming they are final skinning matrices.

The converted declaration has `aObjMatrix[2]` despite variable palette indices in the body. Treat this as a reconstruction limitation to investigate against original shader reflection, not a two-bone engine limit or a reliable array bound.

For our player/Torrent work, these artifacts can help verify a rendered draw's matrix palette. They do not find Torrent's importer, actor lifetime, or correct game-thread transform-write API. The existing CPU root/origin investigation remains necessary.

## 2. Camera and culling data

**[CONFIRMED]** The same shader declares `cbSceneParamUBO` with:

| Declaration | Converted packoffset | Research purpose |
| --- | --- | --- |
| SC_CameraPos | c3 | Projection origin |
| FC_MatrixView | c5 | View matrix |
| FC_MatrixInvViewProj | c8 | Reconstruction |
| FC_MatrixInvProj | c12 | Projection inverse |
| FC_MatrixProj | c16 | Projection / future FOV correlation |
| FC_MatrixInvView | c20 | Camera/world conversion |
| VC_MatrixViewProj | c24 | Vertex projection |
| SC_PrevCameraPos | c28 | Previous-frame camera declaration |
| VC_PrevMatrixViewProj | c29 | Previous-frame projection declaration |
| SC_RenderCameraPos | c71 | Separate named render camera field |
| SC_CullingCameraPos | c98 | Separate named culling camera field |

**[LIKELY]** A detached camera could need coordination with visibility/culling state; these declarations identify what to investigate. Their presence alone does not prove which pass uses each field.

**[UNKNOWN]** Camera setter/owner, FOV units, authoritative camera lifetime, update order and CPU structure offsets.

`packoffset(cN)` describes GPU constant-buffer packing (16-byte vector slots), not an address in eldenring.exe. `register(bN/tN/sN, space0)` comes from converted HLSL; original DX12 root-signature bindings must be checked separately. Do not derive CPU pointer chains from these values.

## 3. Native lights

**[CONFIRMED]** `gxrenderershader.shaderbnd.dcx/GXLightAcc_DPointLight1_ppo.hlsl`, lines 1-9, declares PositionAndRadius, DiffColor, SpecColor, TransLightParam and LightType. Shader body reads position/radius around line 198 and lighting colors around lines 209/246.

**[CONFIRMED]** `GXLightAcc_DSpotLight1_ppo.hlsl`, lines 1-13, additionally declares PositionAndRange, ViewToLightSpaceMatrix, MinDist and MaxDist. Body reads the light-space matrix around lines 204-207 and position/range around line 217.

**[HIGH CONFIDENCE]** These are useful schemas for later read-only correlation of native point/spot light uploads. They support investigating real cinematic lighting parameters.

**[UNKNOWN]** Spawn/destruction APIs, owning scene containers, CPU handles, shadow-resource allocation and safe editing/restoration. Shader variants with numbered suffixes are not evidence of a global engine light-count limit.

## 4. Depth of field

**[CONFIRMED]** `gxposteffect.shaderbnd.dcx/LensSimu2_ComputeCoC_ppo.hlsl` declares CBDofParamUBO with depth reconstruction, near/far parameters, lens parameters, no-blur distances, CoC clipping, sample scale, fade and temporal parameters.

The shader actually samples depth and computes:

`z = -DepthComputeParam.y / (DepthComputeParam.x - depth)`

Near and far CoC contributions use clamped depth ramps and `LensParam.z/w` strengths. This is evidence of a real depth-dependent blur control path, not just a filename.

**[UNKNOWN]** Physical aperture, focal length/focus distance units, CPU gparam mapping and setters. Do not expose a fake physical aperture slider based only on the name LensParam.

## 5. Color grading and exposure

**[CONFIRMED]** `gxposteffect.shaderbnd.dcx/ColorGrading_ppo.hlsl` samples a source image and a 3D `g_InputTexLut` texture. RGB LUT coordinates use `rgb * 0.9375 + 0.03125`. This is a concrete LUT-based color grading path.

**[CONFIRMED]** The scene constant block declares SC_SceneExposure, SC_InvSceneExposure, SC_PreExposure and SC_InvPreExposure at c72. ToneMap and fog shader families appear in the filename index.

**[UNKNOWN]** LUT dimensions/format at runtime, upload ownership, adaptation control, engine Look/gparam mapping and compatibility with other post-process mods. Not all tone-map/fog bodies were analyzed.

## 6. Temporal effects and seeking

**[CONFIRMED]** `MotionBlurWriteCMBVelocity_ppo.hlsl` declares and reads `g_mCurToPrevScreen`, samples depth and reconstructs screen/depth relationships. It exposes motion-blur intensity and velocity encode parameters.

**[CONFIRMED]** `LensSimu2_ScaledTemporal_ppo.hlsl` samples velocity and two source textures and exposes g_vTemporalParam. The scene block also declares previous camera/projection fields and per-instance previous matrix offsets.

**[LIKELY]** A discontinuous replay seek may require handling renderer history to avoid motion-blur/temporal artifacts. This is separate from replay interpolation and cannot explain all previously reported player stutter without runtime evidence.

**[UNKNOWN]** History invalidation APIs and which previous matrices are consumed by the exact target's skinned velocity pass. Declared but unused fields are not proof of execution.

## Recommended use in our project

1. Preserve the existing CPU recording/replay integration. This repository provides rendering evidence, not an alternative replay backend.
2. When pursuing camera/render features, collect a read-only DX12 capture in the existing offline environment and correlate the actual draw/PSO, original shader reflection and uploaded constants with these artifacts.
3. For skeletal/root placement, compare palette changes with recorded local/model transforms and physics movement. Resolve inverse-bind and origin conversion before considering any renderer-side alternative.
4. For native lighting/DOF/Look controls, identify the CPU owners through our exact-target Ghidra/SDK research, then observe values at runtime before controlled writes.
5. For seek artifacts, observe current/previous projection and velocity history before designing a reset mechanism. Avoid guessed patches.

## Changes and limitations of this research session

- Added this report and a separate local reference checkout/full filename index.
- No TheaterMode runtime code, game files, baseline builds or original Claude project were changed.
- No new release build or implementation tests were needed for this documentation-only research.
- No camera, light, DOF or shader writes were performed. No new in-game functionality is claimed.

Most useful starting files: GXFlver_Col_GBuf_Skin_vpo, GXLightAcc_DPointLight1_ppo, GXLightAcc_DSpotLight1_ppo, LensSimu2_ComputeCoC_ppo, ColorGrading_ppo, MotionBlurWriteCMBVelocity_ppo and LensSimu2_ScaledTemporal_ppo, in the folders identified above.
