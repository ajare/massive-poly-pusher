# PBR Pipeline Editor Implementation Plan

**Status:** Implementation in progress.

## Current progress (2026-08-05)

- [x] Created branch `pbr-pipeline-editor`.
- [x] Saved the approved implementation plan (`28a1058`).
- [x] Vendored pinned ImGui docking commit `83f668625ad45364de71d385aeb6a5dd04bee02e` under `ext/imgui` and migrated DemoSuite to it without enabling docking (`6926a8f`).
- [x] Added the shared structured diagnostic model and CPU contract tests (`25b02da`, `9b45d9b`).
- [x] Extracted reusable ImGui/SDL input support from DemoSuite into the `MppAppSupport` static library.
- [x] Added document ID, immutable snapshot/generation, command-stack/save-point, deterministic path, and atomic-write foundations with startup tests.
- [x] Verified Debug and Release builds for MPP, MppAppSupport, and DemoSuite.
- [x] Verified DemoSuite structured diagnostics, document foundations, material migration, and render-graph GPU startup tests.
- [x] Added stable produced-value IDs, authored-order validation, dependency auto-ordering, and saved pass enable state.
- [x] Added the curated PBR graph format set and exact colour/depth render-target allocation.
- [x] Added explicit practical raster state with complete OpenGL state restoration.
- [x] Added allocation byte/alias-group introspection, triangle accounting, and per-pass execution statistics.
- [x] Extended standalone RenderGraph XML round-trips with stable values and enabled state.
- [x] Added declarative pass metadata for inputs, outputs, parameters, material slots, program/raster capabilities, ranges, UI hints, and fallbacks.
- [x] Registered metadata for built-in PBR, shadow, bloom, presentation, custom fullscreen, and custom material-raster passes.
- [x] Added metadata-driven factory/type/count/format/program/raster/uniform contract diagnostics and CPU tests.
- [x] Added the versioned native PbrPipeline document DTO, embedded RenderGraph parser, semantic validator, sample full pipeline, and startup validation.
- [ ] Phase 4 remains in progress: deterministic serializer, legacy importer, document-local resource parsing, runtime template/stream, and transactional instantiation remain.
- [x] Added the Scene document DTO/parser/validator and shipped XML preview scene with sphere grid, ground, primitives, camera, layers, and PBR lights.
- [ ] Phase 5 runtime SceneTemplate/SceneStream instantiation and serializers remain after Phase 4 completes.

## 1. Goal

Add a standalone `PipelineEditor` application beside DemoSuite. It authors versioned PBR pipeline and preview-scene XML, validates them using the same runtime compiler that executes them, and previews the last valid pipeline in a docked ImGui viewport.

The implementation must preserve DemoSuite and the manual/legacy render-pipeline paths. PipelineEditor authors declarative PBR pipelines only.

## 2. Confirmed product contract

### 2.1 Application

- New `pipeline-editor/` executable and VS2026 x64 Debug/Release project.
- SDL/OpenGL platform matching DemoSuite.
- One native window with ImGui docking; no native multi-viewport support.
- One pipeline workspace and one referenced scene at a time.
- Windows `IFileDialog` behind a replaceable file-dialog interface.
- Pipeline path argument and `--validate [--warnings-as-errors]` CLI modes.

### 2.2 Workspace

Default dock layout:

- left upper: ordered pipeline/scene hierarchy;
- left lower tabs: Inspector, Diagnostics, Allocations;
- right: render-to-texture 3D viewport;
- fixed menu bar and toolbar;
- fixed status bar with document/build state, rolling FPS, and submitted triangles.

The hierarchy is an ordered outline, not a node canvas. Pass order is authored state. Invalid reorder/delete operations are allowed, diagnosed, and undoable. Automatic ordering is an explicit stable topological operation.

### 2.3 Documents

Native documents:

- `<PbrPipeline version="1">`;
- `<Scene version="1">`.

Pipeline XML contains pipeline-owned resources, resource-library references, typed imports, the render graph, preview scene reference, logical bindings, and preview instance overrides. Scene XML contains models/primitives, absolute transforms, layers, logical material/environment bindings, lights, one camera, and portable editor defaults.

Existing standalone `<RenderGraph>` XML can be imported into an unsaved pipeline document. It is never overwritten implicitly.

Serialization is deterministic and atomic. Relative paths resolve from the containing document. Comments and original whitespace need not round-trip. Unknown core fields and unsupported versions fail; explicit extension payloads are preserved canonically. Structurally valid documents with semantic errors may be opened and saved after confirmation.

### 2.4 Editing and preview

