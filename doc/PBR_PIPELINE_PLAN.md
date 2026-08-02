# Optional PBR Pipeline Plan

## Goal

Add an opt-in, forward, metallic-roughness PBR pipeline with image-based lighting (IBL), HDR rendering, and tone mapping. Existing callers and the existing `Default` legacy forward/Phong pipeline must retain their current behaviour.

The initial implementation deliberately does **not** introduce deferred rendering or a general render graph. It establishes the target, texture, material, and pass capabilities that a render graph could use later.

## Progressive DemoSuite requirement

Every milestone is incomplete until DemoSuite is updated to visibly render `demo-suite/resources/res/statue/statue.mppmodel` through the opt-in `PBR` pipeline in the state delivered by that milestone. The preview must be selectable or enabled in DemoSuite, must not be hidden by the existing model visibility controls, and must retain previously completed PBR capabilities. A screenshot/manual validation record should accompany each checkpoint.

## Compatibility contract

- Keep `RenderSystem::getOrCreateRenderPipeline(name)` and the `Default` pipeline as legacy rendering.
- Add an options-based overload, such as `getOrCreateRenderPipeline(name, RenderPipelineOptions)`, with `LegacyForward` and `PbrForward` modes.
- Select PBR by named pipeline (for example, `"PBR"`) in the existing `renderScene(..., pipelineName)` flow.
- Do not change the legacy gamma, material, light, 2D, or UI paths as part of the PBR opt-in.

---

## Milestone 1 — Render-target foundation

**Outcome:** render targets correctly represent HDR and depth resources and safely follow display changes.

- [x] Introduce a render-texture/attachment description: dimensions, internal format, external format/type, filtering, wrapping, mipmaps, and attachment type.
- [x] Make `RenderTexture::loadImpl` honour `RenderTextureStream` format and texture parameters instead of hard-coding `GL_RGBA8`, `GL_RGBA`, `GL_UNSIGNED_BYTE`, and nearest filtering.
- [x] Support an HDR colour target (`GL_RGBA16F`, linear filtering) for the PBR scene.
- [x] Support selectable depth-only and depth-stencil attachments; retain a renderbuffer option and add a sampleable depth-texture option where needed.
- [x] Correctly delete all colour attachments, depth/stencil resources, and framebuffer resources during unload.
- [x] Expose colour attachments by index, with validation of attachment bounds.
- [x] Add target recreation/resizing and notify pipeline-owned targets when the display size changes.
- [x] Separate physical display dimensions from active render-target dimensions; `setRenderTarget()` must not overwrite display size.
- [x] Remove or give a defined owner/use to the currently unused `RenderSystem::mSceneTarget`.
- [x] Add the initial `PbrForwardPipeline` preview path: render the statue through an HDR target with temporary linear-compatible surface and presentation shaders. Later milestones replace these shaders incrementally; this establishes the opt-in path before its features are complete.

**Acceptance:** an RGBA16F target is framebuffer-complete, can be rendered to and sampled, all GL objects are released, and a resized display recreates pipeline targets at the correct dimensions.

### DemoSuite checkpoint

- [x] Add/enable a visible `PBR` pipeline preview of `statue/statue.mppmodel`, using the Milestone 1 HDR target and temporary PBR-path shaders.
- [ ] Verify the statue remains visible after a window resize and capture a baseline image.

**Implementation note (Milestone 1):** implemented on `pbr-pipeline-plan`; Debug/x64 DemoSuite builds and a 12-second smoke run completed without logged OpenGL or resource errors. The interactive resize/screenshot check remains manual validation.

---

## Milestone 2 — General texture and colour-space support

**Outcome:** materials and passes can bind all textures required by PBR and can distinguish colour from data textures.

- [x] Replace the fixed two-texture assumption in `VertexBufferRenderCommand`, `MeshInstance`, and `RenderSystem::setupRenderMeshInstance` with dynamic texture lists.
- [x] Retain the existing two-texture methods/initializers as compatibility wrappers.
- [x] Update render-state caching and sorting so arbitrary sampler counts are correct; favour program/material grouping over the present two-texture packed sort key.
- [x] Validate sampler count against `Caps::maxFragmentTextureUnits` and emit actionable errors.
- [x] Make `Texture::bind` bind its actual target rather than always `GL_TEXTURE_2D`.
- [x] Implement cube-map allocation, face upload, mipmap generation, and binding.
- [x] Add texture colour-space metadata:
  - [x] sRGB: base-colour and emissive maps;
  - [x] linear: normal, metallic-roughness, occlusion, BRDF LUT, and environment maps;
  - [x] linear HDR: radiance/environment input and HDR targets.
