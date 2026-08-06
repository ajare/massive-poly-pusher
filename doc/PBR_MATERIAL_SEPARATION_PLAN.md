# BasicMaterial / PbrMaterial Separation Plan

**Status:** Phase 1 complete (common base and BasicMaterial rename). Phases 2–5 remain planned.

## 1. Objective

Replace the current hybrid concrete `Material` resource with two explicit concrete resource types:

- `BasicMaterial`: the renamed legacy/simple/custom-shader material path.
- `PbrMaterial`: a dedicated metallic-roughness PBR material path.

`Material` remains the abstract common resource base used by meshes, mesh instances, render passes, and renderer binding code. It is never a concrete resource type, stream type, XML tag, or serializer output.

This removes PBR fields, `PBR_*` inference, PBR neutral-map selection, and PBR-specific parsing from legacy materials. It also makes PBR loading, validation, shader contracts, serialized model data, and documentation explicit.

## 2. Non-goals

This work does not:

- replace the existing `Default`, `PbrForward`, graph PBR, or graph legacy reference pipelines;
- add another PBR workflow beyond metallic-roughness;
- add clearcoat, transmission, sheen, anisotropy, specular-glossiness, displacement, or texture-transform core features;
- add compute/render-graph features; or
- remove old material-data loading immediately. A scoped compatibility reader remains until the next breaking asset-format version.

## 3. Final terminology and resource identities

| Role | Class | Resource/stream type | XML tag | New serializer output |
| --- | --- | --- | --- | --- |
| Common abstract runtime contract | `Material` | None | None | None |
| Legacy/simple/custom material | `BasicMaterial` | `BasicMaterial` | `<BasicMaterial>` | `BasicMaterial` |
| Metallic-roughness PBR material | `PbrMaterial` | `PbrMaterial` | `<PbrMaterial>` | `PbrMaterial` |
| Old compatibility input only | none | legacy `Material` | legacy `<Material>` / generic ModelSpec wrapper | Never written |

Filenames are not type identifiers. A neutral filename such as `stone.xml` is valid; the root XML tag determines whether the loader produces a basic or PBR stream.

## 4. Common abstract `Material` base

### 4.1 Responsibility

`Material` derives from `Resource` and provides only the renderer-facing contract shared by both implementations. It must not own a `PbrSurface`, generic legacy specification, PBR fallback knowledge, or `PBR_*` inference.

It exposes the common operations required by present mesh/render code:

- resolved `Program` access;
- material uniform binding;
- sampler count, sampler identity, and texture access for renderer binding/sorting;
- render classification needed by render passes (including alpha/sidedness behavior); and
- stable material/program binding identity.

Renderer, `MeshInstance`, `Model`, `RenderPass`, `RenderSystem`, and sorting code will replace unsafe `static_cast<Material*>` assumptions with this shared abstract type. Meshes continue to store `ResourcePtr`, but invalid non-material resources must report a clear error when assigned.

### 4.2 `BasicMaterial`

`BasicMaterial` inherits `Material` and retains the current flexible legacy feature set:

- arbitrary program resource or embedded program definition;
- arbitrary declared uniforms;
- arbitrary named texture/sampler bindings;
- existing child-resource behavior and quality settings.

Its dedicated data/API types are renamed from the legacy generic names:

- `BasicMaterialSpecification`
- `BasicMaterialStream`
- `ProgrammaticBasicMaterialStream`
- `FileBasicMaterialStream`

`BasicMaterial` contains no `PbrSurface`, `setPbrSurface`, `isPbr`, PBR uniform mirroring, PBR sampler-name detection, or PBR map fallback code.

## 5. Dedicated `PbrMaterial` model

### 5.1 Classes and streams

Add independent PBR types rather than subclassing basic streams/specifications:

- `PbrMaterial`
- `PbrMaterialSpecification`
- `PbrMaterialStream`
- `ProgrammaticPbrMaterialStream`
- `FilePbrMaterialStream`
- PBR-specific serializer read/write helpers and parser validation.

`PbrMaterialSpecification` owns:

- one validated program choice;
- a metallic-roughness surface definition;
- semantic surface-map resource references/child definitions;
- explicitly declared `PBR_EXT_*` uniforms and textures;
- quality-setting variants; and
- mesh specification / vertex-layout requirements used to create or validate its program.

### 5.2 Core surface definition

The sole v1 workflow is glTF-style metallic-roughness. It has these fields:

