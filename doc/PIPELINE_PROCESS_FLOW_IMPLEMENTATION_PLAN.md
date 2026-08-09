# PipelineEditor Pass Process-Flow View Implementation Plan

## Implementation status

- **Phases 1–8 complete**: the live Process Flow feature, generation-safe selection/sampling, hardening, tests, and user documentation are implemented.
- Phase 2 propagates non-owning `SceneModel3d` identity through model/mesh instances, records every sorted `flushVertexBuffers()` submission without aggregation, and records direct shadow submissions under their active graph pass.
- Phase 3 records actual MSAA colour/depth resolves at their execution points; enabled and disabled TAA, SSAA, FXAA, and presentation stages; bypass reasons; named-output identity; per-event physical inputs/outputs; and generation-safe output resource descriptors.
- The model combines authored and actual pass order, exact batches, renderer stages, bypass diagnostics, typed dependencies, stable generation-local IDs, and independently filtered authored/physical resources.
- The layout provides a top-to-bottom execution spine, non-overlapping disabled/resource columns, named-output branches, deterministic relayout, Fit All, cursor-centred zoom, and native vertical scrolling.
- The Process Flow tab is docked with Pipeline Hierarchy and provides resource filters, edge controls, live-sample status, refresh, pan/zoom, clipped drawing, hover details, in-node RenderDoc marker summaries with exact hover labels, selection targets, and expandable batch nodes.
- Selection is synchronized in both directions for passes, images/imports, materials, and resolved scene objects; scene-generation changes invalidate pointer-derived UI data immediately.
- Snapshot polling is gated to 0.25 seconds with immediate refresh for pipeline, scene, filter, and manual invalidations; unchanged samples preserve the model and view transform.
- Empty, waiting, stale, invalid, malformed-snapshot, and large-graph states are explicit, and the authoring guide documents operation and the 500-node warning threshold.
- Telemetry remains render-thread-only, opt-in, exception-isolated, and transactionally published only after a complete frame.
- Debug and Release renderer/DemoSuite GPU validation and PipelineEditor snapshot smoke validation pass, including the combined MSAA + TAA + SSAA + FXAA path.

## 1. Goal

Add a **Process Flow** tab alongside **Pipeline Hierarchy** that visualizes the active pipeline as the process actually executed by the renderer. The view must combine:

- authored render-graph passes;
- every render-batch submission made inside scene/object passes;
- renderer-owned MSAA resolve, TAA, SSAA, FXAA, and presentation stages;
- execution-order connections;
- colour, depth, shadow, history, import, and output dependencies;
- optional authored and physical resource nodes.

The first version is read-only but its data model, selection model, and canvas interactions must not prevent later pass reordering or connection editing.

## 2. Confirmed product decisions

- A node represents an authored pass, a same-material batch group within one pass, an output-processing stage, or an optionally displayed resource.
- Batch-group nodes retain the renderer's exact submitted records: mesh, material, layer/object source, program, textures, render state, primitive range, and instance count.
- Telemetry submissions are never discarded or renderer-aggregated; submissions under the same pass and effective material are pooled into one visual batch-group node ordered by that material's first submission.
- A batch-group node starts collapsed and can expand to list its participating scene objects.
- Only batches actually submitted in the sampled frame are shown. Culled and otherwise omitted objects are not synthesized.
- The main order is the dependency-compiled order actually used by `RenderGraphExecutor`.
- A pass whose actual position differs from its authored position displays a warning icon with a tooltip containing both positions.
- Execution sequence and resource dependencies are both shown. Execution uses solid arrows; resource dependencies use typed colours and labels.
- Disabled or conditionally bypassed passes and output stages remain visible, dimmed, with the exact bypass reason.
- Enabled and disabled MSAA, TAA, SSAA, FXAA, and presentation stages are shown.
- Resource nodes are optional and hidden by default. A local toolbar exposes category toggles.
- Resource categories are independently selectable rather than represented by one all-or-nothing switch.
- Layout is automatic, top-to-bottom, and recalculated when structure changes.
- Relayout preserves pan and zoom. The user may pan, zoom, and press **Fit All**.
- The view is a tab docked with **Pipeline Hierarchy**.
- Pass selection synchronizes with the pass Inspector. Batch/material/object selection synchronizes with the existing material or scene-object Inspector selection.
- Invalid dependency/order state produces an error banner rather than a partially misleading graph.
- Live batch data is sampled every 0.25 seconds, not rebuilt every frame.
- No CPU/GPU timing is displayed in nodes in this version.
- Node positions are not serialized and are not manually persisted.

