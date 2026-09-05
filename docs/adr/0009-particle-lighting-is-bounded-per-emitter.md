---
status: accepted
---

# Particle lighting is bounded per emitter

Particle lighting creates at most one Particle proxy light and one Particle
volumetric contribution per live Emitter, never one dynamic light per Particle.
Opted-in proxy lights fill only unused slots in the renderer's fixed PBR light
array, preserving authored scene lights; excess proxies are omitted
deterministically by emitter index rather than expanding the light budget.

The volumetric contribution is a pure, repeatable render-graph draw through
`MPP.ParticleVolumetricLighting`. It integrates a depth-clipped spherical proxy
along the camera ray and adds the result to HDR colour and the optional emissive
attachment. A 3D froxel texture was rejected because the engine has no general
volumetric-fog volume to own, composite, or expose through its 2D/cubemap render
graph; introducing that unrelated renderer would make particle lighting define
the engine's future volumetric architecture. Per-particle lights were rejected
because their count scales with the GPU particle pool and would violate the
bounded-light constraint.