| Field | Accepted value |
| --- | --- |
| Base-colour factor | RGB non-negative; alpha in `[0, 1]`. |
| Metallic factor | `[0, 1]`. |
| Roughness factor | `[0, 1]`; the shader may apply its documented non-zero numerical floor. |
| Emissive factor | RGB non-negative; HDR values above `1` are allowed. |
| Normal scale | Non-negative. |
| Occlusion strength | `[0, 1]`. |
| Alpha mode | `OPAQUE`, `MASK`, or `BLEND`. |
| Alpha cutoff | `[0, 1]`. |
| Double sided | Boolean. |

The parser and programmatic stream reject invalid ranges; they do not silently clamp values. Diagnostics identify the material, field, and invalid value.

### 5.3 Semantic surface maps

The PBR XML/API owns semantic map slots instead of generic canonical sampler strings:

| Semantic slot | Canonical program sampler | Texture target | Data colour space | Omitted value |
| --- | --- | --- | --- | --- |
| Base colour | `PBR_BASE_COLOUR_MAP` | 2D | sRGB | white |
| Metallic-roughness | `PBR_METALLIC_ROUGHNESS_MAP` | 2D | linear | roughness 1, metallic 1 |
| Normal | `PBR_NORMAL_MAP` | 2D | linear | tangent-space neutral normal |
| Occlusion | `PBR_OCCLUSION_MAP` | 2D | linear | white |
| Emissive | `PBR_EMISSIVE_MAP` | 2D | sRGB | black |

The parser validates semantic slot target/colour-space requirements. Each slot may be a named resource reference or a child resource. `PbrMaterial` supplies the documented neutral maps when a slot is omitted.

### 5.4 Pipeline-owned IBL

Irradiance cubemap, prefiltered specular cubemap, BRDF LUT, and optional background remain `PbrEnvironment` / PBR-pipeline state, not PBR material-owned maps.

The corresponding PBR program sampler contract is:

- `PBR_IRRADIANCE_MAP`: cube map;
- `PBR_PREFILTERED_SPECULAR_MAP`: cube map;
- `PBR_BRDF_LUT`: 2D map.

A valid configured environment is authoritative during PBR rendering. If no environment is configured, the renderer supplies engine-owned neutral IBL fallbacks and logs a one-time warning, allowing direct-light-only PBR scenes. Explicitly configured but missing/incompatible environment resources produce diagnostics.

## 6. Built-in and custom PBR programs

### 6.1 Built-in program

Omitting `<Program>` selects the engine-owned canonical PBR shader. It is created/cached for the stream's mesh specification rather than relying on one ambiguous global vertex layout. It is the default path for new PBR materials.

The canonical vertex contract requires:

- position;
- normal;
- UV0; and
- `tangent4`, including handedness in `.w`.

This is a strict v1 contract, including factor-only materials. A mesh/program that cannot satisfy it fails validation with an actionable diagnostic.

### 6.2 Custom program sources

`<PbrMaterial>` may specify:

- no `<Program>`: use the built-in program;
- `<Program><Ref>Project.PbrProgram</Ref></Program>`: reference a declared `Program` resource; or
- `<Program><Resource>…</Resource></Program>`: create an embedded child `Program` resource.

Both custom forms undergo the same reflection validation as built-in program setup.

### 6.3 Mandatory core interface

Every custom PBR program must have the complete canonical interface, even if an individual asset omits maps and relies on neutral fallbacks:

- all core material factor uniforms with their documented GLSL types;
- all five surface samplers listed in section 5.3 with matching sampler types;
- all three pipeline-owned IBL samplers listed in section 5.4 with matching sampler types;
- the canonical vertex attribute contract; and
- PBR alpha-mode behavior from section 8.

The exact reflected uniform names/types are written in the PBR material authoring reference alongside this implementation. A custom program that does not conform fails material creation; it is not heuristically treated as basic.

Every PBR fragment program must write colour at fragment location `0`. An emissive bloom-mask output at location `1` is optional. The built-in program is MRT-capable. Existing graph/PBR pipeline logic enables the mask only when hardware and every visible program satisfy the required location-0/1 output contract; otherwise threshold bloom remains the safe fallback.

## 7. Extensions

### 7.1 Scope

Extensions are additive custom data for a conforming custom PBR program. They do not replace core PBR factors, maps, IBL, alpha behavior, vertex attributes, or output requirements.

### 7.2 Namespace and declarations

- Every extension uniform/sampler name must begin with `PBR_EXT_`.
- Core `PBR_*` names and pipeline-owned IBL slots are reserved and forbidden in `<Extensions>`.
- Each extension texture declares its exact target (`2D` or cube) and resource/child resource.
- Each extension uniform uses the existing supported `UniformCollection` scalar, vector, matrix, or array value representation.
- Every extension value is reflection-validated for exact GLSL type; every sampler is validated for sampler target/type.
- Every active reflected `PBR_EXT_*` member must have a matching material declaration. There is no generic extension fallback.
- An extension declaration that is not present/active in the selected custom program is an error unless the extension schema explicitly documents an inactive optional form.

