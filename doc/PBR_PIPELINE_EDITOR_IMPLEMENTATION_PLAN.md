# PBR Pipeline Editor Implementation Plan

**Status:** Implementation in progress.

## Current progress (2026-08-07)

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
- [x] Phase 4 complete: canonical full schema, typed local/external resources, recursive localization, parser and active-GPU validation, imports, environments, bindings, overrides, invalid fixtures, and reusable transactional `PbrPipelineRuntime` resolution/rollback/cleanup.
- [x] Added the Scene document DTO/parser/serializer/validator and shipped XML preview scene with sphere grid, ground, primitives, camera, layers, and PBR lights.
- [x] Phase 5 complete: strict declared layers and shadow-light contracts, complete primitive/model instantiation, scene-owned PBR lights, resolved pipeline materials/environments/overrides, transactional rollback, exact inventories, and runtime inspection tests.
- [x] Extracted shared SDL window/timer support and created a standalone docking-enabled PipelineEditor Debug application.
- [x] Added initial menu, toolbar command surface, hierarchy, inspector, diagnostics, viewport, and FPS/triangle status shell.
- [x] Added command-line pipeline/scene loading, `--validate`, `--warnings-as-errors`, runtime deployment, and shipped editor templates.
- [x] Added live document diagnostics, ordered pass/scene hierarchy, pass enable inspector, and allocation/lifetime/alias reporting.
- [x] Phase 6 complete: standalone shell, native New/Open/Save As/Save All lifecycle, independent dirty prompts, atomic error-preserving saves, invalid-document confirmation, recent workspaces, pipeline/scene recovery, external-file conflict handling, configurable preferences, CLI validation, deployment, and resettable docking.
- [x] Added reflected scalar/vector pass parameter editing and transactional Apply/Rebuild that preserves the previous preview for invalid documents.
- [x] Phase 7 complete: command-backed structural authoring, drag reorder, dependency ordering, metadata-generated controls, complete image/import/attachment/raster/resource/scene inspectors, reference cleanup, typed uniform arrays/matrices, instance overrides, and continuous-edit coalescing.
- [x] Phase 8 complete: offscreen transactional preview, scene/camera interaction, produced-value and mip inspection, diagnostic resolves, colour/channel/alpha/depth/HDR visualization, exact scene/submission statistics, and asynchronous per-pass GPU timings.
- [x] Phase 9 complete: cancellable generation-tagged background preparation, debounced live rebuilding, dependency-aware stable file watching, clean hot reload, dirty conflict protection, render-thread GPU installation, and queued/building progress.
- [x] Phase 10 complete: shared self-contained assets, four pipeline templates and default scene, XML/authoring/diagnostic/CLI documentation, expanded invalid fixtures, deterministic validation and GPU smoke automation, unified Debug/Release solution builds, and clean deployment regression coverage.

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

### Phase 4: PBR pipeline document — Complete

- [x] Add the core editable `PbrPipelineDocument` DTO separately from GPU resources.
- [x] Add version-1 parsing and serialization for pipeline metadata, scene reference, resource-library paths, environment bindings, preview material bindings, and embedded RenderGraph topology.
- [x] Add deterministic basic pipeline/graph round-trip and resource startup tests.
- [x] Add `PbrPipelineStream`, `FilePbrPipelineStream`, `PbrPipelineTemplate`, and ResourceManager factory registration.
- [x] Complete the version-1 schema and lossless canonical serializer.
  - [x] Document-local PBR materials, programs, textures, and samplers.
    - [x] Ordered typed local-resource DTOs and lossless concrete-resource XML payload parsing/serialization for all four resource kinds, with name/kind validation and round-trip coverage.
    - [x] Complete concrete-schema semantic validation, editor DTO controls, and runtime stream instantiation.
      - [x] Instantiate local PBR material/program/texture/sampler definitions through their existing concrete file streams, including built-in PBR materials with authored mesh specifications.
      - [x] Run parser-only concrete validation for CLI/continuous diagnostics and expose command-backed material, texture, and sampler controls plus active PBR reflection details.
  - [x] Typed image-import IDs, semantics, formats, usages, required/optional state, explicit fallback declarations, XML round-trip, and graph-descriptor validation.
  - [x] Preview instance-override model/binding targets and typed scalar/vector values with strict parsing, canonical serialization, semantic validation, and round-trip coverage.
  - [x] Explicit extension-payload preservation and strict unknown-core-field rejection.
    - [x] Strict unknown-field/value rejection across the current PBR pipeline and scene core schemas, with startup regression checks.
    - [x] Unique namespaced extension envelopes with arbitrary payload-tree preservation and canonical round-trip coverage.
  - [x] Atomic replacement through the PbrPipeline serializer path.
