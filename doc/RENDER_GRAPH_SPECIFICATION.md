# Render Graph Specification

**Status:** implemented API and XML format on `render-graph-plan`.

This document specifies the MassivePolyPusher render graph as it exists today. It covers the context-free graph description, target allocation, XML resource templates, execution, application pass factories, and current restrictions. `doc/RENDER_GRAPH_PLAN.md` is design history; this document is the authoring reference.

## 1. Model and terminology

A graph is an immutable, declared rendering topology. It has two kinds of declared object:

| Object | C++ type | XML element | Purpose |
| --- | --- | --- | --- |
| Image | `GraphImageDesc`, `GraphImageHandle` | `Images/Image` | A logical colour or depth attachment, possibly an externally owned target. |
| Pass | `GraphPassInfo`, `GraphPassHandle` | `Passes/Pass` | A graphics operation that samples image versions and creates later versions by writing attachments. |

At runtime, additional objects provide the executable and storage:

| Runtime object | Class | Purpose |
| --- | --- | --- |
| Graph topology resource | `RenderGraphTemplate` | Holds an immutable loaded graph and resolves named `Program` resources. |
| Attachment owner | `RenderGraphTargets` | Allocates, pools, aliases, imports, and resolves physical targets. |
| Executor | `RenderGraphExecutor` | Compiles the graph, makes pass FBO views, performs clears/resolves, and invokes pass code. |
| Import table | `RenderGraphImportRegistry` | Maps an XML import name to an application-owned `RenderTarget`. |
| Factory table | `RenderGraphPassFactoryRegistry` | Maps a stable XML factory identifier to C++ work. |
| Frame state | `RenderGraphFrameContext` | Per-frame scene/camera/application state made available to scene-pass factories. |

The graph itself contains no OpenGL objects, callbacks, scene pointers, or application state. This makes `RenderGraph::compile()` and `buildAllocationPlan()` usable without an OpenGL context.

### 1.1 Versioned images and dependencies

Every image starts at version `0`. Writing it returns a new `GraphImageHandle` with the same image id and its next version. A later pass must read or write the returned handle, not an older handle, when it needs the new content.

A sampled input creates a producer-to-consumer dependency from the pass that produced that exact version. The compiler topologically sorts those dependencies. Declaration order only breaks ties between otherwise independent passes; it is **not** a way to override a dependency.

For example, this sequence is valid:

```text
Scene@0 --ScenePass writes--> Scene@1 --BloomPass samples--> Bloom@1
```

A pass may write the latest version of an image, including an imported image. It may not write a stale version. A pass may not sample the exact version it produces; that is read/write feedback and compilation fails.

## 2. Image declarations

### 2.1 Image fields

The following fields are represented by `GraphImageDesc`. XML uses child elements rather than attributes.

