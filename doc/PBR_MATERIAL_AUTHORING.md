# PBR Material Authoring Workflow

This guide is the practical workflow for authoring a metallic-roughness material for MassivePolyPusher. It covers both model-specification and programmatic materials, source-map preparation, image-based lighting (IBL), export, and current limitations.

For the complete current shader and pipeline contract, see [PBR Material Setup](PBR_MATERIAL_SETUP.md). The statue at `demo-suite/resources/res/statue/statue.modelspec.xml` is the runnable reference.

## 1. Prepare the mesh and shaders

1. Export a mesh with positions, normals, and UV0.
2. Add `tangent4` to its `MeshSpecification`. `ModelConvert` requests Assimp tangent generation when this channel is present.
3. Use a PBR vertex shader that consumes `TANGENT` and a PBR fragment shader that declares the PBR sampler/uniform names. The statue shaders are the reference.
4. Put the material on a separate `PbrForward` pipeline; `Default` is the preserved legacy forward/Phong pipeline.

Normal mapping will not be correct without a tangent frame. The tangent `.w` stores handedness.

## 2. Create surface maps

Use a material authoring package, scanner/photogrammetry workflow, or bake from a high-poly source (for example Blender, Substance 3D Painter/Designer, or Material Maker). Produce matching-resolution images with the same UV layout.

| Map | Required data | Source/authoring workflow | Import colour space |
|---|---|---|---|
| Base colour | RGB albedo; optional alpha | Remove baked lighting from photographs; paint or export base colour | `SRGB` |
| Metallic-roughness | G roughness, B metallic; R is unused by the shader | Pack roughness and metallic into a single image; glTF-style ORM packing is suitable | `LINEAR` |
| Normal | tangent-space normal RGB | Bake high-poly to low-poly or export the material-tool normal map | `LINEAR` |
| Occlusion | R ambient occlusion | Bake AO or use the AO channel from an ORM texture | `LINEAR` |
| Emissive | RGB emitted colour | Paint/export only where the material emits light | `SRGB` |

For an ORM map, pack **R = AO**, **G = roughness**, and **B = metallic**. The renderer reads G/B from `PBR_METALLIC_ROUGHNESS_MAP` and R from `PBR_OCCLUSION_MAP`; bind the same ORM file to both samplers if it contains all three channels.

Do not gamma-encode normal, AO, metallic, or roughness data. Do not use a height map directly: it requires a parallax/displacement feature, which is not part of the current PBR shader.

A factor-only PBR material is supported. Omitted surface maps receive neutral fallback textures, but a normal map still requires tangent geometry when used.

## 3. Author the material in a ModelSpec

Add a `Pbr` block and texture bindings inside the material `Resource`. File names in child `Resource` blocks are relative to the `.modelspec.xml` file. `Ref` names identify resources declared by the application.

