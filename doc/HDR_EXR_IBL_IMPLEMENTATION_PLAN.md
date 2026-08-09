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

### 5.6 GPU orientation and HDR tests — In progress

Added a linear RGB32F programmatic panorama fixture and public conversion smoke test that verifies all generated faces retain HDR values above 1.0. The fixture's longitude gradient now verifies the documented `-Z < +X < +Z` centre-face orientation. Seam and invalid-source state tests remain.

1. Add a small directional floating-point equirectangular fixture with distinct colours for cardinal directions and values above 1.0.
2. Convert it and read each cubemap face back, verifying the documented face convention.
3. Test longitudinal seam continuity near `u = 0/1` and verify no vertical inversion.
4. Verify invalid sources fail without changing active framebuffer/viewport/scissor state.

**Acceptance:** The API returns a complete cubemap whose faces sample the documented panorama directions without seams or upside-down orientation.

## Phase 6 — Diffuse irradiance convolution

1. Add irradiance convolution shader/pass for the generated cubemap.
2. Render six low-resolution faces.
3. Use deterministic sample count/sequence and configurable resolution.
4. Validate neutral environments remain neutral and directional environments produce expected diffuse directionality.

**Acceptance:** Generated irradiance cubemap is suitable for `PBR_IRRADIANCE_MAP`.

## Phase 7 — Specular prefilter generation

1. Add GGX importance-sampled prefilter shader/pass.
2. Generate complete mip chain, mapping roughness to mip level.
3. Use a documented fixed sample budget per mip/face.
4. Validate roughness-dependent blur and no NaNs/black faces.

**Acceptance:** Generated prefilter cubemap is suitable for `PBR_PREFILTERED_SPECULAR_MAP`.

## Phase 8 — BRDF LUT ownership

1. Reuse the existing renderer-owned BRDF LUT when compatible.
2. Document its resolution/format and lifetime.
3. Permit an explicit pipeline BRDF LUT only as an advanced override.

**Acceptance:** HDR IBL pipelines need no authored BRDF LUT file.

## Phase 9 — Runtime integration

1. Update `PbrPipelineRuntime` to resolve HDR IBL source declarations through the cache.
2. Bind generated irradiance, prefilter, and LUT into `PbrEnvironment`.
3. Keep neutral fallback behavior on failed generation, while surfacing diagnostics.
4. Ensure pipeline replacement and forced preview rebuild release old generated environments only after active scene references retire.

**Acceptance:** PBR pipelines render HDR diffuse IBL and reflections from EXR without material changes.

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
