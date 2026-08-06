# PBR Fragment Shader Specialization

MassivePolyPusher compiles PBR fragment programs on demand from the selected material quality. Features that cannot contribute are removed from the built-in shader, and custom shaders are validated against the same exact material contract.

## Material contract versus instance values

Material-level semantic values define the **maximum compiled feature set**. A zero material weight means that feature is unavailable and its shader code, uniform, and sampler are omitted where possible. An instance may reduce an enabled feature to zero, but it cannot enable a feature absent from the material contract.

For a feature that should start at zero but later vary per instance, author a nonzero material value and apply zero as the initial instance override. DemoSuite does this for metallic: the statue material enables metallic with a material factor of one while its initial instance factor is zero.

Texture presence, alpha mode, and double-sided state are material-owned. Changing them requires another material resource/quality and therefore another deterministic variant.

## Derived features

The specialization mask is derived rather than serialized.

| Feature | Enabled when | Active material interface |
|---|---|---|
| Base-colour texture | `BaseColourMap` is declared | `PBR_BASE_COLOUR_MAP` |
| Metallic | `metallicFactor > 0` | `PBR_METALLIC_FACTOR` |
| Roughness | `roughnessFactor > 0` | `PBR_ROUGHNESS_FACTOR` |
| Metallic-roughness texture | map declared and metallic or roughness enabled | `PBR_METALLIC_ROUGHNESS_MAP` |
| Normal mapping | normal map declared and `normalScale > 0` | `PBR_NORMAL_MAP`, `PBR_NORMAL_SCALE` |
| Occlusion | occlusion map declared and `occlusionStrength > 0` | `PBR_OCCLUSION_MAP`, `PBR_OCCLUSION_STRENGTH` |
| Emissive | emissive map declared and any emissive factor component is positive | `PBR_EMISSIVE_MAP`, `PBR_EMISSIVE_FACTOR` |
| Alpha mask | `alphaMode = MASK` | `PBR_ALPHA_CUTOFF` and static discard code |
| Alpha blend | `alphaMode = BLEND` | static authored-alpha output |
| Double-sided | `doubleSided = true` | static back-face normal correction |

`PBR_BASE_COLOUR_FACTOR`, `PBR_IRRADIANCE_MAP`, `PBR_PREFILTERED_SPECULAR_MAP`, `PBR_BRDF_LUT`, and fragment output location 0 are always required. `PBR_ALPHA_MODE` and `PBR_DOUBLE_SIDED` are not runtime uniforms in specialized programs.

Direct-light evaluation, IBL, pipeline shadow capability, and bloom/MRT policy remain runtime pipeline concerns. `SHADOW_MAP` and fragment output location 1 are optional custom-program capabilities rather than material-owned interface requirements.

## Public introspection

`PbrMaterial::getFeatures()` returns the derived `PbrMaterialFeatures` mask. Test a bit with `hasPbrFeature()`. `PbrMaterial::getFeatureSummary()` returns a stable readable summary such as:

```text
BaseColourMap|Metallic|Roughness|MetallicRoughnessMap|NormalMap|Occlusion
```

`Minimal` represents no optional material features. Program OpenGL labels include this summary for RenderDoc and other KHR_debug tools.

## Built-in variants

An omitted PBR program selects the engine-owned shader. Before parser/build, the engine injects specialization values after `@@Version`. GLSL preprocessing removes disabled declarations and calculations. Program reflection then retains only linked active samplers.

Variants compile synchronously on first resource creation. The normal generated-source plus mesh-layout cache deduplicates them:

- equal source, mesh layout, and feature mask reuse one program;
- different masks produce different programs;
- different nonzero factor values with the same mask reuse one program because those factors remain uniforms.

There is no generic-shader fallback after compilation or validation failure.

## Custom shader define contract

Material-owned custom fragment sources receive these numeric `0`/`1` definitions:

```glsl
#define PBR_SPEC_LEGACY_FULL_CONTRACT 0
#define PBR_SPEC_BASE_COLOUR_MAP 1
#define PBR_SPEC_METALLIC 1
#define PBR_SPEC_ROUGHNESS 1
#define PBR_SPEC_METALLIC_ROUGHNESS_MAP 1
#define PBR_SPEC_NORMAL_MAP 1
#define PBR_SPEC_OCCLUSION 1
#define PBR_SPEC_EMISSIVE 0
#define PBR_SPEC_ALPHA_MASK 0
#define PBR_SPEC_ALPHA_BLEND 0
#define PBR_SPEC_DOUBLE_SIDED 0
```

