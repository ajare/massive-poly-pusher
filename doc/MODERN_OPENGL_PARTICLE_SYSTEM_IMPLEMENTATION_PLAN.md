# Modern OpenGL Particle System Implementation Plan

## Goal

Implement the GPU-driven particle system described in
`doc/modern_opengl_particle_system_spec.md`, scoped to that document's §31
initial implementation scope, working identically under the graph-driven PBR and
legacy forward pipelines, with a dedicated DemoSuite demo.

The central rule from spec §34 holds throughout: **the CPU manages particle
effects and emitters; the GPU manages particles.** No per-particle CPU work and
no required per-frame readback.

Terminology follows `CONTEXT.md`. "Particle system" is not a domain term — the
authored asset is a **particle effect**, its authored parts are **emitter
templates**, their live instances are **emitters**. `ParticleSystem` is only the
name of the owning class.

Architectural decisions are recorded in `docs/adr/0005-particles-simulate-once-per-frame-outside-the-graph-pass.md`
and `docs/adr/0006-particle-simulation-branches-at-runtime-while-particle-draws-are-permuted.md`.

## Scope

Spec §31, plus two deliberate additions reasoned through during design:

- **All seven §6 spawn shapes** (point, line, box, sphere, hemisphere, disc,
  cone), not §31's four. Each is one case in a spawn-shader switch and one
  schema enum value; none affects any architectural decision.
- **Five of §11's six billboard modes** (camera-facing, screen-aligned,
  cylindrical, axis-locked, velocity-aligned). These differ only in how the
  vertex shader builds the quad basis. Velocity-stretched is excluded because it
  makes quad *expansion* mode-dependent, which is a structural difference rather
  than an enum case — and §32 independently ranks it as the first follow-up.

Everything else in §32 is out of scope and filed as follow-up issues.

**Known limitation to carry into review:** alpha-blended particles are unsorted
in this version, because GPU sorting is §32. This is visible as inter-particle
ordering artefacts in dense smoke. It is a deliberate trade, not a defect.

## 1. Capability probing

Extend `Caps` with the limits the system must validate against rather than
assume:

```cpp
bool supportsCompute{ false };
uint32_t maxComputeWorkGroupCount[3]{};
uint32_t maxComputeWorkGroupSize[3]{};
uint32_t maxComputeWorkGroupInvocations{ 0 };
uint32_t maxShaderStorageBlockSize{ 0 };
uint32_t maxShaderStorageBufferBindings{ 0 };
```

Populate them alongside the existing queries in `RenderSystem`. The context is
requested at 4.4 core (`WindowSDL.cpp`), so compute is normally present, but the
request is a request: SDL may return a lower context and a driver may still fail
to compile a valid kernel.

Unavailable compute is **not** fatal. The particle system logs one warning and
renders nothing, following the existing incomplete-PBR-environment precedent in
`RenderPipeline` rather than the throwing SSAA precedent in `RenderSystem`. There
is no CPU fallback simulation.

**Status:** done. `Caps` reports `supportsCompute` and the compute/SSBO limits,
queried in `RenderSystem::checkCaps` and logged with the rest. A build defining
`MPP_FORCE_NO_COMPUTE_SUPPORT` takes the unsupported path: one warning, no
particles, no throw.

**Acceptance:** a build forced to report no compute support runs DemoSuite
normally, logs exactly one warning, and draws no particles.

## 2. Shader program plumbing

`Program` compiles vertex and fragment stages only and is bound to
`mesh::MeshSpecification`, `@Token` markup substitution and generated
`_mpp_vs_in_*` attribute plumbing. None of that applies to particles, which have
no mesh and no vertex attributes.

Add a minimal raw-GLSL `Resource` base with two concrete types:

| Type | Purpose |
|---|---|
| `ComputeProgram` | Raw GLSL compute source; `use()` and `dispatch(x,y,z)`. |
| `ParticleDrawProgram` | Raw GLSL vertex + fragment source, no attributes. |

Both support `#define` injection so a program can be specialised. Both are
`Resource` types so they participate in `ResourceManager` lifetime, naming and
create/load/unload ordering.

