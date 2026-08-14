# Generic Post-Effect Chain for the PBR Pipeline

## Context

Bloom today is not one implementation but three, and none of them go through the `PostEffect`
resource type that exists for exactly this purpose:

1. **XML render-graph passes** — `BloomExtractPass`/`BloomBlurPass`/`BloomCompositePass`
   (`mpp/src/RenderGraphBuiltInPasses.cpp:55-93`), each a bespoke `RenderGraphScenePass` hard-coding
   its own shader call (`RenderSystem::renderBloomExtract/renderBloomBlur/renderBloomCombine`).
   Registered by factory name (`mpp/src/RenderGraphBuiltInPasses.cpp:151+`) and wired/enabled by
   hand-written, bloom-specific rewiring logic in `PbrPipelineDocument::setBloomEnabled`
   (`mpp/src/PbrPipelineDocument.cpp:20-78`). **This is the path this plan replaces.**
2. A second, smaller bloom-graph variant embedded directly in `RenderPipeline.cpp:467-576`
   (`BloomGraphStep` enum).
3. **Legacy fixed-pass bloom** on `LegacyForward`'s RGBA8 scene target, inlined in
   `RenderPipeline::render()` (`RenderPipeline.cpp:723-754`).

Meanwhile `PostEffect` (`mpp/include/mpp/PostEffect.h`), `PostEffectStream`
(`mpp/include/mpp/PostEffectStream.h`), and `RenderPipeline::mPostEffects`/`addPostEffect()`
(`RenderPipeline.h:152,244`; `RenderPipeline.cpp:327-330`) are dead code: `PostEffect::createImpl()`
is an empty comment block (`PostEffect.cpp:30-39`), and nothing in `RenderPipeline::render()` ever
executes anything in `mPostEffects` — its only other use is a fallback in `getOutputRenderTarget()`
(`RenderPipeline.cpp:258-265`). This is confirmed in `doc/POST_EFFECTS_AND_BLOOM.md`, which already
lists the extensions a real post-effect system needs (explicit pass interface, executable contract,
transient target allocator, colour-space metadata, named input binding, per-effect shader/material
resources, ordered composition, resize/failure handling, profiling, graph validation) — this plan
implements that list.

## Goals

- A generic, declarative post-effect chain for the **PBR render-graph pipeline only**, running after
  the 3D scene and before any 2D UI.
- Each effect is a fullscreen quad + a material (shader + uniforms + sampler inputs) — adding a new
  effect requires a shader and XML authoring, **not** a new C++ pass class.
- Effects are an author-reorderable ordered list; reordering automatically re-wires each stage's
  primary input to the new previous stage's output, reusing/reallocating transient render targets as
  needed.
- Tone mapping becomes just another chain entry — no structurally special "pre-tone-map" vs
  "post-tone-map" stage. Authors are responsible for correct ordering.
- Bloom is reimplemented as three chain entries (extract, blur ×N, composite) with **zero** new C++
  pass classes, replacing path (1) above.
- Runtime tuning (thresholds, intensity, exposure, etc.) via a generic name/value parameter API, not
  a typed options struct per effect.

## Non-goals (this pass)

- `LegacyForward`'s hard-coded bloom (item 2 and 3 above) — left untouched. Bloom stays duplicated
  between the legacy path and the new generic PBR chain; not addressed here.
- A graph-compiler-level "true skip" for disabled effects (zero-cost elision). Disabled effects
  still execute as a copy-through blit, matching today's bloom-blur behavior.
- A first-class "repeat N times" graph primitive. Iteration (e.g. blur pass count) continues to use
  the existing bounded-pre-authored-instances + `ITERATION` parameter pattern, generalized to any
  effect.
