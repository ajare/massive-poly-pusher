# Render Graph Implementation Status

Last updated: 2026-08-02

This is the implementation status for `render-graph-plan`. Existing `Default` and `PbrForward` pipeline paths remain unchanged.

## Completed

- [x] Versioned `GraphImageHandle` and `GraphPassHandle` declarations.
- [x] Image descriptors for format, absolute/relative size, usage, sampling, colour space, external, and transient intent.
- [x] Programmatic graph declaration: images, passes, sampled reads, colour writes, and depth writes.
- [x] Validation for invalid/stale handles, duplicate names, sampled usage, missing producers, read/write feedback, dependency cycles, depth/colour format-usage mismatches, and incompatible MRT dimensions/sample counts.
- [x] Deterministic topological pass ordering.
- [x] `Caps` reports draw-buffer and colour-attachment limits; caps-aware compilation rejects oversized MRT declarations.
- [x] Context-free graph diagnostics through `RenderGraph::describe()`.
- [x] Context-free allocation planning through `RenderGraph::buildAllocationPlan(viewport)`, including per-version resolved size and first/last-use interval.
- [x] Physical attachment allocation through `RenderGraphTargets`: RenderTexture mappings for RGBA8, RGBA16F, RG16F, depth24, and depth24-stencil8, with compatible non-overlapping plan intervals aliased to one target.
- [x] Imported-target binding, resolving every version of an external logical image to its application-provided backing target.
- [x] Graphics-pass execution through `RenderGraphExecutor`: per-pass framebuffer views, MRT draw buffers, colour/depth clear load operations, callback execution, and RenderSystem target-stack restoration on exceptions.
- [x] Nested XML topology parser (`RenderGraphParser`) for images, sampled reads, colour outputs, one depth output, load/store operations, and clear values.
- [x] Debug builds of `MassivePolyPusher` and `MppResourceParsers` after the graph work.

## In progress

- [x] RG5 graph legacy forward: `GraphLegacyForward` renders declared LDR colour/depth images with the existing legacy material/light contract, graph bloom/presentation, and explicit DemoSuite selection. `Default` remains unchanged.
- [~] RG3 graph PBR: `GraphPbrForward` executes graph shadow, HDR scene, bloom, tone-map, and imported-screen presentation passes; manual `PbrForward` remains the reference. DemoSuite offers an explicit graph-PBR selection, but screenshot/RenderDoc equivalence captures remain outstanding.
- [x] RG4 optional bloom-mask MRT: GraphPBR can write HDR scene colour plus an emissive bloom mask, blur/composite the mask, and falls back to threshold extraction below two draw buffers/colour attachments. DemoSuite exposes the mode; the statue `.mppmodel` was regenerated after its second shader output changed.
- [~] Resource authoring: `RenderGraphStream`/`RenderGraphTemplate` declare immutable programmatic graph resources through ResourceManager, and `FileRenderGraphStream` loads the XML topology subset as a resource. Serialization, program references, typed parameters, imported-target names, and executable bindings are not implemented.
- [~] RG2 allocation/execution: same-plan aliasing, cross-frame compatible-target pooling, imported target bindings, and capability-guarded `DontCare` store invalidation work. MSAA, mip allocation, compute passes, and automated GPU frame tests remain absent.

## Not started

- [~] Automated GPU frame/lifetime tests, MSAA, mip allocation, and compute passes. Framebuffer binding, `glDrawBuffers`, clears, callbacks, cross-frame pooling, and capability-guarded `DontCare` invalidation are implemented.
- [ ] Shader output-location validation and runtime MRT fallback.
- [ ] Opt-in graph PBR pipeline and migration of shadows, bloom, tone mapping, and presentation.
- [ ] DemoSuite graph controls and screenshot/RenderDoc comparisons.

## Current safe-use boundary

The implemented graph is safe for topology declaration, XML parsing, validation, capability checks, diagnostics, and allocation planning. It is **not** an executable renderer and must not yet replace manual pipeline target creation or pass sequencing.

See `RENDER_GRAPH_PLAN.md` for design/migration detail and `RENDER_GRAPH_IMPLEMENTATION_ISSUES.md` for blockers.
