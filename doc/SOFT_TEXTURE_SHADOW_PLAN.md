# Generic Soft Texture Shadow Implementation Plan

## Goal

Add opt-in, texture-based soft shadows to **any forward render pipeline**, including PBR and non-PBR/legacy pipelines. The first deliverable is one directional shadow light rendered to a depth texture and filtered with percentage-closer filtering (PCF).

In this document, *soft texture shadows* means filtered shadow-map visibility, not ray-traced shadows, screen-space shadows, or physically based area-light penumbrae. The first PCF radius is expressed in shadow-map texels, yielding a stable, artist-controlled soft edge. Contact-hardening PCSS is deferred.

## Compatibility and genericity contract

- Shadows belong to a generic per-frame **shadow domain** owned by `RenderSystem`, not to `PbrEnvironment`, `PbrLight`, or a PBR material. A participating `RenderPipeline` references a domain; all participating pipelines sample its same depth map.
- A named pipeline enables shadows only when it explicitly joins a shadow domain. `getOrCreateRenderPipeline("Default")` must remain unshadowed and retain current legacy rendering behaviour.
- A PBR pipeline and a legacy/Phong/custom forward pipeline use the same shadow-domain depth pass, depth texture, frame UBO, sampler binding, bias controls, and PCF implementation.
- Shaders opt in by declaring the generic shadow UBO and `SHADOW_MAP` sampler, then applying the provided visibility function to the direct-light term that their own lighting model chooses. No PBR texture slots or PBR light UBO are required.
- Opaque meshes from every material type cast by default. Masked/custom casters require an explicit generic material shadow-caster contract; PBR alpha-mask support is an adapter to that contract.
- The implementation must not assume that a material has PBR tangent, normal-map, IBL, or metallic-roughness data.

## Initial scope

- One 2D directional-light shadow map per enabled shadow domain, shared by every participating pipeline.
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

