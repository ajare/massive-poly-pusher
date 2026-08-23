---
status: accepted
---

# Ambient occlusion is a pipeline method selection

Pipeline ambient occlusion is represented as one mutually exclusive method—None, SSAO, or GTAO—with method-specific settings retained together so authors can switch methods without losing tuning. SSAO and GTAO share the fixed post-opaque depth-only blur/composite sequence established by ADR-0001; only the raw occlusion pass changes. This avoids simultaneously applying competing AO methods, keeps placement non-authorable, and preserves forward-renderer compatibility without a normals attachment. Legacy `SSAO` document sections are read as the SSAO method, while native saves use the `AmbientOcclusion` section.
