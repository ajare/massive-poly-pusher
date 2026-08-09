# HDR EXR Image-Based Lighting Implementation Plan

## Goal

Allow a linear HDR `.exr` equirectangular environment image to be authored as an IBL/reflection source. MPP converts it into renderer-owned PBR resources: environment cubemap, diffuse irradiance cubemap, prefiltered-specular cubemap, and a BRDF integration LUT. This is not a material/albedo texture workflow.

## Non-goals for the first release

- Using EXR as a 2D material map.
- HDR panorama editing/painting.
- Runtime support for arbitrary glTF IBL extensions.
- Saving generated GPU textures as user-authored package files.

## Source contract

Add an explicit PBR environment source representation:

```xml
<Environment>
  <binding>Studio</binding>
  <hdrEquirectangular>environments/studio.exr</hdrEquirectangular>
  <resolution>512</resolution>
  <irradianceResolution>32</irradianceResolution>
  <prefilterResolution>128</prefilterResolution>
</Environment>
```

- `hdrEquirectangular` is an HDR linear equirectangular image.
- Existing explicit `irradiance`, `prefilteredSpecular`, and `brdfLut` bindings remain supported as the manual/advanced path.
- `hdrEquirectangular` and explicit cubemap bindings are mutually exclusive, except for an optional explicit BRDF LUT override.

## Phase 1 — EXR decode audit

1. Verify FreeImage EXR codecs are included in Debug and Release distribution.
2. Add tests for `FIT_RGBF` and `FIT_RGBAF` input.
3. Confirm `ImageLoader` retains floating-point pixels and channel order.
4. Ensure texture upload selects floating-point internal formats (`RGB16F`/`RGBA16F` or 32F), never 8-bit normalized formats.
5. Add clear diagnostics for unsupported EXR channels/types.

**Acceptance:** An EXR can load as a linear floating-point 2D source texture with expected dimensions and non-clamped pixel values.

## Phase 2 — Environment source document and parser

1. Extend `PbrPipelineEnvironmentDocument` with HDR source path and preprocessing settings.
2. Update parser, serializer, validation, authoring guide, and package manifest dependency collection.
3. Validate source extension, non-zero resolution limits, and mutual exclusion with manual irradiance/prefiltered bindings.
4. Preserve existing pipeline documents unchanged.

**Acceptance:** A pipeline can serialize, reload, validate, and package an HDR IBL source declaration.

## Phase 3 — Renderer-owned IBL cache contract

1. Add a renderer `IblEnvironmentCache` keyed by:
   - canonical source path;
   - source file timestamp/hash;
   - target resolutions;
   - pixel format/preprocessing version.
2. Define a generated environment result containing source cubemap, irradiance cubemap, prefiltered cubemap, and BRDF LUT.
3. Make cache results reference-counted and safe across preview/pipeline replacement.
4. Add explicit invalidation and cleanup APIs.

**Acceptance:** Equivalent pipelines reuse generated IBL resources; changed source/settings create a new generation.

## Phase 4 — Cubemap render-target infrastructure

The existing renderer supports 2D render targets but has no public/generated cubemap render-target path. This infrastructure is required before HDR conversion, irradiance convolution, or specular prefiltering can be implemented.

### 4.1 Render-target data model — Complete

Implemented in `72d1222 Add cubemap render texture data model`.

1. Extend `RenderTextureOptions` and `RenderTextureStream::Definition` with an explicit attachment target (`Texture2D` or `TextureCube`), cubemap face size, and declared mip count/base/max level.
2. Retain the current `Texture2D` defaults and serialized behavior unchanged.
3. Reject unsupported target combinations early: cubemap depth attachments, array/3D targets, and multisampled cubemaps. The first IBL implementation needs colour-only, single-sampled cubemaps.
4. Require a sized floating-point colour internal format for generated HDR IBL targets (`RGB16F`, `RGBA16F`, `RGB32F`, or `RGBA32F`); reject normalised/integer formats in the IBL creation helper.

