# PBR-to-Legacy Pipeline Conversion & Export Plan

**Status:** Implemented on `feature/legacy-pipeline-export` (merged to `master`). DemoSuite now requires `--package` and no longer builds the legacy `ModelScene` demo content.

## 1. Objective

Add a one-way conversion from a `PbrPipelineDocument` (and the `PbrMaterial`
resources it references) down to a new `LegacyPipelineDocument` (referencing
`BasicMaterial` resources), and expose it in pipeline-editor as an export
format so a pipeline authored for the PBR path can also be shipped as an
`.mpppackage` that renders through the old `GraphLegacyForward` /
`BasicMaterial` contract. The resulting package is loadable by DemoSuite to
demonstrate "the legacy way of rendering things" alongside the PBR original.

## 2. Non-goals

- No `LegacyPipelineDocument` authoring/editing UI in pipeline-editor. The
  legacy document is a derived, regenerate-on-demand export artifact, not a
  format you open and hand-edit.
- No general Basic&harr;Pbr round-trip converter. Conversion is strictly
  PBR &rarr; Legacy, and is lossy by design (see &sect;4).
- No attempt to approximate PBR environment/IBL lighting in the legacy
  ambient+point-light model. It is dropped, not translated.
- No changes to `RenderPipelineMode`, `RenderPass.cpp`'s PBR-contract
  enforcement, or the render graph pass system. `GraphLegacyForward` and the
  graph-template execution path already exist and are mode-agnostic; this
  work only produces documents/materials for them to consume.
- No standalone "export pipeline XML only" action; only the
  `.mpppackage` export gets a legacy option.

## 3. Prior-state facts (repo as of this plan)

- `RenderPipelineMode::GraphLegacyForward` exists but nothing in the repo
  currently sets `RenderOptions::graphTemplate` for it, so it always falls
  back to a hand-built C++ graph. The graph-template path itself
  (`RenderPipeline::renderGraphForward`, `mpp/src/RenderPipeline.cpp:347-358`)
  is **not** PBR-specific &mdash; it just requires *some*
  `RenderGraphTemplate` resource, regardless of mode.
- `RenderGraphScenePass` and the built-in graph passes are mode-agnostic:
  they render whatever materials are bound to scene models. The Basic-vs-Pbr
  contract is enforced at material-render time
  (`RenderPass.cpp:91-93`, throws `"PbrMaterial requires a PBR forward
  pipeline."`), not baked into the graph's pass types.
- `PbrPipelineDocument` (`mpp/include/mpp/PbrPipelineDocument.h`) is the only
  pipeline document type. There is no `LegacyPipelineDocument`,
  `LegacyPipelineRuntime`, or similar anywhere in the repo.
- `PbrPipelineDocumentLoader::fromFile` already sniffs the XML root element
  (`"PbrPipeline"` vs `"RenderGraph"`) to decide how to parse
  (`mpp-resource-parsers/src/PbrPipelineDocumentLoader.cpp:12-33`). The same
  pattern extends naturally to a `"LegacyPipeline"` root tag.
- `PackageManifest` (`mpp-app-support/include/mpp/app/PackageManifest.h`)
  only carries `version`/`pipeline`/`scene` paths; it is not pipeline-type
  aware today and doesn't need to become so.
- `PackageScene::setupImpl` (`demo-suite/src/PackageScene.cpp:59-145`) is
  currently hardcoded to `PbrPipelineDocument` + `PbrPipelineRuntime` +
  `RenderPipelineMode::XmlGraphPbrForward`, with no branching at all.
- `exportPipelinePackage` (`pipeline-editor/src/Main.cpp:793-919`) is
  strictly typed to `PbrPipelineDocument`. It localizes external/library
  resources into `localResources`, copies referenced asset/shader/model
  files into a temp dir via `rewritePackagePayloadPaths`
  (`pipeline-editor/src/Main.cpp:723-778`), writes `pipeline.xml`/
  `scene.xml`/`manifest.xml`, and zips the result. Its caller in the GUI
  (`pipeline-editor/src/Main.cpp:3420-3440`) wraps it in a
  `try { ... } catch (std::exception const&) { reportOperationError(...); }`
  pattern with `packageDiagnosticSummary(DiagnosticBag const&)`
  (`pipeline-editor/src/Main.cpp:780`) formatting validation diagnostics into
  the thrown exception's message.
