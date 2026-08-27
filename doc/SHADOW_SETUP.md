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

The generic shadow light must match the receiving pipeline's direct light at `shadow.light.lightIndex` (zero by default). DemoSuite uses the same direction for its PBR directional light and `MainDirectionalShadow`.

For an omnidirectional point shadow, select `ShadowLightType::Point` and provide its finite position/range. MPP allocates one shared Depth24 cubemap and executes the canonical `+X`, `-X`, `+Y`, `-Y`, `+Z`, and `-Z` graph face passes:

```cpp
shadows.light.type = mpp::ShadowLightType::Point;
shadows.light.position = { 0.0f, 2.0f, 0.0f };
shadows.light.range = 30.0f;
shadows.light.lightIndex = 0;
shadows.nearPlane = 0.25f;
shadows.constantBias = 0.0008f;
shadows.normalBias = 0.0025f;
shadows.filterMode = mpp::ShadowFilterMode::Pcf3x3;
shadows.filterRadiusTexels = 1.0f;
shadows.fadeStartNormalized = 0.9f;
```

Point casters are opaque and two-sided. Point domains use hard comparison or a nine-tap 3-by-3 tangent-space PCF kernel. Its radius is expressed in cubemap texels; every tap offsets the lookup direction, so cube sampling remains continuous across face edges. Visibility begins fading at `fadeStartNormalized * range` and is fully unshadowed at range. If Depth24 cubemap allocation is unavailable, only that domain is disabled with one warning; direct lighting remains active.

Render the scene through a pipeline that joined the domain:

```cpp
renderSystem->renderScene(scene, camera, {}, "PBR");
```

To share one map between multiple colour pipelines, configure one domain name and assign that exact name to each participating `RenderPipelineOptions::shadowDomain`. Pipelines with an empty domain remain completely unshadowed.

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

For a ModelSpec PBR shader, add the `ShadowFrame`/`SHADOW_MAP` contract to the fragment shader, apply visibility only to the chosen directional direct-light term, and re-export the `.mppmodel`. `demo-suite/resources/res/statue/statue_pbr.frag` is the reference. A custom non-PBR shader must implement the same contract to receive shadows; `FragmentShader3dTemplate` is the legacy reference.

## Configure casters and one-sided receivers

Shadow casting is model-render state, independent of whether the material is PBR:

```cpp
// Default flags are visible and cast shadows.
params->setModelFlags(mpp::ModelRenderParams::Flag_Visible |
                      mpp::ModelRenderParams::Flag_CastShadows);

// A visible marker/receiver that must not cast.
params->setModelFlags(mpp::ModelRenderParams::Flag_Visible);

// A single-plane wall that shows only its front face.
params->setModelFlags(mpp::ModelRenderParams::Flag_Visible |
                      mpp::ModelRenderParams::Flag_CullBackFaces);
```

`Flag_CullBackFaces` affects colour rendering only. Orient a one-sided wall so its front face points toward the intended viewer/receiver side. DemoSuite places the camera inside the inward-facing walls; viewing their exterior intentionally culls them.

## Casting behaviour and limitations

- Visible opaque PBR, legacy, and custom meshes cast into the domain depth pass.
- PBR `BLEND` meshes do not cast shadows.
- PBR `MASK` currently casts as opaque; generic mask-caster metadata and alpha-tested depth rendering are not implemented yet.
- A material can receive only if its shader implements the generic contract.
- A domain supports one directional 2D shadow map or one point-light depth cubemap. Spot lights, cascades, and shadow caching are not implemented.
- Shadow maps use explicit orthographic bounds. Increase `orthoHalfWidth` to cover more scene area at the cost of resolution; set near/far tightly around the shadowed scene.

## Tuning

- **Constant bias:** raises all comparison depths. Increase it to reduce acne; too much causes peter-panning.
- **Normal bias:** adds more bias at grazing angles. Prefer increasing this before using a large constant bias.
- **Filter radius:** PCF radius in shadow-map texels. Larger values soften edges and can increase light leakage.
- **Resolution:** 512 is a fast preview; 1024 is the DemoSuite default; 2048 improves detail at a higher memory/fill cost.

DemoSuite exposes these controls for `MainDirectionalShadow`. The visible statue is PBR; its floor, walls, and rear cube use the legacy adapter, exercising mixed-material shadowing. See [Shadow Validation](SHADOW_VALIDATION.md) for the manual checks and captures.

## Possible improvements

| Improvement | What it achieves | Main trade-off |
|---|---|---|
| Tight light-space fit and texel snapping | Fits the directional orthographic projection to visible casters/receivers and snaps it to depth-map texels, improving effective resolution and reducing shimmer while the camera moves. | More camera/frustum logic; must handle scene changes robustly. |
| Cascaded shadow maps | Uses several directional maps at increasing ranges, preserving nearby detail in large scenes without an enormous single map. | Multiple depth passes/maps and cascade-transition tuning. |
| Alpha-mask caster metadata | Lets foliage, fences, decals, and cut-out PBR materials discard transparent texels in the depth pass instead of casting solid silhouettes. | Requires serialized generic caster metadata and a mask-aware depth shader. |
| Point and spot shadow maps | Adds omnidirectional point-light cube shadows and perspective spot-light shadows. | Point lights require six faces per update; more light/shader management. |
| PCSS/contact hardening | Makes penumbrae widen as receiver distance from the caster increases, approximating finite area lights. | More depth samples, more noise/artifacts, higher GPU cost. |
| Poisson/rotated PCF or temporal filtering | Reduces the visible 3×3 PCF grid and permits higher-quality soft edges. | Can introduce temporal noise/shimmer and needs careful filtering. |
| VSM/EVSM | Enables wide, efficiently filtered soft shadows using moment textures. | Light bleeding, floating-point targets, blur passes, and extra memory. |
| Receiver-plane depth bias | Derives bias from depth derivatives, reducing acne/peter-panning compared with only constant/normal bias. | Sensitive to derivatives and difficult geometry; needs fallback tuning. |
| Shadow-map preview and diagnostics | Displays the depth map, bounds, active caster counts, and receiver capability to make projection/bias problems obvious. | Debug UI and depth-to-display conversion work. |
| Static shadow caching | Reuses a domain map until a caster, light, or relevant transform changes. | Invalidation tracking; animated scenes still update every frame. |
| Per-material receive/cast policies | Supports unshadowed emissive markers, receive-only decals, and explicit custom shader policies without relying solely on model flags. | Extends material contracts and serialization. |
| Resolution/quality scaling | Adapts map size and PCF preset to device performance or screen size. | Variable quality and additional profiling/quality policy. |

The detailed staged roadmap, including deferred point lights, cascades, PCSS, and caching, is in [Generic Soft Texture Shadow Implementation Plan](SOFT_TEXTURE_SHADOW_PLAN.md).
