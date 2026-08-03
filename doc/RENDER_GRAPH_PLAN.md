# Render Graph, Attachment, and MRT Design Plan

## Goal

Replace hand-wired render-target sequencing with an opt-in render graph that declares pass inputs, outputs, attachment load/store operations, target descriptors, and multiple-render-target (MRT) writes. The graph must support the existing PBR scene, shadow, bloom, tone-map, and UI flow without changing the legacy `Default` pipeline until that pipeline is explicitly migrated.

This is an architectural follow-up to the current `RenderPipeline` implementation. The current path remains valid while the graph is introduced incrementally.

## Why this is needed

Today `RenderPipeline` manually creates, binds, and orders scene, shadow, bloom-extract, bloom-ping, bloom-pong, bloom-composite, and presentation targets. That is workable for a short fixed chain, but it has no declarative validation of:

- which pass produces a texture consumed by another pass;
- whether a pass reads and writes the same attachment;
- whether dimensions, samples, formats, and colour spaces are compatible;
- when an intermediate target can be released or reused;
- whether a shader's declared outputs match the active framebuffer attachments; or
- whether an MRT shader has enough draw buffers/attachments on the current GPU.

A graph makes these requirements explicit, then compiles a safe execution plan.

## Compatibility contract

- `RenderSystem::getOrCreateRenderPipeline("Default")` retains its current non-graph legacy path until explicitly migrated.
- The graph is selected by new immutable `RenderPipelineOptions`/pipeline mode options; it must not silently alter existing callers.
- Existing `RenderTexture`, `RenderPass`, `PostEffect`, `ResourceManager`, material, shader markup, and named pipeline APIs remain source-compatible during migration.
- The first graph consumer is an opt-in PBR pipeline. Legacy materials can render into a graph scene pass later because graph resources are material-agnostic.
- UI/2D remains after scene presentation initially. It is imported as an external screen target, not forced into the first graph milestone.
- OpenGL 3.2 remains the baseline. Use framebuffer ordering and attachment rebinding; do not require Vulkan-style barriers or OpenGL features unavailable on the baseline.

## Core design

### Resource handles and descriptors

Passes never hold a raw `Texture*` or `RenderTargetPtr` as a dependency. They use lightweight, versioned graph handles:

```cpp
struct GraphImageHandle { uint32_t id; uint32_t version; };
struct GraphPassHandle  { uint32_t id; };

enum class GraphImageFormat { Rgba8, Rgba16f, Rg16f, Depth24, Depth24Stencil8 };
enum class GraphImageUsage : uint32_t
{
    Sampled = 1 << 0,
    ColourAttachment = 1 << 1,
    DepthAttachment = 1 << 2,
    Presentation = 1 << 3
};

struct GraphImageDesc
{
    GraphImageFormat format;
    glm::uvec2 absoluteSize{ 0 };       // used when scale is zero
    glm::vec2 relativeSize{ 1.0f };     // relative to graph viewport
    uint32_t samples{ 1 };
    uint32_t mipLevels{ 1 };
    GraphImageUsage usage;
    TextureParams params;
    TextureColourSpace colourSpace;
    bool transient{ true };
};
```

A handle version increments when a pass writes an image. This makes an accidental same-version read/write visible to validation. Imported/external images (screen, a caller-owned environment map, or a persistent history target) are explicitly marked non-transient.

The first implementation uses 2D single-sample images only. Cube maps, arrays, multisample resolve, 3D textures, and mip-subresource views are documented extensions, not implied by the first API.

### Pass declaration

A pass is declarative until graph compilation:

```cpp
enum class GraphLoadOp  { Load, Clear, DontCare };
enum class GraphStoreOp { Store, DontCare };

struct GraphColourAttachment
{
    GraphImageHandle image;
    GraphLoadOp load{ GraphLoadOp::DontCare };
    GraphStoreOp store{ GraphStoreOp::Store };
    glm::vec4 clearColour{ 0.0f };
};

struct GraphDepthAttachment
{
    GraphImageHandle image;
    GraphLoadOp load{ GraphLoadOp::DontCare };
    GraphStoreOp store{ GraphStoreOp::Store };
    float clearDepth{ 1.0f };
};

class RenderGraphBuilder
{
public:
    GraphImageHandle createImage(std::string_view name, GraphImageDesc const& desc);
    GraphImageHandle importImage(std::string_view name, RenderTargetPtr target, GraphImageDesc const& desc);
    GraphPassHandle addPass(std::string_view name, GraphPassType type,
        std::function<void(GraphPassBuilder&)> declare,
        std::function<void(RenderGraphContext&)> execute);
};
```