| Field / XML child | C++ type | Values and range | Default | Notes |
| --- | --- | --- | --- | --- |
| `name` | `std::string` | Non-empty; unique among graph images. | Required | Case-sensitive lookup name in XML. |
| `format` | `GraphImageFormat` | `RGBA8`, `RGBA16F`, `RG16F`, `DEPTH24`, `DEPTH24_STENCIL8`. | Required in XML; C++ default `Rgba8` | Depth formats are depth-only; colour formats are colour-only. |
| `usage` | bitmask `GraphImageUsage` | Comma-separated `sampled`, `colourAttachment`, `depthAttachment`, `presentation`. | Required in XML; C++ default `None` | Token spelling is case-insensitive in XML. |
| `width`, `height` | `glm::uvec2 absoluteSize` | Positive unsigned integer dimensions. Specify both or neither. | `0`, `0` | If both are non-zero, allocation uses these pixels rather than `scale`. |
| `scale` | `glm::vec2 relativeSize` | Each component must be `> 0`; normally `0 < value <= 1`. | `1 1` | Multiplied by the allocation viewport and truncated, with minimum one pixel. Values above one are accepted. |
| `samples` | `uint32_t` | `1..Caps::maxSamples`. | `1` | `> 1` allocates an MSAA write target and a single-sample resolved target. |
| `mipLevels` | `uint32_t` | `1..floor(log2(max(width,height))) + 1`. | `1` | Must not exceed dimensions after viewport/scale resolution. Must be `1` when `samples > 1`. |
| `colourSpace` | `TextureColourSpace` | `LINEAR` / `LINEAR_HDR`, or `SRGB` / `DISPLAY`. | `LINEAR` | A texture/storage interpretation setting; use linear HDR targets before tone mapping. |
| `minFilter` | GL filter enum | `NEAREST`, `LINEAR`, `NEAREST_MIPMAP_NEAREST`, `LINEAR_MIPMAP_NEAREST`, `NEAREST_MIPMAP_LINEAR`, `LINEAR_MIPMAP_LINEAR`. | `TextureParams` default | Mipmap minification filters only make sense when `mipLevels > 1`. |
| `magFilter` | GL filter enum | `NEAREST`, `LINEAR`. | `TextureParams` default | |
| `wrap` | GL wrap enum | `REPEAT`, `MIRRORED_REPEAT`, `CLAMP_TO_EDGE`, `CLAMP_TO_BORDER`. | `TextureParams` default | |
| `transient` | `bool` | `true`, `false`, `1`, `0` (case-insensitive). | `true` | Enables same-frame aliasing only when compatibility and lifetimes permit. |
| `external` | `bool` | Boolean as above. | `false` | The graph does not allocate external images. An imported image should be external and non-transient. |
| `import` | `std::string` | Non-empty application import name. | None | Implies `external=true` in XML. Must be registered before execution. |

`GraphImageUsage` flags have these meanings:

- `sampled`: the image may be supplied as a `Sampled` pass input. Sampling an image without this flag is a compile error.
- `colourAttachment`: a non-depth image may be written through `Colours/Output` / `writeColour`.
- `depthAttachment`: required for `DEPTH24` and `DEPTH24_STENCIL8`; these formats may not have `colourAttachment`.
- `presentation`: descriptive metadata for a final display target. It does not itself bind the window; use an imported screen target.

### 2.2 Image validity and format restrictions

`RenderGraph::createImage()` rejects:

- empty or duplicate names;
- zero `samples` or `mipLevels`;
- an axis with both zero absolute size and non-positive relative size;
- a depth format without `depthAttachment` usage;
- a depth format with `colourAttachment` usage; or
- a non-depth format with `depthAttachment` usage.

`compile(Caps)` additionally rejects an image whose samples exceed `Caps::maxSamples` and an image that combines MSAA (`samples > 1`) with multiple mip levels. Allocation rejects mip counts larger than the resolved dimensions permit.

Only two-dimensional render attachments are implemented. Each graph image presently has one colour attachment when it is a colour image.

### 2.3 Allocation, pooling, and aliases

Call `graph.buildAllocationPlan(viewport)` after compilation and use the resulting plan with `RenderGraphTargets::allocate()`.

The allocation plan resolves relative size, computes the inclusive first/last pass-use interval of every non-external image version, and may reuse physical storage:

- Targets are reused across frames when descriptors are compatible.
- A transient image version may alias another transient version only when their inclusive intervals do not overlap.
- Compatible means matching resolved size, format, samples, mip count, colour space, filtering/wrapping properties, and relevant attachment role.
- Overlapping lifetimes are never aliased; the allocator throws if an overlap would be assigned to the same storage.
- `external` image versions are listed as imports, not allocated.

Do not retain a `RenderTargetPtr` obtained from a transient image beyond graph execution or assume that different transient image versions have distinct backing storage.

### 2.4 MSAA and resolved images

For an internal image with `samples > 1`, `RenderGraphTargets` creates:

1. a multisample **write target**, attached while a pass writes the image; and
2. a matching single-sample **resolved target**, returned by `RenderGraphTargets::get()` and `RenderGraphExecutionContext::getImage()`.

After a pass writes an MSAA output with `store=store`, the executor resolves it with a framebuffer blit. Colour and depth outputs are supported. `store=dontCare` skips the resolve. Consequently ordinary downstream `sampler2D` consumers always receive the resolved single-sample target, not `sampler2DMS` storage.

MSAA targets cannot have mip chains. An imported multisample target has no graph-created resolved partner; import only a target compatible with how the pass uses it.

