# Modern OpenGL Particle System
## High-Level Feature List and Specification

## 1. Overview

A real-time particle system for a modern OpenGL game engine.

The system should be designed around GPU simulation and GPU-driven rendering, with the CPU responsible primarily for:

- Creating and destroying particle emitters
- Updating emitter transforms and parameters
- Submitting spawn requests
- Managing particle effect assets
- Selecting materials and rendering behaviour

Individual particles should remain on the GPU for their entire lifetime wherever possible.

The system should scale from small local effects to hundreds of thousands of active particles.

---

## 2. Core Design Goals

- GPU-based particle simulation using compute shaders
- Minimal CPU-to-GPU synchronization
- Shared global particle pool
- Support large numbers of independent emitters
- Data-oriented design
- Indirect GPU-driven rendering
- Modular particle behaviour
- Multiple rendering and transparency modes
- Soft integration with scene depth
- Extensible architecture for trails, collisions, mesh particles, and volumetric effects

---

## 3. Particle Pool

Use a global fixed-capacity particle pool stored in GPU buffers.

Each particle should contain, at minimum:

- Position
- Velocity
- Age
- Lifetime
- Colour
- Size
- Rotation
- Angular velocity
- Emitter ID
- Random seed
- Flags or particle type data

Recommended initial implementation:

- Array-of-Structures layout for simplicity
- SSBO storage
- Fixed maximum particle capacity

A Structure-of-Arrays layout may later be added if profiling shows a benefit.

---

## 4. Particle Allocation

The GPU should manage particle allocation.

Required buffers:

- Free particle index list
- Active particle index list A
- Active particle index list B
- Active particle count
- Free particle count

Particle spawning removes entries from the free list.

Dead particles return their indices to the free list.

Simulation should process only active particles rather than iterating across the full particle pool.

The active lists should use double buffering:

1. Read active particles from list A
2. Simulate particles
3. Write surviving particle indices to list B
4. Return dead particle indices to the free list
5. Swap lists A and B

---

## 5. Emitters

Emitters should be lightweight objects controlled by the CPU but represented on the GPU.

Emitter properties should include:

- Transform
- Spawn rate
- Spawn shape
- Initial position range
- Initial velocity range
- Lifetime range
- Size range
- Colour range
- Rotation range
- Material
- Behaviour flags
- Random seed
- User-defined effect parameters

The system should support both:

- Continuous emitters
- One-shot burst emitters

---

## 6. Spawn Shapes

Initial supported emitter shapes:

- Point
- Line
- Box
- Sphere
- Hemisphere
- Disc
- Cone

Later extensions:

- Mesh surface
- Mesh volume
- Spline
- Texture or mask-based spawning

Spawn position and initial velocity should be generated on the GPU.

---

## 7. Spawn Commands

The CPU should submit spawn commands rather than individual particles.

A spawn command should contain:

- Emitter ID
- Number of particles to spawn
- Random seed
- Optional event parameters

The GPU spawn compute shader should:

1. Allocate particle slots
2. Generate spawn positions
3. Generate initial velocities
4. Initialize particle properties
5. Add new particles to the active list

---

## 8. Particle Simulation

Particle simulation should run in a compute shader.

Base simulation features:

- Position integration
- Velocity integration
- Lifetime and age
- Gravity
- Linear drag
- Angular velocity
- Size-over-life
- Colour-over-life
- Alpha-over-life

Optional simulation modules:

- Velocity-over-life
- Acceleration-over-life
- Attractors
- Repulsors
- Turbulence
- Curl noise
- Vortex forces
- Wind
- Buoyancy
- Collision
- Kill volumes

Particle behaviour should be modular rather than implemented through separate particle subclasses.

---

## 9. Curves and Gradients

Effects should support designer-defined curves.

Typical curve-driven properties:

- Size
- Alpha
- Colour
- Velocity multiplier
- Drag
- Rotation speed
- Emissive intensity

Curves should be baked into small lookup textures.

Recommended representation:

- X axis: normalized particle lifetime
- Y axis: curve or gradient index

This avoids evaluating complex splines per particle.

---

## 10. Randomisation

Every particle should contain a deterministic random seed.

The GPU should provide inexpensive random functions for:

- Scalar random values
- Random vectors
- Random directions
- Random ranges
- Shape sampling

Random values should be deterministic given the particle seed.

---

## 11. Particle Rendering

Initial rendering mode:

- Instanced camera-facing quad billboards

Particles should not store four geometry vertices each.