### 4.2 Allocation, resize, and destruction — Complete

Implemented in `559263a Allocate floating point cubemap render textures`. Cubemap faces and declared mip levels allocate, resize/recreate, and release through the existing render-texture lifetime; target-aware sampler/mipmap handling is in place.

### 4.3 Face/mip framebuffer attachment — Complete

Implemented in `4a688ac Add cubemap face mip attachment API` and this change: `attachColourFace()` validates and attaches faces/mips, and `restoreColourFaces()` restores conventional +X/mip-zero framebuffer attachments after a face-render sequence.

1. Add a `RenderTexture::attachColourFace(attachment, face, mipLevel)` API (or scoped equivalent).
2. Validate colour attachment index, cube face `[0,5]`, and mip level against declared levels before issuing OpenGL calls.
3. Attach with `glFramebufferTexture2D(..., GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, textureId, mipLevel)`.
4. Revalidate framebuffer completeness after attachment changes and report target name, attachment, face, and mip in failures.
5. Preserve/recover the level-zero face attachment expected by existing activation code when a scoped attachment operation ends.

### 4.4 Render-system state and command contract — Complete

Implemented in `f2618fd Add scoped cubemap face rendering` and this change. `RenderSystem::CubemapFaceRenderScope` selects a face/mip, adjusts the viewport, restores target/viewport/scissor/draw/read-buffer/default attachments, rejects nested face scopes, and emits target/face/mip GPU debug labels for RenderDoc tooling.

1. Add a scoped `RenderSystem` cubemap-face render target operation that saves and restores framebuffer bindings, viewport, scissor enable/box, draw/read buffers, and active render target bookkeeping.
2. Set viewport dimensions to `max(1, faceSize >> mipLevel)` while a face/mip is active.
3. Prohibit nested target changes that could leave an IBL face attached after an exception; restoration must be RAII-based.
4. Integrate GPU marker labels containing target, face, and mip for RenderDoc/Process Flow diagnostics.

### 4.5 Cache-compatible creation helpers — Complete

`RenderSystem::createIblCubemap()` creates named, colour-only, single-sample cubemap render targets with validated RGB/RGBA 16F/32F formats, clamp-to-edge sampling, and declared mip views. Cache generation code supplies generation-unique names and owns the returned resource.

1. Add renderer-owned helpers that create named, colour-only floating-point cubemap render textures for environment, irradiance, and prefiltered-specular cache outputs.
2. Names must be generation-unique; cache/resource references own the generated objects and pipeline replacement must not invalidate an in-flight/active generation.
3. Expose read-only cubemap `Texture` resources suitable for existing PBR `samplerCube` bindings.

### 4.6 Tests — Complete

Added GPU coverage for six independent cubemap-face writes, non-zero mip attachment, and RGBA16F values above 1.0. The suite also rejects LDR IBL formats and verifies viewport, scissor, draw/read-buffer, and scissor-enable restoration; existing render-graph GPU tests continue covering 2D, MSAA, mip, depth, and graph paths.

1. Unit-test validation for target, format, samples, face, mip, and dimension limits.
2. GPU test: render six distinct solid colours to mip zero and sample/read back each cubemap face.
3. GPU test: attach and render to at least one non-zero mip, verifying reduced viewport dimensions and retained mip-zero data.
4. GPU test: verify `RGB16F`/`RGBA16F` cubemaps preserve values above 1.0.
5. GPU test: verify framebuffer, viewport, scissor, draw/read-buffer state, and current target bookkeeping are restored after success and failure.
6. Regression-test all existing 2D render-texture, MSAA resolve, mipmap, depth-only, and render-graph paths.

**Acceptance:** Renderer code can safely render a different known colour to every face of an HDR cubemap and sample each result through `samplerCube`; all existing 2D render-target behavior remains unchanged.

## Phase 5 — Equirectangular HDR to cubemap

### Runtime API

Add a renderer-owned conversion entry point:

```cpp
RenderTargetPtr RenderSystem::convertEquirectangularToCubemap(
    Texture* hdrEquirectangular,
    std::string const& generatedName,
    uint32_t faceSize,
    uint32_t mipLevels = 1);
```

Contract:

1. `hdrEquirectangular` is a loaded, non-null, linear `GL_TEXTURE_2D` texture with floating-point storage (`RGB16F`, `RGBA16F`, `RGB32F`, or `RGBA32F`). Reject all other targets/formats with a diagnostic; no colour-space conversion is performed.
2. `generatedName` is supplied by the IBL cache and must be generation-unique. The returned `RenderTargetPtr` owns the generated cubemap; the cache retains it until invalidated while active pipelines retain shared references safely.
3. `faceSize` and `mipLevels` are passed to `createIblCubemap`. The initial conversion only writes mip zero; later prefilter work writes additional mips.
4. The operation is synchronous and must either return a fully populated cubemap or throw without publishing a partial result. It uses `CubemapFaceRenderScope` for every write and leaves render target/state unchanged.
5. The renderer owns the conversion shader/program and fullscreen mesh. It emits one GPU debug scope per face through the existing scoped face render path.
6. Phase 9 runtime integration, not this API, decides cache lookup, source-file loading, error fallback, and pipeline environment binding.

### Face convention

Use OpenGL cubemap face order `+X, -X, +Y, -Y, +Z, -Z`. For face-local coordinates `u = 2*x - 1` and `v = 2*y - 1` (where `y` increases upward in the render target), sample directions:

```text
+X: ( 1, -v, -u)    -X: (-1, -v,  u)
+Y: ( u,  1,  v)    -Y: ( u, -1, -v)
+Z: ( u, -v,  1)    -Z: (-u, -v, -1)
```

Normalize the direction, then calculate equirectangular UV using:

```text
longitude = atan(direction.z, direction.x)
latitude  = asin(clamp(direction.y, -1, 1))
u = longitude / (2*pi) + 0.5
v = 0.5 - latitude / pi
```

The fragment shader uses output pixel coordinates/face size rather than a scene camera, avoiding matrix-orientation ambiguity.

### 5.1 Conversion shader source — Complete

1. Add a renderer-owned fullscreen fragment shader with `EQUIRECTANGULAR`, `FACE`, and `OUTPUT_SIZE` uniforms.
2. Implement the documented face-direction and longitude/latitude equations in shader code.
3. Keep the shader independent of scene camera matrices; pixel coordinate and output size define each face-local direction.

**Acceptance:** The shader source compiles as a parser program and exposes the required uniforms.

### 5.2 Core-program lifecycle — Complete

Implemented with renderer-owned `__mpp_ibl_equirectangular_to_cubemap__` core resource creation alongside fullscreen post-process programs.

1. Add a dedicated `ResourcePtr` member to `RenderSystem` for the conversion program.
2. Create/load it with the existing renderer-owned fullscreen/bloom program factory during core-resource setup.
3. Register it for normal core-resource destruction and give it a stable internal resource name.

**Acceptance:** An initialized renderer owns a loaded conversion program without per-conversion shader compilation.

### 5.3 Source texture validation — Complete

Added read-only texture target/internal-format accessors and the renderer validation helper used by the forthcoming conversion API. It rejects null/unloaded, non-2D, non-floating-point/sRGB/integer sources and invalid output configuration before allocation.

1. Add read-only `Texture` accessors needed to inspect texture target and internal format without binding side effects.
2. Validate non-null, loaded `GL_TEXTURE_2D`, linear floating-point input and reject cubemaps, LDR, integer, and sRGB sources with explicit errors.
3. Validate `faceSize`, mip count, and generated cache resource name before allocating output.

**Acceptance:** Invalid source/configuration fails before cubemap allocation or render-state mutation.

### 5.4 Single-face conversion helper — Complete

