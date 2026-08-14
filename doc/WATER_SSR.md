# Water: Screen-Space Reflections with Cubemap Fallback

Water-tagged PBR surfaces reflect the live rendered scene where a screen-space ray march
succeeds, and fall back to the scene's prefiltered IBL cubemap everywhere it does not — off-screen
directions, grazing angles, and ray misses. Animated normal distortion and a roughness-driven
mip blur keep the result reading as water rather than a mirror.

This is a specialization of the existing PBR path, not a parallel one. The fallback is literally
the ordinary ambient-specular fetch: with confidence 0 a water surface shades identically to the
same material without the `Water` feature.

## Authoring a water material

Add a `Water` block to a PbrMaterial's `Surface`. Its presence selects
`PbrMaterialFeature::Water` and the `PBR_SPEC_WATER` shader specialization.

```yaml
Surface:
  baseColourFactor: 0.06 0.12 0.16 0.62
  metallicFactor: 0
  roughnessFactor: 0.08
  alphaMode: BLEND
  Water:
    enabled: true
    distortionScale: 6          # ripple UV tiling multiplier
    distortionStrength: 0.07    # 0 = raw mirror; the debugging setting
    scrollSpeed: 0.02 -0.013    # ripple UV per second
    microRoughness: 0.05        # blur added on top of the material roughness
    ssrMaxDistance: 40          # view-space march length
    ssrSteps: 32                # 1..128; the main cost/quality dial
    ssrThickness: 0.5           # assumed surface thickness, in world units
    edgeFade: 0.1               # UV margin over which a hit fades out
    grazingFallbackStart: 0.35  # nDotV at/above which SSR is fully trusted
    grazingFallbackEnd: 0.1     # nDotV at/below which only the cubemap is used
WaterNormalMap:
  Resource: { ... }             # optional; defaults to a flat normal
WaterDetailNormalMap:
  Resource: { ... }             # optional; second octave, scrolled differently
```

Every field is optional — an empty `Water: {}` block is a clean mirror with default march tuning.
`alphaMode: BLEND` is conventional but not required; an opaque "wet floor" variant works the same
way.

Both ripple maps default to the neutral flat normal, so an untextured water material is a mirror
rather than a build error. With two octaves the distortion beats against itself instead of reading
as one sliding texture.

The parameters reach the shader as `PBR_WATER_*` uniforms. They use the `PBR_` prefix rather than
the bare `WATER_` names because `PbrMaterial`'s contract validation reserves that namespace for
canonical material values; anything outside it must be `PBR_EXT_`.

## Pipeline requirements

SSR reads the shaded opaque scene while drawing into it, which is a read/write hazard against the
live target. Water therefore renders in its own pass, after opaque, sampling a frozen copy. A
pipeline needs three additions (see `resources/shared/pbr/templates/Full.pipeline.yaml` and
`resources/demo-suite/res/PbrPipelineMrt.rendergraph.yaml`):

1. `SceneDepth` gains `sampled` usage, and the scene pass stores it.
2. A new `SceneColourResolved` image — RGBA16F, `colourAttachment,sampled`, with a declared mip
   chain (`mipLevels: 5`, `minFilter: LINEAR_MIPMAP_LINEAR`). The chain is what the roughness blur
   samples; it is regenerated automatically when the copy pass's target is popped.
3. A `SceneColourCopy` fullscreen pass (factory `MPP.SceneColourCopy`) copying `SceneHdr` into it,
   followed by a `WaterScene` pass (factory `MPP.WaterScene`) that samples
   `PBR_SCENE_COLOUR_RESOLVED` and `PBR_SCENE_DEPTH` and draws back over `SceneHdr` with
   `load: load`.

`WaterScene` runs before the bloom chain, so water's bright reflections still feed bloom. **The
bloom chain must read the post-water version of the scene colour.** In a serialized pipeline
document that means `source: SceneHdr.v2`; pointing it at `v1` leaves the water pass's output
unread and the graph compiler culls the pass as dead.

### Why the water pass has no depth attachment

It would be the obvious way to get occlusion, and it does not work here. The render graph
allocates a separate physical target per *image version*, so a pass that both samples `SceneDepth`
and attaches it depth-tests against an uninitialized copy — quite apart from being a sampler
feedback loop. `PBR_SPEC_WATER` therefore discards fragments behind the depth it already samples,
which is the same occlusion result with none of the hazard. The pass declares
`depthTest: false, depthWrite: false`.

An unbound `PBR_SCENE_DEPTH` reads the far plane, so a water material in a pipeline *without* a
water pass never discards by accident: it shades in place during the opaque pass with every march
missing, which is the cubemap-only look. The opaque scene pass only defers water materials when
the graph actually contains an enabled `MPP.WaterScene`.

## How the shader works

Inserted into `mpp/include/mpp/PbrShaders.h` under `#if PBR_SPEC_WATER`:

1. **Occlusion reject** — discard where `gl_FragCoord.z` is behind the sampled scene depth.
2. **Distort the normal** — two scrolling normal-map octaves at different scale and direction,
   blended into the surface normal in tangent space. Only the tangential components are scaled by
   `distortionStrength`, the same convention as `PBR_NORMAL_SCALE`, so strength 0 is exactly the
   undistorted surface.
3. **Ray march** — reflect the view vector about the distorted normal in view space and step along
   it, projecting each step to screen UV and unprojecting the depth buffer there through
   `INVERSE_PROJECTION_MATRIX`. The coarse loop looks only for a *sign change* — the ray going from
   in front of the depth buffer to behind it. Five iterations of binary search then refine the
   crossing, and only then does `ssrThickness` decide whether it was a hit (see below).