`GraphPassBuilder` has distinct methods for `readSampled()`, `readDepth()`, `writeColour()`, `writeDepth()`, and `readWrite()` (the last is rejected initially unless a future input-attachment/subpass implementation supports it). The execution callback receives resolved image views and must not bind graph-owned targets directly.

### XML graph declaration

Graphs must be expressible as a resource so applications can author a fixed pipeline without C++ graph-builder code. Programmatic construction remains the preferred route for dynamic topology, but both paths compile to the same `RenderGraphBuilder` representation.

Declare an XML graph resource with named images and ordered pass declarations. Inputs/outputs refer to image names; an output creates the next version of that image. The parser resolves names to versioned handles and rejects a pass that reads an image version it also writes.

```xml
<RenderGraph>
    <Images>
        <Image name="SceneHdr" format="RGBA16F" colourSpace="LINEAR_HDR"
               scale="1.0 1.0" transient="false" usage="colourAttachment,sampled" />
        <Image name="SceneDepth" format="DEPTH24" colourSpace="LINEAR"
               scale="1.0 1.0" transient="false" usage="depthAttachment,sampled" />
        <Image name="BloomExtract" format="RGBA16F" colourSpace="LINEAR_HDR"
               scale="0.5 0.5" transient="true" usage="colourAttachment,sampled" />
        <Image name="BloomPing" format="RGBA16F" colourSpace="LINEAR_HDR"
               scale="0.5 0.5" transient="true" usage="colourAttachment,sampled" />
        <Image name="Presented" format="RGBA8" colourSpace="DISPLAY"
               external="screen" transient="false" usage="presentation" />
    </Images>

    <Passes>
        <Pass name="Scene" type="scene" stage="linearHdr">
            <Colours>
                <Output image="SceneHdr" load="clear" store="store" clear="0 0 0 1" />
            </Colours>
            <Depth image="SceneDepth" load="clear" store="store" clear="1" />
        </Pass>

        <Pass name="BloomExtract" type="fullscreen" shader="Effects.BloomExtract" stage="linearHdr">
            <Inputs>
                <Sampled semantic="TEX1" image="SceneHdr" />
            </Inputs>
            <Colours>
                <Output image="BloomExtract" load="dontCare" store="store" />
            </Colours>
            <Parameters>
                <Float name="THRESHOLD" value="0.7" />
            </Parameters>
        </Pass>

        <Pass name="BloomBlurHorizontal" type="fullscreen" shader="Effects.BloomBlur" stage="linearHdr">
            <Inputs><Sampled semantic="TEX1" image="BloomExtract" /></Inputs>
            <Colours><Output image="BloomPing" load="dontCare" store="store" /></Colours>
            <Parameters><Vec2 name="DIRECTION" value="1 0" /></Parameters>
        </Pass>
    </Passes>
</RenderGraph>
```

The exact element spelling may evolve, but these rules are required:

- `Image` declares format, size (`scale` or absolute width/height), colour space, usage, filtering/wrapping, sample count, mip count, and transient/external lifetime.
- `Pass` declares `type` (`scene`, `fullscreen`, `compute` when supported, `present`), stage, shader/program resource, inputs, colour outputs in MRT location order, optional depth output, and typed parameters.
- `Output` declares `load`/`store` and clear values. Multiple ordered `Output` elements map to `layout(location = 0..n)` for MRT.
- `Sampled semantic` maps a graph image to a shader sampler name. The parser validates the named sampler/program contract at load/compile time.
- XML may reference a named resource (`shader="Effects.BloomExtract"`) or define a child program resource using the existing program XML conventions.
- `external="screen"` and future named imported resources are the only way XML can refer to non-transient images; arbitrary raw GL names are never serialized.
- Scene passes may use a named scene-render callback/factory registered by the application. XML cannot serialize arbitrary C++ lambdas.

`RenderGraphStream` and a programmatic `ProgrammaticRenderGraphStream` should mirror existing resource stream patterns. Parsed graph resources are immutable topology templates; runtime code can supply a viewport, imported targets, scene callback, and parameter overrides before compilation.

### Compilation and execution

1. **Build:** pipeline code or an XML graph template creates images and declares passes.
2. **Validate:** reject missing producer, format/type mismatch, same-version read/write feedback, unsupported format/usage, invalid attachment load/store combinations, and unsupported MRT count.
3. **Topologically sort:** derive pass order from handle dependencies; report a named cycle.
4. **Calculate lifetimes:** first write through last read for every transient version.
5. **Allocate/alias:** assign compatible non-overlapping transient lifetimes to pooled `RenderTexture`/attachment allocations. First implementation may allocate one target per image; aliasing is enabled only after lifetime tests pass.
6. **Execute:** bind the compiled framebuffer, set draw buffers, perform clear operations, invoke callback, then apply store/discard policy and release expired transient allocations.
7. **Present:** graph output is handed to the existing PBR tone-map or legacy presentation step. The screen is treated as an imported external target.

