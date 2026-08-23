---
status: accepted
---

# SSAO is a fixed, non-reorderable pass using depth-only normal reconstruction

We're adding SSAO to `RenderPipeline`, available on `GraphPbrForward`, `GraphLegacyForward`, and
`XmlGraphPbrForward`, toggleable per pipeline via a new `SSAOOptions` (mirroring `BloomOptions`).
Two deliberate deviations from the "obvious" path, both requirement-driven rather than technical
defaults:

1. **Fixed placement, not a movable chain entry.** The engine already has a generic, authorable,
   reorderable post-effect mechanism (`MPP.FullscreenEffect` + `PostEffectMaterial`), which Bloom
   is migrating toward. SSAO deliberately does *not* use it: it's inserted at a hardcoded point
   (immediately after the opaque scene pass, before bloom extract) via two mechanisms — a
   hardcoded C++ step in `RenderPipeline::renderGraphForward()` (shared by `GraphPbrForward`/
   `GraphLegacyForward`, alongside the existing `BloomGraphStep`) and `PbrPipelineDocument::
   setSSAOEnabled()` mutating the authored graph (mirroring `setBloomEnabled()`) for
   `XmlGraphPbrForward`. This was an explicit requirement: SSAO must be automatically placed and
   cannot be moved/reordered by pipeline authors, unlike a chain-authored effect.

2. **Depth-only normal reconstruction, not a G-buffer normals pass.** `doc/RENDERING_ANALYSIS.md`
   §9.2 suggested SSAO needs a normals MRT attachment. No pipeline mode currently produces one, and
   adding it to `LegacyForward`/plain `PbrForward` would have expanded scope well beyond "add
   SSAO." Since SSAO must be available uniformly across all in-scope pipeline modes, view-space
   normals are reconstructed from depth-buffer derivatives instead (the standard forward-renderer
   fallback technique). Lower quality than a true G-buffer normal, but requires no new MRT
   attachment or scope expansion into pipeline modes that don't have one.

Also decided in the same session: SSAO is applied as a post-multiply on the already-shaded scene
colour (like Bloom's fixed-pass pattern), not sampled during shading against a true ambient-only
term — the latter would require a depth prepass, which doesn't exist in this engine yet
(`doc/RENDERING_ANALYSIS.md` §9.2) and is out of scope here.