- Continuous validation.
- Debounced automatic rebuild for valid edits.
- Explicit Validate and Apply/Rebuild commands.
- Last-known-valid preview remains active while working state is invalid.
- Invalid explicit saves require confirmation; recovery autosaves never replace explicit files.
- Separate pipeline and scene dirty/save state.
- Command-based undo/redo with a default 256-command limit.
- External-file watching, conflict handling, cancellable background parsing/I/O, and render-thread GPU creation.

### 2.5 Pipeline capabilities

- PBR declarative graph execution only.
- Built-in passes and metadata-described custom raster/fullscreen passes.
- Raster passes support material mode and override-program mode.
- Explicit stable produced-value IDs; inputs do not use implicit latest-write lookup.
- Pass enable state without implicit bypass rewiring.
- Typed imports and explicit fallback contracts.
- Images expose dimensions, relative sizing, mips, MSAA, usage, colour space, sampling, wrapping, transient/external state, attachments, and load/store operations.
- Curated formats: R8, RG8, RGBA8, SRGB8_ALPHA8, R16F, RG16F, RGBA16F, R32F, RG32F, RGBA32F, R11G11B10F, RGB10_A2, DEPTH16, DEPTH24, DEPTH32F, DEPTH24_STENCIL8, DEPTH32F_STENCIL8.
- Practical explicit raster state: viewport/scissor, fill mode, front face/culling, depth test/write/compare, blend equations/factors, per-target colour masks, multisampling, and alpha-to-coverage.
- Single-source semantic settings: bloom, exposure, tone mapping, and shadows are represented by their authoritative pass/resource fields rather than duplicate top-level values.
- Pipeline-owned PBR environment with logical scene binding.

### 2.6 Pass metadata

Extend the pass-factory registry with declarative authoring metadata:

- category and display name;
- required/optional inputs and outputs;
- accepted image formats/usages/sample constraints;
- parameter names, reflected types, defaults, ranges, and UI hints;
- material slots and texture target contracts;
- supported raster state;
- validation hooks and explicit fallback declarations.

The editor generates inspectors from this metadata. Unknown factories are retained as document data but diagnosed until registered.

### 2.7 Materials and programs

- PBR materials only.
- All current factors, maps, alpha/double-sided state, samplers, PBR specialization features, custom programs, `PBR_EXT_*` values/textures, and constrained instance overrides.
- Document-local resources and read-only external resource libraries.
- `Make Local Copy` clones an external resource before editing.
- File-referenced or embedded GLSL; embedded source is read-only and may be extracted.
- No integrated writable shader editor.
- Typed widgets for every reflected uniform type supported by `UniformCollection`; samplers remain texture bindings.

### 2.8 Scene

- Reusable engine-level `SceneTemplate` resource and `FileSceneStream`.
- No parent hierarchy; every transform is absolute.
- `.mppmodel` references and box/sphere/cylinder/grid primitives.
- Named render layers and explicit visibility/shadow-caster state.
- Named directional/point lights; current shadow source must be directional.
- Exactly one camera, with orbit/pan/zoom editor controls and no FPS/fly mode.
- Logical material/environment bindings; pipeline maps them to resources.
- Model-specific material overrides live in pipeline preview bindings.
- Missing models instantiate diagnosed placeholder boxes and do not count as scene triangles.
- Ordinary camera navigation is transient; `Save Current View as Scene Camera` creates one undoable scene edit.

### 2.9 Diagnostics and introspection

Diagnostics have severity, stable code, message, document path, XML location where available, object ID, and optional fix action.

Errors block rebuild. Warnings do not. Optional `warnings-as-errors` applies to validation/CLI.

Validation covers:

- XML/schema/version and stable-ID integrity;
- pass order, cycles, missing producers, invalid values, and disabled dependencies;
- imports, required/optional fallback contracts, and host resolution;
- pass metadata, shader reflection, material specialization, texture targets, and uniform types;
- image formats, colour spaces, usages, dimensions, samples, mips, attachments, and active-GPU support;
- physical allocations, estimated bytes, lifetimes, aliases, and external allocations;
- scene references, layers, camera, lights, logical bindings, and material overrides;
- portability of resource and asset paths.

Required pipeline resources never silently fall back. Explicit optional inputs may use declared fallbacks with warnings. Missing preview material mappings use a conspicuous fallback. Missing models use placeholders. Missing PBR environment components use documented neutral fallbacks with warnings.

### 2.10 Viewport and statistics

