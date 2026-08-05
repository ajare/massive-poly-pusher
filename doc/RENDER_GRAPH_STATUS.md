# Render Graph Implementation Status

Last updated: 2026-08-05

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
- [x] RG3 graph PBR parity: manual PBR, hardcoded GraphPBR, and XML GraphPBR execute in DemoSuite and have user-confirmed visual parity. XML bloom threshold/intensity, exposure, tone-map operator, and bloom enabled state follow live pipeline controls through runtime overrides. Automated screenshot comparison and archived RenderDoc captures remain optional hardening.
- [x] RG4 optional bloom-mask MRT: hardcoded and XML GraphPBR have emissive-mask variants and safely fall back to threshold extraction unless hardware and every visible material expose locations 0 and 1. XML threshold/MRT templates contain four blur iterations; disabled iterations pass through their input so the live 1–4 blur-count control works without topology mutation. The statue `.mppmodel` includes its second shader output.
- [x] Context-free regression entry points: `runRenderGraphTopologyTests()` covers valid/missing-producer/feedback topology; `runRenderGraphResourceTests()` covers XML descriptors, imports, pass metadata, parameters, sampler bindings, and XML round-trip serialization. A dedicated CI test executable remains optional.
- [x] Resource authoring: programmatic/XML graph templates, XML serialization, program/sampler references, typed parameters and overrides, named imports, executable factories, and declarative fullscreen passes are implemented.
- [~] RG2 allocation/execution: cross-frame pooling, imported bindings, capability-guarded `DontCare` invalidation, and validated same-frame transient aliasing work. The allocator aliases only physically compatible transient images whose inclusive pass intervals do not overlap, and rejects conflicting assignments. DemoSuite startup validates framebuffer output, resize, MRT locations, generated mip readback, alias/non-alias cases, execution, and target release. MSAA, explicit mip-subresource writes/views, and compute passes remain absent.

## Not started

- [~] MSAA, explicit mip-subresource writes/views, compute passes, and broader effect/readback comparisons. Declared colour/depth mip chains are allocated, constrained to the requested range, regenerated after writes, dimension-validated, and GPU-readback tested.
- [x] Shader output-location validation and runtime MRT fallback. During graph execution every selected program is checked for active fragment locations required by the pass (when program-interface reflection is available); extra outputs remain legal. GraphPBR enables bloom-mask MRT only when hardware and every visible scene material expose locations 0 and 1, otherwise it falls back to threshold bloom.
- [x] Opt-in hardcoded and XML graph PBR pipelines with shadows, bloom, tone mapping, and presentation.
- [~] DemoSuite graph controls and comparisons. Pipeline/pass isolation and live image-effect controls are implemented; automated screenshots and RenderDoc archives remain.

## Current safe-use boundary

The implemented graph is executable and has user-confirmed visual parity with the manual DemoSuite PBR/legacy references. Manual paths remain available as compatibility references. MSAA, compute passes, and automated GPU regression coverage remain outside the validated boundary.

See `RENDER_GRAPH_PLAN.md` for design/migration detail and `RENDER_GRAPH_IMPLEMENTATION_ISSUES.md` for blockers.
