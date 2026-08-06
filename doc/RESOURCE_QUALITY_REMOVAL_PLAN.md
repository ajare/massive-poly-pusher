# Resource Quality Settings Removal Plan

**Status:** Implemented and validated.

## Goal

Remove embedded quality settings from the resource system so every `ResourceStream` describes exactly one resource definition. Asset variants become separate resources with separate stable names.

## Decisions

- Remove quality IDs, quality names, quality vectors, quality XML, and quality-selection APIs.
- Keep resource loading creation-time and single-definition.
- Do not add inheritance, dependency selectors, or runtime quality switching.
- New binary resource streams use `RSE3` and contain one definition.
- The compatibility reader accepts RSE2/RSER data with exactly one quality definition.
- Legacy data containing multiple quality definitions fails clearly rather than silently selecting one.
- Texture mip/LOD, PBR roughness, shader specialization, MSAA, and application presets are unrelated and remain.

## Phase 1: Core resource API — Complete

- Remove `mQualitySetting`, `mQualityNames`, `createQualitySetting()`, and `getQualityNames()` from `ResourceStream`.
- Change `ResourceStream::load(uint32_t)` to `load()`.
- Stop propagating parent numeric indices into child streams.
- Remove the quality argument from `ResourceManager::declareResource()`.
- Update all call sites and program caching.

## Phase 2: Flatten concrete streams — Complete

Replace each `QualitySetting` vector with one direct definition in:

- Basic/PBR material streams;
- program streams;
- texture/render-texture streams;
- sampler streams;
- string streams;
- model and primitive streams;
- post-effect and render-graph streams.

Remove quality arguments from getters and constructors.

## Phase 3: Programmatic APIs — Complete

Remove material-quality parameters from all programmatic setters for:

- Basic/PBR programs, mesh specifications, shaders, uniforms, and textures;
- textures/render textures;
- samplers;
- strings;
- models and primitives.

Constructors initialize the one direct definition instead of creating quality zero.

## Phase 4: File parsers and XML — Complete

- Replace `parseQualitySetting()` helpers with single-definition parsing.
- Remove loops over `<Quality>`.
- Reject `<Quality>` explicitly with a migration diagnostic.
- Remove obsolete quality hooks from `FileStream`.
- Split or simplify DemoSuite/test XML containing quality data.

## Phase 5: Binary serialization and migration — Complete

- Emit `RSE3` magic.
- Remove quality count/name tables and per-quality loops from new writes.
- Read RSE2/RSER quality tables in compatibility mode.
- Convert exactly one legacy definition.
- Fail legacy multi-quality streams.
- Never emit quality metadata from new serializers.

## Phase 6: Model pipeline and assets — Complete

- Update MPP model stream handling for single definitions.
- Rebuild matching MPP, parser, mesh-specification-parser, converter, and DemoSuite binaries.
- Re-export typed DemoSuite `.mppmodel` assets as RSE3.

## Phase 7: Tests and documentation — Complete

- Replace quality round-trip tests with single-definition round trips.
- Test XML `<Quality>` rejection.
- Test RSE2 single-definition conversion and multi-definition failure.
- Run material, specialization, shadow, bloom, and render-graph GPU suites.
- Document separate named resources as the replacement for embedded variants.

## Acceptance criteria

- No public resource API accepts a quality ID/name.
- No concrete stream stores a quality vector.
- No parser accepts `<Quality>`.
- New binary streams contain no quality table and use RSE3.
- Legacy single-definition streams migrate; multi-quality streams fail.
- BasicMaterial, PbrMaterial, programs, textures, samplers, strings, models, render textures, post effects, and render graphs build and load.
- PBR specialization derives from the one material definition.
- DemoSuite typed assets load without compatibility conversion.
- Repository resource-quality symbols and examples are removed, excluding unrelated third-party text.
