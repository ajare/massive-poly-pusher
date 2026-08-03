# Soft Texture Shadow Implementation Plan

## Goal

Add opt-in, texture-based soft shadows to the forward PBR pipeline. The first deliverable is one shadowed directional PBR light, rendered to a depth texture and filtered with percentage-closer filtering (PCF). Existing `Default`/legacy forward rendering must not change.

In this document, *soft texture shadows* means filtered shadow-map visibility, not ray-traced shadows, screen-space shadows, or physically based area-light penumbrae. The initial PCF radius is expressed in shadow-map texels and therefore creates a stable, artist-controlled soft edge. Contact-hardening PCSS is a later extension.

## Scope and compatibility contract

### Initial scope

- PBR-only, named `PBR` pipeline path.
- One directional light casts shadows; all existing directional/point PBR lights remain usable as unshadowed lights.
- One 2D depth texture, orthographic light projection, and a configurable 3×3 PCF kernel.
- Opaque and masked PBR meshes cast and receive shadows. `BLEND` meshes receive shadows but do not cast them initially.
- Static and animated transforms are rendered into the map every frame in the first implementation. Shadow caching is deferred.
- DemoSuite visibly demonstrates a statue, a receiving ground plane, moving directional light, filter/bias controls, a map preview, and a shadow on/off comparison.

### Explicitly out of scope

- Changing `Default`, legacy light UBOs, legacy shaders, or the legacy render order.
- Point-light cube shadows, spotlights, cascaded shadow maps, PCSS/contact hardening, VSM/EVSM, screen-space shadows, and shadow caching.
- Refraction/transmission shadowing and correct sorted-transparent shadow casting.
- A general render graph.

## Design decisions

1. **Shadow ownership:** `RenderPipeline` owns shadow targets and frame state, like its HDR scene target. Materials do not own shadow textures.
2. **Depth representation:** use a depth-only `RenderTexture` with `RenderTextureDepthAttachment::DepthTexture`, no colour attachments, and a `GL_DEPTH_COMPONENT24` depth texture initially.
3. **Comparison/filtering:** configure the depth texture for `GL_TEXTURE_COMPARE_REF_TO_TEXTURE`, `GL_LEQUAL`, clamp-to-border with a white border, and linear filtering. The PBR shader uses `sampler2DShadow` and performs a fixed PCF kernel. White outside the light frustum means fully lit.
4. **Frame contract:** add a dedicated shadow frame UBO at binding 2 rather than altering the existing binding-1 PBR light UBO. It contains the light view-projection matrix, bias/filter parameters, map texel size, and enabled state.
5. **Pipeline sampler binding:** add a pipeline-frame sampler override path by sampler name. It is the generalisation of the existing PBR environment override and binds `PBR_DIRECTIONAL_SHADOW_MAP` without requiring every material to declare or serialize a shadow-map resource.
6. **Bias:** expose constant depth bias and normal/slope-scaled bias. Apply both in shadow texture/reference space; do not use a magic shader-only constant.
7. **Raster state:** enable polygon offset for the depth pass and support front-face culling for closed casters, with a no-cull path for double-sided materials. All changed GL state must be restored before the colour pass.

## Proposed runtime API

Keep this API PBR-specific and additive:

```cpp
struct PbrDirectionalShadowOptions
{
    bool enabled{ false };
    size_t resolution{ 2048 };
    float orthoHalfWidth{ 450.0f };
    float nearPlane{ 1.0f };
    float farPlane{ 1800.0f };
    float constantBias{ 0.0008f };
    float normalBias{ 0.0025f };
    float filterRadiusTexels{ 1.0f }; // 1 = 3x3 PCF
};

// Direction is read from the selected PBR directional light.
void RenderPipeline::setDirectionalShadow(PbrDirectionalShadowOptions const& options);
PbrDirectionalShadowOptions const& RenderPipeline::getDirectionalShadow() const;
```

The initial implementation must define which directional light is shadowed. Prefer an explicit `shadowLightIndex` in `PbrDirectionalShadowOptions`, validated against the current PBR-light list. Reject a point light with a clear diagnostic rather than silently producing invalid shadows.

Add a `PbrShadowFrame` CPU representation and a std140 UBO at binding 2:

```text
mat4 LIGHT_VIEW_PROJECTION
vec4 MAP_TEXEL_SIZE_AND_RADIUS  // xy = 1 / map resolution, z = radius, w unused
vec4 BIAS_AND_ENABLED           // x = constant, y = normal, z = enabled, w unused
```

The exact C++ packing must be checked against std140 offsets with `static_assert`s and a shader reflection/diagnostic test.

## Shader contract

Add this sampler to PBR fragment shaders:

```glsl
uniform sampler2DShadow PBR_DIRECTIONAL_SHADOW_MAP;
```

Add world-space position and normal inputs if not already available. The shadow visibility function should:

1. Transform world position by `LIGHT_VIEW_PROJECTION`.
2. Divide by `w`, remap XY/Z from `[-1, 1]` to `[0, 1]`.
3. Return `1.0` if outside XY bounds or beyond the light depth range.
4. Compute `bias = constantBias + normalBias * (1 - max(dot(N, L), 0))`.
5. Sample a 3×3 grid (or fixed Poisson disk) using `sampler2DShadow`; each sample compares `(uv + offset * texelSize * radius, depth - bias)`.
6. Average samples and multiply **only the direct contribution of the selected light** by visibility. Do not darken IBL, ambient, emissive, or other unshadowed lights.

Use a compile-time maximum kernel and branch/loop only over a small validated radius/count. Do not introduce dynamic, unbounded loops in GLSL 3.2. Start with a deterministic 3×3 kernel; optional rotated Poisson samples can follow after visual validation.

Add a matching depth-only shadow vertex shader that writes the light clip-space position. The shadow fragment shader may be empty for opaque meshes. For `MASK` meshes, it must sample `PBR_BASE_COLOUR_MAP`, multiply by `PBR_BASE_COLOUR_FACTOR.a`, and discard using the existing alpha mode/cutoff contract.

## Implementation milestones

### Milestone S1 — Depth target and shadow frame resources

**Outcome:** the PBR pipeline owns a complete, sampleable depth texture and a stable shadow-frame contract.

- [ ] Add `PbrDirectionalShadowOptions`, validation, and `RenderPipeline` storage/accessors.
- [ ] Create a pipeline-owned, lazily allocated depth-only `RenderTexture` at the configured resolution. Verify that the existing zero-colour-attachment path keeps `GL_DRAW_BUFFER`/`GL_READ_BUFFER` set to `GL_NONE`.
- [ ] Extend `RenderTexture` with an explicit depth-texture bind path, for example `bindDepth(unit)`, rather than treating its absent colour attachment as a normal `Texture` attachment.
- [ ] Configure depth sampler parameters: compare mode/function, linear filtering, clamp-to-border, and white border colour. Add the needed depth-sampler options instead of hard-coding them solely in `RenderTexture`.
- [ ] Create/update the binding-2 shadow UBO and expose a diagnostic label for the depth target and UBO.
- [ ] Resize/recreate only when configured shadow resolution changes; window resize must not change resolution unless a future relative-resolution option requests it.
- [ ] Add framebuffer-completeness and GL-error diagnostics with target name, dimensions, and depth format.

**Acceptance:** a 2048² depth-only target is complete, can be bound as a compare texture, survives pipeline resize/teardown, and leaves no live GL objects.

### Milestone S2 — Directional-light depth pass

**Outcome:** visible PBR geometry writes correct light-space depth before the HDR scene pass.

- [ ] Add a `PbrShadowPass` invoked before PBR colour `RenderPass` execution. It owns no scene colour target.
- [ ] Derive a stable directional-light view matrix from the selected light direction and a configurable scene focus point. Use a safe alternative up vector when direction is nearly parallel to world up.
- [ ] Start with explicit, documented orthographic bounds from `PbrDirectionalShadowOptions`; do not silently attempt automatic camera-frustum fitting.
- [ ] Add a depth-only override-program rendering path in `RenderSystem`. It must preserve mesh/submesh render-command ranges, transforms, indexed/non-indexed draws, instancing, visibility flags, and the normal render queue.
- [ ] Do not mutate the material or mesh permanently to render depth. Pass an override depth material/program through the shadow submission path.
- [ ] Render PBR `OPAQUE` casters. Add mask-aware casting before declaring S2 complete. Skip `BLEND` casters initially and emit a one-time diagnostic/debug counter.
- [ ] Set depth test/write, clear depth to one, choose front-face culling plus polygon offset for closed opaque meshes, and restore cull, polygon-offset, viewport, render target, program, blend, and depth-write state afterward.
- [ ] Handle `doubleSided` casters without front-face culling.

**Acceptance:** a depth-map debug view shows the statue and ground/casters from the light view, does not corrupt the following HDR pass, and works for transformed and animated models.

### Milestone S3 — PBR receive shader and hard-shadow baseline

**Outcome:** the statue and receiver use the map to render a correctly projected, bias-controlled hard shadow.

- [ ] Add the binding-2 frame UBO and `PBR_DIRECTIONAL_SHADOW_MAP` sampler to `statue_pbr.frag` and the PBR shader template/markup source.
- [ ] Implement one center comparison first, gated by `enabled`, and apply it only to the selected direct directional-light term.
- [ ] Implement named pipeline-frame sampler overrides and bind the shadow depth texture by sampler name during PBR mesh setup. Preserve material sampler ordering and all existing IBL overrides.
- [ ] Verify that adding the shadow sampler increases the PBR sampler requirement from eight to nine and report hardware-limit failures clearly.
- [ ] Add normal/constant bias UI controls and a shadow enable control in DemoSuite.
- [ ] Validate light projection bounds and the outside-map-is-lit policy.

**Acceptance:** shadowed direct light is reduced only behind the statue; ambient/IBL remains visible; acne and peter-panning can be tuned with exposed bias values.