- Graph presentation is imported as an offscreen editor target and displayed as an ImGui image.
- Viewport resizing updates the import without mutating authored XML.
- Orbit, pan, zoom, reset, and frame hierarchy-selected object.
- Intermediate image/mip inspection with colour, channel, alpha, depth, and HDR visualization.
- No viewport object picking or transform gizmos.
- Rolling FPS plus instantaneous frame/CPU/GPU times where supported.
- Submitted triangle count includes repeated shadow/scene pass submissions; tooltip breaks down pass totals and unique scene triangles.

### 2.11 Shared dependencies and assets

- Replace DemoSuite’s private ImGui source with a pinned docking-branch snapshot under tracked `ext/imgui/` files.
- Both applications compile the same source directly; no ImGui DLL/submodule.
- DemoSuite does not enable docking.
- Extract reusable SDL/MPP ImGui application support rather than including DemoSuite-private files.
- Move genuinely shared PBR preview assets to `resources/shared/`; keep DemoSuite regression-only assets private.

## 3. Explicit exclusions

The first implementation does not include:

- compute passes, storage bindings/resources, dispatch, or barriers;
- graph image arrays, cube maps, or 3D images;
- HDR panorama/IBL preprocessing;
- ImGui native multi-viewports;
- transform gizmos or viewport object picking;
- FPS/fly camera;
- scene transform hierarchy;
- BasicMaterial authoring;
- writable integrated shader source editor;
- automatic asset collection/packaging;
- multiple open pipeline documents;
- formal XSD files;
- screenshot/pixel-perfect or automated ImGui interaction testing.

## 4. Implementation phases

### Phase 0: Baseline and dependency migration — Complete

1. Record baseline builds and DemoSuite startup tests.
2. Pin the current ImGui docking snapshot and record upstream commit/version/license.
3. Move ImGui to `ext/imgui` and update DemoSuite include/source paths.
4. Extract shared SDL/MPP ImGui setup, input, clipboard, font texture, and frame handling.
5. Keep docking disabled in DemoSuite and verify behavior is unchanged.

**Exit:** DemoSuite builds/runs against the shared docking snapshot; docking APIs compile in a small smoke test.

### Phase 1: Shared diagnostics and editable document foundations — Complete

1. Add stable diagnostic code/severity/source/object structures.
2. Add editor-safe IDs and reference utilities.
3. Add immutable document snapshots and generation IDs.
4. Add deterministic path handling and atomic XML replacement helpers.
5. Add command-stack, save-point, dirty-state, and recovery primitives independent of ImGui.

**Exit:** unit tests prove diagnostic ordering, ID/reference updates, undo/redo, save-points, and atomic writes.

### Phase 2: Render graph authoring model — Complete

1. Add explicit produced-value IDs and stable input references.
2. Preserve compatibility with existing runtime graph handles.
3. Make authored order validation explicit; add stable dependency auto-order.
4. Add enabled pass state.
5. Expand image formats and active-capability validation.
6. Add practical raster-state descriptors and executor state restoration.
7. Extend allocation reports with bytes, physical IDs, aliases, and lifetimes.
8. Add per-pass execution statistics and GPU labels/timers.

**Exit:** CPU/GPU tests cover reordered values, disabled passes, formats, raster state, allocations, aliases, and statistics.

### Phase 3: Pass authoring metadata — Complete

1. Extend `RenderGraphPassFactoryRegistry` with metadata registration/query APIs.
2. Describe all MPP built-in PBR/shadow/bloom/presentation passes.
3. Add generic custom fullscreen and raster factories.
4. Add reflection-driven program, texture, material, and uniform validation.
5. Add required/optional fallback contracts and active fallback reporting.

**Exit:** metadata and runtime validation use the same definitions; missing/unexpected/wrong-type cases are tested.

### Phase 4: PBR pipeline document — In Progress

1. Add editable `PbrPipelineDocument` DTOs separate from GPU resources.
2. Add version-1 parser/serializer with canonical output.
3. Add document-local resources, external resource libraries, typed imports, graph, scene reference, logical bindings, and instance overrides.
4. Add standalone RenderGraph importer resolving implicit writes to stable produced-value IDs.
5. Add semantic, portability, reflection, capability, and fallback validators.
6. Add runtime template/stream and transactional last-valid instantiation.

**Exit:** valid round-trips are deterministic; invalid fixtures diagnose precisely; existing RenderGraph imports preserve topology.

### Phase 5: Scene document and runtime resource — Parser Foundation Added

1. Add `SceneTemplate`, model/primitive/light/camera/layer/editor-setting DTOs.
2. Add `SceneStream`, programmatic stream, file parser, and serializer.
3. Add absolute-transform scene instantiation.
4. Add logical material/environment resolution supplied by a pipeline workspace.
5. Add missing-model placeholder behavior.
6. Add scene validation and triangle inventory.

