# Generic Soft Texture Shadow Implementation Plan

## Goal

Add opt-in, texture-based soft shadows to **any forward render pipeline**, including PBR and non-PBR/legacy pipelines. The first deliverable is one directional shadow light rendered to a depth texture and filtered with percentage-closer filtering (PCF).

In this document, *soft texture shadows* means filtered shadow-map visibility, not ray-traced shadows, screen-space shadows, or physically based area-light penumbrae. The first PCF radius is expressed in shadow-map texels, yielding a stable, artist-controlled soft edge. Contact-hardening PCSS is deferred.

## Compatibility and genericity contract

- Shadows belong to `RenderPipeline`, not `PbrEnvironment`, `PbrLight`, or a PBR material.
- A named pipeline enables shadows only when it explicitly receives shadow options. `getOrCreateRenderPipeline("Default")` must remain unshadowed and retain current legacy rendering behaviour.
- A PBR pipeline and a legacy/Phong/custom forward pipeline use the same depth pass, depth texture, frame UBO, sampler binding, bias controls, and PCF implementation.
- Shaders opt in by declaring the generic shadow UBO and `SHADOW_MAP` sampler, then applying the provided visibility function to the direct-light term that their own lighting model chooses. No PBR texture slots or PBR light UBO are required.
- Opaque meshes from every material type cast by default. Masked/custom casters require an explicit generic material shadow-caster contract; PBR alpha-mask support is an adapter to that contract.
- The implementation must not assume that a material has PBR tangent, normal-map, IBL, or metallic-roughness data.

## Initial scope

- One 2D directional-light shadow map per enabled pipeline.
- Orthographic light projection and configurable 1-tap/3×3 PCF filtering.
- Opaque casters and receivers in any shadow-enabled pipeline.
- PBR `MASK` casters supported through the generic mask-caster adapter; `BLEND` meshes receive but do not cast shadows initially.
- Render the map every frame initially; caching is deferred.
- DemoSuite demonstrates the same scene through PBR and a non-PBR pipeline, with a statue/opaque geometry casting onto a receiving ground plane.

## Explicitly out of scope

- Point-light cube shadows, spotlights, cascades, PCSS, VSM/EVSM, screen-space shadows, and general render graphs.
- Changing a pipeline that has not opted in.
- Correct sorted-transparent, refractive, or transmission shadow casting.
- Automatic understanding of arbitrary material alpha logic without an explicit shadow-caster declaration.

## Generic runtime contracts

### Pipeline options and light descriptor

Define shadow settings independently from either `PbrLight` or the legacy two-light API:

```cpp
enum class ShadowLightType { Directional };

struct ShadowLight
{
    ShadowLightType type{ ShadowLightType::Directional };
    glm::vec3 direction{ 0.0f, -1.0f, 0.0f };
    glm::vec3 focusPoint{ 0.0f };
};

struct ShadowOptions
{
    bool enabled{ false };
    ShadowLight light;
    size_t resolution{ 2048 };
    float orthoHalfWidth{ 450.0f };
    float nearPlane{ 1.0f };
    float farPlane{ 1800.0f };
    float constantBias{ 0.0008f };
    float normalBias{ 0.0025f };
    float filterRadiusTexels{ 1.0f }; // 1 = 3x3 PCF
};

struct RenderPipelineOptions
{
    // Existing options...
    std::shared_ptr<ShadowOptions> shadows; // null/disabled is the default
};
```

`RenderPipeline` exposes `setShadowOptions()`/`getShadowOptions()`. This allows a `PBR`, `Default`, or application-defined named pipeline to independently opt in. It also avoids falsely coupling a shadow map to a particular PBR-light index.

Applications must keep the generic `ShadowLight::direction` consistent with the directional light that their shader renders. Convenience adapters may copy direction from `PbrLight` or a legacy light, but this is application/pipeline setup code, not a renderer dependency.

### Frame UBO and sampler

Create a generic `ShadowFrame` UBO at binding 2. Binding 1 remains the PBR-light UBO; legacy/custom shaders do not need to use it.

```text
mat4 LIGHT_VIEW_PROJECTION
vec4 MAP_TEXEL_SIZE_AND_RADIUS  // xy = 1 / map resolution, z = radius
vec4 BIAS_AND_ENABLED           // x = constant, y = normal, z = enabled
```