4. **Confidence** in `[0,1]` — 0 on a miss; graded by how well the refined hit agrees with the
   depth buffer and how stable that depth is locally; faded near the screen edge over `edgeFade`;
   faded over the `grazingFallbackStart`/`End` range of `nDotV`.
5. **Blend** — `prefiltered = mix(prefiltered, ssr, confidence)`, where `prefiltered` is the
   cubemap sample the ordinary PBR path already computed, using the same distorted reflection
   vector so the two sources agree wherever they are mixed. The Fresnel/BRDF terms downstream are
   untouched, so energy conservation is unchanged.

### Camera data

A scene pass shades through material programs and cannot receive hand-wired `glUniform` calls the
way fullscreen passes do, so the march gets its camera state from a `CameraFrame` UBO at
`binding = 3`, alongside `PbrLights` (1) and `ShadowFrame` (2):

```glsl
layout(std140, binding = 3) uniform CameraFrame
{
    mat4 VIEW_MATRIX;
    mat4 PROJECTION_MATRIX;
    mat4 INVERSE_PROJECTION_MATRIX;
    vec4 VIEWPORT_SIZE;    // width, height, 1/width, 1/height
    vec4 NEAR_FAR_TIME;    // near, far, seconds, unused
};
```

`RenderPipeline` publishes it once per frame via `RenderSystem::setCameraFrame`; `MPP.WaterScene`
republishes it with its own target's dimensions, which need not be the scene viewport. The
projection must be exactly the one the scene was rasterized with — including TAA jitter — because
the march unprojects the depth buffer through it.

## Tuning: cost versus quality

`ssrSteps` is the dial. Cost is linear in it and paid per water fragment, so it scales with how
much of the screen the water covers; water fully off-screen costs nothing beyond the
`SceneColourCopy` blit. The default 32 steps over 40 units is a 1.25-unit step.

`ssrThickness` is how thick the march assumes surfaces are, in world units. It is applied *after*
the binary refinement, not during the coarse stepping, and that ordering is the whole point:

- Landing on a surface and passing behind its silhouette are the *same sign change*. Only the
  refined point separates them — a real hit converges to where the ray meets the surface, so the
  ray and the depth buffer agree there, while a near-miss converges to the silhouette edge with
  the ray still far behind the occluder.
- Testing during the coarse loop cannot work at any value. Below one step's depth advance the
  march tunnels through everything and nothing is ever hit; above it, every ray that passes near an
  occluder is accepted and the occluder is extruded along the ray — which is what made every
  reflected sphere render as a vertical column.

So `ssrThickness` is decoupled from step size. Smaller is tighter and rejects more near-misses;
too small starts rejecting genuine grazing hits, which fall back to the cubemap.

`ssrMaxDistance` should be roughly the distance to the geometry you expect to see reflected.
Raising it without raising `ssrSteps` coarsens the step and costs accuracy, not time.

### Residual silhouette fringe

A faint spiky rim around the edge of a reflected object is inherent to SSR and not fully solvable
here. The depth buffer holds front faces only, so at a silhouette it genuinely cannot distinguish
"the ray landed on this" from "the ray passed just outside it", and neighbouring rays resolve the
ambiguity differently. Three things in the shader keep it small:

- The post-refinement thickness test above, which removes the gross case.
- A local depth-stability check at the hit: a hit landing on a depth discontinuity is faded toward
  the cubemap, since that is exactly where the buffer cannot vouch for itself.
- A per-pixel interleaved-gradient dither of the march start, so the sample lattice does not slide
  coherently across an edge from pixel to pixel.

The depth buffer is point-sampled with `texelFetch` throughout. Filtering depth is meaningless and
actively harmful: across a silhouette a linear tap returns a blend of the occluder and what is
behind it, a depth no surface occupies, and the march then finds a perfect hit somewhere on that
phantom ramp for every ray that passes near an edge.

What remains is softened further by `microRoughness` — a water surface that is not a perfect mirror
blurs the rim along with everything else.

## Debugging

- `distortionStrength: 0` exposes the raw mirror SSR, which makes march errors obvious. It is also
  the regression guard for the blend: with distortion off, the result must match the pre-distortion
  output exactly.
- `grazingFallbackStart: 1.1` forces confidence to 0 everywhere, giving the cubemap-only look for
  direct comparison against the pre-SSR appearance.
- The PipelineEditor material inspector exposes every field under
  **Surface → Water (screen-space reflections)**, with an *Add Water (SSR)* button on materials
  that do not have the block yet.

## Verification

- `runRenderGraphGpuTests` covers the copy/water topology: it allocates, validates against the
  built-in pass factory contracts, executes without GL errors, and keeps `SceneColourResolved`'s
  mip chain across a viewport change.
- `runPbrMaterialSpecializationTests` covers the feature bit, the specialization defines, and the
  structural guarantee that SSR blends into the *existing* prefiltered term rather than a second
  divergent cubemap fetch.
- The DemoSuite package (`resources/demo-suite/packages/workspace.mpppackage`) carries a water
  pool over a dark floor with the sphere grid reflected in it.

## Not covered

Refraction and underwater view, foam, caustics, screen-space shadows on the reflection, planar or
ray-traced reflection alternatives, and reflections of transparent geometry — SSR reads the opaque
scene colour only.