- [x] Complete external resource-library support.
  - [x] Parse and serialize ordered library paths.
  - [x] Resolve libraries, qualified names, duplicate names, and read-only/local-copy ownership.
    - [x] Resolve strict versioned `ResourceLibrary` XML relative to its pipeline, preserve ordered references, qualify resources as `Library::Resource`, diagnose duplicate qualified names, enforce read-only state, and instantiate external child streams.
    - [x] Implement editor `Make Local Copy`, reference rewriting, and complete library resource schemas/fixtures.
      - [x] Clone read-only external payloads into uniquely named local resources, recursively rewrite nested/direct references, expose external/local hierarchy entries, and provide the inspector command.
      - [x] Ship strict versioned per-kind PBR material/program/texture/sampler library fixtures and reject duplicate library identities.
- [x] Add the standalone RenderGraph importer, resolve implicit versions to stable produced-value IDs, and require Save As.
- [x] Complete document validation.
  - [x] Basic version/name/graph/order/library-list/binding validation, relative-path resolution, missing-file diagnostics, and absolute-path portability warnings.
  - [x] RenderGraph topology and available pass-metadata validation.
  - [x] Resource resolution, portability, reflection, active-GPU capability, typed-import, and fallback validation.
    - [x] Typed image-import contract/graph compatibility and pipeline/scene/resource-library file portability checks.
    - [x] Resource-library content resolution, shader reflection, active-GPU import capability, and runtime fallback availability.
      - [x] Resolve typed resource-library contents and reject malformed roots, versions, unknown resource kinds, missing names, and duplicate qualified resources.
      - [x] Validate concrete shader/material reflection during candidate creation, graph formats against active caps, host import allocation on the active GPU, and all neutral environment fallbacks with diagnostics.
  - [x] Add invalid unknown-core, missing-optional-fallback, and malformed-local-resource fixtures with stable codes and document/object locations.
- [x] Complete runtime instantiation.
  - [x] Resource template/stream creation.
  - [x] Editor-side valid graph generation swap that retains the previous pipeline when validation fails.
  - [x] Resolve document-local/external resources, programs, imports, environments, material bindings, and overrides.
    - [x] Resolve document-local typed definitions into pipeline-owned child resources with deterministic qualified runtime names.
    - [x] Resolve external libraries, graph imports, environments, material bindings, and overrides into a complete runtime workspace.
      - [x] Resolve external read-only library resources as qualified pipeline child resources.
      - [x] Allocate typed host imports, resolve complete or neutral-fallback environments, enforce PBR material types/reflection, and apply validated per-model overrides through `SceneRuntime`.
  - [x] Reusable transactional runtime object with deterministic obsolete-generation cleanup.
    - [x] Transactional editor graph/pipeline candidate installation, exception rollback, named pipeline removal, obsolete graph-resource deletion, shutdown cleanup, and removal regression coverage.
    - [x] Reusable `PbrPipelineRuntime` resolves complete candidate workspaces, supports explicit accept/rollback, retains the last valid generation, and deletes nested resources and cached aliases deterministically.

**Exit:** met. Complete local/external documents round-trip canonically, invalid fixtures diagnose with stable codes, standalone RenderGraph imports preserve topology and require Save As, and reusable complete runtime workspaces install or roll back transactionally.

### Phase 5: Scene document and runtime resource — Complete

- [x] Add Scene document DTOs for models/primitives, absolute transforms, layers, material bindings, PBR lights, one camera, and environment binding.
- [x] Add version-1 file parser and deterministic serializer with atomic replacement.
- [x] Add the shipped sphere-grid preview scene and parser/serializer round-trip startup validation.
- [x] Add scene validation and inventory.
  - [x] Version, name, model/light ID, required model file, camera range, and binding diagnostics.
  - [x] Resource existence/type validation, layer validation, light-limit/shadow compatibility, portability, and triangle inventory.
    - [x] Model-file existence, absolute-path portability, empty-layer, light value/direction, and eight-light-limit validation.
    - [x] Loaded resource type, declared-layer references, shadow-light compatibility, and triangle inventory.
      - [x] Exact visible primitive triangle inventory for authored box/sphere/cylinder/grid parameters, plus explicit unknown `.mppmodel` count.
      - [x] Loaded model resource type/triangle metadata, declared-layer references, and shadow-light compatibility.
        - [x] Runtime model stream/type loading, exact loaded/primitive per-model and visible unique triangle inventory, and diagnosed placeholders excluded from totals.
        - [x] Validate unique declared layers, duplicate/undeclared model references, one directional shadow light, unsupported point shadows, finite transforms/light values, and non-zero scales.