Per ADR 0006, `#define` specialisation is used for the **draw** program only.
The simulation kernel is a single program that branches at runtime.

**Status:** done. `RawShaderProgram` is the raw-GLSL `Resource` base, with
`ComputeProgram` and `ParticleDrawProgram` above it and `RawShaderStream`
beneath. `#define` injection lands immediately after `#version`; the kernel
currently uses it only for its work group size, the draw for its blend class.

**Acceptance:** a compute program and an attribute-less draw program both load,
reload and unload through `ResourceManager` with no `MeshSpecification`.

## 3. Buffers

The engine has no SSBO support. `detail::PersistentMappedBuffer` is already
target-agnostic — `create(uint32_t target, ...)` — with triple-buffered
persistent mapping, fence-guarded rotation and a `glBufferSubData` fallback, and
is reused unchanged at `GL_SHADER_STORAGE_BUFFER` for the CPU-written buffers.

Add a plain `ShaderStorageBuffer` for GPU-only buffers: allocate once, bind,
never map, no CPU shadow copy, no triple buffering. GPU-only buffers are
synchronised with memory barriers; CPU-written buffers are synchronised by
segment rotation. Keeping the two types distinct keeps that difference visible
at the call site.

| Buffer | Type | Written by |
|---|---|---|
| Particle pool | `ShaderStorageBuffer` | GPU |
| Free index list | `ShaderStorageBuffer` | GPU |
| Active index list A / B | `ShaderStorageBuffer` | GPU |
| Render index list | `ShaderStorageBuffer` | GPU |
| Counters | `ShaderStorageBuffer` | GPU |
| Indirect draw commands | `ShaderStorageBuffer` | GPU |
| `EmitterSimData` | `PersistentMappedBuffer` | CPU |
| `TemplateRenderData` | `PersistentMappedBuffer` | CPU |
| Spawn commands | `PersistentMappedBuffer` | CPU |

Pool capacity comes from `RenderSystemOptions` (the `[mpp]` INI section),
defaulting to 262,144 particles, and is allocated lazily on first emitter
creation so applications that never use particles pay nothing. Per spec §33 the
supported range is 262,144–1,048,576.

**Status:** particle allocation buffers done. The pool, free list, double-buffered
active lists, counters and CPU-written emitter/template/spawn buffers allocate
lazily at the configured capacity. The render index list remains part of the
compaction milestone (§10).

**Acceptance:** buffers allocate lazily, respect the configured capacity, and
`PersistentMappedBuffer` requires no modification.

## 4. Data layout

### Particle record — 64 bytes, `std430`, array-of-structures

| Field | Type | Bytes |
|---|---|---|
| position + age | `vec3` + `float` | 16 |
| velocity + lifetime | `vec3` + `float` | 16 |
| colour tint | packed RGBA8 `uint` | 4 |
| base size | `float` | 4 |
| rotation | `float` | 4 |
| angular velocity | `float` | 4 |
| emitter index | `uint` | 4 |
| seed | `uint` | 4 |
| flags | `uint` | 4 |
| padding | — | 4 |

262,144 particles is 16 MB; §33's 1,048,576 ceiling is 64 MB.

Colour is stored as an 8-bit-per-channel spawn-time tint because it is
multiplied by an `RGBA16F` gradient from the curve LUT, which carries the HDR
range. Position and velocity stay full precision: half-precision position
quantises visibly once an effect is more than a few hundred units from the
origin.

`emitter index` is load-bearing — compaction (§8 below) counts on it.

### Emitter records — split by reader

The simulation kernel dereferences an emitter record *per particle per frame* but
needs only behaviour data. The draw needs only appearance data. They are
therefore two structures in two buffers:

**`EmitterSimData`** (~128 B) — world transform, spawn shape enum and shape
parameters, initial velocity range, lifetime range, base size range, rotation and
angular-velocity ranges, colour tint range, random seed, behaviour-module
bitmask, module parameter block (gravity vector, drag coefficient, noise
frequency/strength/scroll), particle budget, live count, emission mode
(continuous or burst), spawn rate or burst count, enabled flag, and the six
runtime parameter multipliers.