- Automated migration of existing `.rendergraph.xml`/`PbrPipeline` documents from `MPP.Bloom*` passes
  to the new chain format — hand-edit the in-repo documents (DemoSuite's).
- Any new UI/editor tooling for authoring or reordering chains beyond what DemoSuite already has for
  bloom sliders (adapted to the generic parameter API).

## Architecture overview

### 1. `PostEffectMaterial` — the effect's shader/material resource

New resource type, sibling to `Material` but **not** derived from it (`Material`
(`mpp/include/mpp/Material.h`) is explicitly "the shared renderer-facing base for all *surface*
material resources" — `ShadingModel`, `isDoubleSided()`, `isTransparent()` are 3D-geometry concepts
that don't apply to a fullscreen quad).

- `mpp/include/mpp/PostEffectMaterial.h` / `.cpp`: wraps a `Program` resource (reusing
  `Program`'s existing uniform/sampler reflection, `mpp/include/mpp/Program.h`), and declares:
  - named sampler slots the effect expects (e.g. `TEX0`, `SCENE`, `BLOOM`) — validated against the
    `Program`'s actual reflected samplers (`Program::getNumSamplers`/`getSamplerName`) at load time.
  - default uniform values (threshold, intensity, exposure, ...), overridable per chain entry.
- `mpp/include/mpp/PostEffectMaterialStream.h` / `.cpp`: replaces `PostEffectStream`
  (`mpp/include/mpp/PostEffectStream.h`, currently an unused stub — no `loadImpl` body of
  consequence, no `ResourceStreamSerializer` integration). Follow `PbrMaterialStream`
  (`mpp/include/mpp/PbrMaterialStream.h`) as the template: `friend class ResourceStreamSerializer`,
  a specification struct, real `loadImpl()`.
- **Serialization parity (explicit requirement):** `ResourceStreamSerializer`
  (`mpp/include/mpp/ResourceStreamSerializer.h`) has matched `write*Stream`/`read*Stream` pairs for
  every real resource stream (`writePbrMaterialStream`/`readPbrMaterialStream`,
  `writeProgramStream`/`readProgramStream`, etc., lines 35-44 and 65-74) dispatched from
  `writeStream`/`readStream`. `PostEffectStream` has **no** such pair today — it was never wired in.
  Add `writePostEffectMaterialStream`/`readPostEffectMaterialStream` to
  `ResourceStreamSerializer.h`/`.cpp`, wire them into `writeStream`/`readStream`'s dispatch, matching
  the read/write field coverage `PbrMaterialStream` gets.
- Register in `ResourceManager::ResourceManager()` (`mpp/src/ResourceManager.cpp:76-79`): replace the
  current `mResourceFactories["PostEffect"]` entry with `"PostEffectMaterial"` constructing
  `PostEffectMaterial`.

### 2. `FullscreenEffectPass` — the one generic pass type

New `mpp/src/RenderGraphBuiltInPasses.cpp` class, `RenderGraphScenePass`-derived
(`mpp/include/mpp/RenderGraphScenePass.h`), replacing `BloomExtractPass`, `BloomBlurPass`,
`BloomCompositePass`, **and** `ToneMapPresentPass` (lines 55-98) for the PBR graph path:

```cpp
class FullscreenEffectPass final : public RenderGraphScenePass
{
public:
    void execute(RenderGraphExecutionContext const& context) override;
};
```

- Resolves its `PostEffectMaterial` via `GraphPassInfo::programResource`-equivalent field (the graph
  already carries a `programResource` string per pass, `RenderGraph.h:179`, set today via
  `graph->setPassProgramResource`; extend this pattern to reference the material resource name).
- Binds sampler inputs from `context.getPass().samplerBindings` by name — this is already generic
  (`RenderGraphBuiltInPasses.cpp:45-53`'s `input(context, "SCENE")` helper).
- Binds uniform parameters from `context.getParameters()` — already generic
  (`RenderGraphExecutionContext::getParameters()`, `RenderGraphExecutor::setPassParameterOverrides`,
  `RenderGraphExecutor.h:105-106`).
- Calls one new `RenderSystem::renderFullscreenEffect(PostEffectMaterial*, ...)`
  (`mpp/include/mpp/RenderSystem.h`/`.cpp`, new method, additive — the existing
  `renderBloomExtract`/`renderBloomBlur`/`renderBloomCombine`/`renderToneMappedFullscreenQuad`
  (`RenderSystem.h:677-680` and neighbors) **stay**, since `LegacyForward`'s hard-coded path
  (non-goal) still calls them.
- Handles the disabled case generically: if the chain entry is disabled, blit input straight to
  output (`renderFullscreenQuad`, already generic) instead of running the material — same shape as
  today's `BloomBlurPass`'s copy-through branch (`RenderGraphBuiltInPasses.cpp:78-79`), but no longer
  bloom-specific.
- Handles iteration generically: reads an `ITERATION` parameter the same way
  `bloomBlurIteration()` does today (`RenderGraphBuiltInPasses.cpp:60-66`,
  `RenderGraphBuiltInPasses.h:15-20`), generalized to not be bloom-named — e.g.
  `effectPassIteration(UniformCollection const&, std::string const& passName)` — compared against a
  per-effect "active iteration count" parameter instead of `BloomOptions::blurPasses`.

### 3. Ordered chain authoring and auto-wiring

New concept in `PbrPipelineDocument` (`mpp/include/mpp/PbrPipelineDocument.h`/`.cpp`), replacing
`BloomOptions bloom;` + `setBloomEnabled()`:

```cpp
struct PostEffectChainEntry
{
    std::string name;               // authored identifier, used for parameter API + reordering
    std::string material;           // PostEffectMaterial resource name
    bool enabled{ true };
    float outputScale{ 1.0f };      // 1.0 = inherit previous stage's size
    std::optional<GraphImageFormat> outputFormat; // unset = inherit
    std::vector<std::pair<std::string,std::string>> extraSamplerBindings; // slot -> image/effect name, for non-chain inputs (depth, emissive, ...)
};
struct PostEffectChain
{
    std::vector<PostEffectChainEntry> entries;
};
```

- A build step (`PbrPipelineDocument::buildPostEffectChain()` or invoked from wherever the document
  currently constructs/validates its `graph`, alongside `validate()`
  (`PbrPipelineDocument.cpp:80+`)) expands `entries` into concrete `FullscreenEffectPass` graph
  passes: for each entry, `graph->createImage(...)` sized per `outputScale`/`outputFormat` (default
  inherits the previous entry's — or, for the first entry, the scene target's — descriptor),
  `graph->addPass(...)` with `callbackFactory = "MPP.FullscreenEffect"`,
  `programResource = entry.material`, primary sampler bound to the previous entry's output image
  (or the scene colour target for the first entry), plus any `extraSamplerBindings` resolved by name
  against other known images/prior-entry outputs.
- Reordering `entries` and rebuilding regenerates all primary-chain wiring automatically — this is
  the mechanism that makes "change the order" actually safe, versus manually repointing sampler
  bindings on raw graph passes.
- Uses the render graph's existing transient allocator (`RenderGraphTargets`,
  `mpp/include/mpp/RenderGraphTargets.h`) — no new allocation code, just descriptors derived from
  chain-entry authoring instead of hard-coded bloom target sizes
  (`RenderPipeline::ensureBloomTargets()`, to be deleted for the graph path).
- The final chain entry's output becomes the pipeline's presented image (replaces the current
  `bloomCompositeOutputs`/`ToneMapPresent` rewiring logic in `setBloomEnabled`,
  `PbrPipelineDocument.cpp:71-77`).

### 4. Runtime parameter API

Replaces `RenderPipeline::setBloomOptions()`/`BloomOptions` (`RenderPipeline.h:67,135,181`) for the
PBR graph path only (kept for `LegacyForward`, per non-goals):

```cpp
void RenderPipeline::setPostEffectEnabled(std::string const& effectName, bool enabled);
void RenderPipeline::setPostEffectParameter(std::string const& effectName, std::string const& paramName, float value);
```

- Builds on `RenderGraphExecutor::setPassParameterOverrides(passName, UniformCollection)`
  (`RenderGraphExecutor.h:105`) — the pass name is the chain entry's `name`.
- `setPostEffectEnabled` toggles the copy-through behavior in `FullscreenEffectPass` (§2), no graph
  rebuild required.
- DemoSuite's bloom UI (threshold/intensity/blur-pass sliders, currently calling
  `pipeline->setBloomOptions(...)` per `doc/POST_EFFECTS_AND_BLOOM.md`) is rewired to call
  `setPostEffectParameter("BloomExtract", "THRESHOLD", ...)` etc. — same UI widgets, generic call
  underneath.

### 5. Cleanup — dead code removal

Once the above lands and DemoSuite's PBR pipeline document is migrated (see Milestones):

- Delete `mpp/include/mpp/PostEffect.h`/`.cpp`, `PostEffectStream.h`/`.cpp`.
- Remove the `"PostEffect"` factory entry in `ResourceManager.cpp:76-79` (superseded by
  `"PostEffectMaterial"`, already added in §1).
- Remove `RenderPipeline::mPostEffects`, `addPostEffect()`, and the `getOutputRenderTarget()`
  fallback branch that reads it (`RenderPipeline.h:152,244`; `RenderPipeline.cpp:258-265,327-330`).
- Remove `BloomExtractPass`, `BloomBlurPass`, `BloomCompositePass`, `ToneMapPresentPass`, and their
  factory registrations (`RenderGraphBuiltInPasses.cpp:55-98,151+`) — but only after confirming no
  other `.rendergraph.xml` document still references `MPP.Bloom*`/`MPP.ToneMapPresent` by name
  (grep the resources tree before deleting).
- Remove `PbrPipelineDocument::setBloomEnabled` (`PbrPipelineDocument.cpp:20-78`) and the
  bloom-specific validation block in `validate()` (`PbrPipelineDocument.cpp:91`), replaced by
  generic chain validation (entry references a real material, sampler slots resolve, no cycles).
- `RenderPipeline.cpp:467-576` (`BloomGraphStep`) and `RenderPipeline.cpp:723-754` (legacy fixed
  bloom) are **not** touched — non-goal.

## Files touched

- New: `mpp/include/mpp/PostEffectMaterial.h`, `mpp/src/PostEffectMaterial.cpp`
- New: `mpp/include/mpp/PostEffectMaterialStream.h`, `mpp/src/PostEffectMaterialStream.cpp`
- `mpp/include/mpp/ResourceStreamSerializer.h`, `mpp/src/ResourceStreamSerializer.cpp` — add
  read/write pair for `PostEffectMaterialStream`
- `mpp/src/ResourceManager.cpp` — factory registration swap
- `mpp/src/RenderGraphBuiltInPasses.cpp`, `mpp/include/mpp/RenderGraphBuiltInPasses.h` — remove
  bloom/tonemap-specific passes, add `FullscreenEffectPass`, generalize `bloomBlurIteration` →
  `effectPassIteration`
- `mpp/include/mpp/RenderSystem.h`, `mpp/src/RenderSystem.cpp` — add `renderFullscreenEffect(...)`
- `mpp/include/mpp/PbrPipelineDocument.h`, `mpp/src/PbrPipelineDocument.cpp` — `PostEffectChain`/
  `PostEffectChainEntry`, chain-build/auto-wire step, remove `setBloomEnabled` and `BloomOptions`
  field, generic chain validation in `validate()`
- `mpp/include/mpp/RenderPipeline.h`, `mpp/src/RenderPipeline.cpp` — `setPostEffectEnabled`/
  `setPostEffectParameter`; remove `mPostEffects`/`addPostEffect()`/`setBloomOptions` (PBR path only)
- `mpp/include/mpp/PostEffect.h`/`.cpp`, `PostEffectStream.h`/`.cpp` — deleted (final milestone)
- DemoSuite: PBR pipeline `.rendergraph.xml`/`PbrPipeline` document(s) — replace `MPP.Bloom*`/
  `MPP.ToneMapPresent` passes with a `PostEffectChain` of three bloom entries + one tone-map entry;
  new `.postfx`/material resource files (extension name TBD) for the bloom/tone-map shaders
- DemoSuite: bloom UI code — call `setPostEffectParameter`/`setPostEffectEnabled` instead of
  `setBloomOptions`
- `doc/POST_EFFECTS_AND_BLOOM.md` — rewritten once the new system lands, documenting the generic
  chain in place of the "required extensions" gap list

## Implementation milestones

### Milestone 1 — `PostEffectMaterial` resource, fully serializable, unused by any pipeline yet
- Implement `PostEffectMaterial`/`PostEffectMaterialStream` per §1, including the
  `ResourceStreamSerializer` read/write pair.
- **Acceptance check**: round-trip test — author a `PostEffectMaterial` resource file, load it via
  `ResourceManager`, serialize it back out via `ResourceStreamSerializer::serialize`, deserialize
  again, confirm equivalence (mirrors however existing material round-trip tests work, if any exist
  in the test suite — check first).
- Delete nothing yet; old `PostEffect`/`PostEffectStream` and all bloom paths keep working unchanged.

### Milestone 2 — `FullscreenEffectPass` + chain auto-wiring, no bloom migration yet
- Implement `FullscreenEffectPass`, `RenderSystem::renderFullscreenEffect`, and the
  `PostEffectChain`/`PostEffectChainEntry` build/auto-wire step in `PbrPipelineDocument`.
- Prove it on a trivial single-entry chain first (e.g. a no-op passthrough `PostEffectMaterial`
  inserted between the scene target and `ToneMapPresent`) before touching bloom — this isolates
  chain-wiring bugs from bloom-shader-porting bugs.
- **Acceptance check**: `runRenderGraphGpuTests`-style GPU test (see
  `mpp/src/RenderGraphGpuTests.cpp`) covering chain build/execute/resize with 1, 2, and 3 entries,
  confirming target reuse/regeneration behaves per `RenderGraphTargets`'s generation-bump semantics.
- **Acceptance check**: reordering a 2-entry no-op chain and rebuilding produces the expected wiring
  (assert on the resulting `GraphPassInfo::samplerBindings`, not just visual output).

### Milestone 3 — Port bloom and tone mapping to chain entries
- Write bloom's extract/blur/composite shaders as `Program` resources + wrap each in a
  `PostEffectMaterial` (threshold/intensity/exposure become material-declared uniforms).
- Write tone mapping (currently `ToneMapPresentPass`'s `renderToneMappedFullscreenQuad` call,
  `RenderGraphBuiltInPasses.cpp:97`) as a `PostEffectMaterial` too — same generic mechanism, no
  special stage.
- Update DemoSuite's PBR pipeline document: replace `MPP.Bloom*`/`MPP.ToneMapPresent` passes with a
  4-entry `PostEffectChain` (extract, blur ×`blurPasses` pre-authored bounded instances, composite,
  tonemap) in the desired order.
- Wire DemoSuite's bloom UI to `setPostEffectParameter`/`setPostEffectEnabled`.
- **Acceptance check**: visual A/B — capture the PBR demo scene's bloom output before and after
  migration at matching threshold/intensity/blur-pass settings; confirm no regression.
- **Acceptance check**: reorder the chain in the document (e.g. put tone mapping before bloom
  composite) and confirm the visual result changes as expected (proves reordering is load-bearing,
  not just structurally present) — then restore correct order.
- **Acceptance check**: toggle bloom off via `setPostEffectEnabled` at runtime; confirm copy-through
  behavior (scene renders unchanged, no black/blank output).

### Milestone 4 — Cleanup
- Grep the full resources tree for remaining `MPP.Bloom*`/`MPP.ToneMapPresent`/`PostEffect`
  references; migrate or confirm none remain outside DemoSuite's already-migrated document.
- Delete `BloomExtractPass`/`BloomBlurPass`/`BloomCompositePass`/`ToneMapPresentPass` and their
  registrations, `PbrPipelineDocument::setBloomEnabled`, `BloomOptions` field on
  `PbrPipelineDocument`, `PostEffect`/`PostEffectStream`, `RenderPipeline::mPostEffects`/
  `addPostEffect()`.
- Rewrite `doc/POST_EFFECTS_AND_BLOOM.md` to describe the generic chain instead of the gap list.
- **Acceptance check**: full build + existing render-graph/GPU test suite passes with the old types
  gone; `LegacyForward`'s bloom (non-goal, `RenderPipeline.cpp:723-754`) still works unchanged,
  confirming the PBR-only scope boundary held.

## Verification

- `runRenderGraphGpuTests` coverage for: chain build/execute/resize at 1-4 entries; reordering;
  per-entry `outputScale`/`outputFormat` overrides producing correctly-sized/formatted transient
  targets; disabled-entry copy-through.
- DemoSuite visual check: bloom before/after migration at fixed settings; full chain reorder changes
  output as expected; runtime parameter tuning (threshold/intensity/exposure sliders) still behaves
  identically to the old `BloomOptions`-driven UI.
- Confirm `LegacyForward` bloom is unaffected (non-goal boundary) — run the legacy demo pipeline
  before/after this work, no visual or behavioral change expected.
- Resize test: window resize with an active multi-entry chain, confirming transient target
  regeneration (`RenderGraphTargets::getGeneration()`) doesn't leave stale/mismatched framebuffer
  views, matching existing resize handling for the old bloom targets.

## Open questions

- Exact XML schema for `PostEffectChain`/entries in the `PbrPipelineDocument`/`.rendergraph.xml`
  format is not finalized here — settle it at the start of Milestone 2 alongside the C++ struct
  shapes above.
- Whether `PostEffectMaterial` resource files get a new file extension/convention or reuse an
  existing material-authoring convention — check how `BasicMaterial`/`PbrMaterial` resource files
  are named/discovered before deciding.
- Whether any round-trip/serialization test scaffolding already exists for material streams that
  Milestone 1's acceptance check should reuse rather than write from scratch.