The renderer should use:

- A shared quad
- One instance per visible particle
- Active particle indices to locate particle data

Supported billboard modes:

- Camera-facing
- Screen-aligned
- Cylindrical / vertical
- Axis-locked
- Velocity-aligned
- Velocity-stretched

---

## 12. Particle Materials

Particle materials should support:

- Albedo / colour texture
- Colour tint
- Alpha
- Emissive intensity
- Soft-particle distance
- Blend mode
- Texture animation
- Optional normal map
- Optional distortion output

Recommended blend modes:

- Additive
- Alpha blend
- Premultiplied alpha
- Weighted blended OIT

---

## 13. Texture Animation

Support animated particle textures using:

- Texture atlases
- Flipbooks
- Frame-over-life
- Frame-rate animation
- Random starting frame

Atlas parameters should include:

- Number of columns
- Number of rows
- Frame count
- Animation mode
- Playback rate

---

## 14. Soft Particles

Particles should optionally sample the scene depth buffer.

Near intersections with scene geometry, particle opacity should fade smoothly.

This avoids obvious hard intersections between billboards and geometry.

Required inputs:

- Scene depth texture
- Camera projection information
- Soft-particle fade distance

---

## 15. Transparency

The system should support multiple transparency strategies.

### Additive

Used for:

- Sparks
- Energy
- Fire
- Lasers
- Glows

Does not normally require sorting.

### Alpha-Blended

Used for:

- Smoke
- Dust
- Debris
- Fog-like effects

Optional GPU depth sorting should be supported.

### Weighted Blended OIT

Recommended for large transparent effects where exact ordering is unnecessary.

Useful for:

- Smoke
- Dust
- Steam
- Atmospheric particles

---

## 16. GPU Sorting

GPU sorting should be optional.

Particles requiring conventional alpha blending may generate:

- Depth sort key
- Particle index

A GPU radix sort is recommended for large particle counts.

Sorting should be performed only for particle materials that require it.

---

## 17. GPU Culling

Optional GPU culling should include:

- Camera frustum culling
- Maximum draw distance
- Minimum projected size
- Effect-specific visibility flags

Culling should generate a compact render list.

The CPU should not need to read particle visibility results.

---

## 18. Indirect Rendering

The system should generate draw arguments on the GPU.

Use:

- `glDrawArraysIndirect`
- `glMultiDrawArraysIndirect`

The GPU should write the active or visible particle count directly into indirect draw command buffers.

This avoids GPU-to-CPU readback.

---

## 19. Collision

Initial collision support:

- Plane
- Sphere
- Box
- Capsule

Later collision modes:

- Screen-space depth collision
- Signed distance field collision
- Voxel collision
- Mesh or BVH collision

Collision responses may include:

- Bounce
- Slide
- Stop
- Kill
- Spawn secondary effect

---

## 20. Noise and Turbulence

Support spatial noise fields.

Recommended initial implementation:

- 3D noise texture
- Frequency
- Strength
- Scroll velocity
- Time scale

Later extension:

- Curl noise
- Multi-octave turbulence
- Vector fields

Useful for:

- Smoke
- Steam
- Magic
- Fog
- Atmospheric debris

---

## 21. Secondary Effects

Particles should optionally generate events when:

- Spawned
- Killed
- Colliding
- Reaching a specified age

Events may create:

- Secondary particle bursts
- Decals
- Audio events
- Lights
- Gameplay callbacks

GPU-generated secondary particle events should ideally remain on the GPU.

---

## 22. Trails and Ribbons

Trails should be treated as a separate rendering primitive rather than simulated by spawning very dense particles.

Trail support should include:

- Position history
- Lifetime per trail point
- Width-over-life
- Colour-over-life
- UV generation
- Camera-facing ribbons

Useful for:

- Missiles
- Lasers
- Engine trails
- Sword effects
- Energy weapons

---

## 23. Mesh Particles

Future support should allow particles to render arbitrary meshes.

Mesh particles should support:

- Position
- Rotation
- Scale
- Velocity
- Material
- GPU instancing

Useful for:

- Rocks
- Shell casings
- Debris
- Fragments
- Leaves

---

## 24. Distortion Particles

Particle materials may optionally write to a distortion buffer.

Distortion particles can be used for:

- Heat haze
- Shockwaves
- Energy fields
- Refraction
- Explosions

The distortion buffer should be composited during post-processing.

---

## 25. Particle Lighting

Individual visual particles should not normally become full dynamic lights.