## 3. Terminology and execution model

### 3.1 Authored order

The index of a pass in `RenderGraph::getPassInfo({index})` and the order serialized in XML.

### 3.2 Actual order

The `RenderGraphCompileResult::passOrder` consumed by `RenderGraphExecutor::execute()`. This is authoritative. The view must never infer execution order from screen position or authored order.

### 3.3 Global pass

An executed render-graph pass whose callback is not a sequence of model batch submissions, including fullscreen, presentation, resolve, and renderer-owned output-processing work.

### 3.4 Batch submission

One iteration of the sorted `renderCommands` loop in `RenderSystem::flushVertexBuffers()`, immediately before the corresponding `Mesh::render()` call. This is the exact renderer submission granularity requested for the view.

### 3.5 Resource dependency

A relation from the pass/stage that produces an image or physical result to each pass/stage that reads or presents it. Dependencies preserve image version and mip identity; labels must not collapse different versions into one ambiguous edge.

## 4. Architecture

Keep renderer telemetry, flow-model construction, deterministic layout, and ImGui rendering separate.

### 4.1 Renderer telemetry layer

Add public immutable snapshot types, proposed under:

- `mpp/include/mpp/RenderPipelineFlow.h`
- `mpp/src/RenderPipelineFlow.cpp`

Proposed records:

```cpp
enum class RenderFlowEventKind
{
    PassBegin,
    PassEnd,
    BatchSubmission,
    MsaaResolve,
    Taa,
    SsaaHorizontal,
    SsaaVertical,
    Fxaa,
    Presentation
};

struct RenderBatchSubmission
{
    uint64_t sequence;
    GraphPassHandle parentPass;
    SceneModel3d const* sceneObject;
    std::string meshName;
    std::string materialName;
    std::string programName;
    std::vector<std::string> textureNames;
    mesh::Primitive::Type primitiveType;
    uint32_t offset;
    uint32_t count;
    size_t instanceCount;
    bool transparent;
    bool blend;
    bool cullBackFaces;
    bool wireframe;
};

struct RenderPipelineFlowSnapshot
{
    uint64_t frameSerial;
    uint64_t pipelineGeneration;
    std::vector<GraphPassHandle> actualPassOrder;
    std::vector<RenderBatchSubmission> batches;
    std::vector<RenderFlowEvent> physicalEvents;
};
```

Exact names may change during implementation, but the contract must remain immutable to the UI and safe to retain until the next 0.25-second sample.

### 4.2 Pass execution instrumentation

`RenderGraphExecutor` already owns authoritative compiled order and pass boundaries. Extend it to:

1. retain the last successful `compiled.passOrder`;
2. identify the currently executing pass to renderer telemetry;
3. begin/end a flow scope around each callback/declarative pass;
4. clear incomplete frame telemetry after exceptions;
5. expose the last completed flow snapshot through `RenderPipeline`.

Do not reconstruct order from `GraphPassExecutionStats`: those statistics are asynchronous and are not intended as topology records.

### 4.3 Exact batch-submission instrumentation

Instrument the sorted submission loop in `RenderSystem::flushVertexBuffers()` immediately before `mesh->render(...)`.

To retain scene-object identity through batching:

1. `RenderPass::render()` supplies the source `SceneModel3d*` when queueing each model.
2. `ModelInstance`/`MeshInstance` carry that non-owning source identity until the flush completes.
3. Each sorted command copies source identity into `RenderBatchSubmission`.
4. PipelineEditor resolves pointers back to scene-document IDs using `SceneRuntime`'s active model map.
5. The recorder never extends object lifetime; generation changes discard stale samples before UI resolution.

Record state after sort and immediately before submission so transparent ordering and actual renderer state are represented correctly. Every loop iteration remains an exact telemetry record; the editor model may visually pool contiguous records with the same parent pass and effective material.

### 4.4 Physical output-stage instrumentation

Use the immutable `RenderPipelineOutputPlan` and actual processor calls to represent:

- MSAA disabled or each scheduled colour/depth resolve;
- TAA disabled or the active temporal pass;
- SSAA disabled or horizontal/vertical Lanczos passes;
- FXAA disabled or the active final pass;
- final named-output presentation.