- [x] Add `SceneTemplate`, programmatic `SceneStream::setDocument()`, `FileSceneStream`, ResourceManager factory registration, and resource startup test.
- [x] Instantiate `.mppmodel`, box, sphere, cylinder, and grid resources with absolute transforms.
  - [x] Transactional `SceneRuntime`, neutral specialized PBR material, all four primitive streams, successful `.mppmodel` loading, visibility/shadow flags, and absolute translation/Euler rotation/scale application.
  - [x] Propagate declared render layers to runtime model instances and expose layer-filtered scene queries.
- [x] Resolve logical material/environment bindings from the active pipeline workspace.
  - [x] Runtime material-binding map API with diagnosed neutral PBR fallback and concrete PBR type enforcement.
  - [x] Build mappings from the complete active pipeline workspace, apply them to primitive and loaded models, and enforce the scene-to-pipeline environment binding.
- [x] Add diagnosed missing/failed-model placeholder boxes excluded from authored triangle statistics.
- [x] Convert up to eight authored directional/point lights into scene-owned PBR runtime lights, normalize directional vectors, apply them per PBR render, and preserve the legacy host-managed lighting path for scenes that do not opt in.
- [x] Configure the preview's generic directional shadow domain from the single authored shadow light and bind its depth target to the graph's typed `shadowDepth` import.
- [x] Add runtime tests covering every source type, transforms, layers, lights, bindings, missing assets, and cleanup.
  - [x] Startup runtime coverage for every primitive type, successful and missing `.mppmodel` sources, transactional replacement, placeholder diagnostics, cached-program alias reuse, and deterministic nested resource cleanup.
  - [x] Inspect transforms, layer propagation/filtering, normalized directional and point lights, resolved material/environment bindings, loaded-model types, and failed-candidate retention.

**Exit:** met. Versioned scenes round-trip declared layers and shadow-light intent, complete runtime scenes apply pipeline resources and scene-owned PBR lighting, directional shadow imports are connected to the authored light, and invalid candidates preserve the prior scene generation.

### Phase 6: PipelineEditor shell — Complete

- [x] Create the standalone VS2026 Debug application, shared platform integration, runtime deployment script, and shipped resources.
- [x] Enable ImGui docking only in PipelineEditor and create an initial left hierarchy/lower tabs/right viewport layout.
- [x] Add menu bar, toolbar, status bar, hierarchy, inspector, diagnostics, allocations, and viewport windows.
- [x] Add native Windows Open/Save dialogs, one open workspace, recent-file reopening, recovery autosave, and recovery offer.
- [x] Add pipeline-path startup and deterministic pipeline-plus-referenced-scene `--validate` / `--warnings-as-errors` CLI modes with emitted diagnostics.
- [x] Add persisted Preferences and command-line overrides for startup width/height and recovery interval instead of hard-coded values.
- [x] Complete New/Open/Save Scene/Save All/Exit lifecycle and separate pipeline/scene dirty prompts.
  - [x] Pipeline New/Open/Save As/Exit discard prompts, invalid-document confirmation, atomic pipeline Save, path rebasing, and save-point updates.
  - [x] Scene Save/Save As/Save All, preview-reference updates, and independent scene dirty prompts.
- [x] Add multiple recent-file entries, conflict/error UI, and recovery cleanup for all close/failure paths.
  - [x] Persist, deduplicate, and reorder up to eight recent pipelines, reopen them transactionally through the normal dirty-document prompt, and diagnose/remove missing entries.
  - [x] Preserve the active workspace on load/save failure and present actionable copyable error details.
  - [x] Fingerprint pipeline, scene, and external-library files; report external changes and offer reload, writable-document overwrite, or keep-local handling while retaining read-only library ownership.
  - [x] Autosave and restore pipeline and scene recovery copies independently; remove them after explicit save, rejection, invalid recovery, successful discard/replacement, and clean close while retaining them across failed operations and crashes.
  - [x] Add document-foundation coverage for exact external-change fingerprints, newer-recovery detection, cleanup, and atomic replacement.