```xml
<Resource>
    <Program>
        <Resource>
            <positionType>3D</positionType>
            <VertexShader><file>statue_pbr.vert</file></VertexShader>
            <FragmentShader><file>statue_pbr.frag</file></FragmentShader>
        </Resource>
    </Program>

    <Pbr>
        <!-- Keep RGB/alpha at one to show the authored albedo un-tinted. -->
        <baseColourFactor>1.0 1.0 1.0 1.0</baseColourFactor>
        <!-- Stone/non-metal: zero. Factors multiply texture values. -->
        <metallicFactor>0.0</metallicFactor>
        <roughnessFactor>1.0</roughnessFactor>
        <emissiveFactor>0.0 0.0 0.0</emissiveFactor>
        <normalScale>1.0</normalScale>
        <occlusionStrength>1.0</occlusionStrength>
        <alphaMode>OPAQUE</alphaMode>
        <alphaCutoff>0.5</alphaCutoff>
        <doubleSided>false</doubleSided>
    </Pbr>

    <Textures>
        <Texture>
            <Variable>PBR_BASE_COLOUR_MAP</Variable>
            <Resource>
                <target>2D</target>
                <filename>statue_albedo.png</filename>
                <colourSpace>SRGB</colourSpace>
                <minFilter>LINEAR_MIPMAP_LINEAR</minFilter>
                <magFilter>LINEAR</magFilter>
            </Resource>
        </Texture>
        <Texture>
            <Variable>PBR_METALLIC_ROUGHNESS_MAP</Variable>
            <Resource>
                <target>2D</target>
                <filename>statue_orm.png</filename>
                <colourSpace>LINEAR</colourSpace>
                <minFilter>LINEAR_MIPMAP_LINEAR</minFilter>
                <magFilter>LINEAR</magFilter>
            </Resource>
        </Texture>
        <Texture>
            <Variable>PBR_NORMAL_MAP</Variable>
            <Resource>
                <target>2D</target>
                <filename>statue_normal.png</filename>
                <colourSpace>LINEAR</colourSpace>
                <minFilter>LINEAR_MIPMAP_LINEAR</minFilter>
                <magFilter>LINEAR</magFilter>
            </Resource>
        </Texture>
        <Texture>
            <Variable>PBR_OCCLUSION_MAP</Variable>
            <Resource>
                <target>2D</target>
                <filename>statue_orm.png</filename>
                <colourSpace>LINEAR</colourSpace>
                <minFilter>LINEAR_MIPMAP_LINEAR</minFilter>
                <magFilter>LINEAR</magFilter>
            </Resource>
        </Texture>
        <Texture>
            <Variable>PBR_EMISSIVE_MAP</Variable>
            <Resource>
                <target>2D</target>
                <filename>statue_emissive.png</filename>
                <colourSpace>SRGB</colourSpace>
                <minFilter>LINEAR_MIPMAP_LINEAR</minFilter>
                <magFilter>LINEAR</magFilter>
            </Resource>
        </Texture>
    </Textures>
</Resource>
```

The supported sampler names are exactly:

- `PBR_BASE_COLOUR_MAP`
- `PBR_METALLIC_ROUGHNESS_MAP`
- `PBR_NORMAL_MAP`
- `PBR_OCCLUSION_MAP`
- `PBR_EMISSIVE_MAP`
- `PBR_IRRADIANCE_MAP`
- `PBR_PREFILTERED_SPECULAR_MAP`
- `PBR_BRDF_LUT`

`OPAQUE` and `MASK` write opaque output alpha; `MASK` discards fragments below `alphaCutoff`. `BLEND` retains authored alpha and is rendered after PBR opaque/masked geometry with depth writes disabled.

## 4. Create a material programmatically

First declare/load texture resources by the names that the material will reference. A `ProgrammaticTextureStream` can create a texture resource from an image loader; configure its `TextureColourSpace`, target, filtering, and wrapping before declaration. The material stream then references those resource names with `setTexture`.

```cpp
mpp::MaterialSpecification::PbrSurface surface;
surface.enabled = true;
surface.baseColourFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
surface.metallicFactor = 0.0f;
surface.roughnessFactor = 1.0f;
surface.normalScale = 1.0f;
surface.occlusionStrength = 1.0f;
surface.alphaMode = mpp::MaterialSpecification::PbrAlphaMode::Opaque;

// texture resources must already exist, for example "Statue.Albedo".
auto stream = new mpp::ProgrammaticMaterialStream(resourceMgr);
stream->setProgram2d(false);
stream->setMeshSpecification(statueMeshSpecification); // includes tangent4
stream->setProgramVertexShaderFile("res/statue/statue_pbr.vert");
stream->setProgramFragmentShaderFile("res/statue/statue_pbr.frag");
stream->setPbrSurface(surface);
stream->setTexture("PBR_BASE_COLOUR_MAP", "Statue.Albedo");
stream->setTexture("PBR_METALLIC_ROUGHNESS_MAP", "Statue.Orm");
stream->setTexture("PBR_NORMAL_MAP", "Statue.Normal");
stream->setTexture("PBR_OCCLUSION_MAP", "Statue.Orm");
stream->setTexture("PBR_EMISSIVE_MAP", "Statue.Emissive");

auto material = resourceMgr->declareResource(
    "Statue.PbrMaterial", mpp::ResourceStreamPtr(stream)).first;
```

Use `setTextureChild(sampler, resourceName)` only when the named texture resource is to be a child of that material. Use `setDefaultTexture(sampler)` to request a neutral default texture. The material must be loaded/acquired through the normal resource lifecycle before rendering.

