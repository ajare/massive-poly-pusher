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
- [x] Added deterministic nested RenderGraph/PbrPipeline serialization, PbrPipelineStream/FilePbrPipelineStream, PbrPipelineTemplate resource creation, and round-trip/resource startup tests.
- [x] Added root-dispatched standalone RenderGraph migration with generated stable value IDs and editor Save-As behavior.
- [ ] Phase 4 remains in progress: document-local resource parsing, complete validation, and complete transactional runtime instantiation remain.
- [x] Added the Scene document DTO/parser/serializer/validator and shipped XML preview scene with sphere grid, ground, primitives, camera, layers, and PBR lights.
- [ ] Phase 5 runtime SceneTemplate/SceneStream instantiation remains after Phase 4 completes.
- [x] Extracted shared SDL window/timer support and created a standalone docking-enabled PipelineEditor Debug application.
- [x] Added initial menu, toolbar command surface, hierarchy, inspector, diagnostics, viewport, and FPS/triangle status shell.
- [x] Added command-line pipeline/scene loading, `--validate`, `--warnings-as-errors`, runtime deployment, and shipped editor templates.
- [x] Added live document diagnostics, ordered pass/scene hierarchy, pass enable inspector, and allocation/lifetime/alias reporting.
- [~] Phase 6 shell is operational with native dialogs, pipeline/scene save lifecycle, dirty prompts, recent workspace, recovery autosave/restore, template creation, pipeline-plus-scene CLI validation, deployment, and resettable docking; configurable options and remaining lifecycle polish remain.
- [x] Added reflected scalar/vector pass parameter editing and transactional Apply/Rebuild that preserves the previous preview for invalid documents.
- [ ] Phase 7 is in progress with hierarchy/diagnostic/allocation/pass-enable/uniform controls; comprehensive resource inspectors and structural commands remain.
- [ ] Phase 8 is in progress: XML graphs execute in the editor and rebuild transactionally, but offscreen viewport presentation, scene population, camera interaction, and intermediate inspection remain.

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

- [x] Add the core editable `PbrPipelineDocument` DTO separately from GPU resources.
- [x] Add version-1 parsing and serialization for pipeline metadata, scene reference, resource-library paths, environment bindings, preview material bindings, and embedded RenderGraph topology.
- [x] Add deterministic basic pipeline/graph round-trip and resource startup tests.
- [x] Add `PbrPipelineStream`, `FilePbrPipelineStream`, `PbrPipelineTemplate`, and ResourceManager factory registration.
- [~] Complete the version-1 schema and lossless canonical serializer.
  - [~] Document-local PBR materials, programs, textures, and samplers.
    - [x] Ordered typed local-resource DTOs and lossless concrete-resource XML payload parsing/serialization for all four resource kinds, with name/kind validation and round-trip coverage.
    - [~] Complete concrete-schema semantic validation, editor DTO controls, and runtime stream instantiation.
      - [x] Instantiate local PBR material/program/texture/sampler definitions through their existing concrete file streams as pipeline child resources; cover a loaded local sampler at startup.
      - [ ] Run complete concrete-schema validation without GPU creation and expose typed editor controls.
  - [x] Typed image-import IDs, semantics, formats, usages, required/optional state, explicit fallback declarations, XML round-trip, and graph-descriptor validation.
  - [x] Preview instance-override model/binding targets and typed scalar/vector values with strict parsing, canonical serialization, semantic validation, and round-trip coverage.
  - [~] Explicit extension-payload preservation and strict unknown-core-field rejection.
    - [x] Strict unknown-field/value rejection across the current PBR pipeline and scene core schemas, with startup regression checks.
    - [ ] Namespaced extension-payload preservation.
  - [x] Atomic replacement through the PbrPipeline serializer path.
- [~] Complete external resource-library support.
  - [x] Parse and serialize ordered library paths.
  - [ ] Resolve libraries, qualified names, duplicate names, and read-only/local-copy ownership.
- [x] Add the standalone RenderGraph importer, resolve implicit versions to stable produced-value IDs, and require Save As.
- [~] Complete document validation.
  - [x] Basic version/name/graph/order/library-list/binding validation, relative-path resolution, missing-file diagnostics, and absolute-path portability warnings.
  - [x] RenderGraph topology and available pass-metadata validation.
  - [~] Resource resolution, portability, reflection, active-GPU capability, typed-import, and fallback validation.
    - [x] Typed image-import contract/graph compatibility and pipeline/scene/resource-library file portability checks.
    - [ ] Resource-library content resolution, shader reflection, active-GPU import capability, and runtime fallback availability.
  - [ ] Comprehensive invalid fixtures with stable diagnostic codes and source locations.
- [~] Complete runtime instantiation.
  - [x] Resource template/stream creation.
  - [x] Editor-side valid graph generation swap that retains the previous pipeline when validation fails.
  - [~] Resolve document-local/external resources, programs, imports, environments, material bindings, and overrides.
    - [x] Resolve document-local typed definitions into pipeline-owned child resources with deterministic qualified runtime names.
    - [ ] Resolve external libraries, graph imports, environments, material bindings, and overrides into a complete runtime workspace.
  - [ ] Reusable transactional runtime object with deterministic obsolete-generation cleanup.

**Exit:** not met. Valid complete documents must round-trip without data loss, invalid fixtures must diagnose precisely, legacy RenderGraph imports must preserve topology, and complete runtime generations must swap transactionally.

### Phase 5: Scene document and runtime resource — In Progress