### Shadow domains, pipeline membership, and light descriptor

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
    std::string shadowDomain; // empty: no shadows (the compatibility default)
};
```

`RenderSystem` creates/configures a named `ShadowDomain` from `ShadowOptions`; it owns the depth target, UBO, frame stamp, and caster submission. `RenderPipelineOptions::shadowDomain` joins a pipeline to that domain. For example, both `"PBR"` and `"LegacyLit"` can specify `"MainDirectionalShadow"`. A pipeline with an empty domain never allocates, binds, or samples shadow resources.

At the first participating render of a scene each frame, the domain renders **the union of all eligible visible scene casters**, independent of their material type or the colour pipeline that will later draw them. Later participating pipelines reuse the map only if the scene, camera/light inputs, and frame stamp match; otherwise the domain is refreshed. This is required so an opaque PBR statue can cast onto a legacy receiver and a legacy caster can cast onto a PBR receiver.

Applications must keep the generic `ShadowLight::direction` consistent with the directional light that their participating shaders render. Convenience adapters may copy direction from `PbrLight` or a legacy light, but this is application/pipeline setup code, not a renderer dependency.

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

Generalize the current PBR-environment sampler override into pipeline-frame sampler bindings keyed by sampler name. Every pipeline participating in a shadow domain binds that domain's depth texture to `SHADOW_MAP` only when the active program declares that sampler. It must not require a material texture entry, alter material sampler ordering, or bind a shadow texture in a pipeline outside the domain.

PBR environment overrides remain a PBR-specific use of this generic pipeline-frame binding mechanism. The shadow binding is domain-owned and must not be replaced by a material's PBR or legacy texture binding.

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

1. Use a shadow-domain-owned depth-only `RenderTexture` with `RenderTextureDepthAttachment::DepthTexture`, no colour attachments, and `GL_DEPTH_COMPONENT24` initially.
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

**Outcome:** any participating pipeline uses a shared, complete shadow-domain depth texture and generic shadow UBO.

- [x] Add `ShadowLight`, `ShadowOptions`, `ShadowDomain`, and empty `RenderPipelineOptions::shadowDomain`; the empty name remains the default for all existing pipelines.
- [x] Add `RenderSystem` APIs to create/configure/find a named shadow domain and `RenderPipeline` accessors to join/leave one. A domain owns its one canonical option set, so every member necessarily uses compatible options.
- [x] Allocate a lazily loaded depth-only `RenderTexture` per domain at the configured resolution. Recreate it only when that domain's shadow resolution/options change, not merely because the window resizes.
- [x] Add explicit depth-texture binding and compare-sampler configuration to `RenderTexture`/texture parameters.
- [x] Create/update the generic binding-2 `ShadowFrame` UBO with std140 offset/size checks.
- [x] Refactor PBR environment texture replacement into generic named pipeline-frame sampler bindings without changing current PBR output.

**Acceptance:** a test non-PBR pipeline and `PBR` joined to one domain bind the exact same complete depth target/UBO contract; pipelines outside every domain do not allocate or bind shadow resources.

**Implementation note (Milestone S1):** `RenderSystem` now owns named, lazily allocated shadow domains. A participating `RenderPipeline` names its domain through `RenderPipelineOptions::shadowDomain`; an empty name remains a no-op. Each enabled domain creates a depth-only `GL_DEPTH_COMPONENT24` target with linear compare sampling, clamp-to-border, a binding-2 96-byte std140 frame UBO, and an explicit depth-texture bind API. DemoSuite configures a 1024² PBR domain to validate allocation; no depth-caster pass or receiver shader is active until S2/S3. Pipeline sampler overrides are now generic by shader sampler name, preserving the PBR IBL override behaviour.

### Milestone S2 — Generic depth-caster pass

**Outcome:** visible mesh geometry from every material type is submitted once into the shared domain depth map before participating colour passes.

- [x] Add a `ShadowPass` invoked by a shadow domain before its first participating colour pass each frame.
- [x] Build a stable directional view matrix from `ShadowLight::direction` and `focusPoint`, selecting a safe up vector for near-parallel directions.
- [x] Start with explicit documented orthographic bounds; defer automatic camera-frustum fitting.
- [x] Add a depth-only override-program submission path in `RenderSystem`. It retains mesh/submesh ranges, indexed/non-indexed draws, instance counts, transforms, and visibility flags without permanently replacing materials.
- [ ] Add generic `ShadowCasterSpecification` parsing, serialization, and programmatic-material support.
- [x] Render the union of opaque/default casters regardless of whether their colour material is PBR, legacy, or custom; skip PBR `BLEND` casters. Generic mask shader/material binding and disabled-caster metadata remain outstanding.
- [ ] Test non-PBR meshes, PBR meshes, mixed-material scenes, custom material overrides, and double-sided casters.

**Acceptance:** one depth-map preview contains the same transformed PBR and non-PBR casters, and the pass does not corrupt following HDR, LDR, 2D, or UI draws.

**Implementation note (Milestone S2):** an enabled domain now renders a depth-only pass before its participating colour pass. The pass uses an internal position-only shader, directional orthographic projection, front-face culling, polygon offset, and restores target/viewport/depth/blend/cull state afterward. It submits visible opaque PBR, legacy, and custom meshes through their existing render-command ranges; PBR `BLEND` meshes are skipped. The map is updated each render invocation. Generic caster metadata, alpha-mask casting, double-sided culling policy, cross-pipeline frame caching, and a depth-map UI preview remain subsequent work.

### Milestone S3 — Generic receive contract and shader adapters

**Outcome:** any shader that opts in can receive a hard shadow from the generic map.

- [x] Add the shared `ShadowFrame`/`SHADOW_MAP` GLSL helper and a no-shadow fallback path.
- [x] Add shadow-frame sampler binding only for programs that declare `SHADOW_MAP`; existing sampler-limit validation reports the program requirement.
- [x] Integrate the helper into `statue_pbr.frag`, applying visibility only to directional PBR direct-light contributions.
- [x] Integrate the helper into the default legacy/non-PBR forward shader, applying visibility to its first direct-light contribution.
- [x] Leave shaders that do not declare the generic contract unshadowed, even if their pipeline has shadows enabled.
- [x] Verify the fully textured PBR shader’s sampler requirement increases from eight to nine. The default legacy adapter adds one sampler independently.
- [ ] Expose enabled, map bounds, and constant/normal bias controls in DemoSuite.

**Acceptance:** a PBR caster visibly shadows a legacy adapter receiver and a legacy caster visibly shadows a PBR receiver using the same map. An untouched legacy shader and an unshadowed `Default` pipeline retain their prior output.

**Implementation note (Milestone S3):** `SHADOW_MAP` is a pipeline-owned depth comparison sampler, while the binding-2 `ShadowFrame` UBO supplies projection and bias data. The PBR statue and default legacy forward shader now implement a one-tap directional visibility helper; disabled/unjoined pipelines bind a zeroed frame UBO and retain no-shadow output. The PBR map was re-exported after embedding the updated shader. The legacy adapter assumes its first direct light corresponds to the domain's directional shadow light; explicit light mapping, UI controls, PCF, generic masked casters, and manual mixed-material visual validation remain outstanding.

### Milestone S4 — Generic soft PCF filtering

**Outcome:** all adopting shaders use the same stable soft-shadow helper.

- [x] Replace center comparison with fixed 3×3 PCF using `MAP_TEXEL_SIZE_AND_RADIUS`.
- [x] Keep offsets in texel units so artist-facing radius behaves consistently across resolutions.
- [x] Add bounded `Hard` (1 tap) and `Soft` (3×3) modes. `SoftHigh` (fixed 16-tap Poisson) remains optional follow-up; no unbounded runtime loop sizes are exposed.
- [x] Keep the default deterministic; interleaved/rotated Poisson remains deferred pending temporal-stability review.
- [ ] Profile each preset at 1024² and 2048² with both PBR and legacy adapter scenes.

**Acceptance:** the same settings visibly soften PBR and non-PBR shadows without grid banding, instability, light leakage beyond normal PCF limits, or unacceptable documented frame-time regression.

**Implementation note (Milestone S4):** `ShadowFilterMode` selects either the existing one-tap hard comparison or a deterministic 3×3 PCF kernel. The UBO carries filter mode, texel size, and texel-space radius, so PBR and legacy adapters execute the same bounded filter. `Pcf3x3` is the default. Performance profiling and UI controls remain validation/demo work.

### Milestone S5 — DemoSuite, documentation, and validation

**Outcome:** generic behaviour is observable and regressions are caught.

- [ ] Add a visible ground-plane receiver and a directional key light to DemoSuite.
- [ ] Provide mixed PBR/non-PBR caster and receiver demonstrations using the same named shadow domain, `ShadowOptions`, and `ShadowLight` configuration.
- [ ] Add controls for enable state, map resolution, orthographic extent, light direction/focus, constant/normal bias, PCF preset/radius, map preview, and light-animation pause.
- [ ] Add status showing pipeline name, target format/resolution, active sampler count, caster counts by mode, PCF taps, and whether the active program receives shadows.
- [ ] Extend PBR docs with the PBR adapter behaviour, but make `doc/SHADOW_SETUP.md` the generic source of truth for pipeline, material, and shader integration.
- [ ] Add `doc/SHADOW_VALIDATION.md` with screenshot naming and acne/peter-panning troubleshooting.
- [ ] Capture enabled/disabled, hard/soft, PBR/non-PBR, bias-extreme, masked-caster, resize, and pipeline-switch images.

**Acceptance:** the statue/ground scene demonstrates PBR-to-legacy and legacy-to-PBR soft shadow casting through one shared domain, while a default-created legacy pipeline outside that domain remains unchanged.

## Test plan

### Automated coverage where feasible

- An empty shadow-domain name creates no target, UBO, sampler override, or shader binding.
- Two pipelines joined to one domain receive the same depth-texture ID and frame UBO; the domain submits the union of PBR and non-PBR casters exactly once per frame.
- Depth-only targets have no colour attachments, select `GL_NONE` buffers, are complete, and clean up correctly.
- Option validation rejects zero resolution, invalid near/far/bounds, and zero-length directional vectors.
- UBO byte size/offset tests match std140 layout.
- Named pipeline-frame bindings replace only `SHADOW_MAP`; PBR IBL and ordinary material samplers remain unchanged.
- Material caster classification covers default opaque, disabled, explicit mask, PBR mask adapter, blend skip, and double-sided state.
- Program capability detection distinguishes shaders with and without `SHADOW_MAP`/`ShadowFrame`.
- Sampler validation reports the actual per-program requirement, including nine for fully textured shadowed PBR.
- State restoration tests cover depth target, viewport, blend, depth write, culling, polygon offset, and a following legacy/UI draw.

### Manual graphics validation

- A PBR statue casts onto a non-PBR adapter receiver, and a non-PBR object casts onto a PBR receiver, under the same directional shadow light and shared depth-map preview.
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
| PBR-only assumptions leak into core | Keep `ShadowDomain`, `ShadowLight`, UBO, sampler, caster metadata, and GLSL helper independent of PBR types; test PBR-to-legacy and legacy-to-PBR casting from S3. |
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
