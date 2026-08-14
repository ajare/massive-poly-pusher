# Post Effects and Bloom

## Overview

Post effects run after the 3D scene and before UI as a chain of generic
fullscreen passes. Adding a new effect (or changing bloom's shading) requires
a shader and a `PostEffectMaterial` resource, never a new C++ pass class. This
replaced the old bespoke `BloomExtractPass`/`BloomBlurPass`/`BloomCompositePass`/
`ToneMapPresentPass` classes and the unused `PostEffect`/`PostEffectStream`
stub they sat next to; see `doc/POST_EFFECT_CHAIN_IMPLEMENTATION_PLAN.md` for
the full design history.

There are two authoring surfaces:

- **Raw `.rendergraph.xml`** (e.g. DemoSuite's `PbrPipeline.rendergraph.xml`,
  loaded as a `RenderGraphTemplate`) — effect materials are declared
  programmatically in C++ (`ModelScene.cpp` wraps the engine's built-in
  bloom/tonemap `Program`s in `ProgrammaticPostEffectMaterialStream`
  instances with global resource names) and referenced by name from graph
  passes.
- **`PbrPipelineDocument`/`.pipeline.xml`** (used by `PackageScene` and
  pipeline-editor, e.g. `resources/shared/pbr/templates/Full.pipeline.xml`,
  `resources/shared/preview.pipeline.xml`) — effect materials are authored
  directly as `<PostEffectMaterial>` `LocalResources` entries, parsed by
  `FilePostEffectMaterialStream`.

Both surfaces still hand-author bloom as an explicit sequence of chain
entries (extract → N horizontal/vertical blur pairs → composite → tonemap) in
`<Passes>`, rather than going through `PbrPipelineDocument::buildPostEffectChain()`
(the dynamic auto-wiring builder from Milestone 2) — no document currently
populates `PbrPipelineDocument::postEffects.entries`. `buildPostEffectChain()`
is available but not yet the operative path for any shipped content.

`LegacyForward` and `GraphPbrForward` pipeline modes keep the original
hard-coded fixed-pass bloom (`RenderPipeline::ensureBloomTargets()` and the
`BloomGraphStep` sequence in `RenderPipeline.cpp`) untouched — this was an
explicit non-goal of the migration, not an oversight. `BloomOptions`/
`RenderPipelineOptions::bloom` still drive that path exactly as before.

## The generic pass and material

`FullscreenEffectPass` (`mpp/src/RenderGraphBuiltInPasses.cpp`, factory name
`MPP.FullscreenEffect`) is the one pass type behind every migrated effect. At
execute time it:

1. Resolves its `programResource` to a `PostEffectMaterial` — trying
   `RenderPipelineOptions::resourceRoot + "/" + programResource` first (for
   `PbrPipelineDocument`-authored materials, which only ever exist under a
   dynamically-generated per-rebuild root the document can't predict itself),
   then falling back to a plain global lookup (DemoSuite's path, which never
   sets `resourceRoot`).
2. Binds every sampler the pass authored (`GraphPassInfo::samplerBindings`) by
   name — already-generic machinery, unrelated to this pass type specifically.
3. Layers the pass's graph-authored/executor-overridden parameters on top of
   the material's default `UniformCollection`.
4. Renders via `RenderSystem::renderGraphFullscreen(...)`.
5. If disabled (`ENABLED` parameter, default `1`) or `programResource` is
   empty, blits its primary input straight through instead — an effect
   turning off must never break the chain for whatever reads its output next.

`PostEffectMaterial` (`mpp/include/mpp/PostEffectMaterial.h`) wraps a
`Program` resource plus declared sampler-slot names and default uniform
values, validated against the compiled program at creation. It is
deliberately not a `Material` subclass — `Material` carries surface-only
concepts (`ShadingModel`, double-sidedness, transparency) that don't apply to
a fullscreen quad.

## Runtime tuning

`RenderPipeline::setPostEffectEnabled(passName, bool)` and
`setPostEffectParameter(passName, paramName, float)` replace `BloomOptions`
for chain-authored effects — DemoSuite's Bloom UI (`ModelScene.cpp`) calls
these directly instead of `setBloomOptions()` for its `XmlGraphPBR` pipeline.
`setBloomOptions()`/`RenderPipelineOptions::bloom` remain load-bearing only
for `LegacyForward`/`GraphPbrForward`.

For `PbrPipelineDocument`-authored bloom, `PbrPipelineDocument::setBloomEnabled()`
is still the mechanism pipeline-editor calls (its Bloom toggle mutates the
document, which gets rebuilt) — it now recognizes bloom/tonemap passes under
either the legacy `callbackFactory` scheme or the migrated
`MPP.FullscreenEffect` + `programResource` scheme (matched by a
`PostEffect.Bloom*`/`PostEffect.ToneMap` naming convention). This is an
explicit migration bridge, not a permanent design: it exists only because
authored documents still declare bloom as a fixed pass sequence rather than
`postEffects.entries`, and it goes away once they don't.

## Effect sequence (bloom's shape)

Bloom is authored as: extract (bright-pass threshold) → repeated
horizontal/vertical blur pairs (ping-pong) → composite (add blurred bloom
back onto the scene) → tonemap. This is unchanged from before the migration;
what changed is that each stage is now a `PostEffectMaterial` + generic pass
instead of a bespoke C++ class. See `resources/shared/pbr/templates/Full.pipeline.xml`
for a complete authored example (five `PostEffectMaterial` local resources +
eleven `MPP.FullscreenEffect`/`present`-type passes).

## Tuning

- **Threshold:** lower values bloom more of the image; `0.7` is the shipped
  default.
- **Intensity:** controls only the added bloom contribution; `0.2` is the
  shipped default.
- **Blur passes:** each pre-authored horizontal/vertical pair is toggled by
  its own `ENABLED` parameter — DemoSuite's UI (`ModelScene.cpp`) computes
  which pairs are active from the "Bloom Blur Passes" slider and calls
  `setPostEffectEnabled` per pair.
- **PBR exposure:** the tonemap chain entry's `EXPOSURE`/`TONE_MAP_OPERATOR`
  parameters are independent of bloom's threshold/intensity.

## Current limitations

- Bloom's chain entries are still hand-authored per document (extract + N
  blur-pair instances + composite + tonemap), not generated by
  `PbrPipelineDocument::buildPostEffectChain()`'s dynamic auto-wiring — that
  builder exists (Milestone 2) but nothing populates `postEffects.entries`
  yet.
- No downsample pyramid or transient target reuse beyond what the render
  graph's existing pooled allocator already gives every pass; bloom targets
  are still full resolution.
- pipeline-editor can create/select `PostEffectMaterial` local resources, but
  has no detail-panel editor for their fields yet (Program ref picker,
  sampler-slot list, uniform list) — only the generic name field. There is
  also no equivalent of the old bespoke "add a bloom chain" wizard for the
  generic scheme; authoring a full chain today means adding each
  `PostEffectMaterial` and pass by hand.
- `LegacyForward`/`GraphPbrForward` bloom remains a separate, untouched
  hard-coded implementation — an accepted, explicit scope boundary, not
  planned for migration.