Added the private `RenderSystem::renderEquirectangularCubemapFace` helper. It selects the face/mip through the scoped target API, binds the HDR source, sets conversion uniforms, draws the renderer-owned fullscreen mesh, accounts for the submission, and restores matrix/target state on failure.

1. Add a private `RenderSystem` helper accepting source texture, output `RenderTexture`, face index, and mip level.
2. Enter `CubemapFaceRenderScope`, bind the source to unit zero, set `EQUIRECTANGULAR`, `FACE`, and `OUTPUT_SIZE`, and submit the existing fullscreen quad.
3. Keep render-info accounting consistent with existing fullscreen operations.
4. Verify face attachment and scoped restoration on success and exceptions.

**Acceptance:** One requested cubemap face/mip is populated without leaking renderer or GL state.

### 5.5 Public six-face conversion API — Complete

Added `RenderSystem::convertEquirectangularToCubemap`. It validates source/configuration, creates a caller-named cache candidate, synchronously renders all six mip-zero faces, and returns only after complete conversion. Candidate ownership remains local until return, so failure cannot publish a partial cache resource.

1. Implement `convertEquirectangularToCubemap(...)` using validation, `createIblCubemap`, and six calls to the single-face helper at mip zero.
2. Do not generate/filter unwritten mips; return a complete mip-zero cubemap only.
3. On failure, release the candidate resource and throw before it is inserted into any IBL cache.

**Acceptance:** The API returns a complete six-face floating-point cubemap with no cache publication side effects.

### 5.6 GPU orientation and HDR tests — Complete

Added a linear RGB32F programmatic panorama fixture and public conversion test that verifies all generated faces retain HDR values above 1.0, the documented `-Z < +X < +Z` centre-face orientation, and continuity across the `+X/-Z` cubemap edge. It also verifies an invalid source fails before changing viewport state.

1. Add a small directional floating-point equirectangular fixture with distinct colours for cardinal directions and values above 1.0.
2. Convert it and read each cubemap face back, verifying the documented face convention.
3. Test longitudinal seam continuity near `u = 0/1` and verify no vertical inversion.
4. Verify invalid sources fail without changing active framebuffer/viewport/scissor state.

**Acceptance:** The API returns a complete cubemap whose faces sample the documented panorama directions without seams or upside-down orientation.

## Phase 6 — Diffuse irradiance convolution

### 6.1 Runtime API and settings — Complete

Declared the public `generateDiffuseIrradiance` contract and implemented its pre-allocation source/configuration validator. The six-face implementation follows in Phase 6.5.

1. Add a renderer-owned API:

```cpp
RenderTargetPtr RenderSystem::generateDiffuseIrradiance(
    Texture* environmentCubemap,
    std::string const& generatedName,
    uint32_t faceSize,
    uint32_t sampleCount = 1024);
```

2. Require a loaded floating-point `GL_TEXTURE_CUBE_MAP` source and non-zero name, face size, and sample count.
3. Create a single-mip floating-point output using `createIblCubemap`.
4. Keep cache lookup/publication outside this API; it synchronously returns an unpublished generated candidate.

**Acceptance:** Invalid configuration fails before allocation or render-state changes.

### 6.2 Convolution shader source — Complete

Added `FragmentShaderDiffuseIrradianceTemplate`: it shares the Phase 5 face convention, constructs a robust tangent basis, uses a deterministic Hammersley sequence, applies cosine weighting, and protects normalization from zero accumulated weight.

1. Add renderer-owned `FragmentShaderDiffuseIrradianceTemplate` with `ENVIRONMENT`, `FACE`, `OUTPUT_SIZE`, and `SAMPLE_COUNT` uniforms.
2. Reuse the Phase 5 face-direction convention.
3. Build a stable tangent basis around each normal and integrate the hemisphere using a deterministic low-discrepancy/Hammersley sequence.
4. Weight samples by `NdotL` and normalize by the accumulated weight; guard zero/NaN paths.

**Acceptance:** The shader compiles and implements cosine-weighted diffuse irradiance, independent of scene camera state.

