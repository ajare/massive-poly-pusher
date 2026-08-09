# glTF Material Import Implementation Plan

## Goal

Move glTF 2.0 material conversion from PipelineEditor into `mpp-resource-parsers`. A `.gltf` file loads as a `PbrMaterial` resource. Direct resource loading selects material index `0` and logs a warning if the file defines more than one material. PipelineEditor uses the shared converter and offers a multi-select import dialog whose only enabled item type is Material.

## Supported first release

- JSON `.gltf` files.
- Core metallic-roughness PBR fields and maps.
- External image URIs, image data URIs, and `bufferView` images backed by data-URI buffers.

## Deferred

- `.glb`, KTX2/Basis transcoding, texture transforms, alternate UV sets, extensions, and non-material glTF asset import.

---

## [x] Phase 1 — Loader API only

1. Add `GltfPbrMaterialLoader.h` to `mpp-resource-parsers`.
2. Define `GltfPbrMaterialLoadResult`:
   - converted `utils::StructuredData` material definition;
   - selected material name/index;
   - warnings;
   - generated image paths.
3. Define `GltfPbrMaterialLoader::loadFirstMaterial(path)`.
4. Add the source/header to the parser Visual Studio project and filters.
5. Initially return a clear `not implemented` error from the function.

**Acceptance:** Other MPP code can include and link a stable core loader API.

## [x] Phase 2 — Read and validate file envelope (`544f891`)

1. Open a `.gltf` file as bytes/text.
2. Reject missing files and non-`.gltf` extensions with useful diagnostics.
3. Reject `.glb` with an explicit deferred-feature message.
4. Add a small JSON parser private to the loader implementation.
5. Validate root object and `asset.version` major version `2`.

**Acceptance:** Valid JSON glTF files parse into an internal JSON document; malformed and unsupported inputs fail cleanly.

## [x] Phase 3 — Select first material and warn (`6e0f1c3`)

1. Read the root `materials` array.
2. Fail if it is missing or empty.
3. Select `materials[0]`.
4. Derive a name from `materials[0].name`, falling back to the file stem.
5. When `materials.size() > 1`, append a warning identifying the selected index and ignored count.
6. Route warnings through MPP logging when called by a file stream.

**Acceptance:** A multi-material fixture returns only material zero and produces one warning.

## [x] Phase 4 — Produce minimal MPP PBR material definition (`6e0f1c3`)

1. Create a `PbrMaterial` `StructuredData` root.
2. Add standard mesh specification and a `Surface` block.
3. Populate default MPP values:
   - base colour `1 1 1 1`;
   - metallic `1`;
   - roughness `1`;
   - emissive `0 0 0`;
   - normal scale `1`;
   - occlusion strength `1`;
   - opaque alpha, cutoff `0.5`, not double-sided.
4. Convert glTF scalar fields only:
   - `baseColorFactor`;
   - `metallicFactor`;
   - `roughnessFactor`;
   - `emissiveFactor`;
   - `alphaMode`, `alphaCutoff`, `doubleSided`.

**Acceptance:** The converted result validates through `FilePbrMaterialStream` without any texture maps.

## [x] Phase 5 — Resolve external image URI chain (`6e0f1c3`, `2f1cbec`)

1. Read `textures[index].source`.
2. Read `images[source].uri`.
3. Resolve relative URI paths against the glTF file directory.
4. Reject unsafe/empty URI references with warnings rather than creating a default texture.
5. Add a helper returning an image path for a glTF texture object.

**Acceptance:** An external image URI resolves to the original image file, never `shared/pbr/arrow.png`.

## [x] Phase 6 — Convert base-colour and metallic-roughness maps (`2f1cbec`)

1. Convert `pbrMetallicRoughness.baseColorTexture` to `BaseColourMap` with `SRGB` colour space.
2. Convert `metallicRoughnessTexture` to `MetallicRoughnessMap` with `LINEAR` colour space.
3. Create child `Texture` resource definitions using resolved image paths.
4. Test map definitions and colour-space choices.