For enabled work, record the actual event at its execution position. For disabled techniques, synthesize one dimmed descriptor from effective output settings with reason `Disabled by effective output setting`.

MSAA resolves can occur before sampled reads, not only at final output. Record resolve events where `RenderGraphTargets::resolve()` is actually invoked and associate them with the relevant image/version and following pass.

### 4.5 Snapshot ownership and thread rules

- Rendering and telemetry mutation remain on the render thread.
- At frame completion, swap a completed immutable snapshot into `RenderPipeline`.
- PipelineEditor copies or shares that completed snapshot at most every 0.25 seconds.
- Pipeline regeneration invalidates old pointer identities and clears the sampled snapshot.
- The background document-validation thread must never inspect renderer telemetry.
- Telemetry collection should be enabled only when a consumer requests it, avoiding permanent string/vector work for DemoSuite and other applications.

## 5. Static process-flow model

Add editor-side model/layout files rather than returning complexity to `Main.cpp`:

- `pipeline-editor/include/ProcessFlowModel.h`
- `pipeline-editor/src/ProcessFlowModel.cpp`
- `pipeline-editor/include/ProcessFlowLayout.h`
- `pipeline-editor/src/ProcessFlowLayout.cpp`
- `pipeline-editor/include/ProcessFlowView.h`
- `pipeline-editor/src/ProcessFlowView.cpp`

Register them in `PipelineEditor.vcxproj` and `.filters`.

### 5.1 Node model

```cpp
enum class ProcessFlowNodeKind
{
    AuthoredPass,
    BatchSubmission,
    BatchGroup,
    MsaaResolve,
    Taa,
    Ssaa,
    Fxaa,
    Presentation,
    AuthoredImage,
    Import,
    NamedOutput,
    TaaHistory,
    PhysicalWorkTarget
};
```

Each node has:

- stable generation-local ID;
- display title and subtitle;
- kind and typed visual style;
- actual sequence index, where applicable;
- authored and actual pass positions;
- enabled/bypassed state and reason;
- selection target (pass, material resource, or scene object);
- collapsed/expanded state for batch object lists;
- measured canvas size and automatic position;
- resource category for visibility filtering.

Stable IDs use semantic keys plus generation: pass ID, submission sequence, output name/stage, or image ID/version. Never use display names alone.

### 5.2 Edge model

```cpp
enum class ProcessFlowEdgeKind
{
    Execution,
    Colour,
    Depth,
    Shadow,
    History,
    Import,
    Output
};
```

Execution edges are solid neutral arrows. Resource edges use distinct colours and labels containing image name, version, and mip where relevant. Edge identity includes source, destination, resource handle, and kind.

When resource nodes are hidden, draw a direct producer-to-consumer edge. When a category is enabled, replace that direct edge with producer-to-resource and resource-to-consumer edges.

### 5.3 Disabled and bypassed nodes

Include all authored passes even when absent from actual order. Determine reason using the same effective rules used by PipelineEditor and runtime:

- authored `enabled=false`;
- bloom disabled;
- blur index beyond effective `blurPasses`;
- graph pass category disabled;
- missing optional stage due effective AA setting;
- output technique unsupported only as an error, not a silent disabled state.

Disabled nodes are dimmed and excluded from the solid actual-execution spine. Position them near their authored neighbours and expose the reason in node text/tooltip.

### 5.4 Authored versus actual order

For each executed authored pass:

- store authored index;
- store actual index from compiled order;
- show a warning icon only when they differ;
- tooltip: `Authored position N; executed position M after dependency compilation.`

Do not report a difference merely because batch or physical nodes are inserted between passes.

## 6. Automatic top-to-bottom layout

### 6.1 Main execution spine

Build one ordered list:

1. executed pass node;
2. one node per effective material, ordered by that material's first exact submission and retaining all submission records/counts;
3. inter-pass resolve events at their actual position;
4. next executed pass;
5. TAA, SSAA, FXAA, and named presentation stages in actual output order.

Assign monotonically increasing y coordinates with configurable node and stage gaps. Keep the primary spine centred in its column.

### 6.2 Secondary nodes

- Disabled passes occupy a nearby secondary column anchored between authored neighbours.
- Optional resource nodes occupy columns beside the spine based on dependency type.
- Multiple named outputs branch into a separate output column after the shared pipeline stages.
- Expanded batch object rows increase node height and trigger deterministic relayout.

### 6.3 Determinism and overlap

The same model and visibility settings must produce identical positions. Layout tests must assert:

- strictly increasing main-spine y positions;
- no overlapping visible node rectangles;
- stable positions across repeated layout;
- output branches do not overlap;
- hidden resource categories do not leave layout gaps.

### 6.4 View preservation

Model/layout revision changes recalculate node positions but preserve current canvas pan and zoom. `Fit All` computes bounds and applies a padded transform. Initial opening performs one automatic fit.

## 7. Process Flow UI

### 7.1 Docking

Create an ImGui window named **Process Flow** and dock it into the same dock node as **Pipeline Hierarchy**, creating tabs. Keep Pipeline Hierarchy intact.

### 7.2 Local toolbar

The Process Flow tab contains:

- **Fit All**;
- live sample age and manual **Refresh** fallback;
- independent resource-category toggles:
  - authored images;
  - imports;
  - named outputs;
  - MSAA resolve resources;
  - TAA histories;
  - SSAA work targets;
  - FXAA work targets;
- optional execution-edge and resource-edge visibility controls if clutter requires them during implementation;
- a compact legend for node/edge colours.

All resource categories start hidden each editor launch, as confirmed.

### 7.3 Canvas interaction

Implement with ImGui draw lists; do not add a third-party node-editor dependency for the first version.

- middle-button drag pans horizontally and scrolls vertically;
- a native vertical scrollbar navigates long process flows;
- mouse wheel zooms around cursor position;
- clamp zoom to a usable range, proposed 0.25–2.5;
- clip node and edge rendering to canvas bounds;
- `Fit All` frames all currently visible nodes;
- hover emphasizes related edges and shows detailed tooltip;
- left click selects;
- double-clicking a collapsed batch toggles its object list;
- no drag-to-reorder or drag-to-connect behaviour yet.

### 7.4 Selection synchronization

- Authored pass node: set `selectedPass`, clear conflicting selection fields, and focus the pass Inspector.
- Batch node: select its material resource when the body is clicked.
- Expanded object entry: select the corresponding scene model.
- Material/object selection originating elsewhere highlights relevant visible batch nodes.
- Resource nodes select the corresponding image/import where an authored target exists; physical-only resources show details locally without inventing an Inspector target.

Selection changes must not regenerate the pipeline.

### 7.5 Error and empty states

- Invalid dependency compilation: show a prominent error banner and no process graph.
- No active pipeline: show `No active pipeline generation.`
- Active pipeline but no completed live sample: show static pass/output structure and `Waiting for live batch sample…`.
- Stale retained generation: explicitly label that the flow belongs to the last valid generation.

## 8. Sampling and invalidation

PipelineEditor keeps a 0.25-second sampling timer.

Refresh when:

- at least 0.25 seconds elapsed and a newer completed frame snapshot exists;
- pipeline generation changed;
- effective AA/output plan changed;
- pass enabled/bypass state changed;
- scene runtime generation changed;
- resource-category visibility changed;
- a batch node expands/collapses.

Do not rebuild the model merely because ImGui redraws. If no newer frame is available, preserve the current model and update only sample age.

Invalidate object-pointer resolution immediately on scene regeneration. Never display stale object names against a new scene generation.

## 9. Resource categories

### 9.1 Authored images

Show image/version nodes with format, resolved dimensions, mip, transient/external status, and colour/depth role.

### 9.2 Imports

Show typed import ID and bound runtime target. Missing required imports are validation errors and use the error banner.

### 9.3 Named outputs

Show output name, authored image, logical/raster dimensions, effective MSAA/SSAA/TAA/FXAA, and destination type.

### 9.4 MSAA resources

Show private multisampled write target and resolved single-sample image per actual resolve event where enabled.

### 9.5 TAA resources

Show current depth input, previous depth, two colour-history ping-pong targets, and current history direction.

### 9.6 SSAA resources

Show horizontal and vertical Lanczos work targets with supersampled and logical dimensions.

### 9.7 FXAA resources

Show final LDR work target and destination.

Physical resource nodes must use public immutable descriptors from `RenderOutputProcessor`; the editor must not reach into OpenGL IDs or renderer ownership internals.

## 10. API and source changes

Expected renderer changes:

- new flow snapshot/telemetry types;
- `RenderPipeline` opt-in telemetry and last-snapshot accessor;
- `RenderGraphExecutor` actual-order and current-pass instrumentation;
- `RenderSystem::flushVertexBuffers()` exact batch-event recording;
- source-object propagation through queued model/mesh instances;
- resolve and output-processor event recording;
- immutable physical resource descriptors sufficient for optional nodes.