Use them around declarations and implementation code:

```glsl
@@Version

@@Uniform(vec4 PBR_BASE_COLOUR_FACTOR);
#if PBR_SPEC_NORMAL_MAP
@@Uniform(float PBR_NORMAL_SCALE);
@@Texture(sampler2D PBR_NORMAL_MAP);
#endif
#if PBR_SPEC_ALPHA_MASK
@@Uniform(float PBR_ALPHA_CUTOFF);
#endif

void main()
{
    vec4 baseColour = @Uniform(PBR_BASE_COLOUR_FACTOR);
#if PBR_SPEC_ALPHA_MASK
    if (baseColour.a < @Uniform(PBR_ALPHA_CUTOFF)) discard;
#endif
    // ...
}
```

The MPP parser may initially discover declarations in preprocessor branches, but only samplers and uniforms active after OpenGL linking form the reflected material interface.

### Source ownership

- Built-in source is always specialized.
- Fragment source owned by the PBR material—file source, string source, or embedded child program—receives the definitions.
- A referenced existing `Program` resource is already compiled and is never modified. Its active reflected interface must already match the selected material exactly.

This allows one source file to support many variants while still allowing a deliberately fixed external program.

## Exact custom-program validation

For new typed materials, every enabled interface member in the table is required with its exact GLSL type/target. Any active material-owned canonical member belonging to a disabled feature is rejected. Extension rules remain exact:

- names use `PBR_EXT_*`;
- declarations and active reflection are complete in both directions;
- scalar/vector types and 2D/cube sampler targets match;
- extension textures have no neutral fallback.

An inactive GLSL declaration optimized out by the linker is not part of the reflected interface. Missing enabled members, unexpected disabled members, wrong types, wrong sampler targets, and undeclared active extensions fail material creation with the material name and contract diagnostic.

## Per-instance restrictions

`MeshInstance::setUniformCollection()` validates overrides against the material's active uniform collection and resolved program.

Allowed:

- changing `PBR_BASE_COLOUR_FACTOR`;
- changing an enabled metallic, roughness, normal, occlusion, emissive, or mask factor;
- setting an enabled factor to zero;
- changing a declared active extension uniform.

Rejected:

- any specialized-out factor;
- `PBR_ALPHA_MODE` or `PBR_DOUBLE_SIDED`;
- undeclared or mistyped extension values;
- texture changes through instance uniforms.

Use separate material resources when instances need different specialization boundaries.

## Alpha, shadows, and bloom

Alpha mode is static because it also controls CPU render classification and fixed-function blend/depth behavior. Opaque and mask variants output alpha one; blend variants output authored base alpha. Mask variants alone expose the cutoff uniform.

Shadow evaluation remains present as a pipeline capability. A custom shader may expose `SHADOW_MAP`; the active shadow domain overrides it, while the ordinary fallback keeps non-shadow use valid.

The built-in shader keeps fragment output location 1 and writes zero emissive when emissive is disabled, preserving graph MRT eligibility. Custom location 1 remains optional; graph bloom falls back to threshold extraction if any visible program lacks the required output.

## Legacy assets

Converted RSER PBR streams receive a runtime-only `LegacyFullContract` marker. They retain the historical complete interface, receive no specialization benefit, and emit a deprecation warning. The marker is not serialized by new RSE2 output.

Invalid legacy programs still fail. To adopt specialization, update the typed source material/custom shader and re-export it with matching converter/parser/engine binaries. A binary deserialize/rewrite cannot automatically make a full custom shader obey a feature-dependent source contract.

## Diagnostics and RenderDoc

When investigating a variant:

1. Inspect `PbrMaterial::getFeatureSummary()` or the material/program log.
2. In RenderDoc, find the program label `PBR [<features>]: <program>`.
3. Inspect active uniforms and samplers; specialized-out inputs should be absent.
4. Confirm materials with equal masks and mesh layouts share the same program object.
5. Treat a missing/unexpected-interface error as an authoring error; the engine intentionally does not fall back.

DemoSuite startup compiles minimal, full mask/double-sided, and blend built-in variants; verifies reflection and cache reuse across different nonzero values; validates enabled-to-zero and disabled-feature instance behavior; checks missing/unexpected referenced custom contracts plus wrong uniform-type and sampler-target source-owned custom contracts; loads the specialized custom statue; and then runs existing material and render-graph GPU suites.