- `BasicMaterialSpecification` and `PbrMaterialSpecification`
  (`mpp/include/mpp/BasicMaterialSpecification.h`,
  `mpp/include/mpp/PbrMaterialSpecification.h`) share an identical
  `ProgramOptions` shape (`resourceExists`/`existingResource`/`isChild`/
  `is2d`/`spec` (`mesh::MeshSpecification`, byte-identical field) /
  `vertexShader`/`geometryShader`/`fragmentShader`, each a
  `{Type::Default|File|Resource, data}` `Shader`). `PbrMaterialSpecification`
  additionally carries a typed `PbrSurface` (base colour factor,
  metallic/roughness, emissive, normal scale, occlusion strength, alpha
  mode, alpha cutoff, double-sided) and `TextureOptions::channel` (PBR
  scalar-map channel selection) that `BasicMaterialSpecification` has no
  equivalent for.
- The legacy default fragment shader (`FragmentShader3dTemplate` in
  `mpp/include/mpp/DefaultShaders.h`) has **no material-colour uniform**
  &mdash; only vertex `COLOUR`, a single `TEX1` sampler, ambient/lights, and
  gamma. It has no metallic/roughness/normal/occlusion/emissive support, and
  `BasicMaterial::isTransparent()` is hardcoded `false`
  (`mpp/include/mpp/BasicMaterial.h:32`) &mdash; there is no legacy alpha-blend
  path.
- `mpp::DiagnosticBag`/`Diagnostic` (`mpp/include/mpp/Diagnostic.h`) is an
  existing, general-purpose "collect problems, keep going" mechanism
  (severity Info/Warning/Error, stable `code`, `message`, `location`) already
  used by render-graph/pipeline validation. Reused here rather than inventing
  a parallel diagnostics type.
- `stb_image_write.h` is already vendored and used by `ext/utils/src/Image.cpp`,
  so writing a small generated PNG at export time needs no new dependency.
- pipeline-editor has no existing "export pipeline XML only" action; Save/
  Save As always write the currently-open document to its own native format,
  and `"Export Package..."` is the only export-to-a-different-format action.

## 4. Conversion fidelity and failure policy

`PbrMaterialSpecification` &rarr; `BasicMaterialSpecification`, one material at
a time, collecting diagnostics into a `DiagnosticBag` rather than
succeeding/failing all-or-nothing on the first problem:

| PBR aspect | Legacy treatment |
| --- | --- |
| Base-colour texture | Copied straight across as `TEX1`. |
| Flat `baseColourFactor` only (no base-colour texture) | Baked into a generated image file (small solid-colour PNG via `stb_image_write`), added as a normal file-backed texture resource and used as `TEX1`. Not a procedural/in-memory-only texture &mdash; the exported package must stay self-contained and use the same texture-resource code path as any authored texture. |
| Metallic/roughness/normal/occlusion/emissive maps and factors | Dropped. Diagnostic **warning** per dropped aspect per material. |
| Alpha mode `Mask`/`Blend` | Dropped, treated as opaque. Diagnostic **warning**. (`Opaque` converts cleanly, no diagnostic.) |
| `doubleSided` | Dropped/ignored. Diagnostic **warning** if `true`. |
| `ProgramOptions` where both `vertexShader.type` and `fragmentShader.type` are `Shader::Type::Default` (built-in PBR shader) | Maps to Basic's `Shader::Type::Default` (built-in legacy `DefaultShaders.h` shader). Same `spec`/`is2d` carried across unchanged (identical field on both spec types). |
| `ProgramOptions` using `File`/`Resource` (custom shader source) | **Conversion throws.** A hand-written PBR shader cannot be mechanically translated to the legacy contract; silently emitting broken output would be worse than failing loudly. Diagnostic **error**, and this blocks export. |
| `TextureOptions::channel` (PBR scalar-map channel selection) | N/A &mdash; only arises on maps that are already dropped. |