Graphs should be cached by pipeline topology and recompiled when viewport size, effect options, attachment descriptor, or capability set changes. Dynamic uniform values (exposure, bloom threshold, shadow matrix) update execution state without recompiling topology.

## Attachment load/store semantics

`GraphLoadOp` and `GraphStoreOp` describe required contents, not a promise that every driver can physically discard memory:

| Operation | Required behaviour | Initial OpenGL implementation |
|---|---|---|
| `Load` | Preserve previous contents. | Bind without clear. |
| `Clear` | Replace attachment with supplied clear value. | `glClearBuffer*` after framebuffer bind. |
| `DontCare` load | Previous contents are never read. | Do not clear; optionally invalidate when an available extension can do so safely. |
| `Store` | Later pass/external consumer may read results. | Keep attachment alive until last reader. |
| `DontCare` store | No later consumer requires results. | Release transient allocation at lifetime end; optional invalidate is an optimization. |

Do not depend on `glInvalidateFramebuffer` for correctness because it is newer than the baseline. It may be an optional optimization after capability detection.

## MRT design

An MRT pass declares more than one `GraphColourAttachment`. Compilation:

- verifies attachment count against `Caps::maxDrawBuffers` and `Caps::maxColourAttachments` (add these capability fields if absent);
- verifies every colour attachment has identical dimensions and sample count;
- creates/binds one framebuffer with `GL_COLOR_ATTACHMENT0 + n` attachments;
- calls `glDrawBuffers()` in declared output order;
- validates framebuffer completeness; and
- verifies shader output locations against the attachment count, initially by requiring explicit `layout(location = n)` in graph/MRT shaders.

First MRT PBR use case:

```text
PBR Scene MRT pass
  colour[0] = SceneHdr       (RGBA16F, linear HDR)
  colour[1] = BloomMaskHdr   (RGBA16F or R11F_G11F_B10F, linear HDR)
  depth      = SceneDepth
```

This allows bloom extraction to sample a deliberate emissive/bright mask instead of thresholding all scene colour. It remains optional: the initial migrated graph can retain the current threshold-based extract pass and add MRT only after shader/material output contracts are ready.

Do not use MRT to force deferred rendering. MRT is a forward-PBR enhancement and may coexist with a forward colour-only legacy pass.

## Colour-space and stage rules

Every graph image records intended colour space/stage:

- PBR scene, shadow-independent bloom mask, bloom intermediates, and composite are **linear HDR**.
- PBR tone map consumes linear HDR and writes encoded presentation colour.
- Legacy scene output is initially **legacy display-oriented/LDR**; legacy bloom is supported as an aesthetic compatibility effect, not physically comparable HDR bloom.
- UI is composited after presentation in the initial graph integration.

The compiler must reject using encoded display colour as an HDR bloom input unless a pass explicitly declares conversion. This prevents accidental double gamma/tone mapping.

## Migration plan

### RG1 — Graph data model and validation foundation

- [ ] Add `RenderGraph`, `RenderGraphBuilder`, handles, image descriptors, pass descriptors, compiled graph types, `RenderGraphStream`, and `ProgrammaticRenderGraphStream` under `mpp/include/mpp` and `mpp/src`.
- [ ] Extend `Caps` with draw-buffer/colour-attachment limits and validate graph requirements.
- [ ] Add graph image format mapping to the existing render-texture format system.
- [ ] Support imported targets and transient 2D colour/depth images at absolute/relative sizes.
- [ ] Implement dependency sorting, version validation, cycle diagnostics, attachment/dimension checks, and load/store clear semantics.
- [ ] Add a debug dump describing passes, image versions, lifetimes, formats, and allocations.
- [ ] Parse/serialize XML graph templates, including typed parameters, image descriptors, load/store operations, sampler semantics, child/external resource references, and MRT output order.
- [ ] Unit-test graph validation and XML diagnostics without an OpenGL context where feasible.

**Acceptance:** a synthetic graph rejects feedback/cycle/format errors with named diagnostics and compiles a valid two-pass colour chain.

### RG2 — OpenGL target allocator and execution context

- [ ] Add pooled graph attachment allocation/reuse backed by `RenderTexture` extensions.
- [ ] Bind graph framebuffers, configure draw/read buffers, clear declared attachments, and expose read-only image views to execution callbacks.
- [ ] Add one colour plus optional depth attachment execution first; retain existing `RenderTexture` ownership APIs as compatibility wrappers.
- [ ] Add resize invalidation and GL object-lifetime tests.
- [ ] Add GPU debug labels containing graph/pass/image names.

