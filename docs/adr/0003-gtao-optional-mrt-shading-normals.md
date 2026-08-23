---
status: accepted
supersedes: 0001 (GTAO-specific depth-only normal statement), 0002 (GTAO-specific depth-only normal statement)
---

# GTAO optionally consumes MRT shading normals

GTAO retains depth-reconstructed normals as its default `depth` normal source, preserving the forward-renderer compatibility and historical rationale recorded in ADR-0001 and ADR-0002. Its raw renderer may instead select `mrt` and consume a supplied `RG16F` texture of octahedrally encoded view-space shading normals. Depth remains the source of view-position reconstruction in either mode.

The MRT path is deliberately optional and limited to raw GTAO integration points that supply the texture. Selecting it without a valid normals texture is an error; it must not silently fall back to depth reconstruction. SSAO remains depth-only. This supersedes only the GTAO-specific depth-only statements in ADR-0001 and ADR-0002, not their fixed placement, SSAO, or default-rationale decisions.
