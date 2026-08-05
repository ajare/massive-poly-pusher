# Render Graph Implementation Status and Issues

## Implemented foundation

`mpp::RenderGraph` currently supports programmatic declaration of versioned images and passes, sampled reads, colour/depth writes, dependency sorting, missing-producer diagnostics, read/write-feedback diagnostics, cycle diagnostics, usage validation, and compatible MRT descriptor validation. It deliberately has no OpenGL allocation/execution side effects, so this validation layer can be used before a graphics context exists.

`RenderGraph::describe()` provides a context-free diagnostic dump of images, versions, and pass dependencies. `RenderGraph::buildAllocationPlan(viewport)` additionally resolves per-version dimensions and first/last-use intervals without allocating GL objects.

## Outstanding blockers for the full plan

| Item | Why it is not yet complete | Resolution path |
|---|---|---|
| XML graph resources | XML topology is loadable through `FileRenderGraphStream` and `RenderGraphTemplate`; `<callback>` names resolve through `RenderGraphPassFactoryRegistry`, not serialized code. Serializer support, shader/program/parameter parsing, and imported-target mapping remain absent. | Add serializer support, then expand parser/streams for shader/program resources, typed parameters, and named imports. |
| Graph execution | `RenderGraphTargets` aliases compatible non-overlapping intervals, retains compatible targets for cross-frame reuse, and supports imported backing targets. `RenderGraphExecutor` creates framebuffer views, configures MRT draw buffers, clears load operations, invalidates `DontCare` stores when supported, and invokes pass callbacks. MSAA/mips, compute passes, explicit read-buffer handling, and automated GPU validation remain absent. | Add multisample/mip support, compute/pass-type support, read-buffer policy, and GPU tests. |
| Load/store operations | `Clear` executes through `glClearBuffer*`; `Load` and `DontCare` load safely preserve/ignore existing contents. `DontCare` store invalidates attachments only on OpenGL 4.3 / `ARB_invalidate_subdata`; other systems safely retain contents. | Add GPU tests for all clear/load/store combinations. |
| MRT runtime | `Caps` guards output counts; the executor creates complete pass framebuffers and calls `glDrawBuffers`; descriptor validation checks dimensions/sample counts. Shader output-location reflection/validation and an MRT shader fallback remain absent. | Add program output-location validation and a material/pipeline fallback. |
| Current PBR/bloom migration | PBR, shadows, and bloom still use their existing manual `RenderPipeline` sequencing. | Add an explicit graph-PBR mode and compare against existing reference output before changing defaults. |
| XML custom shaders/effects | Existing `PostEffect` and `PostEffectStream` are stubs. | Define the graph execution/resource contract before exposing resource-authored effect graphs. |

## Resolution policy

Do not route existing PBR or legacy rendering through the graph until RG2 target execution and RG3 screenshot/RenderDoc comparisons pass. This prevents a partially implemented graph from silently replacing validated shadow/bloom behavior.
