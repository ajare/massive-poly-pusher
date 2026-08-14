# OpenGL Engine Improvement Analysis

## Executive assessment

The engine already has a strong foundation: resource-driven shaders/materials, PBR, shadow domains, HDR bloom, several anti-aliasing modes, render-graph allocation/aliasing, XML graph templates, GPU timing, and debug labels. The best next work is not another rendering feature—it is fixing several correctness hazards, then reducing render-graph CPU/driver overhead.

## Priority findings

### 1. Fix capability reporting immediately — implemented

**Status:** Completed on `fix/capability-reporting`.

`RenderSystem` queried texture-unit limits into `maxTextureUnits`, but stored `maxUniforms` instead:

- `mpp/src/RenderSystem.cpp:607-615`

Consequently `Caps::maxFragmentTextureUnits` could report thousands rather than the real hardware limit. The material check at `mpp/src/RenderSystem.cpp:3909-3913` was therefore ineffective and oversized sampler configurations could reach invalid texture units.

**Implemented changes**

- Store the queried vertex, geometry, and fragment texture-unit limits correctly.
- Give every `Caps` member a safe default value.
- Query anisotropy only when an anisotropic-filtering extension is available, and clamp texture/sampler requests to the reported limit.
- Populate `Caps::maxElements` from `GL_MAX_ELEMENTS_INDICES`.
- Report maximum vertex attributes and vertex stride.
- Reject unsupported attribute locations, oversized strides, negative offsets, and attributes extending beyond their stride before issuing GL calls.

---

### 2. Preserve resource sort-ID stability — implemented

**Status:** Completed on `fix/capability-reporting`.

Resources receive permanent sort IDs, and render commands later resolve those IDs through vectors. Removal previously erased vector elements:

- Assignment: `mpp/src/ResourceManager.cpp:250-273`
- Erasure: `mpp/src/ResourceManager.cpp:290-300`
- Unchecked lookup: `mpp/src/ResourceManager.cpp:781-793`
- Draw-time use: `mpp/src/RenderSystem.cpp:3888`

Erasing an element shifted every subsequent vector entry while their stored IDs remained unchanged. Existing render commands could consequently select the wrong texture/program or index beyond the vector.

**Implemented changes**

- Keep ID-indexed vectors sparse and clear removed slots without shifting them.
- Reject zero, out-of-range, and removed sort IDs with actionable exceptions.
- Keep IDs monotonic and non-reusable for the manager lifetime.
- Scope sort-ID counters to each `ResourceManager` instead of mutable process-global statics.
- Remove program-cache entries with checked iterator-safe traversal instead of risking `erase(end())`.
- Add GPU-context regression coverage for texture/program removal, stable later IDs, non-reuse, bounds checks, and cached program aliases.

---

### 3. Make `Program` reflection and loading transactional — implemented

**Status:** Completed on `fix/capability-reporting`.

Several uniform-location members were not initialized by the constructor:

- Declaration: `mpp/include/mpp/Program.h:85-88`
- Constructor initializes only shader and sort IDs: `mpp/src/Program.cpp:30-35`

If a standard uniform was optimized out, its getter could return an indeterminate value. In addition, `unloadImpl()` cleared `mUniformTypes` but not `mUniformIds`, leaving stale locations after reload.

Program creation also owned several raw GL names during a multi-step operation. A shader compile, link, reflection, or labeling exception could leave partially built state.

**Implemented changes**

- Initialize every standard uniform location to `-1`.
- Reset all reflection maps, sampler metadata, locations, and output metadata before load and during unload.
- Build shaders and linked programs in local RAII handles; publish reflection state and the program ID only after every link/reflection operation succeeds.
- Use `std::vector<char>` for shader and program info logs.
- Reflect fragment output locations once after linking and validate MRT requirements from a cached mask.
- Add GPU-context regression coverage for default locations, unload/reload state, cached output validation, and failed-load cleanup.

---

### 4. Replace the lossy mesh “hash” used for program selection — implemented

**Status:** Completed on `fix/capability-reporting`.

`MeshSpecification::getHashCode()` previously omitted data types and most layout details:

- `mpp-mesh/src/MeshSpecification.cpp:360-469`

It collapsed repeated semantics and treated layouts such as `vec2` and `ivec2` identically. `getDescriptor()` derived generated resource names from it, and those descriptors are used by program creation in `ResourceManager`.

This could reuse or name a program for an incompatible vertex layout.

**Implemented changes**

