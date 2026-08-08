# MassivePolyPusher Anti-Aliasing Implementation Plan

## Implementation status

- **Phase 1 complete** on `feature/anti-aliasing-phase-1`: core anti-aliasing types, strict shared INI parsing, immutable `RenderSystemOptions`, startup capability validation/reporting, PipelineEditor `editor.ini` integration, and deployed DemoSuite `demosuite.ini` integration.
- **Phase 2 complete**: shared programmatic/XML named-output descriptors, strict output validation and inheritance checks, XML round trips, legacy sample-authoring removal, migration diagnostics, and repository pipeline/document migration.
- Phase 1/2 validation passed in Debug and Release: PipelineEditor smoke tests, DemoSuite parser/inheritance/output tests, non-default option propagation, legacy `<samples>` rejection, and package export/load smoke testing.
- Rendering remains unchanged and disabled by default; physical output processing begins in phase 3.

## 1. Scope

Add composable output anti-aliasing to MassivePolyPusher:

- MSAA: off, 2x, 4x, or 8x.
- SSAA: off, 2x, 4x, or 8x total pixel samples.
- TAA: fixed-quality camera-jitter implementation.
- FXAA: fixed high-quality preset.
- FSAA is not a separate option.

The supported processing order is:

1. Supersampled rasterization.
2. MSAA resolve.
3. TAA.
4. Lanczos SSAA downsample.
5. FXAA.
6. Final output.

All techniques are off by default. Alpha is preserved throughout the chain.

## 2. Configuration and API

### 2.1 Global defaults

Applications parse their INI and pass typed settings to `RenderSystem`; the renderer does not read files itself.

```ini
[mpp]
msaa=off
ssaa=off
taa=false
fxaa=false
```

Rules:

- Section and values are case-insensitive and whitespace-tolerant.
- `msaa` and `ssaa` accept only `off`, `2x`, `4x`, and `8x`.
- `taa` and `fxaa` accept booleans.
- Unknown `[mpp]` keys and invalid values are fatal configuration errors.
- Invalid or unsupported settings never silently fall back.

Add typed public structures along these lines:

```cpp
enum class AntiAliasingSamples { Off, X2, X4, X8 };

struct AntiAliasingDefaults
{
    AntiAliasingSamples msaa{AntiAliasingSamples::Off};
    AntiAliasingSamples ssaa{AntiAliasingSamples::Off};
    bool taa{false};
    bool fxaa{false};
};

struct AntiAliasingOverrides
{
    std::optional<AntiAliasingSamples> msaa;
    std::optional<AntiAliasingSamples> ssaa;
    std::optional<bool> taa;
    std::optional<bool> fxaa;
};

struct RenderSystemOptions
{
    AntiAliasingDefaults antiAliasing;
};
```

An absent override means inherit. An explicit `off` or `false` disables an enabled global default.

`RenderSystem::getCaps()` will expose supported MSAA counts, maximum texture dimensions, and relevant anti-aliasing limits.

### 2.2 Application files

- PipelineEditor loads `[mpp]` from `editor.ini` beside `PipelineEditor.exe`.
- DemoSuite gains `demosuite.ini` beside `DemoSuite.exe`.
- Deployment scripts preserve existing user configuration where appropriate.
- Both default files contain all four settings with anti-aliasing off.
- Global settings are immutable for a running application and require restart.

### 2.3 Per-output configuration

Programmatic and XML pipelines use named outputs. XML shape:

```xml
<Outputs>
  <Output>
    <name>Main</name>
    <image>Presentation</image>
    <taaDepth>SceneDepth</taaDepth>
    <AntiAliasing>
      <msaa>inherit</msaa>
      <ssaa>inherit</ssaa>
      <taa>inherit</taa>
      <fxaa>inherit</fxaa>
    </AntiAliasing>
  </Output>
</Outputs>
```