## 3. Pass declarations

### 3.1 Pass fields

| Field / XML child | C++ type | Values | Default / rule |
| --- | --- | --- | --- |
| `name` | `std::string` | Non-empty, unique among passes. | Required. |
| `type` | `GraphPassType` | `scene`, `fullscreen`, `present` (case-insensitive in XML). | `scene`. |
| `factory` | `std::string` | Registered callback or scene-pass factory identifier. | Optional. XML accepts legacy synonym `callback`. |
| `program` | `std::string` | Name of a declared `Program` resource. | Optional. Direct declarative execution is restricted to `fullscreen` passes in a `RenderGraphTemplate`. |
| `Inputs` | list of `Sampled` | See section 3.2. | Optional. |
| `Parameters` | typed uniform entries | See section 3.3. | Optional. |
| `Colours` | list of `Output` | Zero or more; constrained by caps. | Optional in declaration but the current executor requires at least one colour or depth output. |
| `Depth` | one depth output | See section 3.4. | Optional; at most one is valid. |

Pass type controls executor state rather than the graph dependency model:

- `scene`: preserves normal scene render state and normally calls a scene factory/callback.
- `fullscreen`: installs identity orthographic transforms and disables depth testing, depth writes, culling, and scissoring during execution; previous state and matrices are restored afterward.
- `present`: has the same image-pass state isolation as `fullscreen`; use it for final display work.

The compiler does not require a particular pass type for a particular attachment or prevent a `present` pass from having an ordinary target. The type is a rendering-state convention.

### 3.2 Sampled inputs and sampler bindings

A `Sampled` input names an image and declares that the pass reads its current version. The image must carry `sampled` usage and must either be external or have been produced by another pass. Inputs may be read without a shader sampler name for application-owned callbacks.

```xml
<Inputs>
  <Sampled>
    <sampler>TEX1</sampler>
    <image>SceneHdr</image>
  </Sampled>
</Inputs>
```

| Child | C++ API | Values / restriction |
| --- | --- | --- |
| `image` | `readSampled(pass, image)` or `bindSampler(...)` | Required; must name an existing image. XML automatically tracks the newest version produced by preceding XML declarations. |
| `sampler` | `bindSampler(pass, sampler, image, mipLevel)` | Optional for callback-only inputs; non-empty and unique per pass when supplied. For a program resource it must match a reflected sampler name in that `Program`. |
| `mipLevel` | fourth `bindSampler` parameter | Optional unsigned `0..mipLevels-1`. Omitted means the full declared chain (`UINT32_MAX`). A concrete level temporarily makes it the texture base and max level during the pass. |

A pass cannot bind two different explicit mip levels of the same physical `RenderTexture`; that requires OpenGL texture-view support and is rejected. A sampler binding is also a sampled input, so do not add a second `readSampled` solely for the same binding.

### 3.3 Parameters

Parameters are stored as `UniformCollection` entries. XML supports scalar, boolean, and float vector entries:

```xml
<Parameters>
  <Float><name>EXPOSURE</name><value>1.0</value></Float>
  <Int><name>MODE</name><value>1</value></Int>
  <Bool><name>ENABLED</name><value>true</value></Bool>
  <Vec2><name>INV_SIZE</name><value>0.5 0.5</value></Vec2>
  <Vec3><name>TINT</name><value>1 0.8 0.5</value></Vec3>
  <Vec4><name>RECT</name><value>0 0 1 1</value></Vec4>
</Parameters>
```

| XML element | C++ value | Accepted value |
| --- | --- | --- |
| `Float` | `float` | A parseable floating-point scalar. |
| `Int` | `int32_t` | A parseable signed integer in `int32_t` range. |
| `Bool` | integer boolean | `true`, `false`, `1`, or `0`; stored as `0`/`1`. |
| `Vec2` | `glm::vec2` | Two whitespace-separated floats. |
| `Vec3` | `glm::vec3` | Three whitespace-separated floats. |
| `Vec4` | `glm::vec4` | Four whitespace-separated floats. |

For C++ authoring call `graph.setPassParameters(pass, parameters)`. At execution, `setPassParameterOverrides(pass, parameters)` replaces the entire declared collection for that pass; callers that override parameters must supply all values their pass expects.