- Define a versioned, unambiguous canonical key containing primitive, storage, indexed state, ordered buffer layouts, and every attribute equality field.
- Replace the packed semantic bits with deterministic FNV-1a over the canonical key.
- Use the full canonical key—not its 32-bit digest—in the program cache key, with length-prefixed shader-stage sections.
- Keep generated descriptors readable while appending the compact digest; retain a serial suffix so digest collisions cannot become resource-name collisions.
- Add topology-test coverage for data type, normalization, padding/offset effects, identifiers, primitive/storage/indexed state, layout grouping, user attributes, stable copies, hashes, and descriptors.

---

### 5. Cache render-graph compilation and framebuffer objects — partially implemented

**Status:** Compiled graph and allocation-plan caching completed on `perf/cache-render-graph-plans`; framebuffer-view caching remains.

Every graph execution recompiles validation:

- `mpp/src/RenderGraphExecutor.cpp:351`

The XML path also rebuilds the allocation plan each frame:

- `mpp/src/RenderPipeline.cpp:264`

Every executed pass constructs and later deletes a framebuffer:

- Creation: `mpp/src/RenderGraphExecutor.cpp:55-90`
- Destruction: `mpp/src/RenderGraphExecutor.cpp:92-95`
- Per-pass construction: `mpp/src/RenderGraphExecutor.cpp:432`

This produces avoidable CPU work and GL object churn even when topology, viewport, and attachments are unchanged.

**Change**

Introduce a compiled graph object cached by:

- graph/template generation,
- viewport and SSAA dimensions,
- effective sample count,
- relevant capability signature,
- import/attachment generations.

Cache framebuffer views by attachment texture IDs, mip levels, depth/stencil aspect, and draw-buffer configuration. Invalidate entries when backing targets are replaced.

**Implemented changes**

- Cache context-free topology compilation for unchanged graph definitions, including invalid results and diagnostics.
- Cache device validation by viewport and the capability fields it actually consumes: maximum texture size, colour attachments, and draw buffers.
- Cache allocation plans by viewport, including invalid plans, while preserving the topology cache shared by allocation and execution.
- Invalidate all dependent plans from every topology, attachment, descriptor, ordering, enabled-state, name, and stable-value-ID edit that can affect compilation or allocation output.
- Keep caches private to each graph generation: copies receive independent empty caches, while moves transfer the cached plans with their graph.
- Expose cache hit/miss/invalidation counters for diagnostics and regression coverage.
- Add context-free tests for compile hits, allocation hits, topology invalidation, viewport isolation, capability-signature isolation, and copy behavior.

**Remaining work**

- Cache framebuffer views by attachment identity and generation, including effective sample count and import replacement.

---

### 6. Eliminate synchronous GL state interrogation per pass — high payoff, medium/high effort

`GraphRasterStateScope` reads extensive driver state whenever explicit state is used:

- `mpp/src/RenderGraphExecutor.cpp:145-156`

Additional reads occur during clears and invalidation:

- `mpp/src/RenderGraphExecutor.cpp:100`
- `mpp/src/RenderGraphExecutor.cpp:189`
- `mpp/src/RenderGraphExecutor.cpp:457`

`glGet*` calls can serialize command submission and make graph cost scale poorly with pass count.

**Change**

- Make `RenderSystem` own an authoritative state cache.
- Have graph passes apply state through the same cache instead of modifying GL independently.
- Restore a known engine state snapshot, not driver-queried state.
- Retain optional debug-only verification that compares cached and actual GL state periodically.

Benchmark CPU frame time with 10, 50, and 100 lightweight passes before and after the change.

---

### 7. Cache MRT shader-output validation — medium/high severity, low effort

`sceneProgramsSupportOutputs()` walks every visible model and mesh:

- `mpp/src/RenderPipeline.cpp:41-55`
- Called at `RenderPipeline.cpp:255` and `:335`

Each call can invoke GL program-interface reflection:

- `mpp/src/Program.cpp:729-752`

This repeats work for shared materials and potentially every frame.

**Change**

Reflect active fragment output locations once after linking and store a bit mask in `Program`. During scene validation, inspect each unique program once. Cache the scene-level result by visible-program-set or material revision.

---

### 8. Complete or quarantine unfinished public features — medium severity

#### Particle system — removed

**Status:** Completed on `fix/capability-reporting`.

The unused `ParticleSystem` API only allocated and leaked a CPU array; it had no GPU buffers, transform feedback, update/render path, or cleanup. The public `ParticleSystem` class, its implementation, and the now-orphaned `Particle` structure have been removed rather than preserving a nonfunctional API. A future particle feature should be designed around the render graph and an explicit compute or transform-feedback backend.

#### Clip rectangle — implemented

**Status:** Completed on `fix/capability-reporting`.