**`TemplateRenderData`** (~64 B) — albedo texture handle, colour tint, alpha
multiplier, emissive intensity, soft-particle fade distance, atlas columns, rows,
frame count, animation mode and rate, curve LUT row offset, billboard mode, and
blend class.

**Status:** done. The 64-byte `ParticleRecord`, split `EmitterSimData` and
64-byte `TemplateRenderData` have matching C++ and GLSL declarations. The GPU
suite introspects the linked spawn kernel's `GL_TOP_LEVEL_ARRAY_STRIDE`.

**Acceptance:** the particle record is exactly 64 bytes under `std430`, verified
by a test rather than by inspection.

## 5. Particle effect asset

Follow the existing specification/stream/programmatic-stream pattern used by
`PbrMaterialSpecification`:

- `ParticleEffectSpecification` — a plain struct, the runtime's input, in the
  core library.
- `*.particle.yaml` parser in `MppResourceParsers`, alongside every other
  authored format, so it is unit-testable with no GL context.
- A programmatic stream so the demo and tests can build effects in C++.

A particle effect groups N emitter templates. Each template carries its own spawn
configuration, behaviour modules, curves, appearance, blend class and particle
budget, plus a transform **relative to the effect**.

Behaviour modules are authored as **named optional blocks** with a fixed,
engine-defined evaluation order — not an ordered list. Most modules are force
contributors that sum commutatively, but the operations where order matters
(drag applied to velocity, integration, ageing, curve evaluation) have exactly
one correct sequence. A fixed pipeline with toggles encodes that once; an
authored order lets an artist express an ordering that is simply wrong, with no
diagnostic.

Effect-level maximum particle counts are validated at authoring time as the sum
of template budgets. The **enforced** budget is per template, because a template
is one appearance, one blend class and one draw, which is what a single spawn
dispatch can clamp.

**Status:** not started.

**Acceptance:** an effect round-trips through the parser; malformed documents
produce diagnostics rather than throwing; parser tests run without a GL context.

## 6. Curves and gradients

Per spec §9, curves bake into a lookup texture at effect load: X is normalized
particle lifetime, Y is the curve index.

One `RGBA16F` LUT per **particle effect asset**, with rows partitioned across its
emitter templates at bake time. Because every template in an asset is known at
load, rows are assigned during the bake with no runtime allocator. Each
template's `TemplateRenderData` carries its row offset.

`RGBA16F` rather than `RGBA8` is deliberate: 8-bit clamps every curve to [0,1],
which silently breaks emissive intensity above 1 (so bloom has nothing to pick
up) and size multipliers above unity, and pre-clamps colour gradients to LDR in
the HDR PBR pipeline.

Scalar curves pack four per row across RGBA; gradients take a row's RGB. At
256×64 an effect's LUT is 128 KB.

§31 needs size-over-life and colour-over-life. §9's full set (alpha, velocity
multiplier, drag, rotation speed, emissive intensity) is authoring, not code.

**Status:** not started.

**Acceptance:** an emissive curve authored above 1.0 survives the bake and drives
bloom.

## 7. CPU API

A `ParticleSystem` class owned by `RenderSystem` and reached through it, rather
than inlined onto `RenderSystem` — which is already 5,191 lines — following the
`IblEnvironmentCache` extraction precedent rather than the shadow-domain inlining
one.

Emitters live in one flat table addressed by **generational handles** (index plus
generation word). Gameplay code creates and destroys emitters constantly, and a
bare index silently retargets a stale handle onto whatever reused the slot.

```cpp
ParticleEffectHandle   createEffect(ResourcePtr asset, Transform const&);
void                   destroyEffect(ParticleEffectHandle);
void                   setEffectTransform(ParticleEffectHandle, Transform const&);
void                   spawnEffect(ResourcePtr asset, Transform const&);

ParticleEmitterHandle  getEmitter(ParticleEffectHandle, size_t index);
void                   setEmitterTransform(ParticleEmitterHandle, Transform const&);
void                   setEmitterParameter(ParticleEmitterHandle, ParticleParameter, float);
void                   startEmitter(ParticleEmitterHandle);
void                   stopEmitter(ParticleEmitterHandle);
```