Pipeline-document-level fields, `PbrPipelineDocument` &rarr;
`LegacyPipelineDocument`:

| Field | Treatment |
| --- | --- |
| `version`, `name`, `sourcePath`, `previewScene`, `resourceLibraries` | Copied as-is. |
| `localResources` / `externalResources` | `PbrMaterial`-kind entries converted via the material converter (&sect;5.1) into `LegacyPipelineResourceKind::BasicMaterial` entries. `Program`/`Texture`/`Sampler`/`PostEffectMaterial`-kind entries copied unchanged (post-effects are image-space, not tied to Basic/Pbr). |
| `imports`, `outputs`, `extensions`, `graph`, `bloom`, `postEffects` | Copied as-is, unchanged &mdash; already mode-agnostic (&sect;3). |
| `previewBindings`, `previewOverrides` | Copied as-is, `materialResource` repointed to the corresponding new Basic material's name. |
| `environment` (IBL: irradiance/prefiltered-specular/BRDF-LUT/background/HDR source) | **Dropped entirely.** No legacy equivalent (legacy lighting is flat ambient + point lights only). Diagnostic **warning** if `environment.binding` was non-empty. |

## 5. Core additions (`mpp`)

### 5.1 `mpp/include/mpp/LegacyMaterialConversion.h` + `.cpp`

Sits next to `BasicMaterialSpecification.h`/`PbrMaterialSpecification.h`.

```cpp
namespace mpp
{
    // Converts one PBR material to its legacy equivalent per the fidelity
    // policy above. Throws mpp::Exception if `source` uses a custom
    // (File/Resource) vertex or fragment shader. Appends Info/Warning
    // diagnostics for every dropped aspect. `bakeTexturePath` is the
    // package-relative (or filesystem, for non-package callers) path to
    // write a baked flat-colour texture to, if one is needed; empty if the
    // material has a base-colour texture already.
    BasicMaterialSpecification convertPbrMaterialToBasic(
        PbrMaterialSpecification const& source,
        std::string const& bakeTexturePath,
        DiagnosticBag& diagnostics);
}
```

### 5.2 `mpp/include/mpp/LegacyPipelineDocument.h` + `.cpp`

New document type, structurally parallel to `PbrPipelineDocument` minus
`environment`, with its own `LegacyPipelineResourceKind` enum
(`BasicMaterial`, `Program`, `Texture`, `Sampler`, `PostEffectMaterial`) and
`LegacyPipelineResourceDocument`/`LegacyPipelineExternalResourceDocument`
structs mirroring the Pbr ones. Root XML tag `LegacyPipeline`.

### 5.3 `mpp/include/mpp/LegacyPipelineConversion.h` + `.cpp`

```cpp
namespace mpp
{
    LegacyPipelineDocument convertPbrPipelineToLegacy(
        PbrPipelineDocument const& source,
        DiagnosticBag& diagnostics);
}
```

Applies the document-level mapping in &sect;4, calling
`convertPbrMaterialToBasic` per `PbrMaterial` resource.

## 6. `mpp-resource-parsers` additions

- `LegacyPipelineDocumentLoader` (mirrors `PbrPipelineDocumentLoader`,
  dispatching on the `LegacyPipeline` root tag).
- `LegacyPipelineSerializer` (mirrors `PbrPipelineSerializer`).
- `LegacyPipelineParser` (mirrors `PbrPipelineParser`).
- `LegacyPipelineRuntime` (mirrors `PbrPipelineRuntime`): resolves
  `LegacyPipelineResourceDocument` resources into live `BasicMaterial`
  instances and exposes `getMaterialBindings()` the same way
  `PbrPipelineRuntime` does.

## 7. `demo-suite` / `PackageScene` changes

`PackageScene::setupImpl` (`demo-suite/src/PackageScene.cpp:59-145`) gains a
branch on `pipeline.xml`'s root tag (reusing the same sniff helper as
`PbrPipelineDocumentLoader::fromFile`, factored out if useful):