Expected PipelineEditor changes:

- process-flow model builder;
- deterministic layout engine;
- ImGui canvas renderer/controller;
- docking and selection synchronization in `Main.cpp`;
- 0.25-second sampler and generation invalidation;
- project file/filter entries;
- authoring-guide documentation.

Keep `Main.cpp` integration narrow: construct the view controller, pass current document/runtime/pipeline selection state, consume selection results, and call `draw()`.

## 11. Transactional and failure behaviour

- Telemetry failure must never fail rendering.
- Allocation/pipeline regeneration remains authoritative and transactional.
- Snapshot construction uses the last completely rendered frame only.
- Exceptions during a pass discard the in-progress snapshot.
- UI model construction catches malformed snapshot references and shows an error banner while retaining the prior valid UI model only if it belongs to the same renderer generation.
- Process Flow never mutates the graph in this phase.

## 12. Performance constraints

- Telemetry is opt-in and records only primitive descriptors, stable names/IDs, and non-owning source identity.
- Reserve event arrays from recent high-water marks.
- Sample/copy at 4 Hz.
- Build strings during snapshot finalization or editor model construction, not for each ImGui paint.
- Cull nodes outside the canvas clip rectangle.
- Cull resource edges when both endpoints and the routed segment are outside view.
- Every batch remains represented as required; do not silently aggregate large scenes.
- Display a non-fatal large-graph warning when node count exceeds a documented threshold, but retain complete data.

## 13. Testing plan

### 13.1 Unit tests

- Authored versus compiled pass-order comparison.
- Warning emitted only when positions differ.
- Disabled/bypassed pass reason resolution.
- Exact telemetry submissions remain retained, while same-material records within a pass form one batch-group node with the correct submission count.
- Batch order equals renderer submission order, including transparent sorting.
- Resource dependency classification by colour/depth/shadow/history/import/output.
- Direct dependency edges transform correctly when resource nodes are toggled.
- Output stage ordering for every AA combination.
- Disabled AA stages appear dimmed with correct reasons.
- Stable node IDs within a generation and invalidation across generations.
- Deterministic layout and non-overlap.
- Fit-all transform and zoom-around-cursor calculations.

### 13.2 Renderer/GPU tests

- One scene pass records pass begin, every batch, and pass end in order.
- Multiple identical submissions produce multiple records.
- Culled objects produce no records.
- Opaque and transparent submission ordering matches `flushVertexBuffers()`.
- Shadow and main scene submissions associate with the correct parent pass.
- MSAA resolve events occur before dependent sampled passes/TAA.
- Combined MSAA + TAA + SSAA + FXAA records exact output ordering.
- Telemetry disabled produces no retained events and no OpenGL errors.
- Pipeline regeneration clears stale snapshots.

### 13.3 PipelineEditor tests

- Process Flow window docks as a tab with Pipeline Hierarchy.
- Pass-node selection synchronizes with Inspector.
- Batch/material and expanded object selection synchronize correctly.
- Disabled passes show reason tooltips.
- Invalid graphs show the error banner.
- Resource categories start hidden and toggle independently.
- Sampling does not occur faster than 0.25 seconds.
- Relayout preserves pan/zoom; Fit All frames visible graph.
- Pipeline/scene/AA regeneration invalidates the correct parts without resetting unrelated editor state.
- Existing preview, image inspection, RenderDoc capture, undo/redo, package export, and AA controls continue to work.

### 13.4 Build and smoke matrix

- PipelineEditor Debug and Release builds.
- DemoSuite Debug and Release builds because renderer telemetry touches shared code.
- Existing DemoSuite render-graph/GPU suite.
- PipelineEditor default Full pipeline smoke test.
- PipelineEditor all-AA smoke test.
- Process-flow-specific smoke mode that verifies a completed snapshot, actual pass order, at least one batch submission, output stages, and selection-model construction.
- Package export/load smoke test to ensure telemetry remains runtime-only and is not serialized.

## 14. Implementation phases