- [x] Add `Window -> Reset Layout` and preserve saved-layout restoration unless reset is requested.
- [x] Add Release deployment and CLI smoke validation; enforce the parser-to-runtime project dependency required by clean Release builds.

**Exit:** met. The application starts and transactionally opens complete workspaces, supports independent pipeline/scene Save As and recovery lifecycles, preserves active edits across operational failures, resolves external-file conflicts explicitly, validates both documents from CLI, and restores or resets its docked shell.

### Phase 7: Editor controllers and inspectors — Complete

- [x] Add pipeline/scene hierarchy selection and structural commands.
  - [x] Select passes, images, imports, local/read-only external resources, environments, bindings, overrides, models, lights, camera, and render layers.
  - [x] Add/remove/duplicate pass, image, typed-import, local-resource, preview-binding, instance-override, model, and light items with collision-free generated identities.
  - [x] Keep external resources read-only and clone them through `Make Local Copy`.
- [x] Add metadata-generated pass inspection and authoring.
  - [x] Select registered factories and expose required/optional input/output contracts, accepted formats, explicit fallbacks, ranges, enum hints, UI hints, material slots, and program-resource controls.
  - [x] Add missing reflected parameters and edit scalar, vector, matrix, and array uniform values.
- [x] Complete image, typed-import, attachment, subresource, and raster-state inspectors.
  - [x] Edit curated formats, usages, absolute/relative sizing, samples, mips, colour space, filters, wrapping, LOD, anisotropy, external/import ownership, and transient state.
  - [x] Add/remove/retarget colour and depth attachments while preserving stable value IDs and dependent sampler references; edit mip, load/store, and clear values.
  - [x] Edit sampler bindings and complete practical raster state including blend operations/factors, per-target write masks, and scissor rectangles.
- [x] Complete concrete-resource and scene inspectors.
  - [x] Edit PBR factors, map resources, extension textures, texture/sampler state, program settings, active reflection details, logical bindings, pipeline environment, and typed instance overrides.
  - [x] Edit scene IDs/sources, model files, primitives, absolute transforms, layers, visibility/shadows, lights, camera, environment, and persisted portable editor preferences.
- [x] Route structural and property changes through independent pipeline/scene command stacks.
  - [x] Add mergeable command sessions so continuous text, slider, vector, colour, and transform edits undo as one gesture without breaking save points.
  - [x] Add drag-reorder commands for passes, local resources, and scene models plus explicit dependency auto-order.
  - [x] Add graph pass/image/output and local-resource deletion cleanup, stable handle/version remapping, resource-reference cleanup, and local cloning.
  - [x] Add topology and document-foundation regression coverage for structural mutation, stable attachment retargeting, and command coalescing.

**Exit:** met. All authored hierarchy categories and core properties are reachable through command-backed controls; invalid structural edits remain diagnosable and undoable, while stable graph values and references survive valid moves and retargeting.

### Phase 8: Live preview and viewport diagnostics — Complete

- [x] Execute complete loaded XML workspaces transactionally in PipelineEditor.
  - [x] Resolve graph resources, imports, concrete pipeline materials, environment, overrides, successful `.mppmodel` assets, and diagnosed scene placeholders.
  - [x] Keep the previous complete graph/pipeline/scene generation when validation or candidate rebuilding fails.
- [x] Present graph output in the docked viewport.
  - [x] Use a host-owned offscreen presentation import, dynamic ImGui texture registration, dock-content resize handling, camera aspect updates, and vertically corrected display.
  - [x] Support orbit, pan, zoom, model framing, authored-view reset, and undoable `Save Current View`.
- [x] Inspect intermediate graph images.
  - [x] Select stable produced-value versions and mip levels against the active last-known-valid generation.
  - [x] Resolve inspected images into a display-safe colour target, including MSAA and attachment-only graph images.
  - [x] Add colour, individual RGB channel, alpha, luminance, linear-depth range, HDR tone-map, and HDR heat-map visualization.
- [x] Report live preview statistics.
  - [x] Show rolling frame time/FPS, viewport dimensions, allocations, lifetimes, aliases, exact loaded-model/primitive unique triangles, excluded placeholders, and submitted triangles/fullscreen quads.
  - [x] Record CPU pass durations and non-blocking timestamp-query GPU durations with pending/unavailable state and graph totals.
  - [x] Add active-context regression coverage for colour/mip and depth visualization plus asynchronous GPU timing collection.