**Acceptance:** A standard externally textured glTF material renders with base colour and ORM maps.

## [x] Phase 7 — Convert normal, occlusion, and emissive maps (`2f1cbec`)

1. Convert `normalTexture` to `NormalMap` and its `scale` to `normalScale`.
2. Convert `occlusionTexture` to `OcclusionMap` and `strength` to `occlusionStrength`.
3. Convert `emissiveTexture` to `EmissiveMap` using `SRGB`.
4. Preserve the flat `emissiveFactor` multiplier.

**Acceptance:** Imported material maps and factors match the glTF core material definition.

## Phase 8 — Data-URI image extraction

1. Decode `images[].uri` values beginning `data:`.
2. Determine a safe extension from MIME type.
3. Write extracted image bytes to a deterministic loader-owned generated-image directory.
4. Return that generated path to material-map conversion.
5. Ignore generated directories in source control.

**Acceptance:** A data-URI image creates a valid MPP texture source file.

## Phase 9 — bufferView image extraction

1. Read `images[].bufferView`.
2. Resolve its buffer, byte offset, and byte length.
3. Decode data-URI backing buffers.
4. Extract exactly the image byte range to a generated file.
5. Diagnose external binary buffers as deferred until binary-buffer support is added.

**Acceptance:** `sphere-test.gltf` embedded normal and ORM images extract and are assigned without arrow fallback.

## Phase 10 — Add `FileGltfPbrMaterialStream`

1. Add a PBR material stream backed by `GltfPbrMaterialLoader`.
2. Have it create child texture streams from the converted definition.
3. Have it publish loader warnings through the engine logger.
4. Add resource-project entries and tests.

**Acceptance:** A caller can create a `PbrMaterial` directly from a `.gltf` path.

## Phase 11 — Dispatch glTF material files

1. Update `FileMaterialStream` to dispatch `.gltf` files to `FileGltfPbrMaterialStream`.
2. Preserve XML material dispatch behavior.
3. Add integration tests for direct `.gltf` material stream loading.

**Acceptance:** File-based MPP material loading works for both XML and glTF sources.

## Phase 12 — Refactor PipelineEditor to use core conversion

1. Remove PipelineEditor’s JSON parser, base64 decoder, extraction logic, and conversion function.
2. Use `GltfPbrMaterialLoader` output for local-resource creation.
3. Preserve unique material/binding naming and selected-model assignment.
4. Verify package export includes generated extracted images.

**Acceptance:** Editor and non-editor paths create identical MPP PBR definitions.

## Phase 13 — Add File menu entry

1. Add **File → Import glTF…**.
2. Open the glTF picker.
3. Parse an import manifest without mutating the open document.
4. Report parser/conversion errors in the existing operation-error UI.

**Acceptance:** The menu can open a glTF file and reach an import-preview state.

## Phase 14 — Import selection modal

1. Add a modal dialog listing glTF categories and items.
2. Enable material rows with checkboxes.
3. Display Meshes, Nodes, Scenes, Cameras, Lights, Animations, and Skins as disabled **Not supported yet** rows.
4. Support selecting multiple material rows.
5. Provide Import and Cancel actions.

**Acceptance:** Users can choose any subset of glTF materials but cannot select unsupported objects.

## Phase 15 — Commit selected editor imports

1. Convert each selected material using the shared loader/converter.
2. Add local PBR material resources and unique preview bindings.
3. Optionally assign the first selected material to the selected model.
4. Record a single undoable pipeline command and, when applicable, a scene command.
5. Refresh preview and inspector state.

**Acceptance:** Multi-selected materials import atomically and survive save/reopen/package export.

## Phase 16 — Regression tests and documentation

1. Add fixtures: scalar-only, external-image, data-URI, bufferView, malformed, and multi-material.
2. Test first-material warning, map colour spaces, scalar conversion, image extraction, and FileStream dispatch.
3. Run Debug/Release parser tests, PipelineEditor smoke tests, and package round trips.
4. Document supported glTF subset and deferred features.