- [x] Map sRGB 8-bit colour textures to appropriate OpenGL sRGB internal formats.

**Acceptance:** a program with at least eight samplers receives the intended textures; a cube map can be sampled; sRGB source textures are decoded to linear shader values.

### DemoSuite checkpoint

- [x] Continue rendering the visible statue through `PBR`; bind representative linear and sRGB textures through the new sampler path and a cube-map placeholder/environment.
- [x] Add a debug/status display for active sampler count and colour-space assignments, then capture the updated preview.

**Implementation note (Milestone 2):** the statue now uses a temporary three-sampler PBR-preview material: an sRGB base texture, a linear detail texture, and an sRGB cube-map placeholder. Debug/x64 DemoSuite builds and smoke-runs without logged OpenGL or resource errors. Capture remains manual validation.

---

## Milestone 3 — PBR resource contracts

**Outcome:** authored resources can express standard metallic-roughness materials and supply the geometry they require.

- [ ] Extend `MaterialSpecification`/material resources with an optional PBR surface definition:
  - [ ] base-colour, metallic, roughness, emissive factors;
  - [ ] normal scale and ambient-occlusion strength;
  - [ ] base-colour, metallic-roughness, normal, occlusion, and emissive texture slots;
  - [ ] alpha mode (`opaque`, `mask`, `blend`), alpha cutoff, and double-sided state.
- [ ] Provide neutral fallback textures and default factor values so every PBR texture is optional.
- [ ] Add a `Tangent4` vertex semantic, or an equivalent documented `TANGENT` vec4 named attribute, including parser and serialization support.
- [ ] Update model creation and `model-convert`/Assimp integration to import or generate tangents from positions, normals, and UV0.
- [ ] Preserve current model/material resources and their legacy shader selection.
- [ ] Add a later, optional glTF 2.0 material mapping task after the resource contract is proven with native resources.

**Acceptance:** a PBR material can be authored with only factors or with all five maps; normal-mapped geometry exposes a valid tangent frame; legacy material files still load unchanged.

### DemoSuite checkpoint

- [ ] Add a PBR material for `statue/statue.mppmodel` (or a PBR material override for each statue mesh) and render it visibly through `PBR`.
- [ ] Exercise factor-only and texture-backed variants, including normal mapping when tangents are available, and capture the result.

---

## Milestone 4 — PBR lighting and environment resources

**Outcome:** PBR shaders have a stable direct-light and IBL contract without changing legacy lighting APIs.

- [ ] Define a dedicated PBR frame-light UBO, separate from the existing ambient-plus-two-light UBO.
- [ ] Support a documented bounded count of directional and point lights with colour/radiance, position or direction, range, and inverse-square attenuation.
- [ ] Choose the initial fixed light limit from OpenGL 3.2 UBO limits and expose overflow diagnostics.
- [ ] Define `PbrEnvironment` resources containing:
  - [ ] diffuse irradiance cube map;
  - [ ] roughness-mip prefiltered specular cube map;
  - [ ] 2D split-sum BRDF integration LUT;
  - [ ] optional background/skybox cube map.
- [ ] Initially support precomputed IBL assets; schedule GPU panorama-to-IBL preprocessing as a follow-up feature.
- [ ] Add HDR panorama/image decoding or a documented preconverted environment-asset format.

**Acceptance:** direct light intensity and distance behave physically plausibly, and the same PBR object visibly responds to diffuse and specular IBL across roughness values.

### DemoSuite checkpoint

- [ ] Render the visible PBR statue under the PBR light UBO and selected IBL environment.
- [ ] Provide a DemoSuite environment/light selector or equivalent fixed demonstrator, and verify direct, diffuse-IBL, and specular-IBL contributions on the statue.

---

## Milestone 5 — PBR shaders and HDR presentation

**Outcome:** the renderer performs physically based shading in linear HDR and only encodes display colour at the final step.

- [ ] Add PBR vertex shader templates that provide world position, transformed normal, UV0, tangent frame, and view direction inputs.
- [ ] Add metallic-roughness fragment shader templates using Cook-Torrance BRDF:
  - [ ] GGX normal-distribution function;
  - [ ] Smith visibility term;
  - [ ] Schlick Fresnel;
  - [ ] direct directional/point lights;
  - [ ] irradiance, prefiltered-specular, and BRDF-LUT IBL;
  - [ ] normal, AO, emissive, alpha-mask, and double-sided handling.