`createEffect` instantiates every emitter template in the asset and returns a
handle owning a span of ordinary emitter handles. Effect-level calls fan out over
that span, composing parent×local transforms. **Grouping is entirely CPU-side —
the GPU sees a flat emitter table and never learns effects exist**, which is what
keeps the flat upload, the flat simulation dispatch and the single-dispatch
budget clamp intact.

`ParticleParameter` is a closed enum of multipliers on authored values, so `1.0`
means "as authored" and authoring and runtime control compose rather than fight:
`SpawnRate`, `SizeScale`, `SpeedScale`, `LifetimeScale`, `AlphaScale`,
`EmissiveScale`. Spec §5's "user-defined effect parameters" are deliberately
omitted: with a closed engine-authored module set they would be written by
gameplay, uploaded every frame and read by nothing.

`startEmitter`/`stopEmitter` toggle the enabled flag rather than setting
`SpawnRate` to zero, so stopping preserves a caller's multiplier.

### Lifetime

One-shot emitters retire when spawning is complete **and** the template's
authored maximum lifetime has elapsed since the last spawn. That bound is known
on the CPU at spawn time and no particle from the emitter can outlive it, so
fire-and-forget `spawnEffect` needs no GPU readback. Retiring at spawn completion
instead would free an emitter record while its particles still read it.

Continuous emitters never auto-retire. An effect instance retires only when all
its emitters have, so an explosion with a long smoke plume and a short spark
burst lives as long as the plume.

**Status:** not started.

**Acceptance:** a destroyed emitter's stale handle is inert; `spawnEffect` leaks
no slots across ten thousand bursts.

## 8. Frame pipeline

Per ADR 0005, simulation runs **once per rendered frame**, dispatched by
`RenderSystem` before graph execution — never inside the graph pass, which may
execute several times per frame (`MPP.PlanarReflectionScene` already renders the
scene from a mirrored camera, and a point-shadow domain expands into six face
passes).

Per frame, in order:

1. Retire completed one-shot emitters; compose effect transforms.
2. Accumulate fractional spawn counts and build spawn commands.
3. Upload `EmitterSimData`, `TemplateRenderData` and spawn commands.
4. Dispatch the spawn kernel — allocate from the free list, generate positions
   and velocities from the shape, initialise properties, append to active list A.
5. Dispatch the simulation kernel over active list A — integrate, age, apply
   modules, evaluate curves, write survivors to active list B, return dead
   indices to the free list.
6. Dispatch compaction — count per template, prefix-sum into offsets, scatter
   into the render index list.
7. Write indirect draw commands.
8. Insert memory barriers.
9. Swap active lists A and B.

`dt` is wall time since the last dispatch, **clamped to ~100 ms**. The clamp is
required, not cosmetic: without it a shader recompile, a dragged window or a
debugger breakpoint hands the simulation a multi-second step, and every particle
teleports along its velocity vector and dies at once — the effect visibly
detonates and vanishes.

Spawn accumulation happens here, in the once-per-frame prepare step, and **not**
in `Scene::update`. DemoSuite's loop runs `update` on a fixed-timestep
accumulator zero to N times per rendered frame; accumulating there would drift
out of step with the `dt` the simulation actually used. Per-emitter remainders
carry across frames so low-rate emitters stay smooth instead of quantising to one
particle per frame.

**Status:** once-per-rendered-frame scheduling, clamped wall-time delta,
fractional continuous-spawn accumulation, CPU-buffer upload, spawn, simulation,
barriers and active-list swapping are done. Emitter retirement/transform
composition belongs to the CPU API milestone (§7); render-list compaction and
indirect command generation remain in §10.

**Acceptance:** particle speed is unchanged when planar reflections are enabled;
a 3-second stall does not destroy live effects.

## 9. Simulation