### 3.4 Colour and depth outputs

Every output reads the current latest version of its named image and creates its next version. XML internally updates its name-to-handle map after each output, so output and later input order is significant while parsing.

Colour output:

```xml
<Colours>
  <Output>
    <image>SceneHdr</image>
    <mipLevel>0</mipLevel>
    <load>clear</load>
    <store>store</store>
    <clear>0 0 0 1</clear>
  </Output>
</Colours>
```

Depth output:

```xml
<Depth>
  <image>SceneDepth</image>
  <mipLevel>0</mipLevel>
  <load>clear</load>
  <store>dontCare</store>
  <clear>1.0</clear>
</Depth>
```

| Child | C++ type/API | Values / rule |
| --- | --- | --- |
| `image` | `writeColour` / `writeDepth` | Required. Must be the latest handle and have the matching attachment usage and format. |
| `mipLevel` | final `writeColour` / `writeDepth` argument | Optional, default `0`; integer `0..mipLevels-1`. The pass viewport becomes the selected mip dimensions. |
| `load` | `GraphLoadOp` | `load`, `clear`, `dontCare`; default in C++ is `dontCare`. XML unknown spelling also currently becomes `dontCare`. |
| `store` | `GraphStoreOp` | `store` or `dontCare`; C++ default is `store`. XML values other than `store` currently become `dontCare`. |
| `clear` | `glm::vec4` / `float` | Required only to specify a non-default clear. Colour requires four floats; depth requires one float. Defaults: colour `0 0 0 0`, depth `1`. Used only with `load=clear`. |

`load=load` preserves the attachment contents, `clear` clears only that attachment before pass work, and `dontCare` permits the previous contents to be discarded. `store=dontCare` lets the executor invalidate the attachment where supported and prevents MSAA resolve.

Multiple colour outputs are MRT attachments in declaration order: output zero is `GL_COLOR_ATTACHMENT0`, output one is `GL_COLOR_ATTACHMENT1`, and so forth. The number of outputs must not exceed both `Caps::maxColourAttachments` and `Caps::maxDrawBuffers`. All MRT attachments must have identical effective dimensions (after `mipLevel`) and identical sample counts. A depth output, if present, must have those same effective dimensions and sample count. Only one depth output is allowed.

## 4. Ordering and graph restrictions

The following rules define valid graph order:

1. Create every image before the pass that refers to it.
2. Each call to `writeColour` or `writeDepth` must use that image's newest handle; use the returned handle thereafter.
3. A sampled non-external image version needs a producer. Reading an unwritten image is invalid.
4. A consumer runs after the producer of the sampled version. The executor uses the compiler's topological order, not blindly source order.
5. A pass cannot read the same version it writes. Use a separate image/version or another pass.
6. Dependency cycles are invalid.
7. The current executor supports graphics passes only, and each executed pass needs at least one colour or depth output.
8. Output dimensions/sample counts obey the MRT/depth compatibility rules in section 3.4.
9. A selected output/sampler mip must be within the image's declared mip range. Mip attachment dimensions must still match the other attachments in the pass.
10. MSAA images must have exactly one mip level. Downstream reads use the resolved image only after a `store` output.

A pass with no sampled dependency may be reordered relative to another independent pass only as allowed by the stable topological sort. Do not rely on incidental order to synchronize callbacks; declare an image dependency instead.

## 5. Complete XML example

This is a small HDR scene, fullscreen tone-map, and imported screen graph. Formatting is optional; element names are significant.

