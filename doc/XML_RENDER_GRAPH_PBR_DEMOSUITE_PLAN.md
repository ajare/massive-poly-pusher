# DemoSuite XML Render Graph PBR Replica Plan

## Goal

Add a DemoSuite-selectable **PBR (XML render graph)** pipeline that reproduces the current manual `PBR` output using XML graph topology, named imports, named scene-pass factories, and existing PBR/shadow/bloom/tone-map rendering code. Manual `PBR` remains the reference path.

## 1. Per-frame graph execution context

Create:

```cpp
struct RenderGraphFrameContext {
    RenderSystem* renderSystem;
    ScenePtr scene;
    CameraPtr camera;
    std::vector<SceneModel3dPtr> visibleModels;
    RenderPipelineOptions const* pipelineOptions;
};
```

Expose it through `RenderGraphExecutionContext`. `RenderPipeline` builds it immediately before graph execution; no live scene/camera objects are serialized.

**Status:** implemented through `RenderGraphFrameContext`, provided by `RenderPipeline` per graph frame.

**Acceptance:** factory-created passes can access the scene, camera, visible models, options, and renderer.

## 2. Built-in graph pass factories

Implement/register these `RenderGraphScenePass` factories:

| Factory | Behaviour |
|---|---|
| `MPP.ShadowDepth` | Render the configured shadow domain with visible models. |
| `MPP.PbrScene` | Render visible models through the existing PBR `RenderPass`; flush batches. |
| `MPP.BloomExtract` | Call existing `renderBloomExtract()`. |
| `MPP.BloomBlurHorizontal` | Call `renderBloomBlur(..., {1, 0})`. |
| `MPP.BloomBlurVertical` | Call `renderBloomBlur(..., {0, 1})`. |
| `MPP.BloomComposite` | Call `renderBloomCombine()`. |
| `MPP.ToneMapPresent` | Call `renderToneMappedFullscreenQuad()`. |

**Status:** complete. Scene, shadow, bloom extract/blur/composite, and tone-map presentation factories are implemented and registered by `RenderPipeline`.

Factories use `RenderGraphExecutionContext` plus `RenderGraphFrameContext` only.

**Acceptance:** replacing current hardcoded GraphPBR callbacks with factories does not change output.

## 3. Named runtime imports

Register imports before execution:

| XML import | Runtime target |
|---|---|
| `screen` | `RenderSystem::getScreenRenderTarget()` |
| `shadowDepth` | named shadow-domain depth target |
| `external.*` | application-defined target |

Execution sequence: allocate graph targets, populate `RenderGraphImportRegistry`, then call `RenderGraphTargets::bindImports()`.

**Status:** complete for the XML PBR graph. `screen` and `shadowDepth` are registered and resolved at runtime.

**Acceptance:** XML uses names only; it never contains raw GL IDs or runtime handles.

## 4. XML PBR graph resource

Create `demo-suite/resources/res/PbrPipeline.rendergraph.xml`.

Topology:

```text
ShadowDepth
  -> SceneHdr + SceneDepth
  -> BloomExtract
  -> BloomBlurHorizontal / BloomBlurVertical x N
  -> BloomComposite
  -> ToneMapPresent -> screen
```

Declare HDR scene/bloom images, LDR imported presentation image, factory names, typed bloom/tone-map parameters, and sampler bindings. Scene shading remains the existing material system.

**Status:** complete. `PbrPipeline.rendergraph.xml` declares a four-iteration threshold-bloom chain; runtime pass-through makes the UI-selected blur count effective without rebuilding the immutable template. `PbrPipelineMrt.rendergraph.xml` starts the same blur chain from `BloomMaskHdr` at scene output location 1. DemoSuite loads/deploys both templates, selects MRT only when requested and validated against hardware plus every visible material program, and otherwise falls back to threshold extraction.

**Acceptance:** `FileRenderGraphStream` loads and validates the graph before rendering.

## 5. Template-backed RenderPipeline mode

**Status:** complete. `XmlGraphPbrForward` executes a loaded `RenderGraphTemplate`; the separate `GraphPbrForward` mode intentionally retains its C++ topology as a comparison/reference path.

Add optional graph-template resource/name to `RenderPipelineOptions` and a distinct `XmlGraphPbrForward` mode.

At runtime:

1. Resolve/load `RenderGraphTemplate`.
2. Build allocation plan for viewport.
3. Allocate/reuse graph targets.
4. Bind imports.
5. Register built-in pass factories.
6. Create frame context.
7. Execute template through `RenderGraphExecutor`.

Keep `GraphPbrForward` hardcoded until output parity is confirmed.

**Acceptance:** XML mode does not rebuild hardcoded topology in `RenderPipeline`.

## 6. DemoSuite selector and diagnostics

**Status:** DemoSuite now offers manual PBR, hardcoded GraphPBR, and `PBR (XML graph)` selections. Graph diagnostics are not wired yet.

Expose:

- `PBR (manual reference)`
- `PBR (hardcoded graph)`
- `PBR (XML graph)`
- `Default (manual reference)`
- `Default (render graph)`

Show XML graph load/validation state, pass order, allocation count, MRT capability, bloom source, and imports.

**Acceptance:** pipeline switching requires no restart and retains PBR controls.

## 7. Validation

**Status:** visual parity confirmed by the user for manual, hardcoded-graph, and XML-graph pipelines. No automated screenshot comparison or archived RenderDoc capture exists yet.

Compare manual PBR and XML PBR with bloom disabled/enabled, varied exposure/tone map, varied shadow settings, window resize, and MRT bloom-mask mode. Capture screenshots and RenderDoc frames.

**Acceptance:** visually accepted for current DemoSuite settings. Automated screenshot tolerance, leak checks, and archived RenderDoc validation remain follow-up hardening.

## 8. Follow-up after parity

After validation, remove duplicated hardcoded GraphPBR topology, add XML legacy template support, add resource-authored custom effects, and add automated GPU/screenshot regression tests.