1. **Flow contracts and compiled order — COMPLETE**
   - [x] Add `RenderPipelineFlow` event, batch, and immutable shared snapshot contracts.
   - [x] Add opt-in ownership/publication to `RenderPipeline`, disabled by default and cleared when disabled.
   - [x] Assign a stable generation to each runtime pipeline and publish only after successful graph execution and output presentation.
   - [x] Retain and expose the last successfully executed compiled pass order from `RenderGraphExecutor` and `RenderPipeline`.
   - [x] Opt PipelineEditor preview generations into telemetry without adding UI ahead of phase 6.
   - [x] Test const snapshot publication contracts, event naming, disabled-pass filtering in actual order, executor order retention, and PipelineEditor snapshot publication.

2. **Exact batch telemetry — COMPLETE**
   - [x] Propagate non-owning source identity from `SceneModel3d` through `ModelInstance` and `MeshInstance`.
   - [x] Record every sorted `flushVertexBuffers()` submission immediately before `Mesh::render()`, including effective material, program, textures, primitive range, instance count, transparency, blend, cull, and wireframe state.
   - [x] Record direct shadow-map mesh submissions under the active shadow graph pass.
   - [x] Keep repeated identical submissions separate and preserve one global event sequence with pass begin/end boundaries.
   - [x] Add recorder and PipelineEditor smoke coverage for duplicate retention, strict ordering, source identity, complete descriptors, and parent-pass association.

3. **Physical output events/resources — COMPLETE**
   - [x] Record actual colour/depth MSAA resolves where multisampled graph targets are resolved.
   - [x] Record enabled TAA, SSAA horizontal/vertical, FXAA, and presentation work in actual call order.
   - [x] Emit disabled/bypassed stage descriptors with exact reasons and named-output identity.
   - [x] Copy immutable `RenderPipelineOutputPlan` descriptors into each completed snapshot and attach named format/size/sample descriptors to every enabled physical event.
   - [x] Validate default-disabled and combined MSAA + TAA + SSAA + FXAA stage combinations through renderer and PipelineEditor smoke coverage.

4. **Editor model builder — COMPLETE**
   - [x] Combine authored graph, compiled order, live snapshot, bypass rules, dependencies, and output plans.
   - [x] Add stable generation-local IDs, selection targets, independent filters, typed resource edges, and diagnostics.
   - [x] Preserve duplicate telemetry submissions, visually group same-material records per pass, and transform direct dependencies through optional resource nodes.

5. **Automatic layout — COMPLETE**
   - [x] Build the horizontally centred vertical execution spine with inline enabled/bypassed AA stages, disabled-authored-pass/resource columns, and named-output branches.
   - [x] Preserve view transforms across deterministic relayout and provide Fit All/cursor-centred zoom transforms.
   - [x] Add deterministic ordering, non-overlap, Fit All, and zoom geometry tests.

6. **ImGui Process Flow tab — COMPLETE**
   - [x] Dock Process Flow alongside Pipeline Hierarchy without replacing the hierarchy.
   - [x] Add local resource/edge controls, sample status, Refresh, legend, and Fit All.
   - [x] Add clipped node/edge rendering, pan, zoom, hover emphasis/details, in-node RenderDoc marker summaries and exact hover labels, click targets, and batch expansion.

7. **Selection and live sampling — COMPLETE**
   - [x] Synchronize pass, image/import, material, and scene-object selections with the existing Inspector and highlight matching flow nodes for hierarchy-originated selections.
   - [x] Poll immutable snapshots at most every 0.25 seconds while supporting immediate generation/filter/manual refresh.
   - [x] Resolve source pointers through `SceneRuntime` IDs and invalidate sampled pointer-derived data immediately when the scene generation changes.

8. **Hardening and documentation — COMPLETE**
   - [x] Add static waiting structure, no-generation, stale last-valid, invalid dependency, malformed-snapshot retention, and complete large-graph states.
   - [x] Retain every node above the documented 500-node warning threshold and keep viewport culling/navigation active.
   - [x] Expand unit/smoke coverage and complete Debug/Release, all-AA, renderer GPU, and package regression validation.
   - [x] Document Process Flow navigation, filters, selection, sampling, stale/error behaviour, and large-graph handling in the authoring guide.

## 15. Completion criteria

The work is complete when the Process Flow tab can display a live Full pipeline frame from authored pass start through every submitted batch represented by its material group and every renderer-owned final output stage; its solid spine preserves actual pass/stage order and material-group first-submission order; dependency edges identify data flow; authored-order differences and bypass reasons are explicit; optional resource categories work; selection synchronizes with the existing Inspector; navigation and automatic layout remain usable; snapshots update at 0.25-second intervals; and all renderer/editor/package regression tests pass in Debug and Release.