Instead support:

- Emissive particle materials
- Bloom
- Emitter-level proxy lights
- Optional light injection
- Optional volumetric lighting contribution

This allows large particle effects without thousands of dynamic lights.

---

## 26. Required GPU Buffers

Suggested initial buffer layout:

- Particle data SSBO
- Active indices A SSBO
- Active indices B SSBO
- Free indices SSBO
- Emitter data SSBO
- Spawn command SSBO
- Counter SSBO
- Draw command buffer
- Optional sort-key SSBO
- Optional collider SSBO

Frame-global camera and timing information may be supplied through a UBO.

---

## 27. Frame Pipeline

Recommended frame execution order:

1. Update CPU emitter state
2. Upload emitter changes
3. Upload spawn commands
4. Dispatch particle spawn compute shader
5. Dispatch particle simulation compute shader
6. Compact active particle list
7. Optionally perform collision
8. Optionally perform visibility culling
9. Optionally generate sort keys
10. Optionally sort transparent particles
11. Build indirect draw commands
12. Insert required OpenGL memory barriers
13. Render particle passes
14. Composite OIT and distortion buffers

---

## 28. CPU API

The CPU-facing interface should work primarily with effects and emitter handles.

Example operations:

```cpp
ParticleEmitterHandle CreateEmitter(ParticleEffectHandle effect);

void DestroyEmitter(ParticleEmitterHandle emitter);

void SetEmitterTransform(
    ParticleEmitterHandle emitter,
    const Transform& transform);

void SetEmitterParameter(
    ParticleEmitterHandle emitter,
    ParticleParameter parameter,
    float value);

void StartEmitter(ParticleEmitterHandle emitter);

void StopEmitter(ParticleEmitterHandle emitter);

void SpawnEffect(
    ParticleEffectHandle effect,
    const Transform& transform);
```

The CPU API should not expose direct per-particle manipulation as the normal usage path.

---

## 29. Particle Effect Asset

A particle effect asset should describe:

- Spawn configuration
- Simulation modules
- Curves and gradients
- Rendering mode
- Material
- Texture or flipbook
- Blend mode
- Sorting requirement
- Collision configuration
- Maximum particle count
- Optional child effects

Effects should be reusable and independent of individual emitter instances.

---

## 30. Debugging and Profiling

Debug tools should expose:

- Active particle count
- Free particle count
- Spawn count per frame
- Particles killed per frame
- Number of active emitters
- Number of rendered particles
- Number of culled particles
- Simulation GPU time
- Sorting GPU time
- Rendering GPU time
- Particle buffer capacity usage

Useful debug rendering:

- Emitter bounds
- Spawn shapes
- Collider shapes
- Particle velocity
- Particle IDs
- Particle lifetime
- Vector fields

---

## 31. Initial Implementation Scope

The first production-ready version should include:

- Global GPU particle pool
- GPU free list
- Double-buffered active index lists
- Compute shader spawning
- Compute shader simulation
- Point emitter
- Sphere emitter
- Box emitter
- Cone emitter
- Gravity
- Drag
- Basic noise
- Size-over-life
- Colour-over-life
- Instanced billboard rendering
- Additive blending
- Alpha blending
- Soft particles
- Texture atlases
- GPU-generated indirect draw commands
- Basic debugging statistics

---

## 32. Recommended Later Features

After the base system is stable, add approximately in this order:

1. Velocity-stretched billboards
2. Weighted blended OIT
3. GPU frustum culling
4. GPU radix sorting
5. Screen-space collision
6. Analytical collider support
7. Curl noise
8. Trails and ribbons
9. Mesh particles
10. Distortion particles
11. Secondary GPU effects
12. Signed-distance-field collision
13. Volumetric particle injection

---

## 33. Performance Targets

The implementation should aim for:

- No CPU processing per particle
- No required per-frame GPU readback
- O(active particles) simulation cost
- Batched emitter updates
- Batched particle rendering
- Minimal draw calls
- Minimal buffer rebinding
- Optional simulation LOD
- Graceful behaviour when the particle pool is exhausted

A reasonable initial capacity is:

```text
262,144 to 1,048,576 particles
```

depending on target hardware and particle data size.

---

## 34. Architectural Principle

The central design rule is:

> The CPU manages particle effects and emitters; the GPU manages particles.

Individual particle allocation, spawning, simulation, death, compaction, visibility, and rendering should remain GPU-driven wherever practical.

This keeps the system scalable while retaining a simple high-level API for game code.