The implementation must add a dedicated extension reference section to PBR authoring documentation with XML and programmatic examples, allowed types, target/type validation, ownership, and failure diagnostics.

### 7.3 Programmatic API

`ProgrammaticPbrMaterialStream` is semantic only. It exposes core setters such as:

- `setSurface(...)`;
- `setBaseColourMap(...)`;
- `setMetallicRoughnessMap(...)`;
- `setNormalMap(...)`;
- `setOcclusionMap(...)`;
- `setEmissiveMap(...)`;
- `setProgram(...)` / built-in selection; and
- `setExtensionUniform(...)` / `setExtensionTexture(...)`.

It does not inherit unrestricted `setTexture(sampler, …)` or `setUniform(name, …)` methods. Those remain on `ProgrammaticBasicMaterialStream`.

## 8. Runtime and pipeline rules

### 8.1 Pipeline eligibility

- `PbrMaterial` is valid only in `PbrForward`, `GraphPbrForward`, and `XmlGraphPbrForward` paths.
- Rendering a `PbrMaterial` through `Default` or `GraphLegacyForward` fails with a named diagnostic.
- `BasicMaterial` remains valid in PBR pipelines and can coexist with PBR models, lights, and generic shadow domains.

### 8.2 Basic materials in a PBR pipeline

A basic shader with no canonical PBR sampler declarations runs unchanged in a PBR pipeline.

If a `BasicMaterial` custom program explicitly declares canonical `PBR_*` samplers, the PBR pipeline supplies the corresponding neutral surface maps and active/neutral environment resources. This supports transitional and custom basic shaders without giving `BasicMaterial` PBR surface fields or making it pass the strict `PbrMaterial` program contract. Features not declared by the basic shader remain disabled by that shader.

### 8.3 Per-instance overrides

Existing model/mesh render parameter uniforms remain usable for PBR instances, but are restricted to:

- validated core factor uniforms; and
- declared `PBR_EXT_*` uniform values.

They are type-validated against the resolved PBR program and may override material defaults per model/mesh instance. Core texture maps are material-owned in v1 and cannot be replaced per instance. Unknown or mismatched runtime overrides fail with model/material context.

### 8.4 Alpha behavior

PBR alpha behavior is a material/pipeline contract and every custom PBR shader must honor it:

- `OPAQUE` and `MASK` render in the opaque pass with depth writes;
- `MASK` discards according to `alphaCutoff`;
- `BLEND` renders after opaque/masked PBR geometry with source-alpha blending and depth writes disabled.

## 9. XML and file dispatch

### 9.1 Standalone files

Use root tags, independent of filename:

```xml
<BasicMaterial>
  <name>Crate</name>
  <!-- basic program, uniforms, and textures -->
</BasicMaterial>
```

```xml
<PbrMaterial>
  <name>Stone</name>
  <!-- optional Program, Surface, semantic maps, Extensions -->
</PbrMaterial>
```

A neutral entry point such as `FileMaterialStream::fromFile(...)` reads the root tag and returns a `FileBasicMaterialStream` or `FilePbrMaterialStream`. Each concrete loader verifies its expected tag. The returned stream type makes `ResourceManager` instantiate `BasicMaterial` or `PbrMaterial`.

### 9.2 Embedded ModelSpec form

New ModelSpec files use direct typed material entries rather than generic `<Material><Resource>` wrappers:

```xml
<Materials>
  <PbrMaterial>
    <name>statue_material</name>
    <Surface>...</Surface>
    <BaseColourMap><Resource>...</Resource></BaseColourMap>
    <MetallicRoughnessMap><Ref>Stone.ORM</Ref></MetallicRoughnessMap>
    <Extensions>...</Extensions>
  </PbrMaterial>
</Materials>
```

`<BasicMaterial>` uses the corresponding basic schema. The old generic ModelSpec material wrapper is compatibility-input-only.

## 10. Serialization and migration

### 10.1 New data

`ResourceStreamSerializer` receives dedicated PBR and basic read/write routines. `.mppmodel` material data gains a versioned explicit type tag per material stream: `BasicMaterial` or `PbrMaterial`.

New serializer output must:

- write only `BasicMaterial` / `PbrMaterial` tags and streams;
- preserve quality settings, child resources, program references, semantic maps, and extensions;
- serialize PBR core data directly, not through `PBR_*` mirrored uniform payloads; and
- never emit legacy `Material` type data.

### 10.2 Compatibility reader