**Acceptance:** a graph executes two fullscreen passes at resized dimensions without framebuffer errors or leaked targets.

### RG3 — Migrate PBR scene, presentation, and bloom

- [ ] Add an opt-in `RenderPipelineMode::GraphPbrForward` (or equivalent explicit graph option); do not replace current `PbrForward` until validated.
- [ ] Express shadow depth, HDR scene, bloom extract/blur/composite, and tone-map presentation as graph passes.
- [ ] Import the screen/presentation target and retain current UI ordering.
- [ ] Replace pipeline-owned bloom target fields with graph images once outputs match existing bloom.
- [ ] Compare current PBR and graph-PBR bloom screenshots/RenderDoc captures.

**Acceptance:** graph PBR renders the statue, shadows, bloom, and tone mapping equivalently to current PBR with no material changes.

### RG4 — Add optional PBR bloom-mask MRT

- [ ] Add shader-output markup/location support required by graph MRT passes.
- [ ] Extend the PBR scene shader to optionally output scene HDR plus an emissive/bright bloom mask.
- [ ] Add an MRT material/program variant and capability fallback to threshold extraction.
- [ ] Validate attachment count, format, and location mapping on supported GPUs.
- [ ] Add DemoSuite controls to select threshold bloom versus bloom-mask MRT and show capability/status diagnostics.

**Acceptance:** an emissive object blooms through the mask while unrelated bright albedo can be excluded; unsupported hardware falls back safely.

### RG5 — Migrate legacy/custom forward passes

- [ ] Add a graph legacy-forward scene pass that imports existing legacy light/material state but writes a declared colour/depth image.
- [ ] Keep default-created `Default` pipeline on the old path; expose graph legacy only by explicit option.
- [ ] Verify PBR and legacy materials can appear in the same graph scene/dependency chain where shaders/targets are compatible.
- [ ] Document legacy LDR bloom limitations and optional linear-legacy migration.

**Acceptance:** graph legacy preserves existing output with bloom disabled and can opt into the same graph bloom chain.

### RG6 — Resource-authored effects and optimizations

- [ ] Replace the stub `PostEffect` execution model with graph-pass declaration/execution APIs.
- [ ] Extend `PostEffectStream`/serialization for input semantics, output descriptor, uniforms, shader resource, stage, and quality settings; allow a `PostEffect` to reference an XML `RenderGraph` subgraph template.
- [ ] Support application-registered XML scene-pass factories and reject unregistered scene callback names with named diagnostics.
- [ ] Add transient lifetime aliasing after correctness tests; add optional framebuffer invalidation/discard optimization.
- [ ] Add pass timing, attachment previews, graph JSON/dot dump, and RenderDoc labels.
- [ ] Migrate built-in bloom to a resource-authored/reference effect only after the custom-effect contract is stable.

**Acceptance:** an application can declare a custom fullscreen effect resource, attach it to a graph pipeline, resize it safely, and receive actionable validation errors.

## DemoSuite and test requirements

At each completed migration milestone:

- render the statue, legacy floor/walls/cube, shadows, light marker, and bloom through the selected graph path;
- retain current non-graph `PBR` and `Default` selections for comparison;
- report graph pass order, attachment dimensions/formats, MRT capability, bloom stage, and allocation count in the UI/debug output;
- capture PBR no-bloom/current-bloom/graph-bloom/MRT-bloom reference images; and
- run Debug and Release smoke tests with matching rebuilt DLLs.

## Risks and mitigation

| Risk | Mitigation |
|---|---|
| Large rewrite regresses PBR/legacy rendering | Introduce graph modes alongside current paths and compare captures before migration. |
| Read/write texture feedback causes undefined output | Versioned handles and compile-time feedback validation; no raw graph target binding in callbacks. |
| MRT capability/format mismatch | Query caps, validate formats/dimensions/output count, and provide single-target fallback. |
| Excessive memory from intermediates | Relative-size descriptors, lifetime analysis, pooling, then tested aliasing/downsampling. |
| Incorrect gamma/tone-map ordering | Explicit colour-space/stage metadata and PBR HDR regression images. |
| Custom effects make errors opaque | Named handles/passes, graph dumps, shader output validation, and debug labels. |
| OpenGL baseline lacks discard/barrier features | Treat store/discard as lifecycle semantics first; use optional capabilities only as optimizations. |

## Deferred work

- Multisample render images and explicit resolve passes.
- Texture arrays, cube/3D image views, mip-level/subresource dependencies.
- Compute-shader passes and explicit image/buffer barriers.
- Temporal-history resources for TAA/exposure adaptation.
- Full deferred/tiled/clustered renderer built on MRT.
