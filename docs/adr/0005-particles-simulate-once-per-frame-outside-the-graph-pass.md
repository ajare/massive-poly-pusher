---
status: accepted
---

# Particles simulate once per frame outside the graph pass

Particle simulation — spawn, integrate, compact, and indirect draw-argument
generation — is dispatched exactly once per rendered frame by `RenderSystem`,
before render graph execution. `MPP.ParticleScene` is a pure draw pass that
consumes the resulting buffers and may appear in a graph any number of times.

The obvious alternative is to put simulation inside the draw pass, which reads
naturally and keeps the subsystem in one place. It is wrong here because a graph
pass is not once per frame: `MPP.PlanarReflectionScene` already renders the
scene a second time from a mirrored camera, and a shadow domain expands into six
ordered face passes. Simulation inside the pass would therefore advance the
simulation once per view, so enabling planar reflections would silently double
every particle's speed. The split also means the indirect draw buffer is written
once and consumed by every view, which is what makes GPU-driven drawing free
across multiple views rather than repeated per view.

Particles are consequently available only to the graph-driven pipeline modes.
`RenderPipelineMode::PbrForward` and `LegacyForward` execute their own manual
pass sequence, never the graph, and are deliberately not supported: they have no
mechanism for declaring the scene-depth input soft particles require, and both
are the pre-graph path that DemoSuite no longer uses.

Soft particles read scene depth through an ordinary named, optional graph input
with a declared fallback, sampling the live depth image rather than a copy. This
is safe only because particles never write depth, which the pass enforces
itself rather than trusting authored raster state to set — a graph that requests
depth writes on a particle pass is overridden and warned about. Where a driver
mishandles the read-only depth feedback case, inserting a depth-copy pass and
rewiring that input is a render graph authoring change with no C++ change,
because the pass only ever knows that some depth texture is bound to the input.
Omitting the input entirely is also valid, and yields hard-edged particles
rather than a failed frame — which is what lets the legacy graph run particles
without a depth image.