`ClipRectangle::intersect()` now computes normalized half-open intersections. Negative dimensions are normalized, disjoint and edge-touching rectangles return an empty rectangle, endpoint arithmetic uses 64-bit intermediates, and unrepresentable dimensions saturate rather than overflowing into invalid negative scissor sizes. Regression coverage includes contained, partial, disjoint, touching, negative-dimension, and `INT_MAX`-adjacent cases.

#### Generic post effects

`PostEffect` lifecycle methods are empty and output allocation is absent:

- `mpp/src/PostEffect.cpp:30-57`

`RenderPipeline::addPostEffect()` stores effects, but there is no execution path:

- `mpp/src/RenderPipeline.cpp:232-235`

Rather than create a second sequencing system, define a post effect as a reusable render-graph fragment with named inputs/outputs, parameters, colour-space stage, and capability requirements.

---

### 9. Implement the advertised streaming-geometry path — medium priority, medium effort

Capabilities report support for buffer storage/map range, but `VertexBuffer::allocate()` throws whenever streaming is selected:

- `mpp/src/VertexBuffer.cpp:203-208`

The fallback paths either re-upload the entire allocation or use blocking `glMapBuffer`:

- `VertexBuffer.cpp:230-276`

**Change**

Implement a persistently mapped ring buffer using `glBufferStorage`, coherent or explicit flush policy, and fences for segment reuse. Apply the same design to dynamic index and uniform buffers. Provide a `glBufferSubData` fallback for unsupported systems.

This should materially improve batches, debug geometry, particles, and frequently updated UI meshes.

---

### 10. Extend render-graph compilation rather than adding more manual pipeline branches

The graph has a separate dependency-order implementation, but normal `compile()` validates authored order and returns that order:

- `mpp/src/RenderGraph.cpp:518-674`

Execution calls this normal caps-aware compile. This leaves useful compiler work available:

- derive topological order automatically,
- cull passes not contributing to exported/presentation outputs,
- report unused outputs,
- fold disabled/pass-through effects,
- compute barriers for future compute/image operations,
- return compilation and allocation as one immutable artifact.

Dead-pass culling is especially useful for optional bloom, debug views, and effect templates, avoiding topology mutation and unnecessary allocations.

---

### 11. Replace temporary mip-state mutation with texture views — medium priority

Single-mip sampling changes `GL_TEXTURE_BASE_LEVEL` and `GL_TEXTURE_MAX_LEVEL` on the underlying texture:

- `mpp/src/RenderTexture.cpp:361-390`
- Applied/restored around passes at `mpp/src/RenderGraphExecutor.cpp:500-560`

These are texture-object-global properties. The executor consequently rejects simultaneous different views and correctness depends on restoration across every exit path.

**Change**

Use `glTextureView` where supported, cached by backing texture/format/mip range. Keep mutation as a capability fallback with strict alias validation. This also prepares the graph for layered rendering and independent mip-chain operations.

---

### 12. Improve bloom/PBR quality through graph-native features — feature extension

The current effect stack is mature enough to extend without another hardcoded pipeline:

1. **Downsampled bloom pyramid** using `R11G11B10F` or reduced-channel formats where appropriate.
2. **Histogram or luminance-pyramid exposure adaptation**.
3. **Colour grading LUT** between HDR composition and display encoding.
4. **SSAO/fog** through explicit depth inputs.
5. **Compute passes** for bloom, luminance, particles, and culling.

Before expanding PBR, add visual-regression scenes for:

- dielectric/metallic sphere grids,
- roughness extremes,
- normal maps,
- emissive bloom,
- alpha mask/blend,
- direct light and shadow bias,
- sRGB texture decoding and final framebuffer encoding.

The current validation documentation explicitly notes several of these asset gaps in `doc/PBR_VALIDATION.md`.

## Recommended implementation order

### Immediate hardening

1. ✅ Fix texture-unit capability assignments and related capability validation.
2. ✅ Fix sort-ID table removal and cache erasure.
3. ✅ Initialize/reset `Program` reflection state and make loading transactional.
4. ✅ Replace the mesh layout key and add collision tests.
5. ✅ Implement `ClipRectangle::intersect()` and remove the unused particle API.

### Performance pass

6. ✅ Cache compiled graph/allocation plans.
7. Cache framebuffer views.
8. Integrate graph state with a `RenderSystem` state cache.
9. Cache reflected output masks and unique-program scene validation.
10. Add persistent mapped streaming buffers.

### Feature growth

11. Make `PostEffect` a render-graph fragment.
12. Add dead-pass culling, texture views, and compute passes.
13. Build downsampled bloom/exposure/colour grading.
14. Add automated image comparisons and archived GPU captures.

The first four fixes should be treated as correctness work rather than optional optimization; they can affect capability enforcement, draw-resource selection, shader uniform updates, and program/layout compatibility.
