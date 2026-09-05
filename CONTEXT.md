# Massive Poly Pusher

An OpenGL 3D graphics engine with render-graph-driven PBR and legacy forward
pipelines, authored resources, and a DemoSuite application that exercises them.

## Language

### Particles

**Particle effect**:
An authored, reusable asset grouping one or more emitter templates. Independent
of any instance of itself.
_Avoid_: Effect (unqualified — see Post effect), particle system, emitter group

**Emitter template**:
The authored unit inside a particle effect: one spawn shape, one behaviour
module set, one appearance, one blend class, one particle budget, its curves,
and a transform relative to its particle effect.
_Avoid_: Emitter definition, emitter config

**Emitter**:
A live instance of one emitter template, addressed by a generational handle.
Owns a transform and runtime parameter multipliers; the CPU controls it, the GPU
represents it.
_Avoid_: Emitter instance, spawner

**Particle**:
A single simulated element, resident on the GPU for its whole lifetime. Never
individually addressable from the CPU.

**Particle event**:
A GPU-generated notification from a Particle being spawned, dying, first
colliding, or crossing an authored age. It selects one typed action and carries
an authored payload.
_Avoid_: Effect event, particle callback (only one possible consumer)

**Secondary particle burst**:
Particles created at a Particle event position from another emitter template in
the same live particle effect. Generated and consumed on the GPU; not a child
particle effect.
_Avoid_: Secondary effect, child effect, event emitter

**Spawn command**:
A CPU-submitted request for an emitter to create a number of particles. The unit
of CPU→GPU spawn communication; the CPU never creates particles directly.
_Avoid_: Spawn request, emit command

**Behaviour module**:
One optional, independently toggled element of particle simulation — gravity,
drag, noise. Composed by selection, never by subclassing.
_Avoid_: Simulation module, particle behaviour, affector

**Particle appearance**:
The render-side description of an emitter template — texture, tint, emissive
intensity, atlas animation, billboard mode, blend class. Deliberately not a
Material: it owns no Program and is not a Resource.
_Avoid_: Particle material

**Particle proxy light**:
A single point-light representation owned by an Emitter, independent of how many
Particles that Emitter contains.
_Avoid_: Particle light, per-particle light

**Particle volumetric contribution**:
Depth-aware inscattered radiance from one Emitter-sized proxy volume, separate
from both its Particle appearance and its Particle proxy light.
_Avoid_: Volumetric particle (the contribution is emitter-level)

**Blend class**:
The transparency strategy a particle appearance composites with — additive,
alpha, or weighted blended OIT. Determines which render pass draws it.
_Avoid_: Blend mode (that is the engine-wide `BlendMode` enum)

**Mesh particle**:
A Particle rendered as an instance of real mesh geometry with an ordinary
Material. Shares the particle simulation and compacted ranges, but is drawn by
the dedicated mesh-particle pass rather than any billboard appearance pass.

**Trail**:
A live visual primitive that records the path of a moving source over time.
Independent of particles and individually controlled by the CPU.
_Avoid_: Dense particle stream, trail emitter

**Trail point**:
One time-limited position sample in a trail's history. Not a Particle.
_Avoid_: Trail particle

**Ribbon**:
The continuous camera-facing strip reconstructed through a trail's live points.
_Avoid_: Trail (the history and its live control, rather than its rendered form)

### Rendering

**Post effect**:
An image-space effect applied to a completed scene target. The unqualified word
"effect" always means this, never a particle effect.
