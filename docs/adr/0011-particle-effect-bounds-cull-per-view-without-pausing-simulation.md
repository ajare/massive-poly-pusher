---
status: accepted
---

# Particle effect bounds cull per view without pausing simulation

Version-2 particle effect assets may author optional local-space bounds as a center and size. MPP conservatively aggregates fully bounded child branches, transforms the result for each live particle effect, and CPU-tests it independently for every render view so out-of-frustum effect draw ranges and visual contributions are not submitted. An absent bound or any unbounded child branch makes the aggregate unbounded; culling never pauses simulation or event processing, because one once-per-frame simulation is shared by all views under ADR 0005.
