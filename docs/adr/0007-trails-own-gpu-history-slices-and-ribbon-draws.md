---
status: accepted
---

# Trails own GPU history slices and ribbon draws

Trails are a rendering primitive separate from particles. A live trail has a
CPU-controlled generational handle and current source position, while a compute
kernel owns its fixed-capacity ring of time-limited trail points. A dedicated
indirect triangle-strip draw reconstructs a camera-facing ribbon from those
points. Particle pool records, spawn commands, compaction buffers, and billboard
draws are not reused.

The tempting alternative is to emit many small particles along the source path.
That makes continuity depend on particle density, spends one independently
simulated pool slot per visual sample, cannot generate stable arc-length UVs,
and turns width continuity into overlapping billboard geometry. Sharing the
particle buffers while adding a point type would still couple two different
allocation and draw lifecycles and make every particle kernel branch on a
primitive it does not process.

Each trail receives a bounded point slice rather than allocating points from a
shared free list. This caps one badly configured weapon effect without atomics,
lets one compute invocation mutate a trail's ring, and permits the ribbon vertex
shader to recover the trail and point ordinal directly from an indirect
command's first vertex. The trade-off is reserved GPU memory per supported trail;
the buffers remain lazy, and the fixed limits keep that cost bounded.

Trail history updates run beside particle simulation once per rendered frame,
before graph execution. `MPP.TrailScene` is a pure draw pass, so reflected or
otherwise repeated views draw the same history without advancing point age or
recording duplicate positions.
