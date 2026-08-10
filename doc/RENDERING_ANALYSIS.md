# Rendering Analysis: PBR, Render Graph, and Pipeline Editor

A code review of the PBR shading path, the render graph (declaration, allocation, execution),
the anti-aliasing output chain, and the PipelineEditor authoring/serialization round trip.

Every item is written to be actionable: it states **where**, **what is wrong or missing**,
**what the observable consequence is**, and **a concrete change**. Items are grouped by kind and
tagged with a severity/effort estimate.

| Tag | Meaning |
|---|---|
| **Bug** | Produces incorrect output, data loss, or undefined behaviour. |
| **Non-standard** | Works, but diverges from the conventional/expected implementation in a way that will surprise contributors or asset authors. |
| **Perf** | Correct but wasteful; measurable CPU or GPU cost. |
| **Extension** | A designed seam that is currently unpopulated. |

Line references are to the state of the tree at the time of review (branch `master`, after
`86256df Merge HDR EXR IBL support`).

---

## Contents

1. [PBR shading and IBL](#1-pbr-shading-and-ibl)
2. [Lights and shadows](#2-lights-and-shadows)
3. [Colour management](#3-colour-management)
4. [Render graph: declaration and allocation](#4-render-graph-declaration-and-allocation)
5. [Render graph: execution](#5-render-graph-execution)
6. [Named outputs and the anti-aliasing chain](#6-named-outputs-and-the-anti-aliasing-chain)
7. [PipelineEditor and serialization](#7-pipelineeditor-and-serialization)
8. [Optimisation opportunities](#8-optimisation-opportunities)
9. [Extension points](#9-extension-points)
10. [Suggested verification work](#10-suggested-verification-work)

---

## 1. PBR shading and IBL

### 1.1 Prefiltered specular LOD is hard-coded to a 5-mip chain — **Bug, high** — ✅ FIXED

> **Resolved.** The shader now scales roughness by a renderer-supplied
> `PBR_PREFILTERED_MAX_LOD` uniform. See [§1.1 resolution](#11-resolution) below.
> Items 3 (capping the generated chain) and 4 (warning on a single-level authored
> map) from the fix list remain open as follow-ups.

`mpp/include/mpp/PbrShaders.h:302`

```glsl
vec3 prefiltered = textureLod(@Texture(PBR_PREFILTERED_SPECULAR_MAP), reflection, roughness * 4.0).rgb;
```

The `4.0` assumes the prefiltered cubemap has exactly 5 mip levels. The generator derives the mip
count from the requested resolution instead — `mpp-resource-parsers/src/PbrPipelineRuntime.cpp:39`:

```cpp
uint32_t mipLevels = 1;
for (auto dimension = key.prefilterResolution; dimension > 1; dimension >>= 1) ++mipLevels;
```

…and `RenderSystem::generatePrefilteredSpecular` (`mpp/src/RenderSystem.cpp:1705`) assigns
`roughness = mip / (mipLevels - 1)`.

At the default `prefilterResolution = 128` this is **8 mips**, so mip *m* holds roughness *m/7*.
The shader asks for mip `roughness * 4`, so a fully rough surface (`roughness = 1.0`) samples mip 4,
which was prefiltered for roughness `4/7 ≈ 0.57`. **Rough materials are systematically too sharp and
too bright**, and the error scales with `prefilterResolution` — changing that authoring value
silently changes the appearance of every rough material.

The manually-authored path is worse: `resources/shared/pbr/templates/Full.pipeline.xml` binds a
plain mipmapped cubemap (`Preview.Environment`) as `prefilteredSpecular`, which has no GGX
prefiltering at all and ~10 mips.

**Fix**

1. Add a uniform to the PBR fragment contract, e.g.
   `@@Uniform(float PBR_PREFILTERED_MAX_LOD);` and use
   `textureLod(..., reflection, roughness * PBR_PREFILTERED_MAX_LOD)`.
2. Set it from the active environment. `RenderSystem::setActivePbrEnvironment`
   (`mpp/src/RenderSystem.cpp:2897`) already has the resource; expose
   `RenderTexture::getMipLevels()` for the prefiltered map and push `mipLevels - 1`.
   Route it through `mActivePipelineSamplerOverrides`' sibling mechanism (a
   `mActivePipelineUniformOverrides` map applied in `setupRenderMeshInstance`) so custom PBR
   programs get it for free.
3. Because the LUT is now data-driven, also **cap the generated prefilter chain**. Prefiltering
   below 8×8 is wasted work and its output is meaningless; clamp to
   `min(mipLevels, 6)` and record the cap in `IblEnvironmentCacheKey` so the cache stays coherent.
4. Add a runtime warning when a *manually authored* `prefilteredSpecular` cubemap is bound whose
   mip count is 1 — that configuration cannot produce roughness-varying reflections at all.

#### 1.1 Resolution

Items 1 and 2 are implemented; items 3 and 4 remain open.

- `PbrShaders.h` declares `@@Uniform(float PBR_PREFILTERED_MAX_LOD)` unconditionally and the IBL
  fetch is now `textureLod(..., roughness * PBR_PREFILTERED_MAX_LOD)`.
- `Texture::getMipLevels()` is new and virtual: the base implementation derives the reachable chain
  length from the texture's dimensions clamped to `[lodBaseLevel, lodMaxLevel]` (returning 1 when
  `useMipmaps` is false), and `RenderTexture` overrides it with its explicitly allocated
  `mMipLevels`. Authored cubemaps and generated IBL targets therefore both report correctly.
- `RenderSystem::setupRenderMeshInstance` captures whichever texture is finally bound to
  `PBR_PREFILTERED_SPECULAR_MAP` — material-owned map, pipeline override, or neutral fallback — and
  uploads `mipLevels - 1`. It is resolved at the binding site rather than from the active
  `PbrEnvironment` precisely so the three cases cannot disagree. The upload is gated on the same
  program/material-change condition as `Material::setUniforms`, so it costs nothing per draw.
- `PbrMaterial::createImpl` lists the uniform in `allCoreUniforms` and `allowedCoreUniforms` so it
  is renderer-owned — never authored, never serialized, rejected as an instance override — but it
  is **optional**, not required. A program that omits it is reported through `warnMessage` naming
  the material.

  > **Correction.** It was initially made *required*, and that broke DemoSuite. Shader source is
  > baked into binary `.mppmodel` assets: `statue.mppmodel` embeds `statue_pbr.frag`, so editing
  > the `.frag` on disk changes nothing until the model is rebuilt with `model-convert`. Requiring
  > the uniform therefore turned a shading fix into an asset migration and made every pre-existing
  > model fail to load. The first verification pass missed this because `PipelineEditor --validate`
  > never loads `.mppmodel` materials — only DemoSuite does. **Any change to the required PBR
  > uniform or sampler contract invalidates every existing binary model**; treat that as a hard
  > constraint when extending the contract (see [§9.1](#91-new-material-features)).

  `statue_pbr.frag` is still updated in source, but `statue.mppmodel` has not been regenerated, so
  the statue currently loads with the warning and keeps the old fixed-mip behaviour until someone
  runs `demo-suite/resources/res/statue/build_mppmodel.bat`.
- `runPbrMaterialSpecializationTests` gained a context-free guard asserting the built-in shader
  declares the uniform and consumes it inside the prefiltered specular fetch.

The 1×1 neutral fallback cubemap reports one level, so the multiplier collapses to zero and the
fallback path is unchanged.

### 1.2 The generated environment cubemap has no mip chain, defeating the prefilter's LOD trick — **Bug, high** — ✅ FIXED

> **Resolved.** The environment cubemap is now built with a full chain and populated
> once every face exists. See [§1.2 resolution](#12-resolution) below.



`mpp/src/RenderSystem.cpp:1729` / `mpp/include/mpp/RenderSystem.h:439`

```cpp
RenderTargetPtr convertEquirectangularToCubemap(Texture* hdrEquirectangular,
    std::string const& generatedName, uint32_t faceSize, uint32_t mipLevels = 1);
```

`PbrPipelineRuntime` calls it with the default (`mipLevels = 1`), and the implementation only ever
renders face mip 0 (the comment at `RenderSystem.cpp:1733` acknowledges this).

The prefilter shader (`mpp/include/mpp/DefaultShaders.h:328-331`) computes a solid-angle-based
source LOD specifically to avoid sampling one bright texel per GGX sample:

```glsl
float lod = roughness <= 0.00001 ? 0.0 : max(0.0, 0.5 * log2(sampleSolidAngle / texelSolidAngle));
sum += textureLod(@Texture(ENVIRONMENT), light, lod).rgb * nDotL;
```

With a single-level source, `textureLod` clamps to level 0 and the computation is discarded. On a
high-dynamic-range EXR (sun disc, bright windows) this reintroduces exactly the firefly/blotch
aliasing the LOD is designed to remove, especially at 64–1024 samples.

**Fix**

- Create the environment cubemap with a full chain:
  `mipLevels = floor(log2(faceSize)) + 1`, render face mip 0, then call
  `RenderTexture::generateMipMaps()` (or `glGenerateMipmap(GL_TEXTURE_CUBE_MAP)`) once before
  handing it to `generateDiffuseIrradiance` / `generatePrefilteredSpecular`.
- `createIblCubemap` (`RenderSystem.cpp:1686`) already sets
  `minFilter = LINEAR_MIPMAP_LINEAR` when `mipLevels > 1` and `params.useMipmaps = false`; set
  `useMipmaps = true` for this target so `generateMipMaps()` is not a no-op, or call
  `glGenerateMipmap` explicitly from `convertEquirectangularToCubemap`.
- Add a GPU test in `RenderGraphGpuTests.cpp` asserting that mip *n* of the converted panorama is
  non-zero for a non-black source.

#### 1.2 Resolution

- `PbrPipelineRuntime` derives a full chain from `environmentResolution` and passes it, instead of
  taking the `mipLevels = 1` default.
- `convertEquirectangularToCubemap` populates the chain **after** all six faces are rendered.
  Doing it per face would regenerate five times from incomplete data, so
  `RenderTexture::generateMipMaps` gained a `force` parameter: IBL cubemaps deliberately keep
  `params.useMipmaps = false` so that popping each face's render target does *not* auto-generate,
  and the conversion generates once explicitly at the end.
- `validatePrefilteredSpecularSource` now warns when its source has no chain, so a manually
  authored single-mip cubemap reports rather than silently aliasing.
- The GPU test asserts the chain is **populated, not merely allocated**: `glGenerateMipmap`'s
  successive box filter makes the 1×1 level the mean of level zero, so it compares against the
  source face's own mean rather than a hardcoded constant. Verified load-bearing by disabling the
  generation and confirming the failure.

### 1.2b The BRDF integration LUT is never rasterized and is entirely black — **Bug, critical** — ✅ FIXED

Found while building the test for §1.3, by reading the LUT back rather than trusting it: every
texel of `__mpp_ibl_brdf_integration_lut__` was `(0, 0)`.

The shared fullscreen quad (`RenderSystem.cpp:1161-1166`) is authored in **window pixel
coordinates**, and `VertexShaderFullscreenTemplate` (`DefaultShaders.h:140-152`) converts to NDC by
dividing by `HALF_WINDOW_SIZE`:

```glsl
vec2 centredPos = vec2(transVertex.x - @HalfWindowSize.x, transVertex.y - @HalfWindowSize.y);
gl_Position = vec4(centredPos / @HalfWindowSize, 0, 1);
```

Every other offscreen pass therefore does two things together — scales the quad model matrix to the
target size, and sets `HALF_WINDOW_SIZE` to half that size. Compare
`renderEquirectangularCubemapFace` (`RenderSystem.cpp:1672-1678`), which does both.
`getOrCreatePbrBrdfIntegrationLut` did **neither**. `HALF_WINDOW_SIZE` stayed at its default zero,
so `gl_Position` was a division by zero, the quad never rasterized, and the render target kept its
cleared contents.

Consequence: `specular = prefiltered * (fresnel * brdf.x + brdf.y)` evaluated to **exactly zero**
wherever this LUT was bound — the entire image-based specular term was missing, for every PBR
material using a generated IBL environment. This is strictly more severe than §1.1 and §1.2, both
of which only affected how roughness mapped onto a chain that was then multiplied by zero anyway.

It survived because the only assertion on the LUT checked that every value was finite and
non-negative, which all-zeros satisfies.

#### 1.2b Resolution

`RenderSystem.cpp` now applies the same scale/uniform pair as every other offscreen pass:

```cpp
scaleTransform2d(glm::vec2(512.0f / (float)getWindowWidth(), 512.0f / (float)getWindowHeight()));
...
GL_CHECK(glUniform2f(program->getHalfWindowSizeId(), 256.0f, 256.0f));
```

The GPU test now pins the content analytically instead of merely sanity-checking it. At
`roughness = 0` the GGX lobe collapses onto the normal and the scale term reduces in closed form to
the Schlick complement `1 - (1 - nDotV)^5`, so the bottom row must run from ~0 at grazing incidence
to ~1 head-on. An unrasterized quad fails this immediately.

**Audit result:** every other caller of `mFullscreenQuad` in the tree was checked by pairing up
`getModelCameraProjectionMatrixId` against `getHalfWindowSizeId` — prefiltered specular, diffuse
irradiance, equirectangular conversion, bloom extract/blur/combine, TAA, FXAA, SSAA Lanczos, the
diagnostic blit and the environment debug cube all set both. The only unpaired
`getModelCameraProjectionMatrixId` left is the shadow depth program, which uses a 3D vertex shader
with no `HALF_WINDOW_SIZE`. So the LUT was the sole offender.

The pairing is nonetheless implicit and repeated at fourteen sites. A helper taking the target size
and setting both would remove the whole class of bug — see §9.7.

### 1.3 The BRDF integration LUT is sampled with `GL_REPEAT` wrapping — **Bug, high** — ✅ FIXED

`mpp/src/SamplerParams.cpp:18` defaults `wrap` to `GL_REPEAT`, and
`RenderSystem::getOrCreatePbrBrdfIntegrationLut` (`mpp/src/RenderSystem.cpp:1741`) never overrides it:

```cpp
RenderTextureOptions options;
options.colourInternalFormat = GL_RG16F;
options.params.minFilter = GL_LINEAR;
options.params.magFilter = GL_LINEAR;
options.params.useMipmaps = false;   // wrap left at GL_REPEAT
```

The shader samples `texture(PBR_BRDF_LUT, vec2(nDotV, roughness))`
(`PbrShaders.h:303`). At `nDotV → 1.0` — i.e. **surfaces facing the camera, the most common case** —
`u = 1.0` maps to texel coordinate 511.5 on a 512-wide LUT, so bilinear filtering blends texel 511
(head-on) with texel 0 (grazing incidence, where the Fresnel scale/bias values are radically
different) at 50/50. The same happens along the roughness axis at `roughness = 1.0`.

The symptom is a subtle but persistent specular error at normal incidence, and a visible ring or
rim on spheres/curved geometry as `nDotV` crosses ~1.

**Fix**

```cpp
options.params.wrap = GL_CLAMP_TO_EDGE;
```

Consider also changing the `SamplerParams` default for **render textures** to `GL_CLAMP_TO_EDGE`;
`GL_REPEAT` is essentially never the right default for an offscreen buffer, and the graph
descriptors already have to opt into clamping explicitly everywhere (see
`RenderPipeline.cpp:322`, and every `<wrap>CLAMP_TO_EDGE</wrap>` in the pipeline templates).

#### 1.3 Resolution

`getOrCreatePbrBrdfIntegrationLut` now sets `options.params.wrap = GL_CLAMP_TO_EDGE`. All the IBL
cubemaps were checked at the same time and already clamp via `createIblCubemap`
(`RenderSystem.cpp:1701`), so the LUT was the only IBL texture left on the `GL_REPEAT` default.

The GPU test queries `GL_TEXTURE_WRAP_S/T` back off the texture, but first measures how far apart
the two edge columns actually are and fails if that gap is under 0.25 — otherwise a future change
that flattened the LUT would leave the wrap assertion passing while asserting nothing. Verified
load-bearing by reverting the fix: *"BRDF integration LUT is not clamped (wrap s/t 10497/10497)"*.

The broader `SamplerParams` default change was **not** made — it would alter every existing render
texture in the engine, which is beyond this fix and wants its own decision.

### 1.4 Seamless cubemap filtering is never enabled — **Bug, high** — ✅ FIXED

`glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS)` does not appear anywhere in the tree.

The irradiance cubemap defaults to **32×32 per face** and the prefiltered chain reaches 2×2/1×1.
Without seamless filtering, hardware bilinear taps clamp inside each face independently, so every
cube edge produces a visible discontinuity in the ambient diffuse term and in rough reflections.
This is the single most visible IBL artifact at these resolutions.

**Fix**

Enable it once during context setup in `RenderSystem` initialisation. It is core since GL 3.2 and
has no downside for the cubemaps this engine uses (all of them want seamless filtering). If a
per-texture opt-out is ever needed, `ARB_seamless_cubemap_per_texture` provides
`GL_TEXTURE_CUBE_MAP_SEAMLESS` as a texture parameter.

#### 1.4 Resolution

Enabled once in `RenderSystem::setDefaultState` (`RenderSystem.cpp:1290`), which runs a single time
during initialisation, after `mCaps` is populated. Guarded on `glVersionMajor/Minor >= 3.2` with a
warning on older contexts rather than an unconditional `glEnable`, so a downlevel context degrades
to the old seamed behaviour instead of tripping `GL_CHECK`.

**On the test.** The GPU suite asserts `glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS)` twice: once at
the start of the IBL block, and again after the environment/irradiance/prefilter/LUT passes have
run. The second check is the interesting one — those passes save and restore a good deal of GL
state, and a restore path that clobbered the flag would silently reintroduce the seams.

This is deliberately a **state assertion, not a visual one**. What the flag changes is how the
hardware fetches across a face boundary, which is OpenGL's contract rather than this engine's
behaviour, and a pixel-level assertion on it would be measuring the driver. Verified load-bearing
by stubbing out the `glEnable`: *"seamless cubemap filtering is not enabled, so every cube edge will
show a bilinear seam"*.

Note this interacts with §1.2b/§1.3: with the BRDF LUT black, IBL specular contributed nothing at
all, so the seams this fixes were only visible in the ambient diffuse term. They are now visible in
reflections too, which raises the practical value of this change.

### 1.5 The diffuse irradiance convolution double-counts the cosine term — **Bug, medium** — ✅ FIXED

`mpp/include/mpp/DefaultShaders.h:249-271`

The sampler is cosine-weighted (`cosTheta = sqrt(1 - xi.x)` gives pdf `cosθ/π`), but the estimator
then re-applies the cosine and normalises by its sum:

```glsl
float nDotL = max(dot(normal, light), 0.0);
sum += texture(@Texture(ENVIRONMENT), light).rgb * nDotL;
weight += nDotL;
...
@Out(vec4 COLOUR) = vec4(sum / max(weight, 0.00001), 1.0);
```

For cosine-distributed directions the Monte Carlo estimate of `E/π` (which is what the PBR shader
consumes at `PbrShaders.h:299-300`) is simply the **unweighted mean** of the radiance samples:

```
E = (1/N) Σ L(lᵢ)·cosθᵢ / pdf(lᵢ) = (π/N) Σ L(lᵢ)   →   E/π = (1/N) Σ L(lᵢ)
```

The current form computes a cosine-weighted average, i.e. effectively a `cos²` lobe. A constant
environment is unaffected (which is why the existing GPU tests pass), but any directional
environment produces an irradiance map that is too tightly concentrated around the normal —
ambient diffuse lighting is over-contrasted and the overall ambient level is slightly off.

**Fix**

```glsl
sum += texture(@Texture(ENVIRONMENT), light).rgb;
...
@Out(vec4 COLOUR) = vec4(sum / float(samples), 1.0);
```

Note this applies **only** to the irradiance pass. The prefiltered specular pass'
`Σ L·NdotL / Σ NdotL` (`DefaultShaders.h:331-335`) is the standard split-sum approximation and
should be left alone.

Strengthen the GPU test at `mpp/src/RenderGraphGpuTests.cpp:143` (which currently only compares a
directional source against itself) to assert against an analytically-known value — a single-face
white/black cubemap has a closed-form irradiance.

#### 1.5 Resolution

Applied as written. The prefiltered specular pass was left alone, as noted above.

**On the test.** The suggestion of a closed-form single-face irradiance was not taken directly: the
projected solid angle of a square cube face reduces to `∫∫ du dv / (1+u²+v²)²`, which has no
pleasant closed form and would have had to enter the test as an unexplainable magic constant.

Instead the test carries an **independent numerical oracle** — a deterministic 256×256
uniform-hemisphere quadrature of `E/π` over a single lit face, sharing no code and no sampling
scheme with the shader. It integrates the same physical quantity by a different method, so it
cannot inherit the shader's error. The environment is 16×16 per face so that the bilinear ramp
along the lit face's border stays narrow against the oracle's hard-edged cone.

Measured against a `+X = 8.0` environment at the `(2,2)` texel of a 4×4 output face:

| | value | error vs oracle |
|---|---|---|
| Oracle (CPU quadrature) | 4.179 | — |
| Corrected convolution | 4.152 | **0.6%** |
| `cos²` form (the bug) | 5.086 | **21.7%** |

The tolerance is set at 6% — an order of magnitude above the observed error, and a factor of three
below the bug. Verified load-bearing by restoring the old estimator, which reports both numbers:
*"got 5.085938, expected 4.178953"*.

Note the existing constant-environment assertion was **kept**, not replaced. It is worth keeping
precisely because it is insensitive to this class of error: it pins the absolute level, while the
oracle pins the directional distribution.

**Not affected:** `IblEnvironmentCache` is in-memory only (`IblEnvironmentCache.h:37`), so there are
no persisted irradiance maps carrying the old convention.

### 1.6 The neutral BRDF LUT fallback is white — **Bug, low**

`mpp/src/RenderSystem.cpp:939`

```cpp
addPbrIblFallback("__mpp_tex_pbr_brdf_lut__", TextureTarget::Texture2D, 255, 255, 255);
```

The shader reads `.rg` as `(scale, bias)`: `specular = prefiltered * (fresnel * brdf.x + brdf.y)`.
A white texel means `bias = 1.0`, which adds a full unit of unconditional specular energy. It is
currently masked because the prefiltered fallback is black, but any configuration that supplies a
prefiltered cubemap while falling back on the LUT (for example a partially-authored environment
plus a failed `getOrCreatePbrBrdfIntegrationLut`) will bloom out.

**Fix** — make it `(255, 0, 0)` so the neutral value is `scale = 1, bias = 0`.

### 1.7 Roughness/metallic specialization defaults are surprising — **Non-standard, documentation**

`mpp/src/PbrMaterialFeatures.cpp:30-31`

```cpp
if (surface.metallicFactor  > 0.0f || hasMap(textures, "PBR_METALLIC_MAP"))  enable(Metallic);
if (surface.roughnessFactor > 0.0f || hasMap(textures, "PBR_ROUGHNESS_MAP")) enable(Roughness);
```

A material authored with `roughnessFactor = 0` compiles a shader containing
`const float roughness = 0.04;` (`PbrShaders.h:244`). This is *arithmetically consistent* with the
`clamp(..., 0.04, 1.0)` floor used elsewhere, so it is not a bug — but nothing states it, and a
reader will reasonably expect the glTF default of 1.0. Similarly, `MetallicRoughnessMap` is silently
dropped when both factors are zero (`PbrMaterialFeatures.cpp:34-36`), so a bound MR texture can
vanish from the shader with no diagnostic.

**Fix** — document both rules in `doc/PBR_SHADER_SPECIALIZATION.md`, and emit an authoring
diagnostic when a `PBR_METALLIC_ROUGHNESS_MAP` is declared but specialized away.

### 1.8 The roughness floor of 0.04 is applied to roughness, not to `α` — **Non-standard, low**

`PbrShaders.h:222`, `:237`, `:239`, `:241` all use `clamp(..., 0.04, 1.0)`.

Most engines clamp perceptual roughness to ~0.045 (UE4/Filament) or clamp `α = roughness²` to a
small epsilon. `0.04` here yields `α ≈ 0.0016`, `α² ≈ 2.6e-6`, which is within `float` range but
close enough to the `max(..., 0.000001)` guards in `distributionGgx` that highlight intensity
becomes guard-dependent rather than physically determined. Not currently producing artifacts;
worth a note if specular highlights ever look clipped on near-mirror materials.

---

## 2. Lights and shadows

### 2.1 A point light with a zero direction vector produces NaN — **Bug, medium**

`mpp/include/mpp/PbrShaders.h:269-274`

```glsl
float isPoint = light.directionType.w;
vec3 directionalDirection = normalize(-light.directionType.xyz);
vec3 lightDirection = mix(directionalDirection, pointDirection, isPoint);
```

`PbrLight::direction` defaults to `(0, -1, 0)` (`mpp/include/mpp/PbrLight.h:20`) so the common case
is safe, but nothing prevents a scene from authoring a point light with `direction = (0,0,0)`.
`normalize(vec3(0))` is NaN, and `mix(NaN, p, 1.0)` evaluates as `NaN * 0.0 + p * 1.0` = NaN on
every implementation — the whole pixel goes black or white and the artifact propagates through
bloom.

**Fix** — either branch instead of `mix`:

```glsl
vec3 lightDirection = isPoint > 0.5 ? pointDirection : normalize(-light.directionType.xyz + vec3(0.0, -1e-6, 0.0));
```

…or validate in `RenderSystem::setPbrLights` (`mpp/src/RenderSystem.cpp:2598`) and substitute a
default direction for degenerate vectors. Validation at upload is preferable — it can produce a
diagnostic instead of a silent substitution.

### 2.2 Point-light attenuation has a hard range cutoff — **Non-standard, medium**

`PbrShaders.h:275-279`

```glsl
float attenuation = mix(1.0, 1.0 / (distanceToLight * distanceToLight), isPoint);
if (isPoint > 0.5 && light.positionRange.w > 0.0 && distanceToLight > light.positionRange.w)
    attenuation = 0.0;
```

Inverse-square falloff plus a binary cutoff at `range` produces a **visible hard circle** wherever
the light still has non-negligible intensity at its range boundary — the classic artifact that
windowed falloff exists to fix. It also pops as objects or the light move.

**Fix** — use the standard windowed inverse-square (Karis / glTF `KHR_lights_punctual`):

```glsl
float d2 = distanceToLight * distanceToLight;
float window = 1.0;
if (light.positionRange.w > 0.0)
{
    float ratio = d2 / (light.positionRange.w * light.positionRange.w);
    window = clamp(1.0 - ratio * ratio, 0.0, 1.0);
    window *= window;
}
float attenuation = mix(1.0, window / max(d2, 0.0001), isPoint);
```

This is a pure shader change; the UBO layout is unchanged.

### 2.3 One shadow map is applied to every directional light — **Bug, medium**

`PbrShaders.h:291`

```glsl
float shadow = isPoint > 0.5 ? 1.0 : directionalShadowVisibility(@In(WORLD_POSITION), normal, lightDirection);
```

`directionalShadowVisibility` uses the single global `LIGHT_VIEW_PROJECTION` from the `ShadowFrame`
UBO (`PbrShaders.h:98-103`). With two or more directional lights in
`AMBIENT_AND_COUNT.a`, *all* of them are shadowed by the same map — geometry lit by the second
light is darkened along the first light's shadow silhouette.

Worse, the shadow light is configured **independently of the light list**:
`ShadowOptions::light.direction` (`RenderSystem::configureShadowDomain`,
`mpp/src/RenderSystem.cpp:2688`) has no relationship to any entry in `setPbrLights`. In the
PipelineEditor preview these are wired to agree
(`pipeline-editor/src/Main.cpp:1445-1455` uses `previewScene->getShadowLightDirection()`), but
nothing enforces it, and a scene that changes its directional light without updating the shadow
domain gets shadows cast from the wrong direction with no diagnostic.

**Fix (incremental)**

1. Short term: add a shadow-caster index to the `ShadowFrame` UBO (`vec4 BIAS_AND_ENABLED` has a
   free `.w` component) and apply `directionalShadowVisibility` only when `i == casterIndex`.
2. Medium term: promote the shadow domain to an array of `{ lightViewProjection, texelSize, bias }`
   entries indexed by light, matching the `LIGHTS[8]` array. `ShadowFrameData`
   (`RenderSystem.cpp:56-121`) already has static asserts on its std140 layout, so extending it is
   mechanical.
3. Add a validation warning when `ShadowOptions::light.direction` is not (approximately) parallel to
   any directional light in the scene.

### 2.4 The shadow projection is not fitted or texel-snapped — **Non-standard, medium**

`RenderSystem::renderShadowDomain`, `mpp/src/RenderSystem.cpp:2757-2765`

```cpp
float lightDistance = (domain.options.farPlane - domain.options.nearPlane) * 0.5f;
glm::mat4 lightView = glm::lookAt(focusPoint - direction * lightDistance, focusPoint, up);
glm::mat4 lightProjection = glm::ortho(-extent, extent, -extent, extent, nearPlane, farPlane);
```

Three consequences:

- **Depth precision is wasted.** The eye is placed at the midpoint of `[near, far]` from the focus
  point, so with the defaults the near plane sits far in front of any geometry. Fitting the
  ortho box to the visible casters' bounds would recover several bits of depth.
- **No texel snapping.** Because the light matrix is derived from a continuously-varying focus
  point (`previewScene->camera.target` in the editor), shadow-map texels do not land on stable world
  positions. Shadow edges crawl/shimmer whenever the camera target moves.
- **Single cascade only.** `orthoHalfWidth` is a single authored number, so shadow resolution is a
  global trade-off between coverage and quality.

**Fix**

For snapping, quantise the light-space origin to whole texels before building the projection:

```cpp
float texelWorldSize = (2.0f * extent) / (float)domain.options.resolution;
glm::vec3 lightSpaceFocus = glm::vec3(lightView * glm::vec4(focusPoint, 1.0f));
lightSpaceFocus.x = std::floor(lightSpaceFocus.x / texelWorldSize) * texelWorldSize;
lightSpaceFocus.y = std::floor(lightSpaceFocus.y / texelWorldSize) * texelWorldSize;
// rebuild lightView so that focusPoint maps to the snapped position
```

Cascades are a larger change; see [§9.3](#93-shadows).

### 2.5 Triple biasing of the shadow pass — **Non-standard, low**

`RenderSystem.cpp:2793-2794` enables `glCullFace(GL_FRONT)` **and** `glPolygonOffset(2.0f, 4.0f)`,
while the shader additionally applies `constantBias + normalBias * (1 - NdotL)`
(`PbrShaders.h:170`). Front-face culling alone is normally used *instead of* depth bias (it is the
peter-panning trade), so the three mechanisms compound.

The visible effect is exaggerated peter-panning (contact shadows detaching from their casters) and
a much larger tuning space than necessary for `ShadowOptions::constantBias`/`normalBias`.

**Fix** — pick one primary mechanism. Recommended: drop `glPolygonOffset`, keep front-face culling
for closed geometry, and keep the small shader-side normal-offset bias for open/thin geometry.
Then re-tune the defaults in `ShadowOptions` and re-run `doc/SHADOW_VALIDATION.md`.

### 2.6 `doubleSided` never affects rasterizer culling — **Bug, medium-high** — ✅ FIXED

`PbrMaterialSpecification::PbrSurface::doubleSided` reaches the shader
(`PbrShaders.h:212-215`, which flips the normal for back faces) and reaches the feature bitset
(`PbrMaterialFeatures.cpp:43`), but **nothing ever disables `GL_CULL_FACE` for it**.

Culling is driven exclusively by `MeshInstance::mCullBackFaces`, which comes from a per-model flag:
`mpp/src/ModelInstance.cpp:142`

```cpp
mi->cullBackFaces((rp->flags & ModelRenderParams::Flag_CullBackFaces) != 0);
```

…and is consumed at `mpp/src/RenderSystem.cpp:3910`. A glTF material imported through
`GltfPbrMaterialLoader` (`mpp-resource-parsers/src/GltfPbrMaterialLoader.cpp:89`) with
`"doubleSided": true` therefore still gets single-sided rasterization, and the `PBR_DOUBLE_SIDED`
shader branch is dead code. Foliage, cloth, and thin-shell assets render with holes.

**Fix**

In `RenderPass::render` (`mpp/src/RenderPass.cpp:70-84`) — which already inspects the material to
set `blend`/`sortTransparent` — also drive culling:

```cpp
auto pbr = dynamic_cast<PbrMaterial*>(material);
if (pbr && pbr->getSurface().doubleSided) meshInstance->cullBackFaces(false);
```

Add a `PbrMaterialTests` case asserting the resulting `MeshInstance` state, and note in
`doc/PBR_MATERIAL_AUTHORING.md` that material `doubleSided` now overrides the model-level flag.

#### 2.6 Resolution

Fixed in `RenderPass::render`, as suggested, but the decision was **extracted into a pure function**
rather than written inline:

```cpp
PbrForwardMeshClassification classifyPbrForwardMesh(bool pbrShadingModel, bool transparent,
                                                    bool doubleSided, bool modelCullBackFaces);
```

Two reasons. First, the suggested `dynamic_cast<PbrMaterial*>` runs per mesh per frame in the scene
loop, which §8.6/§8.7 are already unhappy about; a `Material::isDoubleSided()` virtual (defaulted to
`false`, overridden in `PbrMaterial`) costs nothing and keeps `RenderPass.cpp` free of the PBR
header. Second, and mainly, the precedence rule is the part worth testing, and as plain values it
tests without a GL context.

`meshInstance->cullBackFaces()` is read back as the function's input because `setParams` has already
applied the model flag by that point, so the override is expressed as data rather than as a mutation
ordering that a later edit could silently reorder.

**Precedence.** The material wins over the model flag rather than combining with it — a surface with
no meaningful back face cannot be rasterized single-sided whatever the model asked for. This is now
stated in `doc/PBR_MATERIAL_AUTHORING.md` §3, since it is a behaviour change for any asset that set
both.

**On the test.** Six cases in `runPbrMaterialSpecializationTests` (context-free, so it runs in
DemoSuite *and* `PipelineEditor --validate`): the override itself, a single-sided material retaining
the model flag, a double-sided material not *enabling* culling the model never asked for, a non-PBR
material being left alone, and the two blend classifications that share the function — those last
guard against the culling change perturbing alpha behaviour. Verified load-bearing by dropping the
override term: *"a double-sided PBR material did not override the model's back-face culling flag"*.

**Coverage limit, stated plainly:** this tests the decision, not the wiring. Deleting the
`classifyPbrForwardMesh` call from `RenderPass::render` would not fail any test. Closing that would
need a scene-level fixture with a real model and a flow-snapshot assertion on
`RenderBatchSubmission::cullBackFaces`, which is worth doing if this area is touched again — see
§10.

### 2.7 Blended PBR geometry uses non-separate alpha blending and per-object sorting — **Non-standard, low**

`mpp/src/RenderSystem.cpp:3919-3922`

```cpp
GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
GL_CHECK(glDepthMask(GL_FALSE));
```

- Destination alpha accumulates as `src.a·src.a + (1-src.a)·dst.a`, which is wrong for any target
  whose alpha is later consumed (the anti-aliasing chain explicitly preserves centre alpha through
  FXAA, `DefaultShaders.h:556`). Use `glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
  GL_ONE, GL_ONE_MINUS_SRC_ALPHA)`.
- Transparent sorting (`RenderSystem.cpp:4020-4027`) is per mesh-instance origin. Intersecting or
  large transparent meshes will sort incorrectly. This is an accepted forward-rendering limitation,
  but it should be stated in `doc/PBR_MATERIAL_AUTHORING.md`.

---

## 3. Colour management

### 3.1 `GL_FRAMEBUFFER_SRGB` is never enabled, so `SRGB8_ALPHA8` graph images do not encode — **Bug, medium**

`GraphImageFormat::Srgb8Alpha8` is a first-class authorable format
(`mpp/include/mpp/RenderGraph.h:26`), maps to `GL_SRGB8_ALPHA8`
(`mpp/src/RenderGraphTargets.cpp:44`), and is explicitly permitted as an FXAA output format
(`mpp/src/PbrPipelineDocument.cpp:102`). But `glEnable(GL_FRAMEBUFFER_SRGB)` appears nowhere.

In core OpenGL, writing to an sRGB-encoded colour attachment performs linear→sRGB conversion **only
when `GL_FRAMEBUFFER_SRGB` is enabled**. As written, a shader's linear output is stored raw and then
decoded as sRGB on read — a double-darkening of roughly `x^2.2`.

**Fix** — choose one and document it:

- **(a) Support it properly.** Enable `GL_FRAMEBUFFER_SRGB` when the bound target's attachment
  format is sRGB, and disable it otherwise. The graph executor already knows the target
  (`RenderGraphExecutor.cpp:398-406`), so this fits naturally in `GraphRasterStateScope` or in
  `RenderTarget::activate`. Note that with (a) the tone-map pass must stop applying
  `pow(colour, 1/GAMMA)` when writing to an sRGB target, or the encoding happens twice.
- **(b) Remove the format.** Reject `Srgb8Alpha8` in `RenderGraph::createImage` with a diagnostic
  pointing at the manual gamma in `FragmentShaderToneMapTemplate`.

(a) is the better long-term answer: hardware sRGB encoding is filtered correctly during
blending/resolve, whereas manual `pow()` is not.

### 3.2 The output chain resamples in gamma space — **Non-standard, medium**

Ordering in `RenderPipeline::renderGraphForward` / `RenderOutputProcessor::present`:

1. Graph runs, `ToneMapPresentation` writes `pow(tonemapped, 1/gamma)` into the output image
   (`DefaultShaders.h:472`).
2. `RenderOutputProcessor::present` (`mpp/src/RenderOutputProcessor.cpp:83`) then runs
   TAA → SSAA Lanczos → FXAA on that gamma-encoded image.

TAA and FXAA in gamma space are correct and conventional (FXAA's luma weights assume it). But the
**separable Lanczos SSAA downsample runs in gamma space**, which is not — resampling a non-linear
signal darkens high-contrast edges (the classic "gamma-incorrect downsample" error). At
`ssaa=2x`/`4x` this is visible on bright geometry against dark backgrounds.

**Fix** — either linearise inside `FragmentShaderSsaaLanczosTemplate`
(`DefaultShaders.h:601`) before weighting and re-encode after:

```glsl
sum += pow(texelFetch(...), vec4(vec3(2.2), 1.0)) * weight;   // alpha stays linear
...
@Out(vec4 COLOUR) = pow(sum / total, vec4(vec3(1.0/2.2), 1.0));
```

…or, better, re-order the chain so the downsample happens before tone mapping. The latter is a
bigger change because the output plan currently derives its formats from the graph output image
(`RenderOutputProcessor.cpp:60-62`).

### 3.3 No dither on the 8-bit presentation write — **Extension, low**

`FragmentShaderToneMapTemplate` writes directly to an 8-bit target. Smooth HDR gradients (sky,
bloom falloff, ambient-only regions) will band. A single line of ordered/blue-noise dither before
quantisation removes it essentially for free.

---

## 4. Render graph: declaration and allocation

### 4.1 `buildAllocationPlan` can alias a transient image onto a non-transient allocation — **Bug, high (latent)** — ✅ FIXED

`mpp/src/RenderGraph.cpp:710-737`

```cpp
uint32_t allocation = UINT32_MAX;
if (lifetime.desc.transient)                                // only the *candidate* is checked
{
    for (uint32_t candidate = 0; candidate < allocationMembers.size(); ++candidate)
    {
        auto const& representative = plan.allocatedImages[allocationMembers[candidate].front()];
        if (!aliasCompatible(representative, lifetime)) continue;
        ...
        if (!overlaps) { allocation = candidate; break; }
    }
}
```

The **representative** of an existing allocation group is never tested for `transient`. A
non-transient image (one whose contents must survive the frame — history buffers, cached
intermediates, anything the editor or a later frame reads) gets its own allocation on the first
pass, and a subsequent transient image with a compatible descriptor and a non-overlapping lifetime
will be assigned to the same physical allocation, overwriting it.

The real allocator, `RenderGraphTargets::allocatePhysical`
(`mpp/src/RenderGraphTargets.cpp:124-133`), gets this right:

```cpp
if (!lifetime->desc.transient) continue;
bool safe = all_of(used.begin(), used.end(), [&](Assignment const& previous)
{
    return previous.transient && !intervalsOverlap(previous, *lifetime);
});
```

So the bug does not currently corrupt rendering — but `RenderGraphAllocationPlan::physicalAllocation`
and `estimatedPhysicalBytes` are **wrong** for any graph mixing transient and non-transient images,
and any future consumer of `physicalAllocation` (a memory view in the editor, a different backend)
inherits a real corruption bug.

**Fix** — mirror the `allocatePhysical` predicate:

```cpp
bool const representativeTransient = std::all_of(allocationMembers[candidate].begin(),
    allocationMembers[candidate].end(),
    [&](uint32_t member) { return plan.allocatedImages[member].desc.transient; });
if (!representativeTransient) continue;
```

#### 4.1 Resolution

**Correction to the severity above:** this was filed as latent on the grounds that only rendering
matters. It is not. `PipelineEditor` displays both affected fields directly — `Main.cpp:6016` prints
`estimatedPhysicalBytes` as "Physical estimate: N MiB" and `Main.cpp:6021` prints
`physicalAllocation` per image. Any pipeline mixing transient and non-transient images has been
showing a wrong memory figure and wrong alias grouping in the editor's own UI.

The suggested `all_of` was not used. Only a group's *first* member can be non-transient (every later
member joined by passing the transient test), so scanning all members re-derives each time a fact
that is fixed when the group is created. The groups now carry it:

```cpp
struct AllocationGroup { vector<uint32_t> members; bool aliasable; };
...
if (!allocationGroups[candidate].aliasable) continue;
...
allocationGroups.push_back({ {}, lifetime.desc.transient });
```

O(1) instead of O(members), and it states the rule rather than depending on an invariant a later
edit could break.

**On the test.** Added to `runRenderGraphTopologyTests` — context-free, so it needs no GL. A
four-image graph: a non-transient `Keep` over passes 0–1, then three scratch images. `ScratchB`
(2–3) is lifetime-disjoint from `Keep`, so *only* transience separates them. Three assertions: that
those two do not share an allocation; that `ScratchA` (1–2) and `ScratchC` (3–3) still do, so the
fix did not simply disable aliasing; and that `estimatedPhysicalBytes` equals the sum over the
groups the plan itself reports, which ties the byte estimate to the grouping rather than to a
hardcoded number.

**Note on where this runs.** `runRenderGraphTopologyTests` is invoked by `PipelineEditor --validate`
and **not** by DemoSuite. Verified load-bearing there specifically: *"MPP-PIPELINE-CLI-002: render
graph topology tests failed: a transient graph image was planned on top of a non-transient
allocation"*.

**Harness caveat found while verifying:** `tools/ValidatePipelineEditorPhase10.ps1` does not build.
It validates whatever binary is already in `pipeline-editor/build/vs2026/bin/x64/<Configuration>`,
so it will happily report "passed" against a stale executable. Build the configuration you intend to
validate first — the default is Release, which a Debug-only build cycle never refreshes.

### 4.2 Aliasing compatibility is implemented twice, with different rules — **Bug, medium** — ✅ FIXED

Two independent predicates:

| | `RenderGraph.cpp:53` `aliasCompatible` | `RenderGraphTargets.cpp:20` `compatibleForAliasing` |
|---|---|---|
| size, format, mipLevels, colourSpace | ✔ | ✔ |
| minFilter, magFilter, wrap | ✔ | ✔ |
| useMipmaps, lodBaseLevel, lodMaxLevel | ✘ | ✔ |
| lodBias, maxAnisotropy | ✘ | ✔ |
| `desc.usage` | ✘ | ✘ |
| sample count | ✘ | ✔ (`candidatePool[index].samples`) |

They will drift. Neither checks `desc.usage`, so an image with `Presentation` usage can share an
allocation with one that does not — harmless today because presentation images are `external` and
skipped, but it is not enforced.

**Fix** — export a single `bool graphImagesCanAlias(GraphImageDesc const&, glm::uvec2, ...)` from
`RenderGraph.h` and call it from both sites. Include `desc.usage` in the comparison.

#### 4.2 Resolution

`graphImagesCanAlias(GraphImageLifetime const&, GraphImageLifetime const&)` is now exported from
`RenderGraph.h` and is the only definition; both `aliasCompatible` and `compatibleForAliasing` are
gone. It takes lifetimes rather than the suggested `(GraphImageDesc, glm::uvec2, ...)` because both
call sites already hold a `GraphImageLifetime` and `size` is a resolved field on it, so a parameter
pack would only give the callers a chance to pass mismatched pieces.

The strict field set won — it is `RenderGraphTargets`' old one. That is not arbitrary: the allocator
also reuses a pooled texture **across** frames (`RenderGraphTargets.cpp:108`, the `used.empty()`
branch), which is the same question asked about the same texture object, so every sampler field has
to match there. Sample count stays at the call site: it is derived from usage and the requested MSAA
level rather than carried on the descriptor.

`desc.usage` is included as suggested. Checked before committing to it: in the shipped templates
every non-external colour image is `colourAttachment,sampled` and every depth image is
`depthAttachment,sampled`, and the only images with `presentation` usage are external — which are
never allocated here at all. So it enforces the intent at no cost in aliasing.

**On the tests.** Three, and each was verified by making it fail:

1. Context-free, in `runRenderGraphTopologyTests` — the same three-pass graph planned three times:
   identical descriptors alias, a `lodBias` difference does not, a `usage` difference does not.
   Reverting the predicate to the old loose one gives *"graph images differing only in sampler LOD
   bias were planned onto one allocation"*; dropping just the usage term gives the matching message.
2. GPU suite — a pairwise assertion that the plan's `physicalAllocation` grouping and the targets
   the allocator actually handed out agree for **every** pair. This is the invariant that matters,
   since the plan is what PipelineEditor displays.
3. The GPU alias fixture gained a fourth image differing only in `lodBias`. Without it (2) was
   vacuous — all three original images shared one descriptor, so no predicate difference could ever
   show up. With it, giving the plan and the allocator deliberately different predicates produces
   *"planned allocation grouping disagrees with the allocated targets for 'GpuTestAliasMiddle' and
   'GpuTestAliasVariant'"*.

### 4.3 `RenderGraph::compile(Caps const&)` contains a dead loop — **Bug, medium** — ✅ FIXED (partly)

`mpp/src/RenderGraph.cpp:784-797`

```cpp
RenderGraphCompileResult RenderGraph::compile(Caps const& caps) const
{
    auto result = compile();
    for (auto const& image : mImages)
    {
    }
    for (auto const& pass : mPasses) { /* MRT count check */ }
```

The empty loop is clearly a placeholder for per-image capability validation that was never written.
Consequently **no image is checked against device limits**:

- `absoluteSize` / resolved relative size vs `GL_MAX_TEXTURE_SIZE`
- `mipLevels` vs the resolved dimensions at execution viewport (the check in
  `buildAllocationPlan:676-683` only runs during allocation, not during editor validation, so the
  editor accepts an invalid mip count and fails at runtime)
- format support (`GL_RGBA32F` renderability, `GL_RGB10_A2` blending, depth-stencil availability)
- multisample sample-count support per format

The result is that `PbrPipelineDocument::validate(caps, registry)`
(`mpp/src/PbrPipelineDocument.cpp:150`) reports a pipeline as valid that will throw at
`allocatePhysical` on the target device.

**Fix** — implement it, and add the corresponding fields to `Caps`. At minimum:

```cpp
for (auto const& image : mImages)
{
    if (image.desc.absoluteSize.x > caps.maxTextureSize || image.desc.absoluteSize.y > caps.maxTextureSize)
        result.diagnostics.push_back("Image '" + image.name + "' exceeds the maximum texture size.");
    uint32_t maxMips = 1;
    for (auto d = std::max(image.desc.absoluteSize.x, image.desc.absoluteSize.y); d > 1; d >>= 1) ++maxMips;
    if (image.desc.absoluteSize.x && image.desc.mipLevels > maxMips)
        result.diagnostics.push_back("Image '" + image.name + "' declares more mip levels than its size supports.");
}
```

Relative-size images cannot be validated without a viewport; add a
`compile(Caps const&, glm::uvec2 viewport)` overload for the editor, which always knows its preview
size.

#### 4.3 Resolution — size and mip levels only

**Two of the four listed checks are implemented; two are not.** Read the "not done" list below
before assuming a pipeline validated here will allocate.

Done — the empty loop now checks, per non-external image:

- resolved size against `caps.maxTextureSize`
- `mipLevels` against what the resolved size supports

The `compile(Caps const&, glm::uvec2 viewport)` overload was added as suggested, and
`PbrPipelineDocument::validate` gained a matching overload. `PbrPipelineRuntime::configure` already
had `viewportWidth`/`viewportHeight` in scope and now passes them, so a relative-sized image that
exceeds the device limit is reported as `MPP-PIPELINE-029` instead of throwing later at
`allocatePhysical`. The no-viewport overload still exists and skips relative images — it cannot do
otherwise — so it is documented in the header as the weaker choice.

**Not done, and deliberately:**

- **Format renderability** (`GL_RGBA32F` as an attachment, `GL_RGB10_A2` blending, depth-stencil
  availability). `Caps` carries no format capability data at all, so this needs new `glGetInternalformativ`
  probing at startup and new `Caps` fields. That is a larger change than fixing the dead loop and
  should be its own item.
- **Per-format multisample sample counts.** Sample count is not on `GraphImageDesc` — it is chosen
  by `allocatePhysical(plan, samples)` at allocation time — so `compile` has nothing to check.
  `caps.supportedMsaaSampleMask` is already applied at the point where the count is known.

The size/mip resolution itself was extracted to `resolveGraphImageSize` and `maxGraphImageMipLevels`
in `RenderGraph.h`, and `buildAllocationPlan` now calls them too. This is the same consolidation as
§4.2: validation that disagrees with allocation about what a descriptor resolves to would just move
the failure rather than prevent it.

**On the tests.** Seven cases in `runRenderGraphTopologyTests` against a `Caps` with
`maxTextureSize = 256`: oversized absolute rejected, in-range absolute accepted, excess mip levels
rejected, and — the point of the overload — a relative image accepted without a viewport, rejected
at a 512 viewport, accepted at 128. Verified load-bearing twice: restoring the empty loop gives
*"an image larger than the maximum texture size compiled against caps"*, and making the viewport
overload ignore its viewport gives *"a viewport-relative image exceeding the maximum texture size
compiled against caps"*.

### 4.4 There is no dead-pass culling — **Perf / Extension, medium**

`RenderGraph::compile` (`RenderGraph.cpp:571-575`) returns *every enabled pass in declaration order*:

```cpp
for (uint32_t pass = 0; pass < mPasses.size(); ++pass)
    if (mPasses[pass].enabled) result.passOrder.push_back({ pass });
result.valid = true;
```

A pass whose outputs are never sampled, never presented, and never a named output still executes.
This is one of the defining features of a render graph and its absence means the editor cannot
report "this pass is doing nothing", and disabling a consumer does not automatically skip its
producer.

**Fix** — add a reverse reachability pass over `compile()`'s result:

1. Seed the live set with passes writing an image that is `external`, has `Presentation` usage, or
   is referenced by a `RenderPipelineOutput`.
2. Walk producers of each live pass's `sampledInputs` transitively.
3. Emit the culled passes as *informational* diagnostics rather than dropping them silently, and
   gate the actual dropping behind a `RenderGraphCompileOptions::cullUnusedPasses` flag so the
   editor can show the full graph while the runtime executes the pruned one.

`RenderGraphAllocationPlan` should use the pruned order too — lifetimes currently extend across
dead passes, inflating `estimatedPhysicalBytes` and blocking aliasing.

### 4.5 `GraphLoadOp::Load` on a transient image is not diagnosed — **Bug, low**

An aliased transient image's contents at pass start are whatever the previous occupant left.
`clearPassOutputs` (`RenderGraphExecutor.cpp:98`) honours `Clear`, and `Load`/`DontCare` both fall
through to "preserve whatever is there". For a transient image, `Load` therefore reads garbage that
varies with the aliasing decision — a genuinely non-deterministic result.

**Fix** — in `RenderGraph::compile`, emit an error when a pass declares
`GraphLoadOp::Load` for an output whose image is `transient` and which has no earlier producer in
the pass order.

---

## 5. Render graph: execution

### 5.1 A framebuffer object is created and destroyed for every multi-attachment pass, every frame — **Perf, high**

`mpp/src/RenderGraphExecutor.cpp:398-406`

```cpp
if (colours.size() == 1 && colourMips.front() == 0 && !depth)
    passTarget = colours.front();
else
    passTarget = make_shared<GraphFramebufferTarget>(pass.name, colours, colourMips, depth, depthMip);
```

`GraphFramebufferTarget`'s constructor (`RenderGraphExecutor.cpp:55-90`) does
`glGenFramebuffers` → `glBindFramebuffer` → `glObjectLabel` (with a freshly-constructed
`std::string`) → one `glFramebufferTexture2D` per attachment → **`glCheckFramebufferStatus`** →
`glBindFramebuffer(0)`. The destructor deletes it. Every frame.

`glCheckFramebufferStatus` forces the driver to validate the attachment set; on several drivers this
is a synchronisation point. In the `Full.pipeline.xml` template this happens twice per frame
(`PbrScene` with 2 colour + 1 depth, and `ShadowDepth`), and any pipeline authoring more MRT or
mip-level passes scales linearly.

**Fix** — cache the FBOs in the executor:

```cpp
struct FramebufferKey
{
    std::vector<std::pair<GLuint, uint32_t>> colours;   // texture id + mip
    std::pair<GLuint, uint32_t> depth{0, 0};
    auto operator<=>(FramebufferKey const&) const = default;
};
std::map<FramebufferKey, std::shared_ptr<GraphFramebufferTarget>> mFramebufferCache;
```

Key on the resolved GL texture names plus mip levels (not on pass identity, so cross-frame target
reuse hits the cache). Evict on `RenderGraphTargets::allocatePhysical` bumping its pool, and on
`clearPassCallbacks`.

### 5.2 The graph is compiled twice per frame — **Perf, medium**

- `RenderPipeline::renderGraphForward` calls `graph->buildAllocationPlan(...)`
  (`RenderPipeline.cpp:256` and `:415`), which internally calls `compile()`
  (`RenderGraph.cpp:643`).
- `RenderGraphExecutor::execute` then calls `graph.compile(caps)`
  (`RenderGraphExecutor.cpp:324`).

`compile()` is O(passes × inputs) and constructs `std::string` diagnostics on every branch it
inspects. It is pure with respect to the graph, so the result should be memoised.

**Fix** — add a monotonically-increasing revision counter to `RenderGraph`, bumped by every mutating
method, and cache `{revision, viewport, caps} → RenderGraphCompileResult` in the executor and in
`RenderGraphTargets`. `RenderGraphTemplate`-backed graphs are immutable at runtime, so this is
effectively a one-time cost for the XML path.

### 5.3 The dynamic (non-template) path rebuilds the entire graph every frame — **Perf, high**

`RenderPipeline::renderGraphForward`, `mpp/src/RenderPipeline.cpp:312-503`

Every frame this constructs a fresh `RenderGraph`, creates ~12 images and ~11 passes for a 4-pass
bloom configuration, builds an allocation plan, binds imports, and registers a callback lambda per
pass. Several of those lambdas capture `models` **by value**:

```cpp
mGraphExecutor->setPassCallback(scenePass, [this, scene, models, camera](RenderGraphExecutionContext const&)
```

`models` is `vector<SceneModel3dPtr>` — a full vector allocation plus one atomic refcount increment
per visible model, per capturing lambda, per frame (there are at least two).

**Fix** — cache the built graph keyed on the values that actually affect topology:
`{ pbr, useMrtEmissiveMask, bloom.enabled, bloom.blurPasses, shadowDomain.empty(), taa, ssaa }` plus
the resolved viewport. Rebuild only when the key changes. Change the callbacks to read `models` from
`RenderGraphFrameContext` (which already carries `visibleModels`,
`mpp/include/mpp/RenderGraphFrameContext.h`) instead of capturing it, so the lambdas become
capture-light and can be registered once.

### 5.4 MRT capability probing issues GL queries per material per frame — **Perf, high**

`RenderPipeline.cpp:39-53` → `Program::validateFragmentOutputLocations`
(`mpp/src/Program.cpp:721-745`)

```cpp
bool sceneProgramsSupportOutputs(vector<SceneModel3dPtr> const& models, size_t requiredCount)
{
    for (auto const& sceneModel : models)
        for (int meshIndex = 0; meshIndex < model->getNumMeshes(); ++meshIndex)
            if (!program->validateFragmentOutputLocations(requiredCount, diagnostic)) return false;
```

`validateFragmentOutputLocations` calls `glGetProgramInterfaceiv` and then
`glGetProgramResourceiv` once per program output. These are driver round-trips. This runs for
**every mesh of every visible model, every frame**, in both graph paths
(`RenderPipeline.cpp:247` and `:327`).

**Fix** — the answer is a property of the *program*, not of the frame. Compute
`Program::getMaxFragmentOutputLocation()` once at link time in `Program::loadImpl` and cache it;
`sceneProgramsSupportOutputs` becomes an integer comparison. Better still, cache the aggregate
result on the pipeline keyed by a scene-content revision.

### 5.5 Executor state is keyed by pass **index** — **Bug, medium** — ✅ FIXED

`RenderGraphExecutor` stores `mCallbacks`, `mScenePasses`, `mParameterOverrides`, and `mGpuTimings`
in `std::map<uint32_t, ...>` keyed by `GraphPassHandle::id`, which is a **positional index** into
`RenderGraph::mPasses`. `RenderGraph::removePass`, `movePass`, and `reorderPasses`
(`RenderGraph.cpp:630`) all renumber passes.

In the editor's live-edit loop, removing or reordering a pass while stateful scene passes exist
means a `RenderGraphScenePass` instance created for pass *N* is silently reused for whatever pass
now occupies index *N*. `mGpuTimings` partially guards this by also comparing `pass.name`
(`RenderGraphExecutor.cpp:515`), which shows the hazard was noticed for timings but not for
callbacks.

**Fix** — key on the stable authored identifier the graph already maintains
(`RenderGraph::getValueId` has the same concept for images; passes need an equivalent
`passId`/GUID). As an interim, call `clearPassCallbacks()` whenever the graph topology revision
changes, and record the pass name alongside each `mScenePasses` entry with a mismatch assertion.

#### 5.5 Resolution

**No GUID was added, and no interim guard was needed** — the stable authored identifier already
exists. `RenderGraph::addPass` and `setPassName` both reject a duplicate name
(`RenderGraph.cpp:222`, `:241`), so a pass name is unique graph-wide, is what the XML author writes,
and is untouched by `removePass`/`movePass`/`reorderPasses`, which only renumber. All four maps —
`mCallbacks`, `mScenePasses`, `mParameterOverrides`, `mGpuTimings` — are now keyed by name.

`mGpuTimings` lost its defensive `gpuTiming->second.name == pass.name` comparison: with a name key
that check *is* the lookup.

The registration API had to change, because `setPassCallback(GraphPassHandle, ...)` cannot resolve a
name — the executor has no graph until `execute`. It now takes a name, with a
`(RenderGraph const&, GraphPassHandle, ...)` overload that resolves it at the call site. Every
in-tree caller uses the overload, so nothing reads more verbosely; the point is that a handle can no
longer be stored as though it were an identity. Same for `setPassParameterOverrides`, where
`RenderPipeline` already had the `GraphPassInfo` in hand and now just passes `info.name`.

**Known limitation:** renaming a pass orphans its state, so a stateful scene pass restarts — for TAA
that is one frame of history. Deleting a pass and creating a new one with the same name inherits the
old state. A GUID would avoid both. Neither is a silent misapplication of one pass's state to a
different pass, which is what this fixes, and both require deliberate authoring actions.

**On the test.** GPU suite, "pass identity across topology edits": three passes each register a
callback that asserts the pass it actually runs as, then pass 0 is removed so the rest renumber.
Verified load-bearing by keying the executor on `pass.id` again, which reports *"the callback
registered for 'GpuTestIdentity0' ran as 'GpuTestIdentity1'"* — precisely the misattribution this
section describes.

### 5.6 `renderGraphFullscreen` silently ignores unknown sampler names — **Bug, medium → high** — ✅ FIXED

`mpp/src/RenderSystem.cpp:2989-3017`

```cpp
for (uint32_t unit = 0; unit < samplers.size(); ++unit)
{
    GL_CHECK(glUniform1i(p->getUniformId(samplers[unit].first), (GLint)unit));
    samplers[unit].second->bind(unit, 0);
}
```

`Program::getUniformId` returns `-1` for an unknown name (`Program.cpp:592`), and
`glUniform1i(-1, ...)` is legal and does nothing. So a declarative fullscreen pass that binds
`<sampler>TEX2</sampler>` against a program that declares `TEX1` binds the texture to unit 1 and
leaves the shader sampling unit 0 — the wrong image, with no error anywhere.

**Fix** — validate at bind time and throw:

```cpp
auto location = p->getUniformId(samplers[unit].first);
if (location < 0)
    THROW_MPP("Graph fullscreen pass binds sampler '" + samplers[unit].first +
              "' which program '" + p->getName() + "' does not declare.", ...);
```

Better: validate declaratively in `RenderGraphPassFactoryRegistry::validate` so the editor reports
it as a diagnostic before the pipeline runs.

#### 5.6 Resolution — the diagnosis above was wrong, and understated it

**Correction.** The analysis said an unknown sampler name "binds the texture to unit 1 and leaves
the shader sampling unit 0". The symptom is right; the mechanism is not, and the real mechanism is
worse.

`getUniformId` marks its argument up as `_mpp_u_NAME_` and looks in `mUniformIds`. Samplers are
marked up as `_mpp_t_NAME_` and live in a **separate** `mTextures` list (`Program.cpp:301`,
`:512-521`). So `getUniformId(samplerName)` returned `-1` for **every** sampler, correct name or
not, and `glUniform1i(-1, ...)` did nothing every single time. That line never had any effect.

Texture units were therefore decided entirely by `Program::bind`, which assigns `mTextures[i]` to
unit `i` (`Program.cpp:760-764`) — the shader's *declaration* order — while `renderGraphFullscreen`
bound each texture to its position in the *pass's* binding list. The name was never consulted.

So the bug is not confined to unknown names. **Any declarative fullscreen pass whose
`<sampler>` elements are authored in a different order from the shader's `@@Texture` declarations
sends every texture to the wrong sampler, silently.** That is a plain wrong-image render from
correct-looking XML, which is why this is re-rated high.

**Fix.** `Program::getSamplerUnit(name)` returns the unit the program actually samples that name
from, and `renderGraphFullscreen` binds to it, throwing when the name is undeclared. The dead
`glUniform1i` is gone — `Program::bind` already assigns sampler uniforms.

**The declarative check already existed.** `RenderGraphTemplate::createImpl` (`:42-58`) already
rejects a binding naming a sampler the program does not declare, and every reachable call to
`renderGraphFullscreen` goes through a template. So the *unknown-name* half of this item was
covered before the fix; the runtime throw is a backstop for the public API. The ordering half was
covered nowhere. No change was made to `RenderGraphPassFactoryRegistry::validate`, which is
context-free and cannot resolve a program's uniforms.

**On the test.** GPU suite, "fullscreen sampler routing": a two-sampler program, two sources
distinguishable in the red channel, bound in **reverse** declaration order, plus a case binding an
undeclared name. Verified load-bearing by restoring positional binding, which reports
*"got r=64 g=255, expected r=255 g=64"* — the two textures exactly swapped.

### 5.7 `discardDontCareOutputs` uses attachment enums that can be invalid — **Bug, low**

`RenderGraphExecutor.cpp:181-195`

- When the pass target is the default framebuffer (an imported `screen`), the valid enums are
  `GL_COLOR`/`GL_DEPTH`/`GL_STENCIL`, not `GL_COLOR_ATTACHMENT0`. Authoring
  `store="dontCare"` on a presentation output therefore raises `GL_INVALID_ENUM`.
- For a `Depth24Stencil8` / `Depth32fStencil8` attachment, `GL_DEPTH_ATTACHMENT` invalidates only the
  depth aspect; `GL_DEPTH_STENCIL_ATTACHMENT` is what was actually attached
  (`RenderGraphExecutor.cpp:83`).

**Fix** — branch on whether the bound target is the default framebuffer, and select the
depth/depth-stencil enum from `RenderTexture::hasStencilBuffer()`.

### 5.8 `GraphLoadOp::DontCare` never invalidates at load time — **Perf, low**

Only `store == DontCare` invalidates. A `load == DontCare` attachment could equally issue
`glInvalidateFramebuffer` *before* the pass, telling the driver the previous contents are dead. On
desktop this mostly avoids a decompress; the win is larger on tiled/mobile-class drivers and on
some laptop iGPUs.

### 5.9 Mip views mutate texture object state globally — **Non-standard, known**

`RenderTexture::applyMipView` / `restoreMipView` (`mpp/src/RenderTexture.cpp:358-390`) set
`GL_TEXTURE_BASE_LEVEL` / `GL_TEXTURE_MAX_LEVEL` on the texture object, which is why the executor
must reject two different mip views of the same texture in one pass
(`RenderGraphExecutor.cpp:413-415`). `doc/RENDER_GRAPH_IMPLEMENTATION_ISSUES.md` already lists this.

**Fix** — `glTextureView` (GL 4.3 / `ARB_texture_view`) creates a real view with an independent
level range and no state mutation, removing the restriction entirely. Fall back to the current path
when the extension is absent.

### 5.10 Minor state-restoration issues

- `GraphRasterStateScope`'s destructor restores `glPolygonMode(GL_FRONT_AND_BACK, mPolygonMode[0])`
  (`RenderGraphExecutor.cpp:174`), discarding a distinct back-face mode. Harmless in core profile
  (separate modes are deprecated) but worth a comment.
- `clearPassOutputs` saves/restores the colour mask with non-indexed
  `glGetBooleanv`/`glColorMask` (`RenderGraphExecutor.cpp:100`, `:116`), which flattens any
  per-attachment masks previously set with `glColorMaski`. It is safe today only because
  `GraphRasterStateScope` re-applies indexed masks *after* the clear.
- `RenderTexture::generateMipMaps` and `applyMipView` unconditionally leave
  `GL_TEXTURE_2D` bound to 0 on the active unit (`RenderTexture.cpp:372`, `:407`), invalidating the
  renderer's texture-binding cache assumptions in `setupRenderMeshInstance`
  (`RenderSystem.cpp:3891`, which caches by pointer). This has not caused a visible bug because
  `currentTextureKeys` is per-flush, but it is fragile.

---

## 6. Named outputs and the anti-aliasing chain

### 6.1 An offscreen named output silently skips the entire AA chain — **Bug, medium** — ✅ FIXED (TAA/FXAA); SSAA is a design question

`mpp/src/RenderOutputProcessor.cpp:85`

```cpp
if (input == destination && !state.plan.antiAliasing.taa)
{
    bypass(RenderFlowEventKind::Taa, "TAA");
    bypass(RenderFlowEventKind::SsaaHorizontal, "SSAA");
    bypass(RenderFlowEventKind::Fxaa, "FXAA");
    bypass(RenderFlowEventKind::Presentation, "Presentation");
    return;
}
```

For an **external** output the graph writes into `state.input` and `destination` is the imported
target, so they differ and the chain runs. For a **non-external** output, `RenderPipeline` sets both
`destination` and `source` to `mGraphTargets->get(handle)`
(`RenderPipeline.cpp:269` and `:305`), so `input == destination` and everything is skipped — FXAA
included, even though `PbrPipelineDocument::validate` accepted the `<fxaa>true</fxaa>` authoring and
`RenderOutputProcessor::rebuild` allocated the FXAA work target for it
(`RenderOutputProcessor.cpp:61`).

The README states that "Screen and offscreen graph outputs now pass through a shared transactional
renderer-owned output chain"; offscreen outputs do not.

**Fix** — treat offscreen outputs like external ones: allocate the `Input` physical image, bind it
as the graph's write target for that image, and let `present` copy through the chain into the
graph-visible target. Alternatively, if the bypass is intentional, make it an authoring **error** in
`validateOutputAntiAliasing` rather than a silent runtime no-op.

#### 6.1 Resolution

**TAA and FXAA now run for offscreen outputs.** The bypass condition was `input == destination &&
!taa`, and for an offscreen output the input *is* the destination by construction, so the whole
chain was dropped. It now skips only when the chain would genuinely do nothing:

```cpp
bool const chainDoesNothing = !taa && ssaa == Off && !fxaa;
if (input == destination && chainDoesNothing) { ...; return; }
```

With any stage enabled the intermediate targets are distinct, so the final blit never reads and
writes the same texture — which is the only thing the original condition was really protecting
against.

**SSAA is different, and neither of the two suggested fixes is right for it.** The suggested
approach — bind the processor's `Input` as the graph's write target — works for external outputs
because their destination is the screen, at *logical* size, while the graph renders at raster size.
An offscreen output's destination is the graph's own image, which `RenderPipeline` already allocated
at `ssaaDimension(viewport)` (`RenderPipeline.cpp:256`). It is *already* the supersampled image.
There is nothing left to downsample from, and no logical-size target to put the result in.

Making SSAA meaningful offscreen would require deciding what an offscreen output's declared size
means — the raster size it is rendered at, or a logical size it should be resolved to. That is a
design question, not a bug, and it is **left open**.

What was fixed is the silence. `RenderOutputProcessor::rebuild` now forces `ssaa = Off` for a
non-external output, so `rasterSize == logicalSize` and the work targets are sized against a
destination the chain can actually reach; and `validateOutputAntiAliasing` reports
**MPP-PIPELINE-050**. That is a *warning*, not an error, because MPP-PIPELINE-042 requires every
output to share one effective SSAA setting — a pipeline mixing a screen output with an offscreen one
legitimately inherits SSAA on both, and erroring would reject working configurations.

**Fixture correction.** The existing named-output GPU test declared its image non-external while
driving it exactly as an external output (graph renders into the processor's `Input`, presents into
a separate destination). It now sets `external = true`, which is what it was always modelling.

**On the tests.** A new "offscreen output anti-aliasing chain" case captures the render flow around
`present` and asserts the FXAA and Presentation events actually executed, that the image survives
the round trip, and that an SSAA request is planned away rather than sized against a raster-size
destination. Verified load-bearing twice: restoring the old bypass gives *"an offscreen named output
skipped its anti-aliasing chain"*, and removing the `rebuild` guard gives *"SSAA on an offscreen
output was planned against a raster-size destination"*.

### 6.2 TAA is a depth-reprojection resolve with a fixed blend weight — **Non-standard, medium**

`mpp/include/mpp/DefaultShaders.h:560-599`

Current design: 8-sample Halton jitter, depth reprojection, depth rejection at `0.01`, RGB min/max
neighbourhood clamp, fixed `mix(current, history, 0.9)`.

Known limitations, all of which should at least be documented in
`doc/ANTI_ALIASING_CONFIGURATION.md`:

- **No motion vectors.** Reprojection uses only camera motion, so any object that moves relative to
  the world ghosts persistently. The depth-rejection test (`abs(storedPreviousDepth -
  expectedPreviousDepth) <= 0.01`) catches disocclusion but not same-depth object motion.
- **RGB min/max clamp**, not variance *clipping* in YCoCg. Min/max over a 3×3 in RGB is the loosest
  possible box; it under-rejects (ghosting) on low-contrast content and over-rejects (flicker) on
  chroma edges.
- **Fixed 0.9 feedback** with no luminance weighting, so a single bright sample dominates the
  history for ~10 frames.
- **Bilinear history fetch** (`texture(HISTORY_COLOUR, previousUv)`), which softens the image every
  frame. Catmull-Rom history sampling is the standard remedy.
- The 3×3 neighbourhood loop includes the centre texel twice (it is seeded from `current` and then
  re-visited at `x=y=0`) — harmless, but it should be removed for clarity.

**Fix path (in order of value/effort)**

1. Catmull-Rom history sampling — ~15 lines, immediate sharpness win.
2. YCoCg variance clipping (`mean ± γ·stddev`, γ ≈ 1.0) replacing the RGB box.
3. Luminance-weighted feedback (`weight = 1/(1+luma)`), which kills fireflies.
4. A motion-vector graph image and a `GraphImageUsage`/pass-metadata contract for producing it —
   this is the largest change and unlocks correct dynamic-object TAA.

### 6.3 Bloom extraction uses a hard knee and full-resolution ping-pong — **Non-standard / Perf, medium**

`FragmentShaderBloomExtractTemplate` (`DefaultShaders.h:647`):

```glsl
@Out(vec4 COLOUR) = vec4(max(colour - vec3(@Uniform(THRESHOLD)), vec3(0.0)), 1.0);
```

Two issues:

- **Hard knee.** Subtracting the threshold both hard-cuts at the threshold (visible boundary as
  a value crosses it) and darkens everything above it. The conventional form is a soft-knee curve
  (Karis / Unity), plus a Karis average over a 2×2 neighbourhood to suppress single-pixel fireflies:

  ```glsl
  float brightness = max(colour.r, max(colour.g, colour.b));
  float soft = clamp(brightness - THRESHOLD + KNEE, 0.0, 2.0 * KNEE);
  soft = soft * soft / (4.0 * KNEE + 0.00001);
  float contribution = max(soft, brightness - THRESHOLD) / max(brightness, 0.00001);
  ```

- **Full-resolution ping-pong.** Every bloom image in `Full.pipeline.xml` uses the default
  `scale = 1 1` and `RGBA16F`. With `blurPasses = 4` that is 8 full-resolution 16F blur passes
  (each a 5-tap separable Gaussian) plus extract and composite — 10 full-screen 64-bpp passes. At
  1920×1080 that is roughly 10 × 8.3 MB of bandwidth per direction, per frame, for a visual effect
  that is by definition low-frequency.

  The graph already supports `<scale>` (`RenderGraphParser.cpp:144`) and `mipLevels`, so the fix is
  partly an authoring change: set `<scale>0.5 0.5</scale>` on `BloomExtract` and then halve again
  per blur level. Fully solving it means a proper downsample/upsample pyramid — see
  [§9.2](#92-post-processing-passes).

### 6.4 Bloom blur iteration count is derived from the pass *name* — **Bug, medium**

`mpp/src/RenderGraphBuiltInPasses.cpp:60-76`

```cpp
uint32_t trailingPassIndex(std::string const& name)
{
    auto first = name.find_last_not_of("0123456789");
    if (first == name.size() - 1) return 0;
    try { return (uint32_t)std::stoul(name.substr(first + 1)); } catch (...) { return 0; }
}
...
bool enabled = frame.pipelineOptions->bloom.enabled
    && frame.pipelineOptions->graphPasses.bloom
    && iteration < frame.pipelineOptions->bloom.blurPasses;
```

Pass behaviour is driven by string parsing of the pass name. Renaming `BloomBlurHorizontal2` to
`Blur - wide` in the editor makes it behave as iteration 0 and silently changes how many blur
levels the `blurPasses` slider actually enables. The validation in
`PbrPipelineDocument.cpp:91` counts factory occurrences, so it will not catch the mismatch.

**Fix** — the metadata system already supports typed parameters
(`GraphPassParameterMetadata`, `RenderGraphPassFactoryRegistry.h:36`). Add an `ITERATION` int
parameter to the `MPP.BloomBlurHorizontal`/`Vertical` metadata, read it via `integerParameter`
(the helper already exists at `RenderGraphBuiltInPasses.cpp:33`), and set it in the templates and
in `RenderPipeline`'s generated graph. Keep `trailingPassIndex` as a migration fallback for one
release, emitting a deprecation diagnostic.

---

## 7. PipelineEditor and serialization

### 7.1 Per-pass raster state is editable and executed but never serialized — **Bug, high (data loss)** — ✅ FIXED

> **Resolved.** A `<Raster>` block is now written and parsed, and the round-trip is
> covered by a test that actually runs. See [§7.1 resolution](#71-resolution) below.



- **Editable**: `pipeline-editor/src/Main.cpp:4238-4356` exposes fill mode, front face, cull mode,
  depth test/write/compare, blend enable, separate colour/alpha blend ops and all six factors,
  multisample, alpha-to-coverage, scissor + rectangle, and per-attachment colour write masks. Each
  edit goes through the undo stack via `setPassRasterState`.
- **Executed**: `GraphRasterStateScope` (`RenderGraphExecutor.cpp:132-179`) applies all of it when
  `explicitState` is set.
- **Not written**: `RenderGraphSerializer::toNode` (`mpp-resource-parsers/src/RenderGraphSerializer.cpp:79-122`)
  emits `name`, `enabled`, `type`, `factory`, `program`, `Inputs`, `Parameters`, `Colours`, `Depth`
  — and nothing else.
- **Not read**: `RenderGraphParser::fromData` (`mpp-resource-parsers/src/RenderGraphParser.cpp:168+`)
  has no raster-state branch (grep for `rasterState` returns only the editor UI).

**Every raster-state edit is lost the moment the pipeline is saved and reloaded**, with no warning.
The undo stack shows the change, the preview reflects it, and reopening the file reverts it.

**Fix**

1. Add a `<Raster>` block to the render graph XML schema, e.g.:

   ```xml
   <Raster>
     <explicit>true</explicit>
     <fill>fill</fill><frontFace>ccw</frontFace><cull>back</cull>
     <depthTest>true</depthTest><depthWrite>true</depthWrite><depthCompare>less</depthCompare>
     <Blend><enabled>true</enabled><colourOp>add</colourOp><alphaOp>add</alphaOp>
       <sourceColour>srcAlpha</sourceColour><destinationColour>oneMinusSrcAlpha</destinationColour>
       <sourceAlpha>one</sourceAlpha><destinationAlpha>oneMinusSrcAlpha</destinationAlpha></Blend>
     <multisample>true</multisample><alphaToCoverage>false</alphaToCoverage>
     <Scissor><enabled>false</enabled><rect>0 0 0 0</rect></Scissor>
     <ColourWriteMask><rgba>true true true true</rgba></ColourWriteMask>
   </Raster>
   ```

2. Emit it only when `explicitState` is true, so existing documents round-trip byte-identically.
3. Add a round-trip test: build a graph with non-default raster state, serialize, parse, and assert
   `GraphPassInfo::rasterState` equality. `mpp/src/RenderGraphTests.cpp` is the natural home.

#### 7.1 Resolution

- `GraphRasterState` and `GraphColourWriteMask` gained defaulted `operator ==`, which both the
  serializer and the tests need.
- `RenderGraphSerializer` emits a `<Raster>` block, and `RenderGraphParser` reads it. Enumerations
  round-trip as descriptive names (`reverseSubtract`, `oneMinusSourceAlpha`, `greaterEqual`);
  unknown spellings **throw** rather than defaulting, matching how `parseFormat`/`parseWrap`
  already behave, so a typo in an authored pipeline is reported instead of silently changing how a
  pass rasterizes.
- The block is emitted whenever the state differs from default, **not** only when `explicitState`
  is set. Emitting on `explicitState` alone would still lose a configuration the author had
  temporarily switched off. Documents that never touched raster state gain nothing, so every
  existing template round-trips unchanged.

**The test suites were dead code.** `runRenderGraphResourceTests` and `runRenderGraphTopologyTests`
were exported but called from nowhere — `grep` found no invocation in the entire tree. That is why
this gap survived: there was an XML round-trip test file that never executed. All three
context-free suites (including `runPbrMaterialSpecializationTests`, which was only reachable by
running DemoSuite's model scene interactively) are now run at the top of `PipelineEditor --validate`,
so `ValidatePipelineEditorPhase10.ps1` exercises them on every invocation and they report as
`MPP-PIPELINE-CLI-002/003/004`.

The new coverage was verified to be load-bearing by disabling the serializer's `<Raster>` emission
and confirming validation fails with "graph XML round trip lost raster state", rather than by
assuming a passing test proves anything.

### 7.2 Sampler LOD/anisotropy image settings are editable but not persisted (and partly overwritten) — **Bug, medium**

`pipeline-editor/src/Main.cpp:4949-4953` exposes:

| Control | Serialized? | Parsed? | Survives to GL? |
|---|---|---|---|
| `Use mipmaps` | ✘ | ✘ | ✘ — overwritten by `makeOptions` |
| `LOD base` | ✘ | ✘ | ✘ — overwritten by `makeOptions` |
| `LOD maximum` | ✘ | ✘ | ✘ — overwritten by `makeOptions` |
| `LOD bias` | ✘ | ✘ | ✔ |
| `Maximum anisotropy` | ✘ | ✘ | ✔ |

`RenderGraphTargets::makeOptions` (`mpp/src/RenderGraphTargets.cpp:36-38`) unconditionally does:

```cpp
options.params.useMipmaps    = desc.mipLevels > 1;
options.params.lodBaseLevel  = 0;
options.params.lodMaxLevel   = (int32_t)desc.mipLevels - 1;
```

So three of the five controls are dead UI, and the other two silently reset on save/reload.

**Fix** — decide the contract and enforce it:

- Remove `Use mipmaps` / `LOD base` / `LOD maximum` from the editor (they are correctly derived
  from `mipLevels`), and
- Add `<lodBias>` and `<maxAnisotropy>` to the image XML schema in both parser and serializer.

Also worth doing: `RenderGraphSerializer` does not emit `desc.params` fields beyond
min/mag/wrap, so `compatibleForAliasing`'s comparison against `lodBias`/`maxAnisotropy`
(`RenderGraphTargets.cpp:28`) is comparing values that can never differ between a saved and a
reloaded document — a subtle way for the editor preview and the runtime to disagree.

### 7.3 The editor forces every image to be non-transient — **Non-standard / testing gap, medium**

`pipeline-editor/src/Main.cpp:1405-1410`

```cpp
for (uint32_t image = 0; image < previewDocument->graph->getImageCount(); ++image)
{
    auto desc = previewDocument->graph->getImageInfo(handle).desc;
    if (!desc.external && desc.transient) { desc.transient = false; previewDocument->graph->setImageDesc(handle, desc); }
}
```

The rationale (making intermediates inspectable after the frame) is sound and documented in place.
The consequence is that **transient aliasing is never exercised in the editor**. Every aliasing bug
— including [§4.1](#41-buildallocationplan-can-alias-a-transient-image-onto-a-non-transient-allocation--bug-high-latent)
and any future `Load`-on-aliased-image issue — is invisible in the preview and appears only in
DemoSuite/package mode. The preview also over-allocates: an 11-image bloom graph that would alias
down to 3 physical targets keeps all 11.

**Fix** — keep the retention behaviour but make it opt-in and visible:

1. Add a `View > Retain Intermediate Images` toggle, defaulting **on** (preserving today's
   behaviour), so authors can flip to production aliasing.
2. When aliasing is active, drive the image inspector from the `RenderGraphAllocationPlan` so it can
   report "this image shares physical allocation *N* and its contents after the frame belong to
   *other-image*" rather than showing a misleading picture.
3. Show `estimatedPhysicalBytes` and the alias groups in the process-flow view — this is the natural
   place to surface [§4.1](#41-buildallocationplan-can-alias-a-transient-image-onto-a-non-transient-allocation--bug-high-latent)'s
   plan data, and it makes the plan/allocator divergence in
   [§4.2](#42-aliasing-compatibility-is-implemented-twice-with-different-rules) observable.

### 7.4 Named-output resolution is O(images × passes) per output, per frame — **Perf, low**

`RenderPipeline.cpp:268` and `:421` both do, for every named output, every frame:

```cpp
for (uint32_t id = 0; id < graph->getImageCount(); ++id) { /* find by name */ }
for (uint32_t pass = 0; pass < graph->getPassCount(); ++pass)
    for (auto const& attachment : passInfo.colourOutputs)
        if (attachment.image.id == handle.id && attachment.image.version > handle.version) handle = attachment.image;
```

`getPassInfo` itself constructs a `GraphPassInfo` by value — copying four vectors and a
`UniformCollection` per call (`RenderGraph.cpp:486-494`). For a 15-pass graph with 2 outputs this is
~60 vector allocations per frame, purely to find the latest version of an image.

**Fix** — resolve the output handles once when the graph or output set changes and cache them
(same trigger as [§5.2](#52-the-graph-is-compiled-twice-per-frame--perf-medium)). Additionally, add
`RenderGraph::getPassInfoRef` returning a lightweight view, or a dedicated
`RenderGraph::getLatestVersion(std::string const& imageName)` helper.

### 7.4b The smoke test rejected a pipeline with no passes — **Bug, medium** — ✅ FIXED

`pipeline-editor/src/Main.cpp:6521` asserted `snapshot->actualPassOrder.empty()` as a telemetry
failure:

```cpp
if (!snapshot || snapshot->actualPassOrder.empty() ||
    snapshot->actualPassOrder.size() != pipeline->getLastGraphExecutionOrder().size())
    throw std::runtime_error("Process-flow phase-one snapshot was not published.");
```

`resources/shared/pbr/templates/Empty.pipeline.xml` declares `<Passes />` — it is the documented
blank starting template — so `actualPassOrder` is legitimately empty and the assertion threw
unconditionally. `snapshot->batches.empty()` at `:6527` and `smokeFlowModel.edges.empty()` at
`:6621` had the same problem: they assume drawn geometry. The whole GPU section of
`tools/ValidatePipelineEditorPhase10.ps1` therefore failed, which made `RebuildAll2026.bat` fail,
which in turn masked any *real* regression the suite would have caught.

The three conditions also conflated distinct failures under one message ("snapshot was not
published"), so the cause was not diagnosable from the output.

**Resolution**

Expectations are now derived from what the document declares rather than assumed:

- `expectedPasses` counts the graph's enabled passes and `actualPassOrder.size()` is asserted equal
  to it. This is **stricter** than the old check: it verifies every enabled pass actually executed,
  where before an empty order merely had to be non-empty. (If pass culling is ever added —
  [§4.4](#44-there-is-no-dead-pass-culling--perf--extension-medium) — this assertion must be
  updated deliberately.)
- `expectsBatches` is true only when the graph has an enabled `Scene` pass *and* the preview scene
  has models. Batch telemetry is required when it is true and required to be **absent** when it is
  false, so both directions are asserted rather than one.
- Process-flow edges are required only once the graph declares at least one pass.
- The single "was not published" message is split into distinct messages for a missing snapshot, a
  pass-count mismatch, missing batch telemetry, unexpected batch telemetry, and missing output
  telemetry.

Both new predicates are exercised in both directions by the existing template set: `Full` proves
the non-zero/`expectsBatches == true` path and `Empty` proves the zero/`expectsBatches == false`
path, so a miscomputation of either would fail the suite.

### 7.5 Missing-environment warning latches permanently — **Bug, trivial**

`mpp/src/RenderPipeline.cpp:548-552` sets `mWarnedMissingPbrEnvironment = true` and never clears it.
`setPbrEnvironment` (`RenderPipeline.cpp:143`) should reset the flag so that re-breaking the
environment in a live editing session re-reports.

---

## 8. Optimisation opportunities

Ordered by expected value. Items already described above are cross-referenced rather than repeated.

| # | Opportunity | Where | Expected win |
|---|---|---|---|
| 1 | Cache pass framebuffers instead of recreating per frame | [§5.1](#51-a-framebuffer-object-is-created-and-destroyed-for-every-multi-attachment-pass-every-frame--perf-high) | Removes 2+ `glCheckFramebufferStatus` sync points/frame |
| 2 | Cache the MRT program-output capability at link time | [§5.4](#54-mrt-capability-probing-issues-gl-queries-per-material-per-frame--perf-high) | Removes O(meshes) driver queries/frame |
| 3 | Cache the dynamically-built graph | [§5.3](#53-the-dynamic-non-template-path-rebuilds-the-entire-graph-every-frame--perf-high) | Removes ~25 container allocations + N shared_ptr copies/frame |
| 4 | Bloom at half/quarter resolution or as a mip pyramid | [§6.3](#63-bloom-extraction-uses-a-hard-knee-and-full-resolution-ping-pong--non-standard--perf-medium) | 4–8× bloom fill-rate reduction, better quality |
| 5 | Memoise `RenderGraph::compile` on a revision counter | [§5.2](#52-the-graph-is-compiled-twice-per-frame--perf-medium) | Removes 2 full validations + string construction/frame |
| 6 | Precompute a per-program sampler binding table | below | Removes a `std::map<std::string,…>` lookup per texture per draw |
| 7 | Cache uniform locations instead of rebuilding strings | below | Removes 1 string construction + 2 map lookups per uniform set |
| 8 | Sort opaque geometry front-to-back within material buckets | below | Recovers early-Z rejection |
| 9 | Only resolve MSAA attachments that are later sampled | below | Skips redundant `glBlitFramebuffer` |
| 10 | Avoid copying `UniformCollection` per fullscreen pass | below | One allocation-heavy copy per pass per frame |

### 8.6 Per-draw sampler-name map lookups

`RenderSystem::setupRenderMeshInstance`, `mpp/src/RenderSystem.cpp:3872-3888`

```cpp
auto const& samplerName = program->getSamplerName((int)i);
if (samplerName == "SHADOW_MAP" && mActiveShadowDepthTarget) { ... }
auto pipelineSampler = mActivePipelineSamplerOverrides.find(samplerName);
```

For every texture unit of every draw call: one string comparison against `"SHADOW_MAP"` and one
`std::map<std::string, ResourcePtr>` lookup (O(log n) string compares). A PBR material has 7–10
samplers, so a 500-draw scene costs ~4,000 map lookups per frame on the CPU-critical path.

**Fix** — resolve once per `(program, pipeline-override-set)` pair into a small
`std::vector<uint8_t>` classifying each unit as `Material | ShadowDepth | PipelineOverride(index)`.
Invalidate when `setActivePipelineSamplerOverrides` or `setActiveShadowDomain` changes. The override
set changes at most once per pipeline per frame (`RenderPipeline.cpp:554`).

### 8.7 `Program::getUniformId` builds a string on every call

`mpp/src/Program.cpp:582-598`

```cpp
string markedUpUniform = MPP_PROGRAM_MARKUP_UNIFORM(name);
if (index >= 0) markedUpUniform += STR_FORMAT("[{}]", index);
if (mUniformIds.find(markedUpUniform) == mUniformIds.end()) return -1;
else return mUniformIds.at(markedUpUniform);
```

Three problems: a heap allocation per call, `fmt` formatting for indexed uniforms, and a
double lookup (`find` then `at`). It is called from every fullscreen helper
(`renderToneMappedFullscreenQuad`, `renderFxaa`, `renderTaa`, `renderSsaaLanczos`,
`renderBloom*`, `renderGraphFullscreen`, all three IBL face renderers) — a dozen or more calls per
pass, and from `UniformCollection::bindUniforms` per material.

**Fix** — at minimum use a single `find` and return `it->second`. Better: have callers cache the
`GLint` once (the existing `getModelCameraProjectionMatrixId()` / `getHalfWindowSizeId()` accessors
show the pattern), and add cached ids for the fixed engine uniforms
(`EXPOSURE`, `GAMMA`, `TONE_MAP_OPERATOR`, `DIRECTION`, `OUTPUT_SIZE`, `THRESHOLD`, `INTENSITY`, …).

### 8.8 No opaque depth sorting or depth prepass

`RenderSystem::flushVertexBuffers`, `mpp/src/RenderSystem.cpp:4011-4029`

Opaque geometry is sorted by `a.key < b.key` — program/material sort key only. There is a
commented-out distance-key implementation immediately above (`RenderSystem.cpp:3995-4002`),
confirming this was planned. With a heavy PBR fragment shader (7+ texture fetches, an 8-light loop,
3-tap-squared PCF, and 3 IBL fetches), overdraw is expensive; front-to-back ordering within each
material bucket typically recovers 20–40% of scene fragment cost in a depth-complex scene.

**Fix** — reinstate the distance key in the low bits of the sort key, below the program/material
bits, so it acts as a tiebreaker and does not increase state changes. A `GraphPassType`-level
depth-prepass option would be the next step; it fits cleanly as a new built-in pass
(see [§9.2](#92-post-processing-passes)).

### 8.9 Unconditional MSAA resolve on store

`RenderGraphExecutor.cpp:495-496` calls `targets.resolve(output.image, …)` for **every** colour and
depth output with `store == Store`. `RenderGraphTargets::resolve`
(`mpp/src/RenderGraphTargets.cpp:205`) then blits whenever the write target is multisampled.

An MSAA image that is written and never sampled (a depth buffer used only for testing, or a colour
attachment consumed only by a later MSAA pass) is resolved anyway. With the graph's reachability
information this is trivially avoidable.

**Fix** — precompute, per image version, whether any later pass samples it or whether it is a named
output. Only resolve then. This composes with
[§4.4](#44-there-is-no-dead-pass-culling--perf--extension-medium)'s reachability analysis.

### 8.10 `renderGraphFullscreen` copies the parameter collection

`mpp/src/RenderSystem.cpp:2997`

```cpp
auto uniformCopy = parameters;
uniformCopy.bindUniforms(program);
```

A full `UniformCollection` copy — map plus heap-allocated uniform payloads — per declarative
fullscreen pass, per frame, purely because `bindUniforms` is non-const.

**Fix** — make `UniformCollection::bindUniforms` const (`mpp/src/UniformCollection.cpp`); if it
caches resolved locations internally, make that cache `mutable`.

---

## 9. Extension points

These are seams the architecture already provides. Each entry names the exact registration point.

### 9.1 New material features

`makePbrSpecializationDefines` (`mpp/src/PbrMaterialFeatures.cpp:67`) + the `PBR_SPEC_*` `#if` blocks
in `PbrShaders.h` form a complete shader-specialization system. Adding a feature requires: a new
`PbrMaterialFeature` bit, a derivation rule in `derivePbrMaterialFeatures`, a define in
`makePbrSpecializationDefines`, guarded uniform/sampler/shading blocks in `BuiltInPbrFragmentShader`,
and entries in the required-uniform/required-sampler lists in `PbrMaterial::createImpl`
(`mpp/src/PbrMaterial.cpp:183-245`). The validation there is strict in both directions — declared
features must be present in the program *and* specialized-out uniforms must be absent — so the
system fails loudly rather than silently, which makes this a safe place to extend.

High-value candidates, in glTF `KHR_materials_*` order:

| Feature | Uniforms/maps to add | Shading change |
|---|---|---|
| **Clearcoat** | `PBR_CLEARCOAT_FACTOR`, `PBR_CLEARCOAT_ROUGHNESS_FACTOR`, `PBR_CLEARCOAT_MAP`, `PBR_CLEARCOAT_NORMAL_MAP` | Second specular lobe with `f0 = 0.04`, base attenuated by `1 - F_clearcoat` |
| **Sheen** | `PBR_SHEEN_COLOUR_FACTOR`, `PBR_SHEEN_ROUGHNESS_FACTOR` | Charlie distribution + Ashikhmin visibility |
| **Anisotropy** | `PBR_ANISOTROPY_STRENGTH`, `PBR_ANISOTROPY_ROTATION` | GGX with `αt`/`αb`; tangent frame already available |
| **Transmission / IOR** | `PBR_TRANSMISSION_FACTOR`, `PBR_IOR` | Needs a scene-colour input — best as a graph image sampled by the scene pass |
| **Emissive strength** | `PBR_EMISSIVE_STRENGTH` | One multiply; pairs naturally with the existing bloom MRT |
| **Specular (KHR_materials_specular)** | `PBR_SPECULAR_FACTOR`, `PBR_SPECULAR_COLOUR_FACTOR` | Replaces the hard-coded `vec3(0.04)` at `PbrShaders.h:263` |

Two prerequisites are missing and worth doing first:

- **Multiple UV sets and texture transforms.** `BuiltInPbrVertexShader` forwards exactly one
  `TEXCOORDS` (`PbrShaders.h:16`). glTF allows a per-texture `texCoord` index and
  `KHR_texture_transform`. Add `PBR_SPEC_TEXCOORD1` plus per-map `vec4 PBR_*_TRANSFORM`
  (scale.xy, offset.xy) uniforms.
- **Vertex colours.** The mesh specification in the pipeline templates declares a `colour4` channel
  (`resources/shared/pbr/templates/Full.pipeline.xml:31-35`) but the built-in PBR vertex shader
  never reads or forwards it, so authored vertex colour is silently discarded. Add a
  `PBR_SPEC_VERTEX_COLOUR` feature multiplying into `baseColour`.
- **Skinning / morph targets.** No joint or weight handling exists in the vertex shader; this
  is the main gap for importing animated glTF content.

### 9.2 Post-processing passes

`registerBuiltInRenderGraphPasses` (`mpp/src/RenderGraphBuiltInPasses.cpp:137`) is the registration
point. A new pass needs a `RenderGraphScenePass` subclass (or a plain callback) plus a
`GraphPassAuthoringMetadata` describing its inputs, outputs, and typed parameters — which the editor
then renders automatically, so the UI cost is zero.

Note that `MPP.CustomFullscreen` (`RenderGraphBuiltInPasses.cpp:175-180`) already allows entirely
XML-authored fullscreen effects with an arbitrary program and arbitrary inputs/outputs/parameters —
so simple effects (vignette, chromatic aberration, colour grading, film grain) need **no C++ at
all**, only a `Program` resource and graph authoring. This should be called out in
`doc/PIPELINE_EDITOR_AUTHORING_GUIDE.md`; it is the cheapest extensibility win available.

Passes worth adding as built-ins:

| Pass | Inputs | Notes |
|---|---|---|
| **Depth prepass** | — | Writes only depth; enables early-Z for the main scene pass. Composes with [§8.8](#88-no-opaque-depth-sorting-or-depth-prepass). |
| **Skybox / environment background** | `samplerCube` environment | `renderEnvironmentDebugCube` (`RenderSystem.cpp:3137`) is 90% of the implementation; promote it from a debug toggle to a real pass with rotation and intensity parameters. |
| **SSAO / GTAO** | scene depth, scene normals | Requires a normals output from the scene pass — a third MRT attachment. The scene pass metadata already declares an optional "Emissive MRT" output, so the pattern exists. |
| **Bloom pyramid** | scene HDR | Downsample chain + progressive upsample; replaces the ping-pong in [§6.3](#63-bloom-extraction-uses-a-hard-knee-and-full-resolution-ping-pong--non-standard--perf-medium). `mipLevels` and `GraphSamplerBinding::mipLevel` already support per-mip attachment and sampling. |
| **Depth of field** | scene colour, scene depth | Circle-of-confusion + separable bokeh. |
| **Motion blur** | scene colour, motion vectors | Shares the motion-vector prerequisite with [§6.2](#62-taa-is-a-depth-reprojection-resolve-with-a-fixed-blend-weight--non-standard-medium). |
| **Colour grading LUT** | scene colour, 3D LUT | Blocked on 3D-texture support in `GraphImageFormat`; see [§9.5](#95-graph-resource-model). |

### 9.3 Shadows

`ShadowOptions` / `ShadowDomainState` (`mpp/include/mpp/RenderSystem.h`, implementation at
`RenderSystem.cpp:2650-2895`) is a clean, self-contained subsystem. `ShadowFilterMode` already
enumerates more than one mode, and `ShadowFrameData`'s std140 layout is asserted, so extending it is
low-risk.

- **Cascaded shadow maps.** Split the view frustum, produce an array texture or an atlas, and extend
  `ShadowFrameData` to `mat4 LIGHT_VIEW_PROJECTION[4]` + `vec4 CASCADE_SPLITS`. The shader change is
  a cascade selection from view depth plus an optional blend band. This is the single biggest
  shadow-quality improvement available.
- **Point and spot shadows.** Cube shadow maps for point lights; a single perspective map per spot.
  `CubemapFaceRenderScope` (`RenderSystem.cpp:1394`) already provides per-face rendering.
- **Filtering modes.** `ShadowFilterMode` exists; add PCSS (with a blocker-search pass) and/or a
  Poisson-disc rotated kernel. The current 3×3 PCF is hard-coded at `PbrShaders.h:177-186`; make the
  tap count a specialization define like the PBR features.
- **Per-domain light binding.** See [§2.3](#23-one-shadow-map-is-applied-to-every-directional-light--bug-medium).

### 9.4 Lighting

`PbrLights` UBO (`PbrShaders.h:112-116`) is fixed at 8 lights of type `{Directional, Point}`.

- **Spot lights.** `directionType.w` is already a float type discriminator (0 = directional,
  1 = point); 2 = spot needs only a cone angle pair. There is no spare component in the current
  layout, so this requires extending `PbrLight` to a fourth `vec4` (`vec2 coneCosines` + 2 spare) —
  do it once and reserve the spare components.
- **More than 8 lights.** Move from a UBO to an SSBO, or add clustered/tiled culling. `MaxPbrLights`
  is a single constant, so the UBO route to 32 or 64 is a two-line change plus a size check against
  `GL_MAX_UNIFORM_BLOCK_SIZE` (which `Caps` should learn to report — see
  [§4.3](#43-rendergraphcompilecaps-const-contains-a-dead-loop--bug-medium)).
- **Area lights.** LTC-based rect/disc lights need two lookup textures; they would slot in alongside
  the BRDF LUT in `PbrEnvironment`.
- **IES profiles.** A 1D/2D intensity texture per light; needs a texture-array binding.

### 9.5 Graph resource model

- **Compressed formats.** `GraphImageFormat` has no BC/ASTC entries. Not needed for render targets,
  but the *material* texture path would benefit; `TextureInternalType` would need a compressed
  branch in `Texture::loadImpl`.
- **3D textures and texture arrays.** Required for colour-grading LUTs, volumetrics, and cascaded
  shadow arrays. `RenderTextureOptions::target` already has a `TextureTarget` enum with `CubeMap`
  support (`RenderSystem.cpp:1765` gates cubemap render textures), so the pattern for adding
  `Texture3D`/`Texture2DArray` is established.
- **Buffers.** The graph has no notion of a storage buffer, which blocks compute passes, GPU
  culling, and indirect draws. Adding `GraphBufferDesc`/`GraphBufferHandle` alongside the image
  types is the prerequisite for `GraphPassType::Compute`.
- **Stencil.** `Depth24Stencil8` and `Depth32fStencil8` are declarable formats, but
  `GraphRasterState` has no stencil test/op/mask fields and `GraphRasterStateScope` neither saves
  nor restores stencil state. Any pipeline authoring a stencil format today gets an attachment it
  cannot use. Adding stencil to `GraphRasterState` is a small, self-contained task.
- **Per-image border colour and depth compare mode.** `GraphImageDesc::params` has no border colour,
  and `RenderTextureDepthParams::compareRefToTexture` is not reachable from the graph descriptor.
  This means a shadow map cannot be authored *as a graph image* — it must come in through the
  `shadowDepth` import from `RenderSystem`'s private shadow domain. Exposing both would let
  pipelines own their shadow resources, which is a natural step toward
  [§9.3](#93-shadows)'s cascades.

### 9.6 IBL

- **Reflection probes.** `PbrEnvironment` is a single global environment
  (`mpp/include/mpp/RenderPipeline.h:27`). Local probes need a probe list, per-object or per-pixel
  probe selection, and blending. Parallax-corrected box/sphere probes are the standard first step and
  require only a bounding volume per probe plus a ray-box intersection before the cubemap fetch.
- **Spherical-harmonic irradiance.** Nine `vec3` coefficients replace the 32×32 irradiance cubemap:
  cheaper to evaluate, no seams (sidestepping
  [§1.4](#14-seamless-cubemap-filtering-is-never-enabled--bug-high)), no sampler slot, and trivially
  interpolable between probes. The projection can reuse the existing face-rendering machinery with a
  readback, or be computed CPU-side from the loaded EXR.
- **Prefilter quality controls.** `IblEnvironmentCacheKey` (`mpp/include/mpp/IblEnvironmentCache.h:14`)
  already versions on resolutions and a `preprocessingVersion`. Add sample counts and the mip cap
  from [§1.1](#11-prefiltered-specular-lod-is-hard-coded-to-a-5-mip-chain--bug-high) as authorable,
  cached parameters.
- **Disk cache.** The cache is in-memory only and is cleared wholesale on any workspace change
  (`pipeline-editor/src/Main.cpp:2030`). Generating a 512³ prefiltered environment at 1024 samples is
  seconds of GPU time; serialising the generated cubemaps next to the source EXR, keyed by the
  existing cache key plus the source write time, would make HDR IBL iteration instant.
- **Additional source formats.** `validateEquirectangularConversionSource` requires a float
  Texture2D, and the document validator hard-requires `.exr`
  (`mpp/src/PbrPipelineDocument.cpp:145`). Radiance `.hdr` is the other ubiquitous IBL format and
  costs only a loader.

### 9.7 Output chain

`RenderOutputProcessor` (`mpp/src/RenderOutputProcessor.cpp`) has a clean physical-image role model
(`PhysicalOutputImageRole::{Input, Work, TaaColourHistory, TaaDepthHistory}`) that makes new stages
straightforward: add a role, allocate in `rebuild`, and insert a step in `present`.

- **SMAA** as an alternative to FXAA (needs two extra work targets and two lookup textures).
- **Contrast-adaptive sharpening** after the SSAA downsample, which pairs naturally with TAA's
  softening.
- **Per-output tone mapping and exposure.** Currently global on the pipeline; making them per-output
  would let one pipeline drive an SDR viewport and an HDR capture simultaneously.
- **Screenshot/readback output role**, which would give the editor and the CLI smoke tests a
  first-class capture path instead of reading the presentation target.

### 9.8 Editor

- Surface the allocation plan (alias groups, `estimatedPhysicalBytes`) in the process-flow view —
  see [§7.3](#73-the-editor-forces-every-image-to-be-non-transient--non-standard--testing-gap-medium).
- Live shader editing for `MPP.CustomFullscreen` programs, with recompile-on-save and diagnostics
  routed into the existing `DiagnosticBag` UI.
- A pass library/palette driven by `RenderGraphPassFactoryRegistry::getRegisteredMetadataNames`
  (`mpp/include/mpp/RenderGraphPassFactoryRegistry.h:85`), which already returns everything needed to
  populate an insert menu.
- Per-pass GPU timings already exist (`GraphPassExecutionStats::gpuMilliseconds`); a
  frame-over-frame history graph would make the cost of the bloom chain in
  [§6.3](#63-bloom-extraction-uses-a-hard-knee-and-full-resolution-ping-pong--non-standard--perf-medium)
  immediately visible.

---

## 10. Suggested verification work

Concrete tests that would have caught the bugs above, roughly in order of value per unit of effort.

1. **Serialization round-trip** (`mpp/src/RenderGraphTests.cpp`) — build a graph exercising every
   `GraphRasterState` field and every `GraphImageDesc` field, serialize with
   `RenderGraphSerializer::toFile`, reparse with `RenderGraphParser::fromFile`, and assert full
   equality. Catches [§7.1](#71-per-pass-raster-state-is-editable-and-executed-but-never-serialized--bug-high-data-loss)
   and [§7.2](#72-sampler-lodanisotropy-image-settings-are-editable-but-not-persisted-and-partly-overwritten--bug-medium).

2. **Analytic IBL tests** (`mpp/src/RenderGraphGpuTests.cpp`) — for a cubemap with one white face and
   five black faces, the irradiance at the white face's centre normal has a closed form. Assert
   against it rather than against a second run of the same code. Catches
   [§1.5](#15-the-diffuse-irradiance-convolution-double-counts-the-cosine-term--bug-medium).

3. **BRDF LUT boundary test** — sample the generated LUT at `nDotV = 1.0` and assert continuity with
   `nDotV = 0.99`. Catches [§1.3](#13-the-brdf-integration-lut-is-sampled-with-gl_repeat-wrapping--bug-high).

4. **Prefilter monotonicity test** — for a directional environment, assert that the prefiltered value
   at `roughness = 1.0` (as the *shader* would fetch it) is measurably smoother than at
   `roughness = 0.25`. §1.1 now has a source-level guard in
   `runPbrMaterialSpecializationTests`, but a behavioural GPU test would be stronger: build a
   prefiltered chain at two different `prefilterResolution` values and assert the shaded result
   matches, which is exactly the invariant the bug broke.

5. **Allocation-plan consistency test** — assert that
   `RenderGraph::buildAllocationPlan`'s `physicalAllocation` grouping agrees with the grouping
   `RenderGraphTargets::allocatePhysical` produces for the same plan, on a graph mixing transient and
   non-transient images. Catches
   [§4.1](#41-buildallocationplan-can-alias-a-transient-image-onto-a-non-transient-allocation--bug-high-latent)
   and [§4.2](#42-aliasing-compatibility-is-implemented-twice-with-different-rules).

6. **Aliasing-enabled preview mode** — run the PipelineEditor CLI validation
   (`doc/PIPELINE_EDITOR_CLI.md`) with production aliasing rather than the forced
   `transient = false`, so the smoke tests exercise the real allocator. Addresses
   [§7.3](#73-the-editor-forces-every-image-to-be-non-transient--non-standard--testing-gap-medium).

7. **NaN guard** — add a debug-only pass that scans the scene HDR target for non-finite values and
   reports the pixel count via the existing diagnostic channel. Catches
   [§2.1](#21-a-point-light-with-a-zero-direction-vector-produces-nan--bug-medium) and any future
   division-by-zero regression in the shading code.

8. **Frame-time regression harness** — `RenderGraphExecutor` already collects per-pass GPU timings
   and `DemoSuite --package-smoke-test` already renders a fixed 30 frames. Recording per-pass GPU
   milliseconds from that run into a baseline file would make the optimisations in
   [§8](#8-optimisation-opportunities) measurable and guard against regressions.

9. **Equirectangular orientation test** — the conversion at `DefaultShaders.h:212-213` maps
   `y = +1` to `v = 0`, whose correctness depends on the row order the EXR loader produces. Render a
   panorama with a known marker at the zenith and assert it lands on the +Y face. This was not
   verifiable by inspection and should be confirmed empirically. The longitude ordering *is* now
   covered by the existing `RenderGraphGpuTests` face-centre assertion; the zenith/nadir axis is not.

### 10.1 DemoSuite was not startable, so none of its suites ran

Two pre-existing defects meant `DemoSuite.exe` aborted during startup, and with it the render-graph
GPU tests — the only automated coverage of the renderer's actual GL behaviour. Both are now fixed.

- **`runMaterialResourceTests` never created its own directory.** `root` is used as a filename
  *prefix* for every other artefact (`root.string() + "_basic.xml"`) but as a *directory* for the
  glTF fixture (`root / "mpp-material-test.gltf"`). The `ofstream` silently failed and the loader
  reported a missing file. It only passed on a machine where that temp directory happened to
  already exist.
- **The equirectangular seam assertion was geometrically unsatisfiable.** It compared the +X face's
  rightmost texel against the −Z face's leftmost. Those faces meet at longitude −45°, but their
  outermost texel *centres* sit at `atan(1 − 1/8) = 41.2°` either side of it — a 7.6° gap — while
  the 8-wide source encodes 45° per texel. Worse, the fixture used the default `GL_NEAREST`, so the
  two samples quantised into different source texels and differed by a full step against a
  tolerance of 0.15. The fixture now uses linear filtering (which is what a continuity check needs,
  and what real panoramas use), the tolerance is derived from that 7.6° gap rather than being a
  magic number, and the failure message reports the measured values so a genuine seam bug is
  distinguishable from a sampling-geometry mismatch.

Both were confirmed pre-existing by reverting the working tree to `86256df` and reproducing them.
Neither is caused by the fixes in this document.
