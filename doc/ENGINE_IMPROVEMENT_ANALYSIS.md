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

### 3. Make `Program` reflection and loading transactional — high severity, low/medium effort

Several uniform-location members are not initialized by the constructor:

- Declaration: `mpp/include/mpp/Program.h:85-88`
- Constructor initializes only shader and sort IDs: `mpp/src/Program.cpp:30-35`

If a standard uniform is optimized out, its getter can return an indeterminate value. In addition, `unloadImpl()` clears `mUniformTypes` but not `mUniformIds` at `Program.cpp:584-590`, leaving stale locations after reload.

Program creation also owns several raw GL names during a multi-step operation. A shader compile, link, reflection, or labeling exception can leave partially built state.

**Change**

- Initialize every location to `-1`.
- Clear all reflection maps and reset locations before load and during unload.
- Build shaders/program in local RAII GL handles and publish `setId()` only after successful linking/reflection.
- Use `std::vector<char>` for info logs rather than manual arrays.
- Cache fragment-output capability during reflection rather than querying it later.

---

### 4. Replace the lossy mesh “hash” used for program selection — high severity, medium effort

`MeshSpecification::getHashCode()` explicitly omits data types and most layout details:

- `mpp-mesh/src/MeshSpecification.cpp:360-469`

It collapses repeated semantics and treats layouts such as `vec2` and `ivec2` identically. `getDescriptor()` derives generated resource names from it at `MeshSpecification.cpp:305-349`, and those descriptors are used by program creation in `ResourceManager.cpp:604` and `:680`.

This can reuse or name a program for an incompatible vertex layout.

**Change**

- Define a canonical layout key containing primitive/storage/indexed state and every ordered attribute field.
- Hash that key with a normal hash-combine routine.
- Use the canonical key, or a collision-checked digest, for caches.
- Keep the current compact descriptor only as a human-readable suffix.
- Add tests proving differing data types, normalization, offsets, layout grouping, and user attributes produce distinct keys.

---

### 5. Cache render-graph compilation and framebuffer objects — high payoff, medium effort

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

#### Particle system

`ParticleSystem::create()` only allocates an array and immediately loses it:

- `mpp/src/ParticleSystem.cpp:25-28`

No GPU buffers, transform feedback objects, update path, draw path, destruction, or repeated-create protection exist, despite public members implying ping-pong transform feedback.

Either:

1. Implement transform-feedback ping-pong with RAII buffers, spawn/update/render counts, bounds checks, and cleanup; or
2. Mark the API experimental and remove it from the normal public surface until usable.

A compute-shader backend could later fit naturally into an extended render graph.

#### Clip rectangle

`ClipRectangle::intersect()` always throws:

- `mpp/src/ClipRectangle.cpp:39-42`

Implement normalized half-open rectangle intersection, define behavior for negative dimensions, and test disjoint/touching/contained/overflow-adjacent cases.

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
3. Initialize/reset `Program` reflection state.
4. Replace the mesh layout key and add collision tests.
5. Implement `ClipRectangle::intersect()` and remove the particle leak.

### Performance pass

6. Cache compiled graph/allocation plans.
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
