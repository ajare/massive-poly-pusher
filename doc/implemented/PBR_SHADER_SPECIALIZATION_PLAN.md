# PBR Shader Specialization Plan

**Status:** Implemented and validated.

## 1. Goal

Compile and cache built-in PBR fragment-program variants that omit material features which cannot contribute. New custom PBR programs must expose an exact reflected interface matching the material specialization. Preserve deterministic resource loading, existing pipeline behavior, and temporary legacy RSER compatibility.

## 2. Principles

- Specialization is derived once from the material definition during resource creation.
- Material values define the maximum compiled feature contract.
- Instance overrides may reduce enabled contributions, including setting an enabled factor to zero, but cannot enable a feature specialized out by the material.
- Texture presence is material-owned and immutable per created material.
- Variants compile on demand and are reused through the existing source/mesh program cache.
- Compilation or contract failure is fatal and diagnostic; there is no silent full-shader fallback.
- This scope specializes fragment behavior only. The built-in PBR vertex and mesh contract remains unchanged.
- Direct lights, IBL, shadows, bloom MRT, and arbitrary nonzero values remain runtime/pipeline state and are not specialization dimensions.
- New XML and `.mppmodel` data do not serialize a mask. The selected material data remains authoritative.

## 3. Public feature model

Add an inspectable `PbrMaterialFeatures` bitmask with these derived dimensions:

- `BaseColourMap`
- `Metallic`
- `Roughness`
- `MetallicRoughnessMap`
- `NormalMap`
- `Occlusion`
- `Emissive`
- `AlphaMask`
- `AlphaBlend`
- `DoubleSided`
- temporary internal compatibility state `LegacyFullContract`

Derivation rules:

- Base-colour texturing is enabled only when its semantic map is declared.
- Metallic is enabled when `metallicFactor > 0`.
- Roughness is enabled when `roughnessFactor > 0`.
- The shared metallic-roughness sampler is enabled only when its map is declared and metallic or roughness is enabled.
- Normal mapping requires a declared normal map and `normalScale > 0`.
- Occlusion requires a declared occlusion map and `occlusionStrength > 0`.
- Emissive requires a declared emissive map and at least one positive emissive-factor component.
- Alpha mask/blend are mutually exclusive static modes; opaque has neither bit.
- Double-sided is static.

Expose the selected mask and a stable readable feature summary from `PbrMaterial` for tests, diagnostics, tooling, and GPU labels.

## 4. Exact material/program interface

Always active:

- `PBR_BASE_COLOUR_FACTOR`
- `PBR_IRRADIANCE_MAP`
- `PBR_PREFILTERED_SPECULAR_MAP`
- `PBR_BRDF_LUT`
- fragment output location 0

Conditionally active:

- `PBR_BASE_COLOUR_MAP` for `BaseColourMap`.
- `PBR_METALLIC_FACTOR` for `Metallic`.
- `PBR_ROUGHNESS_FACTOR` for `Roughness`.
- `PBR_METALLIC_ROUGHNESS_MAP` for `MetallicRoughnessMap`.
- `PBR_NORMAL_MAP` and `PBR_NORMAL_SCALE` for `NormalMap`.
- `PBR_OCCLUSION_MAP` and `PBR_OCCLUSION_STRENGTH` for `Occlusion`.
- `PBR_EMISSIVE_MAP` and `PBR_EMISSIVE_FACTOR` for `Emissive`.
- `PBR_ALPHA_CUTOFF` for `AlphaMask`.

Never active as runtime material uniforms in specialized programs:

- `PBR_ALPHA_MODE`
- `PBR_DOUBLE_SIDED`

`SHADOW_MAP`, shadow/light blocks, and fragment output location 1 are optional pipeline/program capabilities rather than material-owned interface requirements.

Every active material-owned canonical uniform/sampler outside the selected set is rejected for new custom programs. Existing `PBR_EXT_*` declaration, reflected type/target, completeness, and per-instance rules remain exact.

## 5. Shader-source contract

Inject a stable define block after `@@Version` into built-in and material-owned custom fragment sources:

- `PBR_SPEC_BASE_COLOUR_MAP`
- `PBR_SPEC_METALLIC`
- `PBR_SPEC_ROUGHNESS`
- `PBR_SPEC_METALLIC_ROUGHNESS_MAP`
- `PBR_SPEC_NORMAL_MAP`
- `PBR_SPEC_OCCLUSION`
- `PBR_SPEC_EMISSIVE`
- `PBR_SPEC_ALPHA_MASK`
- `PBR_SPEC_ALPHA_BLEND`
- `PBR_SPEC_DOUBLE_SIDED`

The built-in shader conditionally declares inputs and compiles out disabled calculations. A custom source may use the same defines. An already-compiled referenced `Program` is never modified and must already reflect the exact selected contract.