- `PbrPipeline` root &rarr; existing path, unchanged
  (`PbrPipelineDocument`/`PbrPipelineRuntime`/`XmlGraphPbrForward`).
- `LegacyPipeline` root &rarr; new path: `LegacyPipelineDocumentLoader`,
  `LegacyPipelineRuntime`, `renderOptions.mode =
  RenderPipelineMode::GraphLegacyForward`, `renderOptions.graphTemplate` set
  from the document's `RenderGraphStream`/`RenderGraphTemplate` the same way
  the PBR path does today (`PackageScene.cpp:81-94`).

No `PackageManifest` schema change (root-tag sniffing only, per &sect;3).

## 8. `pipeline-editor` changes

### 8.1 Export flow

`exportPipelinePackage` (`pipeline-editor/src/Main.cpp:793-919`) stays as-is
for the Pbr path. A new function alongside it:

```cpp
void exportLegacyPipelinePackage(
    PbrPipelineDocument const& sourcePipeline,
    SceneDocument const& sourceScene,
    std::string pipelinePath,
    std::string scenePath,
    std::filesystem::path destination);
```

which:

1. Runs `convertPbrPipelineToLegacy` to get a `LegacyPipelineDocument` +
   `DiagnosticBag`.
2. Merges those diagnostics with the existing pipeline/scene validation
   diagnostics (localization, environment-binding-vs-scene check &mdash; note:
   for the legacy path a non-empty `scene.environmentBinding` is a
   **warning**, not the hard error it is today for the Pbr path, since
   `environment` no longer exists to compare against).
3. Throws (formatted via `packageDiagnosticSummary`) if any diagnostic is an
   **Error** &mdash; e.g. an unconvertible custom-shader material.
4. Otherwise reuses the same localization/asset-copy/model-copy/zip steps
   `exportPipelinePackage` already has (factored out into shared helpers
   where the two functions diverge only in document type), including writing
   any baked flat-colour textures (&sect;5.1) into the package's `assets/`
   folder alongside authored ones.
5. Writes `LegacyPipeline` XML (via `LegacyPipelineSerializer`), `scene.xml`,
   `manifest.xml`, and zips, exactly as the Pbr path does.

### 8.2 UI

The existing `"Export Package..."` menu item/dialog
(`pipeline-editor/src/Main.cpp:2511`, handled at `:3420-3440`) gains a
Pbr/Legacy format choice (dropdown or radio in the save dialog, or a
follow-up choice before the file dialog &mdash; implementation detail). Whichever
is selected calls `exportPipelinePackage` or `exportLegacyPipelinePackage`
inside the same existing `try { ... } catch (std::exception const&) {
reportOperationError(...); }` block
(`pipeline-editor/src/Main.cpp:3424-3439`), and the same
`operationMessageIsSuccess`/success-message path on success. No new
diagnostics-presentation UI needed &mdash; this is the same mechanism the Pbr
export already uses.

The CLI `--export-package` flag (`pipeline-editor/src/Main.cpp:1029-1045`)
gains a parallel `--export-legacy-package <pipeline.xml> <package.mpppackage>`.

No standalone "export pipeline XML only" action is added (&sect;2).

## 9. Open implementation details (not design decisions, resolve while coding)

- Exact CMake target wiring for the new `mpp`/`mpp-resource-parsers` source
  files.
- Whether `rewritePackagePayloadPaths` needs any signature change to be
  shared between `exportPipelinePackage` and `exportLegacyPipelinePackage`,
  or whether it's already generic enough (it walks `StructuredData` by key
  name, independent of the outer resource kind).
- Naming/format for generated baked-texture filenames (must not collide with
  other localized assets in the same package).
- Unit/integration test coverage: material conversion fidelity cases (base
  colour texture vs factor vs neither; each dropped-aspect warning; custom
  shader throw), document conversion (environment drop warning, resource
  kind remap), and an end-to-end package export + `PackageScene` load
  round-trip test if the test harness supports it.