```xml
<RenderGraph>
  <Images>
    <Image>
      <name>SceneHdr</name>
      <format>RGBA16F</format>
      <scale>1 1</scale>
      <samples>4</samples>
      <usage>colourAttachment,sampled</usage>
      <colourSpace>LINEAR</colourSpace>
      <minFilter>LINEAR</minFilter>
      <magFilter>LINEAR</magFilter>
      <wrap>CLAMP_TO_EDGE</wrap>
      <transient>true</transient>
    </Image>
    <Image>
      <name>SceneDepth</name>
      <format>DEPTH24</format>
      <scale>1 1</scale>
      <samples>4</samples>
      <usage>depthAttachment</usage>
    </Image>
    <Image>
      <name>Presentation</name>
      <format>RGBA8</format>
      <usage>colourAttachment,presentation</usage>
      <import>screen</import>
      <external>true</external>
      <transient>false</transient>
    </Image>
  </Images>
  <Passes>
    <Pass>
      <name>Scene</name>
      <type>scene</type>
      <factory>Example.Scene</factory>
      <Colours>
        <Output><image>SceneHdr</image><load>clear</load><store>store</store><clear>0 0 0 1</clear></Output>
      </Colours>
      <Depth>
        <image>SceneDepth</image><load>clear</load><store>dontCare</store><clear>1</clear>
      </Depth>
    </Pass>
    <Pass>
      <name>ToneMap</name>
      <type>fullscreen</type>
      <program>Example.ToneMap.Program</program>
      <Inputs>
        <Sampled><sampler>TEX1</sampler><image>SceneHdr</image></Sampled>
      </Inputs>
      <Parameters>
        <Float><name>EXPOSURE</name><value>1.0</value></Float>
      </Parameters>
      <Colours>
        <Output><image>Presentation</image><load>clear</load><store>store</store><clear>0 0 0 1</clear></Output>
      </Colours>
    </Pass>
  </Passes>
</RenderGraph>
```

The `SceneHdr` and `SceneDepth` write targets are four-sample targets. The executor resolves `SceneHdr` after `Scene` because it stores it; `ToneMap` samples the resolved normal 2D texture. `SceneDepth` does not resolve because its store operation is `dontCare`.

The production examples are `demo-suite/resources/res/PbrPipeline.rendergraph.xml` and `PbrPipelineMrt.rendergraph.xml`.

## 6. Equivalent programmatic graph

The same topology can be authored with `RenderGraphBuilder`. The builder is a convenience layer; its `build()` returns a normal move-only `RenderGraph`.

```cpp
#include <mpp/RenderGraphBuilder.h>

mpp::GraphImageDesc hdr;
hdr.format = mpp::GraphImageFormat::Rgba16f;
hdr.usage = mpp::GraphImageUsage::ColourAttachment | mpp::GraphImageUsage::Sampled;
hdr.samples = 4;
hdr.relativeSize = { 1.0f, 1.0f };
hdr.params.minFilter = GL_LINEAR;
hdr.params.magFilter = GL_LINEAR;
hdr.params.wrap = GL_CLAMP_TO_EDGE;

mpp::GraphImageDesc depth;
depth.format = mpp::GraphImageFormat::Depth24;
depth.usage = mpp::GraphImageUsage::DepthAttachment;
depth.samples = 4;

mpp::GraphImageDesc screen;
screen.format = mpp::GraphImageFormat::Rgba8;
screen.usage = mpp::GraphImageUsage::ColourAttachment | mpp::GraphImageUsage::Presentation;

mpp::RenderGraphBuilder builder;
auto sceneHdr = builder.createImage("SceneHdr", hdr);
auto sceneDepth = builder.createImage("SceneDepth", depth);
auto presentation = builder.importImage("Presentation", screen);
// Builder importImage sets external=true and transient=false. The raw graph
// API also needs graph.setImageImportName(presentation, "screen") after build.

auto scene = builder.addPass("Scene", mpp::GraphPassType::Scene)
    .callbackFactory("Example.Scene");
sceneHdr = scene.colour(sceneHdr, mpp::GraphLoadOp::Clear, mpp::GraphStoreOp::Store,
                        { 0, 0, 0, 1 });
sceneDepth = scene.depth(sceneDepth, mpp::GraphLoadOp::Clear, mpp::GraphStoreOp::DontCare, 1.0f);

auto toneMap = builder.addPass("ToneMap", mpp::GraphPassType::Fullscreen)
    .program("Example.ToneMap.Program")
    .sampler("TEX1", sceneHdr);
presentation = toneMap.colour(presentation, mpp::GraphLoadOp::Clear, mpp::GraphStoreOp::Store,
                               { 0, 0, 0, 1 });

mpp::RenderGraph graph = builder.build();
graph.setImageImportName(presentation, "screen");
```

