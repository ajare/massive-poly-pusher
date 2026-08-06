# PBR Material Setup

This guide describes the PBR material path implemented through Milestones 1–5. It is intentionally scoped to the current forward PBR implementation; deferred work and known limitations are listed at the end.

The runnable reference is:

- specification: `demo-suite/resources/res/statue/statue.modelspec.xml`
- shaders: `demo-suite/resources/res/statue/statue_pbr.vert` and `statue_pbr.frag`
- exported model: `demo-suite/resources/res/statue/statue.mppmodel`

Use [PBR Material Authoring Workflow](PBR_MATERIAL_AUTHORING.md) to create ModelSpec or programmatic materials, [Generic Soft Shadow Setup](SHADOW_SETUP.md) to opt a PBR pipeline into shadows, then use [PBR DemoSuite Validation](PBR_VALIDATION.md) to exercise and capture the current PBR controls.

## 1. Opt in to the PBR pipeline

The existing `Default` pipeline remains the legacy forward/Phong path. Create a separate pipeline with `PbrForward` and render the PBR scene through its name:

```cpp
mpp::RenderPipelineOptions options;
options.mode = mpp::RenderPipelineMode::PbrForward;
options.exposure = 1.0f;
options.toneMapOperator = mpp::PbrToneMapOperator::Aces;

renderSystem->getOrCreateRenderPipeline("PBR", options);
renderSystem->renderScene(scene, camera, {}, "PBR");
```

A PBR pipeline owns an RGBA16F scene target. Surface lighting stays linear in this target. Presentation applies the selected ACES or Reinhard tone map and one final gamma encode. `RenderPipeline::setExposure()` and `setToneMapOperator()` can be used at runtime. DemoSuite exposes `PBR` and `Default` in its **Render Pipeline** combo; `Default` remains the legacy path. See [Post Effects and Bloom](POST_EFFECTS_AND_BLOOM.md) for pipeline bloom and the post-effect-system roadmap.

## 2. Supply PBR lights and an environment

PBR lights are separate from the legacy two-light API. The current UBO supports at most eight directional or point lights.

```cpp
mpp::PbrLight light;
light.type = mpp::PbrLightType::Point;
light.position = { 0.0f, 256.0f, 256.0f };
light.colour = { 1.0f, 1.0f, 1.0f };
light.intensity = 120000.0f;
light.range = 1200.0f; // zero means unlimited

renderSystem->setPbrAmbientColour(mpp::Colour(0.03f, 0.03f, 0.03f));
renderSystem->setPbrLights({ light });
```

IBL resources are caller-owned `ResourcePtr`s:

```cpp
auto environment = std::make_shared<mpp::PbrEnvironment>();
environment->irradianceMap = resourceMgr->getResource("MyEnvironment.Irradiance");
environment->prefilteredSpecularMap = resourceMgr->getResource("MyEnvironment.Prefiltered");
environment->brdfIntegrationLut = resourceMgr->getResource("MyEnvironment.BrdfLut");
environment->backgroundMap = resourceMgr->getResource("MyEnvironment.Background"); // optional
options.environment = environment;
```