For one migration period, a dedicated compatibility path accepts old generic `Material` XML and old untagged `.mppmodel` material streams:

- old basic data converts in memory to `BasicMaterialStream`;
- old PBR-tagged data converts to `PbrMaterialStream` only when its program meets the new strict PBR contract;
- a non-conforming old PBR program fails conversion with a diagnostic listing missing uniforms, samplers, attributes, or output requirements;
- successful compatibility conversion emits a deprecation warning; and
- compatibility conversion is not exposed through normal new authoring APIs and is removed in the next breaking asset-format version.

All repository materials, ModelSpecs, converted models, tests, and DemoSuite assets are migrated and re-exported now. In particular, temporary preview paths that use old/non-conforming PBR program interfaces must be updated to the canonical interface before release.

## 11. Implementation phases

1. **Common base and legacy rename** — **Complete**
   - [x] Made `Material` abstract and moved shared renderer-facing operations to it.
   - [x] Created `BasicMaterial` and renamed/moved the generic stream, specification, file parser, and programmatic stream code.
   - [x] Updated renderer/model/mesh code to consume the abstract base.
   - [x] Registered `BasicMaterial` in `ResourceManager` and removed concrete `Material` creation.
   - [x] Removed PBR fields, PBR detection, and PBR fallback behavior from `BasicMaterial`.
   - [x] Renamed the standalone DemoSuite basic material XML root to `<BasicMaterial>`.
   - [x] Built MassivePolyPusher, MppResourceParsers, and DemoSuite successfully.
   - [ ] The old generic ModelSpec wrapper remains until Phase 4 supplies typed ModelSpec data. The existing untyped binary `Material` wire tag is mechanically decoded as `BasicMaterial` so current models remain loadable; Phase 4 replaces this stopgap with the documented warning-producing typed compatibility converter.

2. **PBR resource and built-in program** — Planned
   - Add PBR spec/stream/resource/parser/programmatic types.
   - Add engine-owned cached PBR shader/program creation by mesh specification.
   - Add core semantic maps, neutral resources, ranges, reflection validation, and alpha classification.
   - Move PBR fallback/environment behavior out of basic material code.

3. **Extensions and runtime contracts**
   - Add `PBR_EXT_*` XML/programmatic representation and reflection validation.
   - Add validated PBR per-instance uniform overrides.
   - Implement BasicMaterial canonical-slot neutral binding behavior in PBR pipelines.
   - Enforce pipeline eligibility, alpha behavior, and output validation.

4. **Loaders, ModelSpec, serializer, and migration**
   - Add tag-dispatching standalone file entry point and concrete typed file streams.
   - Add typed ModelSpec material parsing.
   - Version `.mppmodel` material serialization and re-export assets.
   - Implement isolated legacy reader/converter with warnings/failures.

5. **Documentation and validation**
   - Rewrite PBR material authoring/setup documentation for typed resources.
   - Document BasicMaterial migration and complete PBR extension contract.
   - Add unit/resource/serialization tests and DemoSuite GPU validation.

## 12. Acceptance criteria

### Resource and parser tests

- Basic and PBR XML root dispatch creates the correct stream/resource type.
- Basic/PBR parsers reject the other type's root/schema.
- PBR factor ranges, map target/colour spaces, core reflection, extension namespace, extension types, and extension completeness are validated.
- Built-in PBR program selection and conforming referenced/embedded custom programs succeed.
- Non-conforming custom PBR programs report each missing contract member.
- PBR quality variants independently validate while preserving the workflow/vertex contract.
- Basic materials retain generic program/uniform/texture behavior with no PBR inference.

### Serialization and migration tests

- Basic and PBR streams round-trip independently through binary serialization.
- New `.mppmodel` files carry explicit material type tags.
- New serializers never write legacy `Material` streams.
- Old basic data converts with a deprecation warning.
- Old conforming PBR data converts with a deprecation warning.
- Old non-conforming PBR data fails clearly.
- Re-exported DemoSuite and test `.mppmodel` assets load without compatibility conversion.

### GPU/DemoSuite tests

- Built-in PBR material validates and renders in manual, hardcoded graph, and XML graph PBR pipelines.
- A conforming custom PBR material validates and renders.
- A `BasicMaterial` renders in a PBR pipeline both with no PBR samplers and with declared canonical PBR sampler fallbacks.
- PBR alpha `OPAQUE`, `MASK`, and `BLEND` behavior is validated.
- PBR location-1 emissive-mask MRT is used only when all visible program/capability requirements succeed; threshold bloom fallback remains correct otherwise.
- PbrMaterial rejection from legacy pipelines is tested.
- Existing shadow, HDR, tone-map, and render-graph regression smoke tests continue to pass.