IBL is normally pipeline-owned rather than repeated in every programmatic material. Create a `PbrEnvironment`, set it on `RenderPipelineOptions::environment` when creating the `PbrForward` pipeline, and provide the three resources below:

```cpp
auto environment = std::make_shared<mpp::PbrEnvironment>();
environment->irradianceMap = resourceMgr->getResource("Environment.Irradiance");
environment->prefilteredSpecularMap = resourceMgr->getResource("Environment.Specular");
environment->brdfIntegrationLut = resourceMgr->getResource("Environment.BrdfLut");
environment->backgroundMap = resourceMgr->getResource("Environment.Background"); // optional

mpp::RenderPipelineOptions options;
options.mode = mpp::RenderPipelineMode::PbrForward;
options.environment = environment;
renderSystem->getOrCreateRenderPipeline("PBR", options);
```

During a PBR scene flush, this active environment overrides material bindings for the irradiance, prefiltered-specular, and BRDF-LUT sampler names. ModelSpec `Ref` bindings remain useful as declared fallback/resource dependencies.

## 5. Generate HDR IBL assets

Start with a linear, high-dynamic-range equirectangular panorama (`.hdr` or `.exr`). It must represent scene radiance; do not apply sRGB conversion or tone mapping. Use an IBL preprocessor such as cmftStudio, Filament `cmgen`, Blender, or an equivalent offline tool.

Generate all outputs from the **same** source panorama:

1. Convert the panorama to an unfiltered cubemap for `backgroundMap`. This is for a future skybox/background pass and does not light materials directly.
2. Diffuse/cosine-convolve the panorama into a low-frequency cubemap for `irradianceMap`. 32–64 pixels per face is commonly sufficient.
3. GGX-prefilter the panorama into a cubemap with a complete roughness mip chain for `prefilteredSpecularMap`. Mip 0 is sharp; increasing mip levels are blurrier and correspond to increasing roughness. A 128–512 pixel base face is a common starting point.
4. Generate a linear two-channel BRDF integration LUT for `brdfIntegrationLut`; `RG16F` or `RG32F`, 256×256 or 512×512 are typical. Its lookup coordinates are `NdotV` and roughness.

The irradiance, prefiltered specular, and BRDF LUT are linear floating-point inputs. The shader samples the irradiance map using the normal and samples the prefiltered map using the reflection vector and roughness-selected mip. The background map should visually match the lighting maps but does not contribute lighting by itself.

## 6. Export and validate ModelSpec assets

After changing the OBJ, ModelSpec, shader file, or any child texture definition, regenerate the model:

```bat
cd demo-suite\resources\res\statue
..\..\..\..\model-convert\build\vs2026\bin\x64\Release\ModelConvert.exe statue.obj -s statue.modelspec.xml
```

Build MassivePolyPusher, ModelConvert, and DemoSuite in the same Debug/Release x64 configuration first. Place the matching MassivePolyPusher DLL beside `DemoSuite.exe`. Then follow [PBR DemoSuite Validation](PBR_VALIDATION.md).

## Current limitations

- HDR panorama decoding and GPU IBL preprocessing are not implemented. HDR IBL maps must be created offline.
- There is no built-in asset importer for six HDR cubemap faces plus authored prefiltered mip levels. `Texture::uploadCubeFace()` uploads base-level faces; generated OpenGL mipmaps are not a substitute for GGX-prefiltered mips. A KTX2/DDS or six-face/mip importer, and mip-face upload support, are still required for physically correct imported IBL.
- `backgroundMap` is stored in `PbrEnvironment` but DemoSuite has no skybox/background render pass yet.
- The DemoSuite cool/warm cube maps and BRDF LUT are placeholders, not physically correct HDR IBL assets.
- PBR blend support is conventional source-alpha transparency only: it has no refraction, order-independent transparency, or per-triangle sorting.
- The standard PBR shader uses five surface maps plus three IBL maps (eight samplers). The GPU must expose at least that many fragment texture units.
- Dedicated PBR regression assets (sphere grid, emissive, mask, blend, and normal-map demonstrations) and captured reference images are still outstanding.
