# Shadow setup

A named shadow domain is shared rendering state. A participating PBR, legacy-forward,
or custom-forward pipeline names the same domain and therefore samples the same depth
target and `ShadowFrame`; an empty `RenderPipelineOptions::shadowDomain` is the
unshadowed compatibility path.

## Directional and point domains

The directional configuration remains a 2D orthographic depth map. A point domain is
finite and omnidirectional: it owns one `Depth24` comparison cubemap, has a position,
range, near plane, and the index of the direct light it shadows.

```cpp
mpp::ShadowOptions shadows;
shadows.enabled = true;
shadows.resolution = 1024;             // pixels per cubemap face
shadows.light.type = mpp::ShadowLightType::Point;
shadows.light.position = {0.0f, 2.0f, 0.0f};
shadows.light.range = 30.0f;
shadows.light.lightIndex = 0;          // matching PBR/direct-light array entry
shadows.nearPlane = 0.25f;
shadows.constantBias = 0.0008f;
shadows.normalBias = 0.0025f;
shadows.filterMode = mpp::ShadowFilterMode::Pcf3x3;
shadows.filterRadiusTexels = 1.0f;
shadows.fadeStartNormalized = 0.9f;

renderSystem->configureShadowDomain("Torch", shadows);
mpp::RenderPipelineOptions options;
options.shadowDomain = "Torch";
```

The receiving direct light **must** be the same point light at `lightIndex`.
Visibility multiplies only that direct-light term, never ambient, emissive, IBL, or
other direct lights. All pipelines that use `"Torch"` share the one cubemap; a
pipeline with an empty name allocates and samples neither target nor frame.

Point domains execute canonical `+X`, `-X`, `+Y`, `-Y`, `+Z`, and `-Z` depth faces
when dirty. Each face uses a 90-degree view and stores radial depth normalized by
`range`. The six passes are logical render-graph passes over face subresources of one
imported cubemap, not six independent maps. See [Render Graph Specification]
(RENDER_GRAPH_SPECIFICATION.md) for graph details.

## Casters, receivers, and tuning

`Flag_CastShadows` is independent of material type. Opaque casters are two-sided for
point domains. `MASK` PBR materials use their alpha cutoff in the point-depth pass;
`BLEND` materials receive but deliberately do not cast. A custom masked material must
provide the generic mask-caster declaration/depth variant; otherwise opt it out rather
than casting a solid silhouette. Colour-only back-face culling does not change the
point-depth policy.

`Hard` makes one comparison. `Pcf3x3` makes a nine-tap tangent-space cubemap
comparison; `filterRadiusTexels` is in face texels and direction offsets preserve
sampling over seams. `fadeStartNormalized * range` begins the fade to fully lit at the
range boundary. Increase normal bias before constant bias to address grazing-angle
acne; too much of either causes peter-panning. Use the smallest range and tightest
near plane that cover the intended volume.

Shadow domains are cached. A clean domain performs no face passes; changes to the
light/options, participating caster state/revision, or explicit
`invalidateShadowDomain()` make it dirty. `getShadowDomainDiagnostics()` reports
whether the current map was rendered or reused and the six-face pass count. Dynamic
providers whose GPU writes are not revisioned must call explicit invalidation at their
application's geometry-commit boundary.

If cubemap-depth support or allocation is unavailable, MPP logs one warning, disables
only that domain, and supplies a neutral frame. Direct lighting continues; do not
replace this fallback with a failed frame or a second shadow implementation.

## Deterministic MPP demonstration and capture

`runRenderGraphGpuTests()` contains the deterministic point-shadow fixture
`GpuTestPointQuality`: it configures a finite point domain, exercises hard/PCF,
bias/radius/fade controls, cache reuse and explicit/light/caster invalidation, volume
selection, mask casting, transparent receive-only policy, and unsupported-cubemap
fallback. Its face-order assertions are the automated six-face inspection.

For a visual capture, use an authored scene with one `castsShadows: true` point light,
axis-oriented opaque boxes on the `+/-X`, `+/-Y`, and `+/-Z` sides, a `MASK` cutout,
and a `BLEND` receiver. Keep the light and camera fixed. Inspect all six cube faces in
RenderDoc and cycle hard/PCF, radius, constant/normal bias, and fade start. The cache
diagnostics must report six face passes after the initial dirty frame and zero after a
stationary frame. Move a caster or light and expect exactly six again.

RenderDoc event names are `Pass: PointShadow [<domain>] Face +X` through `Face -Z`.
A shared domain produces those six events once before participating colour pipelines,
not once per pipeline. Capture a dirty frame and a clean frame to preserve this process
flow evidence.

Spot shadows, cascaded shadows, and area-light shadows are not part of this feature.
