# Preview Scene XML Specification

## Scope

A preview scene is reusable and independent of a pipeline. Version 1 supports one camera, absolute model transforms, declared render layers, logical material/environment bindings, primitives or `.mppmodel` files, and at most eight PBR lights. It does not support transform hierarchies, gizmos, picking, or asset packaging.

Core fields and values are strict. Paths are relative to the scene document and should remain portable.

## Structure

```xml
<Scene>
  <version>1</version>
  <name>Preview</name>
  <environmentBinding>Preview.Environment</environmentBinding>
  <Camera><position>0 4 10</position><target>0 1 0</target>
    <fov>55</fov><near>0.1</near><far>1000</far></Camera>
  <Layers><Layer>Default</Layer></Layers>
  <Models>...</Models>
  <Lights>...</Lights>
</Scene>
```

`position`, `target`, translation, rotation, scale, directions, positions, and colours are whitespace-separated vectors. Rotation is absolute XYZ degrees. Camera near distance must be positive and far must be greater than near.

## Models

```xml
<Model>
  <id>Object</id><source>sphere</source>
  <Primitive><radius>1</radius><resolution>3</resolution></Primitive>
  <translation>0 1 0</translation><rotation>0 0 0</rotation><scale>1 1 1</scale>
  <materialBinding>Preview.Main</materialBinding>
  <visible>true</visible><shadowCaster>true</shadowCaster>
  <Layers><Layer>Default</Layer></Layers>
</Model>
```

IDs are unique across models and lights. Sources are `mppmodel`, `box`, `sphere`, `cylinder`, and `grid`. An `mppmodel` requires `file`. Primitive parameters are:

- Box: positive `width`, `height`, and `depth`.
- Sphere: positive `radius`; `resolution` is at most 8.
- Cylinder: positive `height`, non-negative `radius`/`topRadius` with at least one non-zero, and `resolution` at least 3.
- Grid: positive `width`/`depth`, positive `segmentsX`/`segmentsZ`, and optional texture repeats.

Scale components cannot be zero. Missing or failed model files create diagnosed placeholder boxes. Placeholders are excluded from authored and runtime triangle totals.

Every referenced layer must appear in top-level `Layers`. Legacy scenes without `Layers` infer declarations and emit migration warning `MPP-SCENE-030` when parsed through the compatibility path.

## Lights

```xml
<Light><id>Key</id><type>directional</type><colour>1 1 1</colour>
  <intensity>4</intensity><direction>-0.4 -1 -0.3</direction>
  <castsShadows>true</castsShadows></Light>
```

Types are `directional` and `point`. Point lights use `position` and a positive `range`; directional lights require a non-zero direction. Intensity and colour components are non-negative. A scene permits exactly one `castsShadows: true` light, directional or point. Scene runtime configures the named domain from that light and records its exact zero-based light-array index, so visibility affects only that direct-light contribution. Scene-authored lights are opt-in ownership—scenes without authored lights retain host-managed compatibility lighting.

## Binding rules

`materialBinding` resolves through the active pipeline's `PreviewBindings`. `environmentBinding` must match the active pipeline environment binding. Unresolved bindings use diagnosed neutral fallback material/environment resources; required pipeline resources still block installation.

The canonical scene template is `resources/shared/pbr/DefaultPbrPreview.scene.xml`.