One compute kernel over the global active list, branching on each emitter's
behaviour-module bitmask (ADR 0006). §31's modules are gravity, linear drag and
basic 3D-texture noise, alongside the base features: position and velocity
integration, age and lifetime, angular velocity, and size/colour/alpha over life
from the LUT.

Module parameters live in `EmitterSimData` as a fixed struct with a slot per
module. A variable-length parameter blob would add indirection in the hottest
kernel for flexibility this scope cannot use.

Per spec §10, every particle carries a deterministic seed, derived by hashing
(emitter seed, particle slot, spawn counter) so no stored counter is needed. The
kernels provide inexpensive scalar, vector, direction, range and shape-sampling
random functions from it.

Pool exhaustion is graceful per §33: the spawn kernel clamps to what the free
list can supply and to the emitter's template budget, incrementing a dropped-spawn
counter rather than failing.

**Status:** spawn and simulation done. The spawn kernel samples all seven
shapes, derives deterministic per-particle seeds, and reserves both emitter
budget and free-list entries with atomic CAS loops. The single simulation kernel
integrates age, position, velocity and rotation, branches at runtime for gravity,
linear drag and scrolling 3D-texture noise, compacts survivors into the opposite
active list and returns dead slots to the free list. Both kernels dispatch over
GPU-owned work rather than pool capacity. Curve-LUT baking and sampling remains
the independently scheduled §6 milestone (#19).

**Acceptance:** determinism holds for a fixed seed and fixed frame sequence;
over-spawning clamps and reports rather than corrupting the free list.

## 10. Compaction and indirect drawing

Simulation is flat but drawing cannot be: each emitter template has its own
appearance, texture, atlas, LUT rows and blend class. Compaction turns one
interleaved active list into contiguous per-template runs:

1. Count surviving particles per template.
2. Prefix-sum the counts into per-template offsets.
3. Scatter survivors into a render index list at those offsets.
4. Write each template's count directly into its indirect command's
   `instanceCount`.

No readback is required at any step (§18). Templates are ordered by blend class
when uploaded, so each blend class occupies a contiguous span of commands and a
render pass is **one** `glMultiDrawArraysIndirect` regardless of effect count —
spec §33's "minimal draw calls" with no per-frame CPU work.

The per-template counters produced here also serve the budget clamp (§9) and the
statistics (§13), rather than three separate mechanisms.

**Status:** not started.

**Acceptance:** N templates render in one multi-draw per blend class; counts
match a CPU-side reference in the GPU test.

## 11. Rendering

`MPP.ParticleScene` is registered as a `RenderGraphScenePass` factory with
authoring metadata in `RenderGraphBuiltInPasses`, alongside `MPP.WaterScene` and
`MPP.PlanarReflectionScene`. It is a **pure draw pass**: it dispatches no
compute and may appear any number of times in a graph.

Billboards are attribute-less instanced quads — the quad is generated from
`gl_VertexID`, one instance per particle, particle data fetched from the pool via
the render index list, count supplied by the indirect command. There is no vertex
buffer and no `Mesh`.

Supported billboard modes: camera-facing, screen-aligned, cylindrical,
axis-locked, velocity-aligned. All five differ only in basis construction and
share one quad expansion.

Texture animation (§13) supports frame-over-life, fixed-rate playback and random
start frame, the last combinable with either and derived from the particle's
existing seed. The atlas belongs to the template; the frame within it is per
particle.

Particles are emissive-and-bloom, never lit (§25) — which is precisely why one
appearance works unchanged in both the PBR and legacy graphs.

### Blend classes

One authored pass instance **per blend class**, each with its own graph-authored
`GraphRasterState`, selected by a `BLEND_MODE` pass parameter. Premultiplied
alpha, and later weighted-blended OIT, then become new authored passes rather
than new branches in a widening C++ switch, and pass ordering relative to water
or distortion becomes an authoring decision rather than a hardcoded one.

Depth state is **not** the author's choice: the pass forces depth writes off,
because transparent geometry must not write depth and because the soft-particle
depth read depends on it. Graph validation warns on a particle pass authored with
depth writes enabled rather than silently honouring it.

### Soft particles

Scene depth arrives through an ordinary named, **optional** graph input with a
declared fallback, read by name as SSAO and GTAO already read `"DEPTH"`. The pass
samples the **live** depth image, not a copy: particles never write depth, so
this is the specified read-only case rather than a feedback hazard.

Where a driver mishandles that case, inserting a depth-copy pass (mirroring
`SceneColourCopyPass`) and rewiring the input is a render graph authoring change
with no C++ change, because the pass only knows that some depth texture is bound
to that input. Omitting the input entirely yields hard-edged particles rather
than a failed frame, which is what lets a graph without a depth image still run
particles.

**Status:** partially done. `MPP.ParticleScene` is registered with authoring
metadata and draws attribute-less instanced quads through
`glDrawArraysIndirect`, in both the generated graph and an authored template.
Billboard modes, appearances, flipbooks, soft particles and per-blend-class
passes are still to come.

**Acceptance:** identical particle output under `XmlGraphPbrForward` and
`GraphLegacyForward`; particles unaffected by the presence of a reflection pass.

### Unsupported pipeline modes

`RenderPipelineMode::PbrForward` and `LegacyForward` execute a manual pass
sequence, never the graph, and are **not supported**. They have no mechanism for
declaring the depth input, and they are the pre-graph path DemoSuite no longer
uses. This is a documented limitation, not an oversight.

## 12. Statistics and debugging

Spec §30 wants active count, free count, spawns and kills per frame, active
emitters, rendered and culled counts, buffer capacity usage, and simulation, sort
and render GPU times. Almost all live in GPU counter buffers, and §33 forbids
required per-frame readback.

Resolution: **asynchronous, frame-lagged, default-off**. Counters are copied into
a small ring of readback buffers guarded by fences, so the CPU reads only results
that are already two or three frames old and never waits. GPU times use timer
queries through the same ring. With statistics disabled the shipping path
performs no readback at all.

These land in a distinct `ParticleStats` struct, **not** in `RenderInfo`.
`RenderInfo`'s contract is "counters for the frame that just ended, cleared each
frame"; merging frame-lagged GPU counters into it would produce a struct where
some fields describe this frame and others describe a frame from three
milliseconds ago, with nothing marking which is which.

`Profiler` is the wrong home — it is a Windows performance-counter stub behind
`MPP_PROFILE_BUILD`, with no GL timer-query support.

**Status:** not started.

**Acceptance:** with statistics off, no `glMapBuffer`, `glGetBufferSubData` or
query retrieval occurs on the particle path.

## 13. Tests

Following the repo's framework-free convention — `bool runXxxTests(std::string*
failure)` compiled into the libraries and invoked behind a CLI flag.

- `runParticleResourceTests` in `MppResourceParsers`, added to PipelineEditor's
  existing suite next to `runRenderGraphResourceTests`. No GL context required.
- `runParticleGpuTests(RenderSystem*)` in the core library, invoked by a new
  DemoSuite `--particle-tests` flag alongside the existing
  `--package-smoke-test`.

GPU assertions, all of which depend on §12's readback and are the reason to build
it even though it ships disabled:

- Spawn N particles; active count is N.
- Age them out; free count returns to capacity.
- Over-spawn; the clamp fires and the dropped counter increments.
- Run two frames; the double-buffered active lists actually swapped.
- The particle record is exactly 64 bytes under `std430`.

`runRenderGraphGpuTests` and `runDiagnosticTests` currently have **no callers
anywhere in the repo** — they are compiled and never run. Wire
`runRenderGraphGpuTests` into the same `--particle-tests` flag; it is a two-line
change that revives existing coverage.

**Status:** not started.

**Acceptance:** `--particle-tests` runs both new suites plus the revived graph
GPU suite and returns a non-zero exit code on failure.

## 14. DemoSuite demo

A standalone `ParticleScene : ::Scene` selected by a new `--particles` flag,
sitting alongside the package path rather than inside it. DemoSuite currently
requires `--package` and hard-casts `gScenes[0]` to `PackageScene*` for
`present`; both need guarding.

`PackageScene` is driven by a `SceneDocument` from a package plus two pipeline
runtimes, so adding particles there would route effect assets, emitters and graph
passes through the package manifest and both pipeline runtimes before anything
rendered at all. Package integration is a follow-up, not a prerequisite.

The demo ships two render graph YAMLs, PBR and legacy, and **toggles between them
at runtime** on a keypress — a restart-based toggle makes "works in both
pipelines" a claim nobody re-checks.

Contents:

- Procedural geometry only (`GridModelStream`, `BoxModelStream`,
  `SphereModelStream`) — no imported assets, so a failed model load can never be
  mistaken for a particle failure.
- Emitters positioned to **force** billboard/surface intersection at shallow
  angles, which is the only place a soft-particle fade is visible. Emitters
  floating clear of geometry demonstrate nothing.
- Several spawn shapes, both blend classes, and a flipbook using the already
  deployed `resources/demo-suite/res/atlas.png`.
- The `ParticleStats` overlay.
- A **stress mode** on a keypress that burst-fills the pool to capacity. This is
  the only interactive way to observe whether the §33 performance target holds at
  capacity, whether exhaustion degrades gracefully, and whether per-template
  budgets stop one emitter starving the others — the three behaviours most likely
  to be quietly wrong, and the interactive counterpart to the GPU tests.

**Status:** the vertical slice has landed. `ComputeProgram`,
`ShaderStorageBuffer`, `Caps` probing, a trivial kernel and an attribute-less
indirect draw render end-to-end through `MPP.ParticleScene` under
`GraphLegacyForward` and `XmlGraphPbrForward`, covered by a
`runRenderGraphGpuTests` stage. The `--particle-tests` harness is its own
milestone.

**Acceptance:** particles are visually identical across the runtime pipeline
toggle; stress mode reaches capacity without a crash or a frame-time cliff.

## 15. Delivery order

Land a **thin vertical slice first**: `ComputeProgram` + `ShaderStorageBuffer` +
`Caps` probing + a trivial kernel + an attribute-less indirect draw of untextured
quads, rendering end-to-end through `MPP.ParticleScene` in both graphs, with ADR
0005 and the `--particle-tests` harness already wired.

The slice exists to answer the questions that could invalidate the architecture,
while they are still cheap to answer: does a compute dispatch survive the engine's
`GL_CHECK` error handling and debug context; does an indirect draw work inside a
graph pass; does the once-per-frame/many-passes split hold when the reflection
pass runs; does read-only depth sampling behave on the target driver.

Building in spec order instead would mean the first observable output arrives only
after the pool, spawn, simulation and rendering all exist — so a driver-level
surprise surfaces with no way to bisect which layer caused it. GPU work is
unusually hostile to blind building because the failure mode is a blank screen,
not a stack trace.

Subsequent milestones fill in spawn shapes, behaviour modules, curves,
appearances, flipbooks, soft particles, the asset format, statistics and the
demo, each adding assertions to a harness that already exists.

## 16. Validation

- Particle output is identical under `XmlGraphPbrForward` and
  `GraphLegacyForward`.
- Enabling planar reflections does not change particle speed.
- A build reporting no compute support runs, warns once, and draws nothing.
- With statistics disabled there is no readback on the particle path.
- Pool exhaustion clamps and reports; the free list stays consistent.
- `--particle-tests` passes, including the revived graph GPU suite.

## 17. Follow-up

Filed as separate issues, approximately in spec §32's recommended order:
velocity-stretched billboards, weighted blended OIT, GPU frustum culling, GPU
radix sorting, particle collision (screen-space, analytical, then SDF), curl
noise and multi-octave turbulence, trails and ribbons, mesh particles, distortion
particles, secondary GPU effects, and volumetric injection.

Also deferred by design decisions recorded above: child effects (spec §29),
user-defined emitter parameters (spec §5), particle effects in the package and
scene-document pipeline, and a shared albedo atlas to remove the remaining
per-template texture bind.