The material binds the first three resources through the sampler names in [Environment slots](#environment-slots). They must be declared and loadable before the model material loads.

## 3. Geometry requirements

A normal-mapped PBR material requires position, normal, tangent, and UV0 data. Declare the tangent as `tangent4`; its `.w` is the tangent-frame handedness.

```xml
<MeshSpecification>
    <primitive>triangles</primitive>
    <indexed>true</indexed>
    <storage>static</storage>
    <Buffer>
        <Channel><data>position3</data><type>float32</type></Channel>
        <Channel><data>normal3</data><type>float32</type></Channel>
        <Channel><data>tangent4</data><type>float32</type></Channel>
        <Channel><data>texcoord2</data><type>float16</type></Channel>
        <Channel><data>colour4</data><type>uint8</type><normalised>true</normalised></Channel>
    </Buffer>
</MeshSpecification>
```

`ModelConvert` requests Assimp tangent generation when `tangent4` is present. The input mesh must therefore have usable positions, normals, and UV0. Conversion fails if Assimp cannot produce a tangent frame.

The PBR vertex program must consume `TANGENT` so vertex attribute locations remain aligned. Use `statue_pbr.vert` as the reference implementation.

## 4. Author the PBR surface

Place a `Surface` block inside a typed `PbrMaterial` in the model specification. All factors have defaults, but writing them explicitly makes the asset self-documenting.

```xml
<PbrMaterial>
  <name>Example.Pbr</name>
  <Surface>
    <baseColourFactor>1.0 1.0 1.0 1.0</baseColourFactor>
    <metallicFactor>0.0</metallicFactor>
    <roughnessFactor>0.75</roughnessFactor>
    <emissiveFactor>0.0 0.0 0.0</emissiveFactor>
    <normalScale>1.0</normalScale>
    <occlusionStrength>1.0</occlusionStrength>
    <alphaMode>OPAQUE</alphaMode>
    <alphaCutoff>0.5</alphaCutoff>
    <doubleSided>false</doubleSided>
  </Surface>
</PbrMaterial>
```

Supported alpha modes are `OPAQUE`, `MASK`, and `BLEND`. `MASK` discards fragments below `alphaCutoff`; `doubleSided` flips the shading normal for back faces. For an opaque material use an alpha factor of `1.0`.

## 5. Bind material textures

Surface textures use fixed semantic elements inside `PbrMaterial`. A child `Resource` loads a file relative to the model specification. A `Ref` uses a resource declared by the application.

### Surface slots

| Sampler | Image data | Colour space | Channels used | Fallback when omitted |
|---|---|---|---|---|
| `PBR_BASE_COLOUR_MAP` | base colour/albedo | `SRGB` | RGB and alpha | white |
| `PBR_METALLIC_ROUGHNESS_MAP` | metallic-roughness | `LINEAR` | G = roughness, B = metallic | roughness 1, metallic 1 |
| `PBR_NORMAL_MAP` | tangent-space normal | `LINEAR` | RGB | `(0.5, 0.5, 1.0)` |
| `PBR_OCCLUSION_MAP` | ambient occlusion | `LINEAR` | R | white |
| `PBR_EMISSIVE_MAP` | emissive colour | `SRGB` | RGB | black |

Example:

```xml
<BaseColourMap><Resource>
  <target>2D</target><filename>albedo.png</filename><colourSpace>SRGB</colourSpace>
</Resource></BaseColourMap>
<MetallicRoughnessMap><Resource>
  <target>2D</target><filename>metallic_roughness.png</filename><colourSpace>LINEAR</colourSpace>
</Resource></MetallicRoughnessMap>
<NormalMap><Resource>
  <target>2D</target><filename>normal.png</filename><colourSpace>LINEAR</colourSpace>
</Resource></NormalMap>
```

The `baseColourFactor`, `metallicFactor`, `roughnessFactor`, `emissiveFactor`, `normalScale`, and `occlusionStrength` values multiply or scale the corresponding inputs. Therefore a factor-only material is valid: omit every surface map and rely on the neutral fallback maps.

### Environment slots

| Sampler | Target | Purpose |
|---|---|---|
| `PBR_IRRADIANCE_MAP` | cube map | diffuse irradiance IBL |
| `PBR_PREFILTERED_SPECULAR_MAP` | cube map with roughness mips | specular IBL |
| `PBR_BRDF_LUT` | 2D | split-sum BRDF integration |

Environment resources are pipeline-owned through `PbrEnvironment`, not material texture declarations. Set its irradiance, prefiltered-specular, and BRDF-LUT resources on `RenderPipelineOptions::environment`. Missing environments use neutral engine fallbacks and issue one warning. Use linear data for all three inputs. The renderer accepts precomputed resources; it does not yet convert an HDR panorama into these assets.

## 6. Select PBR shaders

The program must use a PBR vertex and fragment shader that declare the sampler and uniform contract above. The statue material uses:

```xml
<Program>
    <Resource>
        <positionType>3D</positionType>
        <VertexShader><file>statue_pbr.vert</file></VertexShader>
        <FragmentShader><file>statue_pbr.frag</file></FragmentShader>
    </Resource>
</Program>
```

Use those two files as a starting point. `statue_pbr.frag` implements Cook-Torrance direct lighting (GGX, Smith, Schlick Fresnel), normal mapping, AO, emissive, alpha mask, double-sided normals, and split-sum IBL.

## 7. Specialization contract

The selected material quality derives an immutable fragment feature mask from semantic map presence, zero/nonzero factors, alpha mode, and double-sided state. Built-in variants omit disabled inputs/code. Material-owned custom sources receive `PBR_SPEC_*` definitions and must expose the exact active reflected interface; an already-compiled referenced Program is validation-only. Material factors describe maximum capabilities, while instances may only reduce enabled contributions.

See [PBR Fragment Shader Specialization](PBR_SHADER_SPECIALIZATION.md) for the normative feature/interface table, define list, custom source example, caching rules, public introspection API, instance restrictions, legacy behavior, diagnostics, and validation procedure.

## 8. Extension contract

Custom PBR programs may add uniforms and 2D/cube samplers under the `PBR_EXT_` namespace. Extensions are strictly additive: the exact specialization-selected canonical metallic-roughness interface remains mandatory. Material creation verifies namespace, declaration completeness, reflected uniform type/shape, sampler target, and absence of undeclared active extension inputs. There are no extension fallbacks.

Use `<Extensions>` in typed `<PbrMaterial>` XML and `ProgrammaticPbrMaterialStream::setExtensionUniform()` / `setExtensionTexture()` in C++. Canonical factor and declared extension uniforms may be overridden per instance; textures remain material-owned. See [PBR Material Authoring Workflow](PBR_MATERIAL_AUTHORING.md#6-custom-pbr-extensions) for complete XML, API examples, supported value types, and restrictions.

## 9. Export the model

After changing the OBJ, model specification, shaders embedded by the model, or child texture definitions, regenerate the `.mppmodel` using matching binaries:

```bat
cd demo-suite\resources\res\statue
..\..\..\..\model-convert\build\vs2026\bin\x64\Release\ModelConvert.exe statue.obj -s statue.modelspec.xml
```

Build `MassivePolyPusher`, `ModelConvert`, and DemoSuite in the same configuration first. The rebuilt `MassivePolyPusher.dll` must be beside `DemoSuite.exe`.

## Current limitations

- `BLEND` PBR meshes render after opaque/masked meshes, back-to-front, with source-alpha blending and depth writes disabled. It is intended for conventional non-refractive transparency.
- DemoSuite selects `PBR` or `Default` and provides cool/warm placeholder environment controls, but those cube maps and the BRDF LUT are not physically correct HDR IBL assets.
- HDR panorama decoding and GPU IBL preprocessing are not implemented. Supply precomputed environment resources.
- Visual reference captures and full PBR regression assets remain outstanding.