### Milestone S4 — Soft PCF filtering

**Outcome:** the hard edge becomes a stable, configurable soft texture shadow.

- [ ] Replace the center comparison with a fixed 3×3 PCF kernel using shadow-map texel size and filter radius.
- [ ] Keep kernel offsets in texel units so output is resolution-independent from the artist perspective.
- [ ] Add a small set of quality presets: `Hard` (1 tap), `Soft` (3×3), and `SoftHigh` (Poisson 16 taps, if performance permits). Do not expose arbitrary unbounded kernel sizes.
- [ ] Optionally rotate the Poisson pattern per screen-space interleaved cell only after checking temporal stability; the default must be deterministic.
- [ ] Profile GPU cost at 1024² and 2048² shadow maps, with each preset, against the existing statue demo.

**Acceptance:** the penumbra is visibly softened without obvious grid banding, swimming, light leakage beyond expected PCF limits, or unacceptable frame-time regression at the documented preset.

### Milestone S5 — DemoSuite, validation, and documentation

**Outcome:** the feature is observable, tunable, and regression-tested.

- [ ] Add a visible PBR receiving plane and a directional key light to DemoSuite. Keep the statue as the primary caster/receiver.
- [ ] Add controls for enabled state, shadowed light selection, map resolution, orthographic extent, constant/normal bias, filter preset/radius, depth-map debug preview, and light animation pause.
- [ ] Add a diagnostic status line: target format/resolution, selected light, active sampler count, PCF taps, and whether masked/blended casters were skipped.
- [ ] Extend `doc/PBR_MATERIAL_SETUP.md` and `doc/PBR_MATERIAL_AUTHORING.md` with receive/cast behavior, sampler use, transparency rules, and performance guidance.
- [ ] Add `doc/SHADOW_VALIDATION.md` with screenshot naming, expected images, and an acne/peter-panning troubleshooting table.
- [ ] Capture hard/soft, enabled/disabled, bias-extreme, masked-caster, resize, and pipeline-switch reference images.

**Acceptance:** DemoSuite demonstrates an obvious soft statue shadow on a ground plane, all controls work, docs describe the contract, and the legacy `Default` pipeline remains visually/functionally unchanged.

## Test plan

### Automated/unit coverage where feasible

- Depth-only `RenderTexture` has no colour attachments, selects `GL_NONE` buffers, is framebuffer-complete, and releases/recreates its depth texture.
- Shadow option validation rejects zero resolution, invalid near/far, invalid orthographic extents, unsupported light index, and point-light selection in S1.
- Shadow UBO byte size/offset tests match expected std140 layout.
- Pipeline sampler-override selection correctly replaces only `PBR_DIRECTIONAL_SHADOW_MAP`; surface and IBL sampler bindings remain unchanged.
- Render command classification verifies opaque/masked cast, blend skip, double-sided no-cull, and correct depth-write/state restoration.
- Sampler-count validation reports nine required units for a fully textured shadowed PBR material.

### Manual graphics validation

- Statue casts onto the receiver with the directional light moved through several azimuth/elevation angles.
- Hard/soft presets show the expected edge change.
- Constant and normal bias extremes demonstrate acne versus peter-panning; documented defaults are stable at normal statue scale.
- Shadow bounds clipping is visible and understandable; outside-map pixels are lit rather than sampling edge garbage.
- Alpha-masked holes do not cast solid silhouettes; blended material does not cast in the initial scope.
- PBR environment, tone map, exposure, material factors, and transparent PBR rendering remain functional.
- `Default` renders as before and does not allocate, bind, or sample shadow resources.
- Window resize, repeated pipeline creation/destruction, and switching PBR/Default do not leak GL objects or log framebuffer/shader errors.

## Risks and mitigations

| Risk | Mitigation |
|---|---|
| Acne/peter-panning | Expose constant and normal bias, use polygon offset in depth pass, and capture bias regression scenes. |
| Aliasing/shimmering | Start with fixed bounds and PCF; later add texel-snapped frustum fitting/cascades only after baseline is stable. |
| Shadow pass bypasses normal mesh behavior | Reuse mesh render-command ranges/transforms and add masked/double-sided tests before expanding scope. |
| Texture-unit exhaustion | Count pipeline shadow sampler in the existing dynamic sampler validation; document nine-unit PBR requirement. |
| State leakage into HDR/UI/legacy draws | Centralize shadow-pass state setup/restore and add PBR-to-Default switch validation. |
| PCF performance | Bound presets/taps and profile at documented target resolutions. |
| Point-light expectations | Clearly reject point-light shadow selection initially; plan cube shadows separately. |

## Deferred extensions

After the above accepts, evaluate in this order:

1. camera-frustum fit and texel snapping for the directional orthographic projection;
2. cascaded directional shadow maps for large scenes;
3. point-light cube shadow maps and spotlight perspective maps;
4. PCSS/contact-hardening or VSM/EVSM, with artifact/performance trade-off documentation;
5. shadow caching for static casters/lights;
6. alpha-blended/transmission-specific shadow policy.