Use these generic shader declarations:

```glsl
layout(std140, binding = 2) uniform ShadowFrame { /* fields above */ };
uniform sampler2DShadow SHADOW_MAP;
```

Add a shared GLSL include/template containing the world-position-to-shadow comparison and PCF function. PBR, legacy Phong, and custom shaders include the same helper. Each shader multiplies the visibility into only the light contribution represented by `ShadowLight`; ambient, emissive, and unrelated lights remain unshadowed.

### Pipeline-frame sampler binding

Generalize the current PBR-environment sampler override into pipeline-frame sampler bindings keyed by sampler name. The shadow-enabled pipeline binds its depth texture to `SHADOW_MAP` only when the active program declares that sampler. It must not require a material texture entry, alter material sampler ordering, or bind a shadow texture in an unshadowed pipeline.

PBR environment overrides remain a PBR-specific use of this generic pipeline-frame binding mechanism.

### Generic material shadow-caster contract

Add additive material metadata rather than inferring all behaviour from `MaterialSpecification::PbrSurface`:

```cpp
enum class ShadowCasterMode { Default, Disabled, Opaque, Mask };

struct ShadowCasterSpecification
{
    ShadowCasterMode mode{ ShadowCasterMode::Default };
    std::string alphaSampler; // required by Mask; e.g. PBR_BASE_COLOUR_MAP
    float alphaCutoff{ 0.5f };
};
```

`Default` means opaque casting for opaque material content, while `Disabled` opts out. `Mask` tells the generic shadow-depth path which existing named material sampler supplies alpha and what cutoff to use. A PBR material with `alphaMode == MASK` automatically supplies `PBR_BASE_COLOUR_MAP` and its PBR cutoff unless an explicit generic override is authored. `BLEND` defaults to `Disabled` for initial casting.

The shadow depth path must never assume PBR-only uniforms. Its mask variant uses the generic alpha sampler/cutoff metadata. Materials with procedural alpha need either a declared shadow-depth shader override or must opt out until that shader is supplied.

## Depth target and raster-state design

1. Use a pipeline-owned depth-only `RenderTexture` with `RenderTextureDepthAttachment::DepthTexture`, no colour attachments, and `GL_DEPTH_COMPONENT24` initially.
2. Retain the existing `GL_DRAW_BUFFER`/`GL_READ_BUFFER = GL_NONE` setup for zero-colour targets and assert framebuffer completeness.
3. Extend `RenderTexture` with an explicit `bindDepth(unit)` path; its inherited colour-texture bind path cannot bind a depth-only target that has no colour attachment.
4. Configure the depth texture for `GL_TEXTURE_COMPARE_REF_TO_TEXTURE`, `GL_LEQUAL`, linear filtering, clamp-to-border, and a white border. A comparison outside the map is therefore lit.
5. During the depth pass, set depth test/write and clear depth to one. Use front-face culling plus polygon offset for closed opaque casters; render double-sided/masked cases with the appropriate no-cull policy. Restore target, viewport, program, blend, depth-write, cull, polygon-offset, and draw/read-buffer state before the colour pass.

## Shader behaviour

The generic helper must:

1. transform a world position by `LIGHT_VIEW_PROJECTION`;
2. divide by `w`, remap clip coordinates to `[0, 1]`;
3. return `1.0` outside XY bounds or beyond the light depth range;
4. calculate `constantBias + normalBias * (1 - max(dot(N, L), 0))`;
5. sample `SHADOW_MAP` at the reference depth minus bias; and
6. average a bounded PCF kernel.

Start with one center comparison to validate projection and bias, then implement a fixed 3×3 texel-space kernel. The helper accepts a normal and light direction from its caller; a simple legacy shader can supply its interpolated world normal, while a PBR shader supplies its final normal-mapped world normal. Do not require tangents or PBR material uniforms.

## Implementation milestones

### Milestone S1 — Generic resource foundation

**Outcome:** any opted-in pipeline owns a complete, bindable depth texture and generic shadow UBO.