### 6.3 Core-program lifecycle and validation — Complete

Added renderer-owned `__mpp_ibl_diffuse_irradiance__` core program creation alongside the equirectangular converter. Phase 6.1 validation supplies the loaded floating-point cubemap/source checks before face work begins.

1. Add/load/destroy a stable renderer-owned diffuse-convolution program with other core post-process programs.
2. Add target/internal-format accessors/validation required for cubemap input, reusing Phase 5 error conventions.
3. Reject source/output aliasing.

**Acceptance:** No per-generation shader compilation and no invalid source proceeds to face rendering.

### 6.4 Single-face convolution helper — Complete

Added private `RenderSystem::renderDiffuseIrradianceFace`. It rejects source/output aliasing, scopes face rendering, binds the cube source and convolution uniforms, submits the fullscreen mesh, records statistics, and restores matrix/target state on failure.

1. Add a private helper accepting source cubemap, output target, face, and sample count.
2. Enter `CubemapFaceRenderScope`, set shader uniforms, bind source cube sampler, and draw fullscreen geometry.
3. Preserve matrix, target, viewport, scissor, draw/read-buffer, and debug-scope behavior through existing scoped APIs.
4. Record fullscreen render statistics.

**Acceptance:** One output face is generated without GL/renderer state leakage.

### 6.5 Public six-face generation API — Complete

Implemented `RenderSystem::generateDiffuseIrradiance`: it validates configuration before allocation, creates a single-mip candidate, rejects aliasing, synchronously convolves all six faces, and returns only a complete unpublished result.

1. Implement `generateDiffuseIrradiance(...)` through validation, `createIblCubemap`, and six face-helper calls.
2. Release the local candidate automatically on any failure; do not publish partial output.
3. Use conservative/default irradiance resolution and deterministic sample count documented in the API.

**Acceptance:** The API returns a six-face diffuse irradiance cubemap suitable for `PBR_IRRADIANCE_MAP`.

### 6.6 GPU tests — Complete

GPU coverage uses reduced-sample fixtures to verify neutral RGBA16F HDR preservation across all faces, directional response toward a bright `+X` source, deterministic repeated output, and invalid-source rejection without viewport mutation.

1. Build a neutral HDR environment fixture and verify generated irradiance remains neutral with values above 1.0.
2. Build a directional/high-intensity fixture and verify expected diffuse directionality on corresponding output faces.
3. Verify six faces are populated, invalid source/configuration leaves render state unchanged, and repeated runs are deterministic within float tolerance.
4. Add a reduced-sample test configuration for fast test execution while retaining the production default separately.

**Acceptance:** Generated irradiance is stable, neutral-environment preserving, directionally correct, and safe under failure.

## Phase 7 — Specular prefilter generation

### 7.1 Runtime API and settings — Complete

Declared `generatePrefilteredSpecular` and added its pre-allocation validator for loaded floating-point cubemap sources, non-zero settings, and at least two roughness mips. The all-mip implementation follows in Phase 7.5.

1. Add a renderer-owned API:

```cpp
RenderTargetPtr RenderSystem::generatePrefilteredSpecular(
    Texture* environmentCubemap,
    std::string const& generatedName,
    uint32_t faceSize,
    uint32_t mipLevels,
    uint32_t sampleCount = 1024);
```

2. Require a loaded floating-point source cubemap, non-zero name/size/mip/sample values, and at least two mip levels for roughness variation.
3. Output uses `createIblCubemap`; mip zero is roughness `0`, final mip is roughness `1`, and intermediate mips map linearly over `[0, 1]`.
4. Cache lookup/publication remains outside this synchronous candidate-producing API.

**Acceptance:** Invalid prefilter requests fail before allocation/state mutation.

### 7.2 GGX prefilter shader source — Complete

Added `FragmentShaderPrefilteredSpecularTemplate`: deterministic Hammersley/GGX importance samples use the established face convention, PDF-derived cube LOD selection, roughness clamping, and zero-PDF/weight safeguards.

