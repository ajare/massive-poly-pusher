# Particle trails and ribbons

Trails are independent of the particle pool. `RenderSystem::getTrailSystem()`
returns the CPU control API:

```cpp
mpp::TrailSpecification spec;
spec.pointLifetime = 1.0f;
spec.minimumPointDistance = 0.05f;
spec.width = 0.25f;
spec.uvScale = 2.0f;
spec.widthOverLife.keys = {{0.0f, 1.0f}, {1.0f, 0.0f}};
spec.colourOverLife.keys = {
    {0.0f, {1.0f, 0.8f, 0.2f}},
    {1.0f, {1.0f, 0.0f, 0.0f}}
};

auto trail = renderer->getTrailSystem().createTrail(spec, initialPosition);
renderer->getTrailSystem().setTrailPosition(trail, currentPosition);
renderer->getTrailSystem().stopTrail(trail); // lets existing points fade
```

Call `setTrailPosition` as the source moves. The once-per-rendered-frame GPU
update records position samples at `minimumPointDistance`, ages each point by
its own lifetime, and tracks cumulative arc length. Ribbon U is cumulative arc
length multiplied by `uvScale`; V spans the two strip edges. Width and colour
lookup rows are sampled by normalized point lifetime. `startTrail` after a stop
begins a disconnected history, `clearTrail` clears history on the next GPU
update, and `destroyTrail` invalidates its generational handle.

Graphs draw trails through separate additive and alpha `MPP.TrailScene` passes.
The pass accepts the same optional `DEPTH` input used for soft particles and a
`BLEND_MODE` integer (`0` additive, `1` alpha). Generated particle-enabled
graphs and the DemoSuite particle graphs already include both trail passes.