- [ ] Add `ShadowLight`, `ShadowOptions`, and nullable `RenderPipelineOptions::shadows`; disabled/null remains the default for all existing pipelines.
- [ ] Add `RenderPipeline` shadow option accessors, validation, diagnostics, and lifecycle ownership.
- [ ] Allocate a lazily loaded depth-only `RenderTexture` at the configured resolution. Recreate it only when shadow resolution/options change, not merely because the window resizes.
- [ ] Add explicit depth-texture binding and compare-sampler configuration to `RenderTexture`/texture parameters.
- [ ] Create/update the generic binding-2 `ShadowFrame` UBO with std140 offset/size checks.
- [ ] Refactor PBR environment texture replacement into generic named pipeline-frame sampler bindings without changing current PBR output.

**Acceptance:** enabling shadows on a test non-PBR pipeline and on `PBR` allocates the same complete depth target/UBO contract; pipelines with no options do not allocate or bind shadow resources.

### Milestone S2 — Generic depth-caster pass

**Outcome:** visible mesh geometry from any shadow-enabled pipeline writes light-space depth before its colour pass.

- [ ] Add a `ShadowPass` invoked before ordinary render passes only when pipeline shadows are enabled.
- [ ] Build a stable directional view matrix from `ShadowLight::direction` and `focusPoint`, selecting a safe up vector for near-parallel directions.
- [ ] Start with explicit documented orthographic bounds; defer automatic camera-frustum fitting.
- [ ] Add a depth-only override-program submission path in `RenderSystem`. It must retain mesh/submesh ranges, indexed/non-indexed draws, instancing, transforms, and visibility flags without permanently replacing materials.
- [ ] Add generic `ShadowCasterSpecification` parsing, serialization, and programmatic-material support.
- [ ] Render opaque/default casters; implement generic mask shader/material binding; skip blend/disabled casters with diagnostics.
- [ ] Test non-PBR meshes, PBR meshes, custom material overrides, and double-sided casters.

**Acceptance:** a depth-map preview shows the same transformed opaque casters for PBR and non-PBR pipelines, and the pass does not corrupt following HDR, LDR, 2D, or UI draws.

### Milestone S3 — Generic receive contract and shader adapters

**Outcome:** any shader that opts in can receive a hard shadow from the generic map.

- [ ] Add the shared `ShadowFrame`/`SHADOW_MAP` GLSL helper and a no-shadow fallback path.
- [ ] Add shadow-frame sampler binding only for programs that declare `SHADOW_MAP`; report sampler-limit failures with the pipeline/program name.
- [ ] Integrate the helper into `statue_pbr.frag`, applying visibility only to the configured directional PBR direct-light contribution.
- [ ] Integrate the helper into one legacy/non-PBR forward shader (for example the DemoSuite lighting material), applying visibility to its equivalent directional direct-light term.
- [ ] Leave shaders that do not declare the generic contract unshadowed, even if their pipeline has shadows enabled; show this clearly in diagnostics.
- [ ] Verify the fully textured PBR shader’s sampler requirement increases from eight to nine. Verify the legacy adapter’s sampler count independently.
- [ ] Expose enabled, map bounds, and constant/normal bias controls in DemoSuite.

**Acceptance:** PBR and legacy adapter shaders receive the same projected hard shadow, while an untouched legacy shader and an unshadowed `Default` pipeline retain their prior output.

### Milestone S4 — Generic soft PCF filtering

**Outcome:** all adopting shaders use the same stable soft-shadow helper.

- [ ] Replace center comparison with fixed 3×3 PCF using `MAP_TEXEL_SIZE_AND_RADIUS`.
- [ ] Keep offsets in texel units so artist-facing radius behaves consistently across resolutions.
- [ ] Add bounded quality presets: `Hard` (1 tap), `Soft` (3×3), and optional `SoftHigh` (fixed 16-tap Poisson). Do not expose unbounded runtime loop sizes.
- [ ] Keep the default deterministic; consider interleaved/rotated Poisson only after temporal-stability review.
- [ ] Profile each preset at 1024² and 2048² with both PBR and legacy adapter scenes.

**Acceptance:** the same settings visibly soften PBR and non-PBR shadows without grid banding, instability, light leakage beyond normal PCF limits, or unacceptable documented frame-time regression.

### Milestone S5 — DemoSuite, documentation, and validation

**Outcome:** generic behaviour is observable and regressions are caught.