- Omitting `<AntiAliasing>` inherits all global defaults.
- `taaDepth` is optional when the output target has its own depth-texture attachment.
- A named output may target the screen presentation chain or an offscreen `RenderTexture`.
- Programmatic pipelines expose equivalent named-output descriptors.
- Overrides are fixed for an output generation but may change when the pipeline/output is regenerated.

## 3. Remove legacy sample-count authoring

Remove public sample-count controls:

- Remove graph XML `<samples>` support.
- Remove public `GraphImageDesc::samples` authoring.
- Remove `RenderTextureOptions::samples`.
- Remove `ProgrammaticRenderTextureStream::setSamples()`.

The renderer retains an internal physical sample count for allocation and resolve operations, but it is not directly authored.

Legacy XML containing `<samples>` fails validation with a migration-specific message directing authors to named-output anti-aliasing. Migrate all repository pipelines and tests; do not silently convert user documents.

The currently proposed fixed 4x presentation change is superseded by this design: repository defaults return to off and built-in pipelines use named outputs with inherited settings.

## 4. Pipeline compilation and output processing

### 4.1 Effective settings

At pipeline creation/regeneration:

1. Merge each named output's overrides with the `RenderSystem` defaults.
2. Validate the resulting settings and output formats.
3. Build an immutable output-processing plan.
4. Allocate all new resources transactionally.
5. Replace the previous generation only after complete success.

All named outputs in one pipeline must have identical effective MSAA, SSAA, and TAA settings because they share rasterization, resolution, and camera jitter. FXAA may differ per output. Mixed TAA-enabled and TAA-disabled outputs are rejected.

### 4.2 SSAA

`2x`, `4x`, and `8x` mean total pixel samples, not per-axis scale. Internal linear scales are approximately `sqrt(2)`, `2`, and `sqrt(8)`, with deterministic dimension rounding.

- Scale viewport-relative pipeline allocations to the supersampled dimensions.
- Do not implicitly scale explicitly absolute resources such as authored shadow-map sizes.
- Run TAA at supersampled resolution.
- Downsample to logical output size using a separable Lanczos filter.
- Preserve alpha.

### 4.3 MSAA

Pipeline compilation assigns the selected private physical sample count to compatible raster color/depth attachments. Resolve multisampled attachments before sampled reads and before TAA. Multisampled TAA depth is automatically resolved to a single-sample depth texture.

The public graph no longer controls attachment sample counts directly. The compiler owns attachment compatibility and inserts or schedules resolves.

### 4.4 TAA

Initial implementation:

- Eight-sample Halton camera-jitter sequence.
- Camera jitter applied by the pipeline render path.
- Depth-based reprojection; no motion vectors in this version.
- Neighborhood clamping and fixed history blend.
- Independent color/depth history per named output.
- Shared per-pipeline jitter sequence.
- Preserve alpha.

TAA is supported only through `RenderPipeline`/render-graph rendering. Pipeline creation fails if TAA is enabled but neither a valid `taaDepth` image nor an output depth-texture attachment exists.

Reset history on:

- Resize.
- Camera cut or teleport.
- Pipeline regeneration.
- Effective output-setting change.
- A frame in which the output is not rendered.

Add explicit camera-cut/revision support and conservative transform/projection discontinuity detection.

### 4.5 FXAA

Use one fixed high-quality FXAA preset at logical output resolution after SSAA downsampling. Preserve alpha.

FXAA requires an LDR color output: `RGBA8`, `SRGB8_ALPHA8`, or `RGB10_A2`. Reject HDR, depth, and incompatible output formats.

### 4.6 Screen output

Do not rely on native-window framebuffer MSAA. Screen rendering uses the same internal named-output chain as offscreen rendering, then presents the final single-sample texture to the default framebuffer. This keeps combinations and behavior consistent across screen and `RenderTexture` outputs.

## 5. Validation and errors

Validation occurs before replacing active resources and produces specific diagnostics for:

- Unknown INI keys or values.
- Unsupported MSAA count.
- Missing or invalid named output image.
- Different effective MSAA, SSAA, or TAA settings among outputs in one pipeline.
- TAA without a valid depth source.
- TAA on a non-pipeline/manual rendering path.
- FXAA on an incompatible format.
- Legacy `<samples>` XML.
- Output dimensions exceeding GPU limits after SSAA scaling.
- Incompatible attachment sets generated by pipeline compilation.
- Allocation failure.

Legacy/non-render-graph pipelines fail when registered if inherited TAA cannot be satisfied; `RenderSystem` construction itself does not reject the setting before pipeline topology is known.

Resize is transactional. If SSAA dimensions exceed capabilities or allocation fails, retain the old size and resources and return/throw a structured error. PipelineEditor and DemoSuite display the message using their existing error UI.

## 6. Implementation phases

1. **Core types and INI parsing — COMPLETE**
   - [x] Add defaults, overrides, effective settings, and capability reporting.
   - [x] Add strict shared `[mpp]` parsing in app support.
   - [x] Pass immutable `RenderSystemOptions` from PipelineEditor and DemoSuite.
   - [x] Add and preserve deployed `demosuite.ini`.
   - [x] Validate supported MSAA counts and startup SSAA dimensions.
   - [x] Add parser, invalid-input, defaults, and override-inheritance tests.

2. **Named outputs and migration — COMPLETE**
   - [x] Add shared programmatic and XML named-output descriptors.
   - [x] Add serialization, round trips, inheritance resolution, and strict structural/effective validation.
   - [x] Remove public `GraphImageDesc`/`RenderTextureOptions` sample controls and XML `<samples>` functionality.
   - [x] Reject legacy `<samples>` with an actionable named-output migration error.
   - [x] Migrate repository templates, invalid fixtures, runtime wiring, packages, docs, and tests.

3. **Transactional output chain — NOT STARTED**
   - Add internal physical image descriptors and screen/offscreen output processors.
   - Implement capability checks, resizing, generation replacement, and history ownership.

4. **MSAA and resolve scheduling**
   - Apply private physical sample counts to raster attachments.
   - Resolve color/depth before sampled use and output processing.

5. **SSAA**
   - Add supersampled viewport planning and alpha-preserving Lanczos downsampling.

6. **TAA**
   - Add jitter, depth reprojection, neighborhood clamping, histories, and reset rules.

7. **FXAA**
   - Add the fixed high-quality LDR post-process.

8. **Application integration and documentation**
   - Expose errors in PipelineEditor and DemoSuite.
   - Update authoring, CLI/package, and configuration documentation.

## 7. Test plan

### Unit and parser tests

- Defaults are all off.
- Case-insensitive/whitespace-tolerant valid INI parsing.
- Unknown keys and invalid values fail with exact diagnostics.
- Override inheritance and explicit-off behavior.
- XML/programmatic named-output round trips.
- Legacy `<samples>` rejection.
- Combination and multi-output consistency validation.
- TAA depth and FXAA format validation.

### GPU tests

- 2x/4x/8x MSAA allocation and color/depth resolve where supported.
- Capability rejection where a requested count is unsupported.
- SSAA internal dimensions and Lanczos output at each factor.
- TAA jitter sequence, history accumulation, depth reprojection, and every reset condition.
- Multisampled depth resolve before TAA.
- FXAA LDR processing.
- Alpha preservation through each technique and supported combination.
- Combined MSAA + TAA + SSAA + FXAA ordering.
- No OpenGL errors or leaked resources across regeneration and resize failures.

### Integration tests

- PipelineEditor screen output and offscreen preview output.
- Pipeline regeneration with changed overrides.
- DemoSuite built-in and package-mode output.
- Package export/load retains named-output settings.
- Resize failure retains the previous valid image and reports an error.
- Debug and Release smoke tests with defaults off and representative combinations.
