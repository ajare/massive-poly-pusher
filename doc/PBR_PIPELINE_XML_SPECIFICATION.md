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
  <Environment>...</Environment>
  <PreviewBindings>...</PreviewBindings>
  <PreviewOverrides>...</PreviewOverrides>
  <Extensions>...</Extensions>
  <RenderGraph>...</RenderGraph>
</PbrPipeline>
```

`version`, `name`, and an embedded `RenderGraph` are required semantically. `PreviewScene` is optional. Saving emits canonical authored order.

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

## Environment and preview binding

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

The embedded graph is an ordered hierarchy of `Images` and `Passes`. Images define format, dimensions or scaling, usage, samples, mip levels, colour space, sampling state, import/external/transient state, and stable produced values. Passes define factory/type, enabled state, sampled inputs, parameters, colour/depth attachments, load/store operations, clears, raster state, blend targets, write masks, and scissor.

Dependencies reference stable authored values. Pass order is authoritative; invalid order is diagnosed and never silently changed. See [RENDER_GRAPH_SPECIFICATION.md](RENDER_GRAPH_SPECIFICATION.md) for the complete graph grammar and [RESOURCE_DEFINITIONS.md](RESOURCE_DEFINITIONS.md) for concrete resource streams.

## Canonical examples

Shipped examples are under `resources/shared/pbr/templates`: `Minimal.pipeline.xml`, `Shadows.pipeline.xml`, `Full.pipeline.xml`, and `Empty.pipeline.xml`.