For direct construction, use `RenderGraph::createImage`, `addPass`, `readSampled`/`bindSampler`, `writeColour`, and `writeDepth` in the same way. Retain each new handle returned by an output call.

For a graph template authored in C++, place a `std::shared_ptr<RenderGraph>` in `RenderGraphStream::setGraph()` and declare it as a resource. XML files use `resource_parsers::FileRenderGraphStream`, which parses the file into that stream.

## 7. Resource references and XML loading

### 7.1 Programs and shaders

`<program>` names a **Program resource**, not a shader filename and not an inline shader. Declare and load the `Program` through the normal `ResourceManager`/program stream path before the graph template is created. During `RenderGraphTemplate::create()` the engine:

1. finds the named resource;
2. requires its type to be `Program`;
3. creates and loads it; and
4. checks every graph `sampler` name against the program's reflected sampler names.

A program-only graph pass is declaratively executable only if all of these are true:

- execution uses `RenderGraphExecutor::execute(RenderGraphTemplate const&, ...)`, not only the raw `RenderGraph` overload;
- pass type is `fullscreen`;
- `<program>` is non-empty; and
- all sampled inputs needing a shader binding declare matching `sampler` names.

The executor draws through `RenderSystem::renderGraphFullscreen()` and applies the graph parameters. Scene and present work that needs application state should use a factory even if it also relies on normal material/program resources.

### 7.2 Imported targets

An image with `<import>screen</import>` names backing storage supplied at runtime, usually the renderer's screen target. The name is stable XML data, never a live pointer.

```cpp
mpp::RenderGraphImportRegistry imports;
imports.registerImport("screen", renderSystem->getScreenRenderTarget());

auto graph = templateResource->getGraph();
targets.allocate(graph->buildAllocationPlan({ width, height }));
targets.bindImports(*graph, imports);
```

The imported target must exist before execution and be compatible with the image's intended attachment role, dimensions, and samples. Missing registrations produce a named error. All versions of an external logical image resolve to the same imported backing target.

### 7.3 XML graph resource

```cpp
#include <mpp/resource-parsers/FileRenderGraphStream.h>

using namespace mpp;
auto stream = std::make_shared<resource_parsers::FileRenderGraphStream>(
    resourceManager, "resources/MyPipeline.rendergraph.xml");
auto graphResource = resourceManager->declareResource("My.Pipeline", stream).first;
graphResource->create();
graphResource->load();
auto* graphTemplate = dynamic_cast<RenderGraphTemplate*>(graphResource.get());
```

`RenderGraphSerializer::toFile(graph, path)` writes the supported XML representation. Binary graph serialization/package caching is not implemented.

## 8. Implementing and registering pass code

XML cannot serialize a C++ lambda or application object. It contains only a stable `<factory>` identifier. Register that identifier before executing the graph.

### 8.1 Stateless callback factory

Use `registerFactory` for straightforward callback work:

```cpp
mpp::RenderGraphPassFactoryRegistry factories;
factories.registerFactory("Example.ClearOrDraw",
    [](mpp::RenderGraphExecutionContext const& context)
    {
        // The executor has already bound the pass target and viewport.
        // context.getImage(handle) exposes a sampled/imported target.
        // context.getParameters() exposes XML values or an override.
    });
executor.setPassFactoryRegistry(&factories);
```

The callback is selected by name for every matching pass. It receives `RenderGraphExecutionContext`, which provides `getImage`, `getParameters`, `getPass`, and (when supplied) `getFrame`.

### 8.2 Stateful scene-pass factory

Derive from `RenderGraphScenePass` for application-facing scene work. The executor creates one instance per pass id on demand and retains it until `clearPassCallbacks()` or executor destruction.

```cpp
class ExampleScenePass final : public mpp::RenderGraphScenePass
{
public:
    void execute(mpp::RenderGraphExecutionContext const& context) override
    {
        auto const& frame = context.getFrame();
        // Render frame.scene/frame.visibleModels using frame.camera, or use
        // any other live state the application supplied in frame context.
    }
};

factories.registerScenePassFactory("Example.Scene",
    [] { return std::make_unique<ExampleScenePass>(); });
executor.setPassFactoryRegistry(&factories);
```