- [ ] Keep all PBR lighting and compositing values linear in the HDR scene target.
- [ ] Add a fullscreen tone-map shader, initially ACES or Reinhard, with exposure and one final gamma encode.
- [ ] Do not apply the legacy per-surface gamma transform in the PBR shader path.

**Acceptance:** high-radiance input is retained in the HDR target; changing exposure changes the final presentation without changing material lighting; no double gamma correction occurs.

### DemoSuite checkpoint

- [ ] Replace the temporary PBR-path shaders used by the statue with the Cook-Torrance and tone-map shaders.
- [ ] Expose at least exposure and tone-map selection in DemoSuite; verify that the visible statue has no double-gamma artefacts and capture comparison images.

---

## Milestone 6 — Optional PBR forward pipeline integration

**Outcome:** PBR is selectable through the current scene-rendering process while `Default` remains unchanged.

- [ ] Add `RenderPipelineMode` and `RenderPipelineOptions` with `LegacyForward` as the default.
- [ ] Implement `PbrForwardPipeline` as a specialised `RenderPipeline`, or dispatch internally from `RenderPipeline` based on its immutable options.
- [ ] Allocate and own a resize-aware RGBA16F scene target per PBR pipeline.
- [ ] Render PBR opaque and masked objects to the HDR target with depth writes enabled.
- [ ] Render PBR blended objects after opaque geometry, sorted back-to-front with appropriate blend/depth-write state.
- [ ] Bind PBR frame data and environment textures around the PBR scene flush.
- [ ] Tone-map the HDR output to the screen, then continue with the existing 2D/UI rendering flow.
- [ ] Keep the legacy `RenderPass` behaviour intact. Do not make its incomplete post-effect support a dependency of PBR.
- [ ] Document pipeline setup and selection, including environment ownership and resize behaviour.

**Example intended usage:**

```cpp
RenderPipelineOptions options;
options.mode = RenderPipelineMode::PbrForward;
options.environment = environment;
renderSystem->getOrCreateRenderPipeline("PBR", options);
renderSystem->renderScene(scene, camera, {}, "PBR");
```

**Acceptance:** a scene can render via `"PBR"` and `"Default"` in the same application; the default pipeline’s output and existing call sites are unchanged.

### DemoSuite checkpoint

- [ ] Make `PBR` and `Default` selectable in DemoSuite and ensure `statue/statue.mppmodel` is visible when `PBR` is selected.
- [ ] Verify the same application can switch modes without hiding the statue or regressing legacy scenes; capture both outputs.

---

## Milestone 7 — Validation, demo, and documentation

**Outcome:** PBR correctness and legacy compatibility are observable and regressions are caught.

- [ ] Add a PBR demo scene containing a metallic/roughness sphere grid, normal-mapped object, emissive object, alpha-mask object, alpha-blended object, and an IBL environment.
- [ ] Add controls for exposure, tone-map operator, environment, and PBR/legacy pipeline selection.
- [ ] Add target-format, attachment-lifetime, resize, cube-map, sampler-limit, and sRGB/linear tests where unit coverage is feasible.
- [ ] Capture visual reference images for the PBR demo and compare them in a repeatable graphics test/manual validation procedure.
- [ ] Re-run existing demo scenes against the legacy `Default` pipeline and verify unchanged behaviour.
- [ ] Document material slots, texture colour-space rules, tangent requirements, environment asset format, light limits, and the PBR opt-in API.

**Acceptance:** the demo demonstrates the expected metallic/roughness and IBL response, tests cover the new resource foundations, and existing legacy scenes remain functional.

### DemoSuite checkpoint

- [ ] Make the statue PBR preview a permanent, visible DemoSuite regression scenario, including its environment and documented setup.
- [ ] Record the final reference image and manual validation steps for the visible statue in both `PBR` and `Default` modes.

---

## Deferred follow-up work

These are intentionally outside the initial forward-PBR milestone set:

- General render graph with declared pass inputs/outputs, load/store operations, and MRT.
- Bloom, screen-space effects, and a completed `PostEffect` implementation.
- GPU panorama conversion and IBL prefilter generation.
- Deferred, tiled, clustered, or Forward+ light management.
- Shadows, reflection probes, screen-space reflections, and transmission/clearcoat material extensions.
- Full native glTF scene/material import.