1. Add a renderer-owned fragment shader with `ENVIRONMENT`, `FACE`, `OUTPUT_SIZE`, `ROUGHNESS`, `SOURCE_RESOLUTION`, and `SAMPLE_COUNT` uniforms.
2. Reuse the established face-direction/tangent-basis convention.
3. Add deterministic Hammersley sampling, GGX normal-distribution importance sampling, Smith-compatible view/reflection geometry, and PDF-derived source LOD selection.
4. Guard degenerate PDFs, zero weights, invalid roughness, and NaN output.

**Acceptance:** Shader source implements deterministic roughness-dependent GGX prefiltering without scene-camera dependency.

### 7.3 Core-program lifecycle and validation — Complete

Added stable renderer-owned `__mpp_ibl_prefiltered_specular__` program creation. Phase 7.1 validation rejects invalid floating-point cubemap/settings before rendering, and the face helper derives source resolution from `Texture::getWidth()`.

1. Add/load/destroy a stable `__mpp_ibl_prefiltered_specular__` core program.
2. Add validation for floating-point cubemap source, output settings, supported mip count, and source/output aliasing.
3. Derive/query source face resolution for shader LOD selection.

**Acceptance:** No invalid request begins rendering and no program compiles per generation.

### 7.4 Single face/mip prefilter helper — Complete

Added private `RenderSystem::renderPrefilteredSpecularFace`. It validates source/destination/mip/aliasing, renders through `CubemapFaceRenderScope`, derives source resolution, sets all GGX uniforms, records fullscreen submission, and restores state on failure.

1. Add private helper accepting source cubemap, output target, face, mip level, roughness, source resolution, and sample count.
2. Enter `CubemapFaceRenderScope` at the requested mip, set all shader uniforms, bind cube source, and draw fullscreen geometry.
3. Preserve scoped renderer/GL state and record fullscreen render statistics.

**Acceptance:** An individual face/mip is populated safely and independently.

### 7.5 Public all-mip generation API — Complete

Implemented `RenderSystem::generatePrefilteredSpecular`: it validates before allocation, creates a multi-mip candidate, rejects aliasing, and synchronously renders every face of every mip with `roughness = mip / (mipLevels - 1)`.

1. Implement `generatePrefilteredSpecular(...)` through validation, cubemap creation, and nested mip/face generation.
2. Compute roughness as `mip / (mipLevels - 1)`; ensure mip zero remains a sharp environment reflection.
3. Release candidate on error and never expose a partially generated cubemap to cache/runtime callers.

**Acceptance:** Returns a complete floating-point prefiltered cubemap suitable for `PBR_PREFILTERED_SPECULAR_MAP`.

### 7.6 GPU tests — Complete

Reduced-sample GPU coverage verifies neutral HDR mip-chain initialization/energy, directional `+X` reflection broadening from sharp to rough mip, deterministic repeated output, and invalid-source viewport-state preservation.

1. Use directional HDR cubemap fixtures to verify high-roughness mips blur more than mip zero while retaining values above 1.0.
2. Verify all faces/mips are initialized, finite, and non-black for neutral environments.
3. Verify deterministic repeat output, invalid request state preservation, and roughness-to-mip mapping.
4. Use reduced face size/sample count in tests while keeping production defaults documented separately.

**Acceptance:** Output has valid complete mip chains, monotonic roughness blur, stable HDR values, and safe failure behavior.

## Phase 8 — BRDF LUT ownership

The current `__mpp_tex_pbr_brdf_lut__` is a 1x1 white fallback, not a generated split-sum BRDF integration LUT. HDR IBL therefore needs a renderer-owned generated LUT.

### 8.1 LUT contract and API — Complete

Declared `RenderSystem::getOrCreatePbrBrdfIntegrationLut()` and renderer-held shared ownership. The contract uses a lazily generated linear floating-point 512x512 LUT (RG16F preferred; RGBA16F fallback); the existing 1x1 white resource remains an error/non-IBL fallback.