`RenderGraphFrameContext` can carry `RenderSystem*`, `ScenePtr`, `CameraPtr`, visible models, pipeline options, and a scene `RenderPass`. The implementation must treat it as live per-frame state; it is not XML data.

Built-in identifiers registered by `registerBuiltInRenderGraphPasses()` are:

- `MPP.ShadowDepth`
- `MPP.PbrScene`
- `MPP.LegacyScene`
- `MPP.BloomExtract`
- `MPP.BloomBlurHorizontal`
- `MPP.BloomBlurVertical`
- `MPP.BloomComposite`
- `MPP.ToneMapPresent`

### 8.3 Executing a graph

The normal frame sequence is:

```cpp
std::string failure;
auto compiled = graph->compile(renderSystem->getCaps());
if (!compiled.valid) { /* log compiled.diagnostics */ }

targets.allocate(graph->buildAllocationPlan({ viewportWidth, viewportHeight }));
targets.bindImports(*graph, imports);
executor.setPassFactoryRegistry(&factories);
executor.setFrameContext(&frameContext);
executor.execute(*graphTemplate, targets, renderSystem->getCaps());
```

Reallocate after a viewport-size change. `RenderGraphTargets` retains compatible pooled targets across `allocate()` calls. `RenderGraphExecutor::setPassCallback(pass, ...)` can instead attach a per-pass callback directly for programmatically controlled graphs.

## 9. Current feature gaps and limitations

Implemented features include dependency sorting/cycle detection, image versions, external imports, transient aliasing, MRT, generated mip chains, explicit mip attachment writes, sampled single-mip views, MSAA colour/depth targets with resolves, XML serialization, typed parameters, program-resource validation, and callback/scene factories.

The following are not implemented or intentionally limited:

- **Compute passes:** no compute pass type, dispatch dimensions, SSBO/image bindings, or memory barriers.
- **Texture-view objects:** only one explicit sampled mip of a physical texture may be bound in a pass. Multiple different mip views require a future texture-view implementation.
- **MSAA+mips:** multisample graph images cannot have generated mip chains or explicit mip attachments.
- **Advanced MSAA policies:** resolve is automatic on `store` and uses framebuffer blits. There is no declarative standalone resolve pass, per-attachment resolve destination, `sampler2DMS` graph input, or configurable depth resolve policy.
- **Texture shapes:** graph-managed attachments are 2D only; no cube, 2D array, 3D, layered attachment, or multisample-array images.
- **Attachment subresources:** only mip level is selectable. Array layer, cubemap face, and attachment read-buffer selection are absent.
- **Pass types:** graphics scene/fullscreen/present are supported; there is no copy, compute, explicit resolve, or readback pass.
- **External target validation:** imports are name-checked, but the graph does not fully introspect/validate arbitrary application target format, sample count, and dimensions before use.
- **Execution output requirement:** a pass with no colour/depth output is rejected by the current graphics executor, even if a callback could otherwise perform useful side effects.
- **XML schema validation:** parsing is structural and does not use an XSD. Some unknown `load`/`store` spelling falls back to `dontCare`; authors should use the exact values in this document.
- **Parameter scope:** executor overrides replace the complete parameter collection rather than merging individual values.
- **Diagnostics/automation:** there is no finished DemoSuite UI for allocation/fallback diagnostics and no automated screenshot comparison/archived RenderDoc regression system.
- **Packaging:** XML loading/serialization is implemented; binary graph packaging/cache integration through `ResourceStreamSerializer` is not.

## 10. Authoring checklist

1. Declare colour/depth images with correct format and usage flags.
2. Use `RGBA16F` and linear colour space for HDR pre-tone-map images.
3. Mark only real consumers `sampled`; make depth sampled only when a later pass needs it.
4. Use a non-transient imported presentation image for the screen.
5. Carry the output handle returned by every write forward to later reads/writes.
6. Give MRT and depth attachments matching effective dimensions and samples.
7. Use `store=store` for every MSAA result needed by a later pass.
8. Declare matching shader sampler names and register every XML factory name.
9. Allocate targets, bind imports, then execute; redo allocation on resize.
10. Call `compile(caps)` and log diagnostics before attempting execution.