Continuously validated background rebuilds, debounce, cancellation, and stale-job rejection are owned by Phase 9 so all parsing/decoding jobs and render-thread GPU marshalling share one generation protocol.

**Exit:** met. The docked viewport renders complete transactional workspaces, retains the last valid generation, supports camera interaction and intermediate image visualization, and reports CPU/GPU and geometry statistics without synchronously stalling the render loop.

### Phase 9: Background work and hot reload — Complete

- [x] Add a reusable single-worker background queue with cooperative cancellation, immutable generation-tagged results, progress stages, exception transport, and latest-submission replacement.
- [x] Debounce working-document changes and prepare cloned pipeline/scene generations off the UI thread.
  - [x] Run platform-independent pipeline, graph, concrete-resource, scene, and binding validation in the worker.
  - [x] Discover document, library, texture, shader, and model dependencies and pre-read/decode supported image assets.
  - [x] Reject cancelled or stale generations before any runtime mutation.
- [x] Marshal active-GPU validation, resource creation, graph/pipeline construction, scene construction, and transactional installation to the render thread.
- [x] Add a content-aware background file watcher.
  - [x] Track pipeline, scene, external library, local/external texture and shader payloads, and referenced model files.
  - [x] Require a stable revision before reporting replacement and suppress acknowledged editor saves.
- [x] Add hot reload behavior.
  - [x] Automatically parse, validate, decode, build, and install clean workspaces after dependency changes.
  - [x] Preserve the previous complete generation when parsing, validation, decoding, shader/resource creation, or GPU installation fails.
  - [x] Route changes to the existing conflict banner whenever pipeline or scene edits are dirty, and retain explicit reload/overwrite/keep-local actions.
  - [x] Refresh document snapshots and dependency baselines only after the complete hot-reload candidate installs.
- [x] Show queued/running stage and fractional progress in the toolbar and status bar, with render-thread installation and stale-preview diagnostics.
- [x] Add deterministic foundation coverage for cancellation, generation replacement, progress results, stable file observation, and own-save suppression.
- [x] Defer shared cached resources whose previous generation remains referenced and retry cleanup after later generations release aliases.

**Exit:** met. Rapid edits cancel older preparation, only the current immutable result can reach render-thread installation, clean dependency changes reload transactionally, and dirty workspaces are never overwritten by hot reload.

### Phase 10: Templates, shared assets, docs, and final validation — Complete

- [x] Move reusable preview scene, resource library, and image assets into `resources/shared/pbr` and remove per-application copies.
- [x] Deploy deterministic self-contained resource trees for PipelineEditor and DemoSuite, removing stale output resources before each copy.
- [x] Add the default preview scene and Minimal, Shadows, Full, and Empty native pipeline templates.
- [x] Expose all four templates through **File > New** as untitled pipeline/scene copies that cannot overwrite shipped assets.
- [x] Add native pipeline XML and scene XML specifications, authoring/controls guide, diagnostics catalogue, CLI reference, and README entry points.
- [x] Expand strict parser, graph, resource, scene, version, and fallback invalid fixtures with an expected-diagnostic manifest.
- [x] Add `MPP-PIPELINE-CLI-002` parser-failure reporting and deterministic CLI syntax/error exit behavior without graphical dialogs.
- [x] Add `--smoke-test` for finite active-context startup/render/shutdown integration tests.
- [x] Add `tools/ValidatePipelineEditorPhase10.ps1` for deployment, x64/removed-symbol, XML/legacy-schema, valid template, invalid fixture, and GPU startup checks.
- [x] Add `RebuildAll2026.bat` and include MppAppSupport/PipelineEditor in the VS2026 solution so Debug and Release libraries, parsers, converters, DemoSuite, and PipelineEditor build together.
- [x] Fix deterministic scene/runtime, internal-font, ImGui texture, SDL window/context, and render-system shutdown ordering exposed by finite smoke tests.
- [x] Run Debug and Release solution builds, template CLI/GPU validation, clean deployment checks, and DemoSuite CPU/GPU regression startup.

**Exit:** met. The acceptance suite passes against cleanly regenerated Debug and Release deployment trees, all shipped templates validate and start with an active context, invalid fixtures return their stable diagnostics, and DemoSuite retains its compatibility behavior.

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
