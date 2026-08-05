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
- [x] Nested XML topology parser (`RenderGraphParser`) for images, sampled reads, colour outputs, one depth output, load/store operations, and clear values.
- [x] Debug builds of `MassivePolyPusher` and `MppResourceParsers` after the graph work.

## In progress

- [~] RG1 resource authoring: parser subset exists, but graph resource streams, serialization, program references, typed parameters, and imported-target names are not implemented.
- [~] RG2 allocation: lifetimes are planned but no `RenderTexture`/framebuffer objects are allocated and no pooling or aliasing occurs.

## Not started

- [ ] Graph framebuffer binding, `glDrawBuffers`, clears, load/store invalidation, and execution callbacks.
- [ ] Shader output-location validation and runtime MRT fallback.
- [ ] Opt-in graph PBR pipeline and migration of shadows, bloom, tone mapping, and presentation.
- [ ] DemoSuite graph controls and screenshot/RenderDoc comparisons.

## Current safe-use boundary

The implemented graph is safe for topology declaration, XML parsing, validation, capability checks, diagnostics, and allocation planning. It is **not** an executable renderer and must not yet replace manual pipeline target creation or pass sequencing.

See `RENDER_GRAPH_PLAN.md` for design/migration detail and `RENDER_GRAPH_IMPLEMENTATION_ISSUES.md` for blockers.
