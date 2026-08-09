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

## Phase 4 — Equirectangular HDR to cubemap

1. Add fullscreen/cubemap capture shader to convert longitude-latitude directions to equirectangular UVs.
2. Render all six cubemap faces at source resolution.
3. Use linear floating-point render targets and seam-safe cube sampling conventions.
4. Add GPU readback/orientation tests with a directional HDR fixture.

**Acceptance:** Each cube face samples the correct panorama direction without seams or upside-down orientation.

## Phase 5 — Diffuse irradiance convolution

1. Add irradiance convolution shader/pass for the generated cubemap.
2. Render six low-resolution faces.
3. Use deterministic sample count/sequence and configurable resolution.
4. Validate neutral environments remain neutral and directional environments produce expected diffuse directionality.

**Acceptance:** Generated irradiance cubemap is suitable for `PBR_IRRADIANCE_MAP`.

## Phase 6 — Specular prefilter generation

1. Add GGX importance-sampled prefilter shader/pass.
2. Generate complete mip chain, mapping roughness to mip level.
3. Use a documented fixed sample budget per mip/face.
4. Validate roughness-dependent blur and no NaNs/black faces.

**Acceptance:** Generated prefilter cubemap is suitable for `PBR_PREFILTERED_SPECULAR_MAP`.

## Phase 7 — BRDF LUT ownership

1. Reuse the existing renderer-owned BRDF LUT when compatible.
2. Document its resolution/format and lifetime.
3. Permit an explicit pipeline BRDF LUT only as an advanced override.

**Acceptance:** HDR IBL pipelines need no authored BRDF LUT file.

## Phase 8 — Runtime integration

1. Update `PbrPipelineRuntime` to resolve HDR IBL source declarations through the cache.
2. Bind generated irradiance, prefilter, and LUT into `PbrEnvironment`.
3. Keep neutral fallback behavior on failed generation, while surfacing diagnostics.
4. Ensure pipeline replacement and forced preview rebuild release old generated environments only after active scene references retire.

**Acceptance:** PBR pipelines render HDR diffuse IBL and reflections from EXR without material changes.

## Phase 9 — PipelineEditor authoring UI

1. Add an **HDR IBL Environment** section under Pipeline Environment.
2. Provide EXR file picker, resolution controls, source preview/status, regenerate, and clear actions.
3. Disable/manual-hide conflicting explicit environment fields while HDR IBL is active.
4. Show cache hit/miss, generation duration, and diagnostic details.

**Acceptance:** Users can select an EXR, rebuild the preview, and see IBL/reflections without hand-authoring cubemaps.

## Phase 10 — Packaging, tests, and documentation

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
