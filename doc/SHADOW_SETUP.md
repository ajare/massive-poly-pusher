# Generic Soft Shadow Setup

Soft texture shadows are opt-in per named render pipeline through a shared shadow domain. The domain is generic: PBR, legacy forward, and custom materials can cast into and receive from the same depth map when their pipelines join the same domain.

## Create a domain and join a pipeline

```cpp
mpp::ShadowOptions shadows;
shadows.enabled = true;
shadows.resolution = 1024;
shadows.light.direction = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
shadows.orthoHalfWidth = 450.0f;
shadows.nearPlane = 1.0f;
shadows.farPlane = 1800.0f;
shadows.constantBias = 0.0008f;
shadows.normalBias = 0.0025f;
shadows.filterRadiusTexels = 1.0f;
shadows.filterMode = mpp::ShadowFilterMode::Pcf3x3;

renderSystem->configureShadowDomain("MainDirectionalShadow", shadows);

mpp::RenderPipelineOptions options;
options.mode = mpp::RenderPipelineMode::PbrForward; // or LegacyForward
options.shadowDomain = "MainDirectionalShadow";
renderSystem->getOrCreateRenderPipeline("PBR", options);
```

An empty `RenderPipelineOptions::shadowDomain` is the compatibility default: it allocates no shadow target and binds a disabled shadow frame. Pipelines joined to the same domain share one depth texture and frame data.

The generic shadow direction must match the directional light contribution that a receiving shader shadows. DemoSuite uses the same direction for its PBR directional light and `MainDirectionalShadow`.

## Shader contract

A receiving shader must declare:

```glsl
layout(std140, binding = 2) uniform ShadowFrame
{
    mat4 LIGHT_VIEW_PROJECTION;
    vec4 MAP_TEXEL_SIZE_AND_RADIUS;
    vec4 BIAS_AND_ENABLED;
};
uniform sampler2DShadow SHADOW_MAP;
```

Transform world position by `LIGHT_VIEW_PROJECTION`, remap to `[0, 1]`, and sample `SHADOW_MAP` with the reference depth minus the configured bias. Apply visibility only to the corresponding direct-light contribution; do not shadow IBL, ambient, or emissive terms.

`statue_pbr.frag` and `FragmentShader3dTemplate` are reference adapters. They support `Hard` (one comparison) and `Pcf3x3` (deterministic 3×3 PCF).

`SHADOW_MAP` is pipeline-owned. Do not add it to a ModelSpec texture list. The material system supplies a no-texture fallback outside a shadow domain and the renderer replaces it with the domain depth texture while shadowing is active.

## Casting behaviour and limitations

- Visible opaque PBR, legacy, and custom meshes cast into the domain depth pass.
- PBR `BLEND` meshes do not cast shadows.
- PBR `MASK` currently casts as opaque; generic mask-caster metadata and alpha-tested depth rendering are not implemented yet.
- A material can receive only if its shader implements the generic contract.
- The first implementation supports one directional 2D shadow map per domain. Point lights, spots, cascades, and shadow caching are not implemented.
- Shadow maps use explicit orthographic bounds. Increase `orthoHalfWidth` to cover more scene area at the cost of resolution; set near/far tightly around the shadowed scene.

## Tuning

- **Constant bias:** raises all comparison depths. Increase it to reduce acne; too much causes peter-panning.
- **Normal bias:** adds more bias at grazing angles. Prefer increasing this before using a large constant bias.
- **Filter radius:** PCF radius in shadow-map texels. Larger values soften edges and can increase light leakage.
- **Resolution:** 512 is a fast preview; 1024 is the DemoSuite default; 2048 improves detail at a higher memory/fill cost.

DemoSuite exposes these controls for `MainDirectionalShadow`. The visible statue is PBR; its flat grid receiver uses the legacy adapter, exercising mixed-material shadowing.