Program-parser sampler metadata must represent active linked samplers so preprocessor-disabled declarations are not treated as material bindings.

## 6. Runtime and caching

- Validate surface values before deriving a mask or selecting a program.
- Generate the define block and specialized built-in source before default-program lookup.
- Existing concatenated generated source plus mesh layout remains the cache key; identical masks/sources/layouts reuse programs.
- Different nonzero factor values with the same mask reuse the same program.
- Program/debug labels and errors include the readable specialization summary.
- Uniform collections contain only active canonical runtime inputs plus declared active extensions.
- Enabled feature factors may be overridden to zero per instance.
- Specialized-out factors, alpha mode, double-sided state, and texture maps cannot be overridden per instance.

## 7. Custom program ownership

- Built-in fragment source is specialized by the engine.
- Custom fragment source owned by a PBR material (file, string source, or embedded child program) receives the define block before compilation.
- Referenced existing `Program` resources are reflection-validated only.
- One custom source can serve many variants by using the specialization defines; generated-source caching deduplicates equal variants.

## 8. Legacy compatibility

- Converted RSER PBR streams receive a temporary, non-serialized `LegacyFullContract` marker.
- Legacy programs retain the old complete canonical interface and receive no specialization benefit.
- Existing deprecation warnings remain.
- Invalid legacy PBR programs still fail rather than downgrading.
- New serializers never emit legacy state. Re-authoring/re-exporting from typed source adopts specialization.

## 9. DemoSuite migration

- Keep the metallic feature enabled at material level so the instance slider can range from zero upward; apply the initial zero as an instance override.
- Convert the custom statue shader to the shared specialization contract.
- Keep a meaningful active `PBR_EXT_*` example.
- Re-export the typed RSE3 statue model with matching binaries.
- Include feature summaries in RenderDoc/debug labels.

## 10. Validation

1. Unit tests for deterministic mask derivation and readable names.
2. Generated-source tests for conditional declarations/code.
3. Reflection tests proving specialized-out inputs are inactive.
4. Cache tests for equal masks, different masks, and equal masks with different nonzero values.
5. Exact custom-contract success and missing/unexpected/wrong-type/wrong-target failures.
6. Per-instance enabled-to-zero success and disabled-feature override failure.
7. GPU smoke variants for minimal, fully featured, mask, blend, and double-sided materials.
8. Typed binary round trips and legacy full-contract migration tests.
9. DemoSuite custom statue/model validation and RenderDoc labels.
10. Existing material, shadow, bloom, and render-graph suites remain green.

## 11. Implementation phases

1. **Feature foundation** — **Complete**
   - [x] Added public feature mask, deterministic derivation, readable summaries, source defines, and context-free tests.
   - [x] Added a non-serialized legacy compatibility marker and binary migration assertions.
2. **Source specialization** — **Complete**
   - [x] Generate/inject stable defines into built-in and material-owned custom fragment sources.
   - [x] Converted built-in fragment declarations and calculations to static feature branches.
   - [x] Active linked sampler metadata now drives bindings after preprocessing/linking.
3. **Material integration and exact validation** — **Complete**
   - [x] Material creation validates selected data, derives the mask, then selects/compiles a variant.
   - [x] Existing source/mesh cache reuses equal variants and separates different masks.
   - [x] Exact enabled/missing and disabled/unexpected interfaces plus one-way instance overrides are enforced.
   - [x] Referenced programs remain validation-only; legacy programs retain temporary full-contract behavior.
4. **DemoSuite and GPU validation** — **Complete**
   - [x] Migrated the custom statue shader/material, retained an active extension example, and re-exported the RSE3 model.
   - [x] Added minimal/full/mask/blend/double-sided compilation, reflection, cache, custom-contract, and instance-boundary startup cases.
   - [x] Validated custom rendering and existing manual/graph/XML graph material, shadow, bloom, and render-graph startup suites.
5. **Comprehensive documentation** — **Complete**
   - [x] Added the normative specialization guide with semantics, interface table, define contract, source ownership, instance restrictions, caching, migration, diagnostics, RenderDoc, and examples.
   - [x] Updated PBR setup, authoring, and validation guides.
   - [x] Documented temporary legacy compatibility and adoption/removal guidance.
6. **Validation hardening** — **Complete**
   - [x] Added real-context source-owned custom shader failures for an enabled canonical uniform with the wrong reflected GLSL type.
   - [x] Added real-context source-owned custom shader failures for an enabled canonical sampler with the wrong reflected texture target.
   - [x] Kept missing-enabled and unexpected-specialized-out referenced-program checks, completing all four exact custom-contract failure classes from the acceptance suite.
   - [x] Re-ran specialized statue startup, material migration tests, and the render-graph GPU suite.
