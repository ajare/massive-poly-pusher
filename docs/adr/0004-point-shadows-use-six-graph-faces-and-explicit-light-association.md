---
status: accepted
---

# Point shadows use six graph faces and explicit light association

A point-shadow domain owns one shared `Depth24` comparison cubemap. One logical
point-shadow update expands into six ordered scene passes named `+X`, `-X`,
`+Y`, `-Y`, `+Z`, and `-Z`; each pass writes the corresponding graph attachment
face and all six versions retain the same imported backing resource. The face
views use OpenGL's canonical cubemap directions and up vectors, a 90-degree
perspective projection, and radial depth normalized by the point light's finite
range. Opaque point-shadow casting is two-sided so authored triangle winding
does not create holes in an omnidirectional shadow.

The shadow contract carries a light type and receiving light-array index as
well as directional parameters or point position/range. Receivers select the
2D or cubemap comparison sampler from that type and multiply visibility into
only the indexed direct-light term. Ambient, emissive, image-based lighting,
and every other direct light remain outside the shadow visibility factor.

Directional domains retain their existing single orthographic graph pass,
2D depth texture, front-face caster culling, and default association with light
index zero. A pipeline with no shadow domain retains the disabled uniform frame,
binds no shadow resource, and performs no shadow allocation.

Invalid authored options remain errors. A valid point domain that exceeds the
GPU cubemap-depth capability, or whose cubemap allocation fails, is instead
disabled with one warning. Its receiver then uses the neutral disabled frame,
so direct lighting is preserved rather than making unsupported hardware fail
the frame.