1. Define a renderer-owned linear floating-point 2D LUT contract (default 512x512 `RG16F`/`RGBA16F`) indexed by `NdotV` and roughness.
2. Add `RenderSystem::getOrCreatePbrBrdfIntegrationLut()` returning a shared core resource; it creates once per renderer/context and returns the same resource thereafter.
3. Keep `__mpp_tex_pbr_brdf_lut__` as a neutral fallback for non-IBL/error paths only.

**Acceptance:** IBL code can request a stable renderer-owned integration LUT without an authored texture resource.

### 8.2 Integration shader and render target — Complete

Added `FragmentShaderPbrBrdfIntegrationTemplate` with deterministic Hammersley split-sum GGX integration. Existing floating-point 2D `RenderTexture` support provides the renderer-owned `RG16F` LUT target; default generation settings are 512x512 and 1024 samples.

1. Add a fullscreen split-sum GGX BRDF integration shader with `NdotV`/roughness output in RG channels.
2. Add a floating-point 2D render-target creation path or equivalent renderer-owned target suitable for the LUT.
3. Use deterministic Hammersley sampling and documented production/test sample counts.

**Acceptance:** Renderer can generate a finite floating-point BRDF LUT without scene resources.

### 8.3 Core lifecycle and generation — Complete

Added `__mpp_ibl_brdf_integration__` core program and lazy `getOrCreatePbrBrdfIntegrationLut()` generation. The LUT is generated once into a 512x512 `RG16F` target, cached by shared ownership for renderer lifetime, and restores render target/viewport/scissor/draw/read state on success or failure.

1. Create/load the renderer-owned program during core initialization.
2. Lazily generate the LUT on first request, preserving target/viewport/scissor/draw/read-buffer state.
3. Retain shared ownership for renderer lifetime; release during normal core-resource teardown.
4. Guard duplicate/re-entrant generation and surface diagnostics on failure.

**Acceptance:** Repeated requests return one completed LUT and never trigger per-frame regeneration.

### 8.4 Runtime environment binding — Complete

`PbrPipelineRuntime` now requests the renderer-owned LUT when `Environment.brdfLut` is absent. A named explicit `Texture2D` remains the advanced override; incompatible overrides produce a diagnostic and fall back to the generated LUT.

1. In HDR IBL runtime creation, bind the renderer-owned LUT unless `Environment.brdfLut` names an explicit valid override.
2. Keep explicit `brdfLut` serialization/parser support as advanced/manual behavior.
3. Report diagnostics when an override is incompatible and use generated LUT fallback.

**Acceptance:** HDR IBL pipelines require no authored BRDF LUT; explicit advanced override remains possible.

### 8.5 GPU tests and documentation — Complete

GPU coverage verifies lazy once-per-renderer reuse, 512x512 dimensions, viewport restoration, and finite non-negative float RG LUT output. The renderer-owned LUT is a linear `RG16F` 512x512 split-sum GGX table generated with 1024 deterministic samples; it survives for renderer lifetime, while a valid explicit `Environment.brdfLut` Texture2D takes precedence.

1. Verify LUT creation once-per-renderer, dimensions/float format, finite values, and state restoration.
2. Validate expected endpoint behavior: low roughness/high `NdotV` remains finite; all texels stay in sensible non-negative ranges.
3. Document LUT resolution, format, sample budget, cache/lifetime behavior, and override precedence.

**Acceptance:** Generated LUT is deterministic, reusable, and documented for HDR IBL.

## Phase 9 — Runtime integration

### 9.1 Cache ownership and keys — Complete

`RenderSystem` now owns `IblEnvironmentCache` and exposes it to runtime integration. The established key carries source path, timestamp validation, resolutions, and preprocessing version; Phase 9.3 builds canonical keys from document declarations.

1. Add an `IblEnvironmentCache` owned by `RenderSystem` so generated resources survive pipeline runtime replacement but die with the renderer/context.
2. Build canonical cache keys from resolved EXR path, source timestamp, HDR/environment/irradiance/prefilter resolutions, and preprocessing version.
3. Expose lookup/store/invalidate operations only through renderer-owned runtime paths.

