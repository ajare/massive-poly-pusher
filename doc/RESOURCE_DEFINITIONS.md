# Single-Definition Resources

Every MassivePolyPusher `ResourceStream` describes exactly one resource definition. Embedded resource quality tables, numeric quality IDs, inheritance, and runtime quality switching are not supported.

## Variants

Represent asset variants as separate resources with stable names:

```text
Textures.Statue.Albedo
Textures.Statue.AlbedoHalf
Materials.Statue
Materials.StatueMobile
```

Each variant may use a separate XML file or a separate programmatic stream. Application-level presets select resource names rather than selecting an internal stream quality.

## Programmatic resources

Setters apply directly to the one definition and no longer accept a quality argument:

```cpp
auto texture = std::make_shared<mpp::ProgrammaticTextureStream>(resourceManager);
texture->setTarget(mpp::TextureTarget::Texture2D);
texture->setFile("albedo.png", imageLoader);
resourceManager->declareResource("Textures.Albedo", texture);
```

The same rule applies to Basic/PBR materials, programs, samplers, strings, models, render textures, post effects, and render graphs.

## XML

The root describes the complete definition. `<Quality>` is rejected with a diagnostic instructing the author to split variants:

```xml
<Texture>
  <target>2D</target>
  <filename>albedo.png</filename>
</Texture>
```

A different texture is another resource/XML document. Basic/PBR material program and texture children are likewise unambiguous single definitions.

## Loading

`ResourceStream::load()` and `ResourceManager::declareResource()` have no quality parameter. Child streams load their one definition rather than receiving a parent numeric index.

Resource selection remains creation-time. Replacing a resource variant requires selecting another named resource or destroying/redeclaring it through the normal lifecycle.

## Binary format

New resource streams use `RSE3`, which stores one definition and no quality-name/count table. The compatibility reader accepts `RSE2`/`RSER` resources only when they contain exactly one definition. Multi-definition legacy data fails rather than selecting an arbitrary variant. Re-export source assets to adopt RSE3.

Texture mip levels/LOD, PBR roughness, shader specialization, MSAA, and application graphics presets are unrelated to the removed resource-quality mechanism.