**Exit:** scene round-trip/runtime tests cover all model sources, transforms, layers, lights, camera, bindings, and missing files.

### Phase 6: PipelineEditor shell

1. Create executable/project, options, configuration, deployment, and shared platform integration.
2. Create docking host and default layout.
3. Add menu bar, toolbar, status bar, hierarchy, inspector/diagnostic/allocation tabs, and viewport panel.
4. Add native Open/Save dialogs, recent files, one-workspace lifecycle, dirty prompts, and recovery offer.
5. Add CLI open/validate modes.

**Exit:** application starts, restores/reset layout, opens/saves documents, and reports startup/CLI failures correctly.

### Phase 7: Editor controllers and inspectors

1. Add pipeline/scene hierarchy selection and structural commands.
2. Add metadata-generated pass inspector.
3. Add image/import/attachment/raster-state inspectors.
4. Add PBR material, texture/sampler, program/reflection, and typed uniform inspectors.
5. Add scene model/primitive/transform/layer/light/camera inspectors.
6. Add drag reorder, move commands, duplicate/delete/reference cleanup, auto-order, and local-resource cloning.

**Exit:** every supported authored field is reachable and undoable through the UI.

### Phase 8: Live preview and viewport diagnostics

1. Add offscreen presentation import and ImGui texture display.
2. Add debounced validation/build generation pipeline.
3. Add last-known-valid swapping and stale-state banner.
4. Add orbit/pan/zoom/frame/reset and committed-camera command.
5. Add intermediate image/mip inspection and visualization shaders.
6. Add pass/GPU/triangle statistics to status/tooltips.

**Exit:** shipped full pipeline renders the shipped scene; invalid edits retain the previous preview; resizing and intermediate inspection are stable.

### Phase 9: Background work and hot reload

1. Add cancellable background parsing, reads, decoding, and validation.
2. Marshal all GPU work to the render thread.
3. Add file watcher with own-save suppression and dependency tracking.
4. Add clean auto-reload, dirty-document conflict banner, and last-valid shader/texture reload behavior.
5. Add progress and queued/building state reporting.

**Exit:** rapid edits/open/reload operations cannot install stale results or corrupt document state.

### Phase 10: Templates, shared assets, docs, and final validation

1. Move selected assets into `resources/shared` and update deployments.
2. Add default scene and Minimal, Shadows, Full, and Empty pipeline templates.
3. Add pipeline/scene XML specifications, authoring guide, controls, diagnostics catalogue, and CLI documentation.
4. Add invalid fixture suites and real-context integration startup tests.
5. Build Debug/Release MPP, parsers, converters, DemoSuite, and PipelineEditor together.
6. Run symbol/XML scans, clean output deployment tests, and DemoSuite regression startup.

**Exit:** all acceptance criteria below pass from a clean checkout.

## 5. Commit strategy

Use reviewable commits at phase boundaries, with additional commits when ABI migrations and generated assets must remain synchronized:

1. shared ImGui docking snapshot;
2. diagnostics/document foundation;
3. graph produced values/formats/raster state;
4. pass metadata/custom passes;
5. pipeline XML/runtime;
6. scene XML/runtime;
7. editor shell;
8. inspectors/commands;
9. viewport/live rebuild;
10. async reload/recovery;
11. shared assets/templates/docs/tests.

Rebuild all matching MPP, parser, converter, DemoSuite, and PipelineEditor binaries after ABI changes. Re-export model assets only when model/material binary contracts change.

## 6. Acceptance criteria

- DemoSuite builds/runs with shared docking-branch ImGui and unchanged non-docking behavior.
- PipelineEditor is a separate Debug/Release executable and opens the shipped Full template.
- Default docking layout matches the confirmed left/right/status arrangement and can be reset.
- Every supported pipeline, pass, image, material, texture, sampler, uniform, and scene field is editable and undoable.
- Pipeline and scene XML round-trip deterministically and atomically.
- Standalone RenderGraph imports require Save As and retain topology/parameters.
- Invalid semantic edits/save/recovery behavior matches the contract.
- Last valid preview survives invalid edits and resource compile failures.
- Required/optional imports and all active fallbacks are visible and correctly classified.
- Validation reports authored order, stable value dependencies, formats, active-GPU support, allocations, aliases, lifetimes, reflection, portability, and scene bindings.
- Viewport presents the graph output and can inspect intermediate images/mips.
- Status bar reports rolling FPS and submitted triangle count with pass breakdown.
- Hot reload cannot overwrite dirty documents or install stale asynchronous work.
- CLI validation returns deterministic diagnostics and exit status.
- CPU fixture, GPU integration, startup smoke, Debug, Release, and existing DemoSuite tests pass.