- [x] Add Scene document DTOs for models/primitives, absolute transforms, layers, material bindings, PBR lights, one camera, and environment binding.
- [x] Add version-1 file parser and deterministic serializer with atomic replacement.
- [x] Add the shipped sphere-grid preview scene and parser/serializer round-trip startup validation.
- [~] Add scene validation and inventory.
  - [x] Version, name, model/light ID, required model file, camera range, and binding diagnostics.
  - [~] Resource existence/type validation, layer validation, light-limit/shadow compatibility, portability, and triangle inventory.
    - [x] Model-file existence, absolute-path portability, empty-layer, light value/direction, and eight-light-limit validation.
    - [ ] Loaded resource type, declared-layer references, shadow-light compatibility, and triangle inventory.
- [x] Add `SceneTemplate`, programmatic `SceneStream::setDocument()`, `FileSceneStream`, ResourceManager factory registration, and resource startup test.
- [ ] Instantiate `.mppmodel`, box, sphere, cylinder, and grid resources with absolute transforms.
- [ ] Resolve logical material/environment bindings from the active pipeline workspace.
- [ ] Add diagnosed missing-model placeholder boxes excluded from triangle statistics.
- [ ] Add runtime tests covering every source type, transforms, layers, lights, bindings, missing assets, and cleanup.

**Exit:** not met. Scene round-trip is tested, but reusable runtime scene resources and instantiation are not implemented.

### Phase 6: PipelineEditor shell — In Progress

- [x] Create the standalone VS2026 Debug application, shared platform integration, runtime deployment script, and shipped resources.
- [x] Enable ImGui docking only in PipelineEditor and create an initial left hierarchy/lower tabs/right viewport layout.
- [x] Add menu bar, toolbar, status bar, hierarchy, inspector, diagnostics, allocations, and viewport windows.
- [x] Add native Windows Open/Save dialogs, one open workspace, recent-file reopening, recovery autosave, and recovery offer.
- [x] Add pipeline-path startup and deterministic pipeline-plus-referenced-scene `--validate` / `--warnings-as-errors` CLI modes with emitted diagnostics.
- [ ] Add real configurable program options/preferences instead of hard-coded window and recovery values.
- [~] Complete New/Open/Save Scene/Save All/Exit lifecycle and separate pipeline/scene dirty prompts.
  - [x] Pipeline New/Open/Exit discard prompts and atomic pipeline Save.
  - [x] Scene Save/Save As/Save All and independent scene dirty prompts.
- [ ] Add multiple recent-file entries, conflict/error UI, and recovery cleanup for all close/failure paths.
- [x] Add `Window -> Reset Layout` and preserve saved-layout restoration unless reset is requested.
- [x] Add Release deployment and CLI smoke validation; enforce the parser-to-runtime project dependency required by clean Release builds.

**Exit:** partially met. The application starts, opens/saves pipeline and scene documents, validates both from CLI, and supports resettable/restored docking; configurable preferences and remaining lifecycle polish remain.

### Phase 7: Editor controllers and inspectors — In Progress

- [~] Add pipeline/scene hierarchy selection and structural commands.
  - [x] Display and select ordered passes; display preview-scene models.
  - [x] Toggle saved pass enable state.
  - [ ] Select all pipeline/scene resource categories and perform add/remove/duplicate operations.
- [~] Add metadata-generated pass inspector.
  - [x] Show pass identity, factory, input/output counts, enabled state, and reflected scalar/vector parameters.
  - [ ] Generate required/optional slots, ranges, enums, UI hints, format constraints, fallbacks, material slots, and program controls from metadata.
- [ ] Add image, typed import, attachment, subresource, and raster-state inspectors.
- [~] Add PBR material, texture/sampler, program/reflection, and typed uniform inspectors.
  - [x] Edit pass float/vector/int/bool values supported by the current `UniformCollection` UI.
  - [ ] PBR materials, maps, extensions, samplers, programs, reflection details, matrices, arrays, and instance overrides.
- [~] Add scene model/primitive/absolute-transform/layer/light/camera/editor-setting inspectors.
  - [x] Model selection, absolute translation/rotation/scale, visibility, shadow-caster state, and logical material binding.
  - [x] Comma-separated layers, directional/point lights, camera, and environment binding.
  - [ ] Primitive parameters and editor settings.
- [ ] Wire the existing command-stack foundation into all edits and expose functional undo/redo/save points.
- [ ] Add drag reorder, move commands, dependency auto-order, duplicate/delete/reference cleanup, and local-resource cloning.

**Exit:** not met. Only pass selection, enable state, and basic parameter editing are currently reachable; edits are not yet undoable.

### Phase 8: Live preview and viewport diagnostics — In Progress

- [~] Execute loaded XML graphs in PipelineEditor.
  - [x] Create graph resources and graph-backed preview pipeline generations.
  - [x] Keep the current generation when validation blocks an explicit rebuild.
  - [ ] Resolve the complete Phase 4 pipeline document and populate the Phase 5 scene.
- [ ] Add offscreen presentation import, viewport-sized render target, ImGui texture registration, and resize handling.
- [ ] Add continuously validated, debounced asynchronous build generations and stale-result rejection.
- [~] Add last-known-valid behavior.
  - [x] Explicit Apply/Rebuild only switches after basic document validation.
  - [ ] Stale-preview banner, compile/resource failure rollback, generation cleanup, and current/stale status reporting.
- [ ] Add orbit/pan/zoom/frame/reset input and `Save Current View as Scene Camera`.
- [ ] Add intermediate image/mip selection, diagnostic resolve, and colour/channel/depth/HDR visualization.
- [~] Add preview statistics.
  - [x] Rolling FPS, global triangle status, allocation bytes, lifetimes, and alias groups.
  - [ ] Submitted-triangle pass breakdown, unique scene triangles, CPU/GPU pass timings, viewport size, and status/tooltips.

**Exit:** not met. Graph execution exists, but the docked viewport still lacks offscreen presentation, scene content, camera interaction, and intermediate inspection.

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
