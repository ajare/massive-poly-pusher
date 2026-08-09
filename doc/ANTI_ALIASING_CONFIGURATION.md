# Anti-aliasing configuration and authoring

MassivePolyPusher composes output anti-aliasing in this fixed order:

1. supersampled graph rasterization;
2. MSAA resolve;
3. TAA;
4. Lanczos SSAA downsample;
5. FXAA;
6. presentation to the screen or offscreen `RenderTexture`.

All techniques are disabled by default.

## Application defaults

PipelineEditor reads `[mpp]` from `editor.ini` beside `PipelineEditor.exe`. DemoSuite reads it from `demosuite.ini` beside `DemoSuite.exe`.

```ini
[mpp]
msaa=off
ssaa=off
taa=false
fxaa=false
```

`msaa` and `ssaa` accept `off`, `2x`, `4x`, or `8x`. `taa` and `fxaa` accept `true` or `false`. Names and values are case-insensitive and surrounding whitespace is ignored. Every key may appear at most once. Unknown keys, duplicate keys, malformed values, unsupported MSAA counts, and startup dimensions that exceed GPU limits are fatal; the applications do not silently downgrade settings.

Applications parse configuration through `mpp::app::loadRenderSystemOptions()` and pass the immutable `RenderSystemOptions` into `RenderSystem`. The renderer does not locate or parse INI files.

## Named pipeline outputs

A PBR pipeline declares one or more named outputs. Omitted settings inherit the application defaults:

```xml
<Outputs>
  <Output name="Main" image="Presentation" taaDepth="SceneDepth">
    <AntiAliasing>
      <msaa>inherit</msaa>
      <ssaa>inherit</ssaa>
      <taa>inherit</taa>
      <fxaa>inherit</fxaa>
    </AntiAliasing>
  </Output>
</Outputs>
```

Output values accept `inherit` in addition to the corresponding global values. An explicit `off` or `false` overrides an enabled global default. In PipelineEditor, the toolbar's **Global AA** combos initially resolve inherited values from `editor.ini`, omit `Inherit`, and write a concrete selection to all named presentation outputs. To retain or author inheritance per output, select a presentation image under **Pipeline Hierarchy > Images** and use its Inspector combos. A change immediately regenerates the preview and participates in undo/redo. Outputs in one pipeline must resolve to identical MSAA, SSAA, and TAA settings because they share rasterization, dimensions, and jitter, so the editor propagates those three selections to all outputs. FXAA may vary per output.

Legacy graph-image `<samples>` elements and public render-texture sample settings are rejected. Move multisampling to the named output's `<AntiAliasing>` block.

## Technique requirements

- **MSAA** uses private 2×/4×/8× colour and depth attachments. Stored attachments resolve before graph sampling and output processing. Multisampled graph attachments cannot have mip chains.
- **SSAA** values are total sample counts. They use approximately √2, 2, and √8 linear scaling with deterministic upward rounding. Only viewport-relative graph images scale; absolute resources such as shadow maps retain their authored size.
- **TAA** is available only to render-graph pipelines. It needs a matching sampled, stored depth source through `taaDepth`, or a compatible external output depth texture. Applications should call `Camera::markCut()` after intentional camera teleports. Resize, regeneration, setting changes, cuts, discontinuities, and skipped output frames invalidate history.
- **FXAA** is the final logical-resolution pass and accepts only `RGBA8`, `SRGB8_ALPHA8`, or `RGB10_A2` colour outputs. Its quality preset is fixed.

## Errors and transactional behavior

Pipeline/output creation and resizing validate effective settings against the active GPU before replacing the current generation. Allocation or validation failure retains the previous valid output resources.

PipelineEditor shows the first blocking diagnostic in the stale-preview banner, keeps all diagnostics in the Diagnostics window, and reports retained-resource resize failures in the viewport. DemoSuite writes fatal configuration/package/rendering errors to stderr and its log, and shows an error dialog when launched without a console.

Package export serializes the named output overrides in `pipeline.xml`. Package loading applies the receiving DemoSuite `demosuite.ini` defaults to fields that remain `inherit`; use explicit values when package rendering must not depend on host defaults.

## Practical combinations

- `msaa=4x`, others off: conventional geometry-edge smoothing at moderate cost.
- `taa=true`, `fxaa=true`: temporal stability plus final residual-edge smoothing.
- `ssaa=2x`, `taa=true`: TAA at supersampled resolution followed by Lanczos downsampling.
- All enabled: highest cost; processing still follows the fixed order above.

Memory and raster cost grow with MSAA sample count and SSAA dimensions. Unsupported requests fail rather than falling back.
