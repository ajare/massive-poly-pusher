# Post Effects and Bloom

## Current bloom implementation

Bloom is a pipeline-owned image-space effect. It runs after every 3D material has rendered into the pipeline scene target, so it works for PBR, legacy forward, and custom materials without a material texture slot or shader change.

`PbrForward` applies bloom to its linear RGBA16F scene target **before** ACES/Reinhard tone mapping. This is the intended HDR workflow: bright emissive/direct-light values contribute to bloom before display compression.

`LegacyForward` uses the same passes on its completed RGBA8 scene target. It is supported for compatibility and visual consistency, but legacy surface shaders have already applied their existing gamma behaviour, so its threshold is not physically comparable to PBR HDR bloom.

## Enable bloom

```cpp
mpp::BloomOptions bloom;
bloom.enabled = true;
bloom.threshold = 0.7f;
bloom.intensity = 0.2f;
bloom.blurPasses = 2;

mpp::RenderPipelineOptions options;
options.mode = mpp::RenderPipelineMode::PbrForward; // LegacyForward also works
options.bloom = bloom;
renderSystem->getOrCreateRenderPipeline("PBR", options);
```

Update a live pipeline with:

```cpp
pipeline->setBloomOptions(bloom);
```

DemoSuite exposes **Bloom Enabled**, threshold, intensity, and blur-pass controls. The same options are applied to its `PBR` and `Default` pipelines.

## Bloom input and blur textures

Bloom does not use a ModelSpec or material-authored texture as its blur input. The completed pipeline scene target is sampled by the pipeline-owned bright-pass shader, which creates the texture that the blur passes consume:

```text
scene target
  -> BloomExtract target   // thresholded bright pixels
  -> BloomPing target      // horizontal blur
  -> BloomPong target      // vertical blur
  -> BloomComposite target // scene + blurred bloom
```

`RenderPipeline::ensureBloomTargets()` allocates these resize-aware RGBA16F render textures through `RenderSystem::createRenderTexture()`. `BloomExtract` is the texture that defines what is rendered into the blur targets: `FragmentShaderBloomExtractTemplate` samples the completed scene target and retains colour above `BloomOptions::threshold`.

To change what blooms, change or replace the extract shader. For example, a future effect graph could extract emissive-only output, a dedicated bloom mask attachment, or an application-defined bright-pass rule. Material textures are not bound directly to the blur pass; they first contribute to the rendered scene colour.

## Effect sequence

When enabled, the pipeline allocates four resize-aware RGBA16F intermediate targets matching the scene target:

1. **Extract** — subtracts the brightness threshold from the scene colour.
2. **Horizontal blur** — a fixed five-sample Gaussian approximation.
3. **Vertical blur** — completes one separable blur pass.
4. **Composite** — adds blurred extraction times intensity back to the original scene.

`blurPasses` repeats horizontal/vertical blur pairs. The final composite is tone mapped for PBR or presented through the existing legacy fullscreen path.

## Tuning

- **Threshold:** lower values bloom more of the image; use values around `0.7–1.5` for the current DemoSuite PBR preview.
- **Intensity:** controls only the added bloom contribution. Start around `0.1–0.3`.
- **Blur passes:** one pass is inexpensive; two is the DemoSuite default; higher values broaden the glow and increase fullscreen work.
- **PBR exposure:** adjust exposure independently. Bloom is computed before PBR tone mapping, so exposure changes final display brightness without changing extraction values.

## Required post-effect-system extensions

The existing `PostEffect` resource/stream is only a stub. Bloom is deliberately implemented as the first built-in pipeline effect while the general system is designed. A reusable post-effect graph needs the following extensions.

| Extension | Purpose | Result |
|---|---|---|
| Explicit pass interface | Declare named colour/depth inputs, outputs, format, scale, filtering, and load/store behaviour. | Effects no longer rely on implicit `RenderTarget` ownership or attachment indexes. |
| Executable effect contract | Add `resize()`, `render(context, inputs, outputs)`, and capability validation to `PostEffect`. | `RenderPipeline` can execute a heterogeneous ordered effect chain. |
| Transient target allocator | Allocate/reuse intermediate textures by descriptor and lifetime. | Ping-pong blur, downsample chains, and future effects avoid persistent one-off targets. |
| Source/output colour-space metadata | Mark linear HDR, linear LDR, and encoded display images. | Effects can enforce that bloom, exposure, and colour grading run in the correct space. |
| Input binding abstraction | Bind colour attachment, depth texture, or pipeline frame texture by semantic name. | Effects can consume scene depth, normals, shadow/debug textures, or prior effect output safely. |
| Per-effect shader/material resources | Give an effect a fullscreen program, uniforms, sampler policy, and resource lifecycle. | XML/programmatic effects can replace hard-coded built-ins. |
| Ordered composition point | Define pre-tone-map HDR, post-tone-map LDR, and post-UI stages. | Bloom remains HDR-correct; vignette, film grain, and UI effects can choose their intended stage. |
| Resize and failure handling | Recreate only affected targets, validate framebuffers, and disable an effect with diagnostics if unsupported. | Stable window resize/device-capability behaviour. |
| Profiling/debug support | Track pass timings, attachment formats, target sizes, and optional image previews. | Makes blur cost, aliasing, and bad inputs diagnosable. |
| Graph validation | Detect read/write feedback, incompatible dimensions/formats, missing inputs, and sampler-limit overflow. | Prevents undefined OpenGL feedback loops and makes custom effect errors actionable. |

## Recommended next effects

1. **Downsampled bloom pyramid** — reduces blur cost and gives wider, smoother bloom than repeatedly blurring full resolution.
2. **Exposure adaptation and colour grading** — operate in the same linear HDR pre-tone-map stage as PBR bloom.
3. **Depth-aware effects** — SSAO, fog, and outlines require the input-binding and depth-format contracts above.
4. **Post-tone-map effects** — vignette, grain, chromatic aberration, and display-space FXAA should run after tone mapping but before UI, or in an explicitly selected stage.
5. **Resource-authored effects** — complete `PostEffectStream` parsing/serialization once the runtime contract is established by built-in effects.

## Current limitations

- Bloom targets are full resolution and RGBA16F; there is no downsample pyramid or transient target reuse yet.
- The blur is deterministic and fixed-kernel; no anamorphic, dirt-mask, lens-flare, or temporal bloom options exist.
- Legacy bloom operates on its legacy LDR/gamma-oriented output and is therefore an aesthetic compatibility feature, not HDR-physical bloom.
- The generic `PostEffect` class remains unimplemented; `RenderPipeline::addPostEffect()` does not yet execute resource-authored effects.
