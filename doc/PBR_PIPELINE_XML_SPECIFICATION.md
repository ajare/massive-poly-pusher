# PBR Pipeline XML Specification

## Scope and version

A native pipeline document has root `<PbrPipeline>` and `<version>1</version>`. Core elements and enum values are strict: unknown core fields, resource kinds, formats, usages, filters, pass factories, and uniform value types are rejected. Namespaced extension payloads are the only open content.

Paths are resolved relative to the pipeline file. Relative paths are portable; absolute paths are accepted only where diagnosed. A stream contains one pipeline definition.

## Top-level structure

```xml
<PbrPipeline>
  <version>1</version>
  <name>Example</name>
  <PreviewScene><file>preview.scene.xml</file></PreviewScene>
  <ResourceLibraries><Library><file>library.xml</file></Library></ResourceLibraries>
  <LocalResources>...</LocalResources>
  <Imports>...</Imports>
  <Outputs>...</Outputs>
  <Environment>...</Environment>
  <Bloom><enabled>true</enabled><blurPasses>4</blurPasses></Bloom>
  <AmbientOcclusion><method>gtao</method><GTAO><normalSource>depth</normalSource></GTAO></AmbientOcclusion>
  <PreviewBindings>...</PreviewBindings>
  <PreviewOverrides>...</PreviewOverrides>
  <Extensions>...</Extensions>
  <RenderGraph>...</RenderGraph>
</PbrPipeline>
```

`version`, `name`, at least one explicit named output, and an embedded `RenderGraph` are required semantically. `PreviewScene` is optional. Saving emits canonical authored order.

## Resources

`LocalResources` accepts concrete `PbrMaterial`, `Program`, `Texture`, and `Sampler` children. Each child requires one unique `name`. Abstract `Material` resources are not valid PipelineEditor authoring targets.

A `ResourceLibrary` is a separate strict version-1 document:

```xml
<ResourceLibrary><version>1</version><name>Library</name>
  <Resources><Texture><name>Albedo</name>...</Texture></Resources>
</ResourceLibrary>
```

External references use `Library::Resource`. Libraries are read-only; use **Make Local Copy** before editing. Library names and qualified identities must be unique.

Texture fields include `target`, `filename`, `colourSpace`, `mipmaps`, `minFilter`, `magFilter`, `wrap`, LOD limits, and anisotropy. Samplers expose the corresponding sampling state. PBR material surface fields include base colour, metallic, roughness, emissive, normal scale, occlusion strength, alpha mode/cutoff, double-sided state, semantic maps, and reflected `PBR_EXT_*` interfaces. Program payloads contain position/texture requirements, mesh specification, shader stages, and reflected uniforms.

## Typed imports

```xml
<Import><id>screen</id><semantic>presentation</semantic>
  <format>RGBA8</format><usage>colourAttachment,presentation</usage>
  <required>true</required>
</Import>
```

Formats: `R8`, `RG8`, `RGBA8`, `SRGB8_ALPHA8`, `R16F`, `RG16F`, `RGBA16F`, `R32F`, `RG32F`, `RGBA32F`, `R11G11B10F`, `RGB10_A2`, `DEPTH16`, `DEPTH24`, `DEPTH32F`, `DEPTH24_STENCIL8`, and `DEPTH32F_STENCIL8`.

Usage is a comma-separated combination of `sampled`, `colourAttachment`, `depthAttachment`, and `presentation`. Required imports must be supplied by the host. Optional imports require an explicit `fallback` resource.

## Named outputs and anti-aliasing

```xml
<Outputs>
  <Output>
    <name>Main</name>
    <image>Presentation</image>
    <taaDepth>SceneDepth</taaDepth>
    <AntiAliasing>
      <msaa>inherit</msaa><ssaa>inherit</ssaa>
      <taa>inherit</taa><fxaa>inherit</fxaa>
    </AntiAliasing>
  </Output>
</Outputs>
```

