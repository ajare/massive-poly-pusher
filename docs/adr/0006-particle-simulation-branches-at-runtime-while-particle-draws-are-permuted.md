---
status: accepted
---

# Particle simulation branches at runtime while particle draws are permuted

Particle simulation is a single compute kernel that reads a behaviour-module
bitmask from each emitter's record and branches on it at runtime. Particle draw
programs take the opposite approach: they are specialised at build time by
`#define` injection, one variant per distinct appearance. The same subsystem
deliberately uses opposite strategies for the two stages, and the reason is not
visible from either shader on its own.

Simulation runs over the global active-particle list, which interleaves
particles from every emitter in the scene. Specialising the simulation kernel
per module set would therefore require partitioning that list by variant every
frame and issuing one dispatch per partition — a full scatter over a list of up
to a million entries, added to the hottest path in the system, to remove
branches that are each a handful of arithmetic operations. Runtime branching
costs warp divergence instead, which is the cheaper of the two by a wide margin
at this module count.

Drawing has no such constraint. Compaction already groups particles into
contiguous per-emitter-template ranges, so each appearance is drawn by its own
indirect command with its own state bound. A specialised program there costs
nothing extra and avoids paying for unused texture fetches and blend-mode
branches per fragment, where fill rate is the binding cost.

If profiling later shows simulation divergence dominating — many emitters with
widely differing module sets — partitioning the active list is an internal
change that no authored asset would observe. The reverse move, permuting the
simulation kernel without partitioning, is not available: it is the partition,
not the permutation, that the single flat dispatch forbids.