**Acceptance:** Equivalent HDR environment requests can share generated `ResourcePtr` outputs safely.

### 9.2 HDR source resource resolution — Complete

`PbrPipelineRuntime` now resolves `hdrEquirectangular` relative to the pipeline document, declares a generation-owned linear Texture2D using the configured image loader, loads it, and rejects non-floating-point/non-2D decode results with a source-specific candidate failure.

1. Resolve `Environment.hdrEquirectangular` relative to the pipeline document path.
2. Declare/load a generation-owned linear floating-point Texture2D using the configured image loader.
3. Validate EXR decode/format before cache generation and report source-specific diagnostics.
4. Keep source texture ownership with the pipeline generation until generated cache result owns/reuses its outputs.

**Acceptance:** A valid relative EXR declaration provides a loaded floating-point source texture to generation code.

### 9.3 Cache generation pipeline

1. On cache miss, call `convertEquirectangularToCubemap`, `generateDiffuseIrradiance`, and `generatePrefilteredSpecular` in order.
2. Obtain the renderer-owned BRDF LUT and store all four generated resources in one cache result only after every stage succeeds.
3. Use generation-unique renderer resource names and clean candidates on failure.
4. On cache hit, validate source timestamp and reuse the existing immutable generated result.

**Acceptance:** Cache miss produces a complete environment; cache hit performs no preprocessing renders.

### 9.4 PBR environment binding and fallback

1. When HDR IBL is declared and generation succeeds, bind generated irradiance/prefilter/LUT into `PbrEnvironment`.
2. Preserve explicit authored irradiance/prefilter/BRDF bindings as manual advanced mode when no HDR source is declared.
3. On decode/generation failure, report diagnostics and retain neutral fallback resources so pipeline rendering continues.

**Acceptance:** HDR IBL pipelines light and reflect from EXR without material changes or authored cubemap/LUT files.

### 9.5 Replacement, invalidation, and tests

1. Ensure `PbrPipelineRuntime` and preview replacement retain shared cache results until old scenes/pipelines retire.
2. Invalidate cached source entries when timestamp changes or an explicit preview rebuild requests invalidation.
3. Add runtime tests for cache hit/miss, timestamp invalidation, failure fallback, and generated PBR environment binding.

**Acceptance:** Preview/pipeline rebuilds are safe, deterministic, and do not leak stale IBL resources.

## Phase 10 — PipelineEditor authoring UI

1. Add an **HDR IBL Environment** section under Pipeline Environment.
2. Provide EXR file picker, resolution controls, source preview/status, regenerate, and clear actions.
3. Disable/manual-hide conflicting explicit environment fields while HDR IBL is active.
4. Show cache hit/miss, generation duration, and diagnostic details.

**Acceptance:** Users can select an EXR, rebuild the preview, and see IBL/reflections without hand-authoring cubemaps.

## Phase 11 — Packaging, tests, and documentation

1. Package the EXR source and preserve relative source paths on extraction.
2. Add parser, cache, GPU conversion, irradiance, prefilter, runtime, package, and PipelineEditor smoke tests.
3. Add EXR fixture(s), including HDR values above 1.0.
4. Document colour-space requirements, panorama orientation, preprocessing cost, cache behavior, and manual cubemap fallback.

**Acceptance:** HDR IBL works from authoring through package export/import with repeatable output.

## Risks and decisions

- **EXR codec availability:** Fail with explicit diagnostics rather than silently converting to LDR.
- **Preprocessing cost:** Cache generated resources and expose progress; do not regenerate every frame.
- **Memory:** Default resolutions must be conservative and caps-validated.
- **Orientation:** Establish and test one equirectangular-to-cubemap convention before exposing UI.
- **Fallback:** Retain neutral IBL fallback if source generation fails so scene rendering remains usable.