Output names are unique. `image` must name a sampled colour-attachment graph image. `taaDepth` is optional unless effective TAA lacks an external output target with its own depth texture; when present it must name a sampled depth-attachment image whose final write uses `store=store`. MSAA and SSAA accept `inherit`, `off`, `2x`, `4x`, or `8x`; TAA and FXAA accept `inherit`, `true`, or `false`. Omitting `AntiAliasing` inherits every global `[mpp]` default. All outputs in one pipeline must resolve to identical MSAA, SSAA, and TAA settings, while FXAA may vary. FXAA requires `RGBA8`, `SRGB8_ALPHA8`, or `RGB10_A2` output. Effective MSAA is applied privately to compatible non-external raster colour/depth attachments; stored writes are automatically resolved before sampled reads and output processing. Multisampled attachments cannot declare mip chains—render and resolve a single-level attachment before generating or sampling mips. Effective SSAA values are total sample counts: 2×, 4×, and 8× use approximately √2, 2, and √8 linear viewport scaling with deterministic upward dimension rounding. Relative graph images use that supersampled viewport; authored absolute-size resources remain unchanged. Outputs return to logical size through a separable alpha-preserving Lanczos filter. Effective TAA applies renderer-owned eight-sample Halton projection jitter and resolved-depth reprojection before that downsample. The named `taaDepth` source must resolve to the same supersampled dimensions as the output. History is rejected for depth mismatch/out-of-bounds reprojection, clamped to the current 3×3 neighbourhood, and reset on generation/size/setting changes, camera cuts or conservative discontinuities, and skipped output frames. Effective FXAA runs last at logical output resolution using the engine's fixed high-quality edge-search/subpixel preset; it preserves centre alpha and remains independently selectable per output.

## Environment, bloom, ambient occlusion, and preview binding

`Bloom` contains `enabled` and `blurPasses`. The count selects how many authored horizontal/vertical blur pairs execute; enabled bloom requires extract/composite passes and cannot request more pairs than the graph authors. Remaining authored blur pairs preserve the image chain without applying additional blur.

`AmbientOcclusion/GTAO/normalSource` accepts `depth` (the default when omitted) or `mrt`. `depth` reconstructs GTAO normals from scene depth. Selecting `mrt` automatically authors the fixed GTAO graph's `RG16F` sampled normals image, attaches it to the `MPP.PbrScene` pass at colour-output location 2, and binds it as the GTAO raw pass `NORMALS` sampler. Location 2 must be written by every participating PBR scene shader as an octahedrally encoded **view-space shading normal**; locations 0 (scene colour) and 1 (Bloom output or an automatic reserved attachment) are also required by the MRT layout. The `RG16F` encoding and a device with at least three colour attachments and draw buffers are mandatory. There is no depth-reconstruction fallback when `mrt` is selected: a missing shader output, graph attachment, normal binding, or hardware capability is a validation error. Switching to `depth`, SSAO, or no ambient occlusion removes only the generated MRT-normal wiring.

`Environment` supports `binding`, `irradiance`, `prefilteredSpecular`, `brdfLut`, and `background`. These resources are pipeline-owned. Missing optional components use diagnosed neutral fallbacks.

`PreviewBindings/Material` maps a scene logical binding to a local or qualified material resource. `PreviewOverrides/Override` targets a model and binding and contains typed `Float`, `Int`, `Bool`, `Vec2`, `Vec3`, or `Vec4` values. Values must match reflected uniforms and cannot enable compiled-out PBR capabilities.

## Extensions

```xml
<Extensions><Extension><namespace>urn:vendor:feature:1</namespace>
  <Payload>...</Payload>
</Extension></Extensions>
```

`namespace` and `Payload` are required. The payload tree is preserved without interpreting vendor fields.

## Render graph

The embedded graph is an ordered hierarchy of `Images` and `Passes`. Images define format, dimensions or scaling, usage, mip levels, colour space, sampling state, import/external/transient state, and stable produced values. Legacy image `<samples>` fields are rejected; anti-aliasing is authored only on named pipeline outputs. Passes define factory/type, enabled state, sampled inputs, parameters, colour/depth attachments, load/store operations, clears, raster state, blend targets, write masks, and scissor.

Dependencies reference stable authored values. Pass order is authoritative; invalid order is diagnosed and never silently changed. See [RENDER_GRAPH_SPECIFICATION.md](RENDER_GRAPH_SPECIFICATION.md) for the complete graph grammar and [RESOURCE_DEFINITIONS.md](RESOURCE_DEFINITIONS.md) for concrete resource streams.

## Canonical examples

Shipped examples are under `resources/shared/pbr/templates`: `Minimal.pipeline.xml`, `Shadows.pipeline.xml`, `Full.pipeline.xml`, and `Empty.pipeline.xml`.