- [ ] Add a visible ground-plane receiver and a directional key light to DemoSuite.
- [ ] Provide PBR and non-PBR/legacy adapter demonstrations using the same `ShadowOptions`/`ShadowLight` configuration.
- [ ] Add controls for enable state, map resolution, orthographic extent, light direction/focus, constant/normal bias, PCF preset/radius, map preview, and light-animation pause.
- [ ] Add status showing pipeline name, target format/resolution, active sampler count, caster counts by mode, PCF taps, and whether the active program receives shadows.
- [ ] Extend PBR docs with the PBR adapter behaviour, but make `doc/SHADOW_SETUP.md` the generic source of truth for pipeline, material, and shader integration.
- [ ] Add `doc/SHADOW_VALIDATION.md` with screenshot naming and acne/peter-panning troubleshooting.
- [ ] Capture enabled/disabled, hard/soft, PBR/non-PBR, bias-extreme, masked-caster, resize, and pipeline-switch images.

**Acceptance:** the statue/ground scene demonstrates the same soft-shadow system through PBR and non-PBR pipelines, while a default-created legacy pipeline remains unchanged.

## Test plan

### Automated coverage where feasible

- Disabled/null shadow options create no target, UBO, sampler override, or shader binding.
- Depth-only targets have no colour attachments, select `GL_NONE` buffers, are complete, and clean up correctly.
- Option validation rejects zero resolution, invalid near/far/bounds, and zero-length directional vectors.
- UBO byte size/offset tests match std140 layout.
- Named pipeline-frame bindings replace only `SHADOW_MAP`; PBR IBL and ordinary material samplers remain unchanged.
- Material caster classification covers default opaque, disabled, explicit mask, PBR mask adapter, blend skip, and double-sided state.
- Program capability detection distinguishes shaders with and without `SHADOW_MAP`/`ShadowFrame`.
- Sampler validation reports the actual per-program requirement, including nine for fully textured shadowed PBR.
- State restoration tests cover depth target, viewport, blend, depth write, culling, polygon offset, and a following legacy/UI draw.

### Manual graphics validation

- PBR statue and non-PBR adapter object cast onto the same receiver under the same directional shadow light.
- Hard/soft presets change edge softness as expected.
- Bias extremes visibly demonstrate acne versus peter-panning; defaults are stable at statue scale.
- Outside-map pixels are lit rather than sampling edge garbage.
- Alpha-masked holes do not cast solid silhouettes; initial blend casters are reported as skipped.
- PBR IBL, exposure, tone mapping, material factors, and PBR transparency remain functional.
- A `Default` pipeline created without `ShadowOptions` remains byte-for-byte/state-equivalent in its intended legacy behaviour.
- Resize, repeated pipeline creation/destruction, and PBR/default switching do not leak GL objects or log framebuffer/shader errors.

## Risks and mitigations

| Risk | Mitigation |
|---|---|
| PBR-only assumptions leak into core | Keep `ShadowLight`, UBO, sampler, caster metadata, and GLSL helper independent of PBR types; test a legacy adapter from S3. |
| Existing legacy output changes unexpectedly | Null/disabled options are no-op; bind nothing unless both pipeline and shader opt in. |
| Acne/peter-panning | Expose constant/normal bias, use polygon offset, and capture bias regression scenes. |
| Aliasing/shimmering | Start with fixed orthographic bounds and PCF; add texel-snapped fitting/cascades only after baseline stability. |
| Arbitrary alpha materials cannot cast correctly | Generic explicit mask metadata/shadow-depth-program override; opt out otherwise. |
| Texture-unit exhaustion | Count pipeline-frame samplers with material samplers and report the program/pipeline requiring too many. |
| State leakage | Centralize shadow pass state setup/restore and validate a subsequent non-PBR/default draw. |
| Point-light expectations | Reject unsupported shadow light types initially; add cube maps later. |

## Deferred extensions

1. camera-frustum fitting and texel snapping for directional projections;
2. cascaded directional maps;
3. spot and point-light shadows;
4. PCSS/contact hardening or VSM/EVSM;
5. static shadow caching;
6. alpha-blended/transmission shadow policy;
7. shader include/variant tooling so custom materials can opt in with less boilerplate.
