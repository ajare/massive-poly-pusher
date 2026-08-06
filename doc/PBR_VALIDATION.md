# PBR DemoSuite Validation

This is the manual regression procedure for the currently implemented PBR milestones. It uses the statue asset and the controls in the DemoSuite **DemoSuite** ImGui window.

## Build and launch

1. Build `MassivePolyPusher`, `ModelConvert`, and `DemoSuite` in the same Debug or Release/x64 configuration.
2. Ensure the matching `MassivePolyPusher[d].dll` is beside `DemoSuite.exe`.
3. Launch DemoSuite. The initial **Render Pipeline** is `PBR` and the statue is visible by default.

## Automated startup checks

DemoSuite startup runs `runMaterialResourceTests()` before the render-graph GPU suite. It fails startup on incorrect Basic/PBR XML root dispatch, cross-root parser acceptance, invalid PBR factor ranges, loss of material type or quality data in RSE2 binary round trips, legacy tag emission, or failed RSER Basic/PBR conversion. The active statue custom program also exercises typed PBR XML, `PBR_EXT_EMISSIVE_SCALE`, typed model serialization, reflection validation, resource binding, and real-context rendering. Successful startup logs both material-resource and render-graph GPU suite pass messages.

## PBR control checks

Perform these checks with **Render Pipeline** set to `PBR`:

| Control | Check | Expected result |
|---|---|---|
| `PBR Exposure` | Move from 0 to 8 | Brightness changes only at final presentation; the HDR surface response is retained. |
| `PBR Tone Map` | Switch Reinhard / ACES | The presentation curve changes without changing material controls. |
| `PBR Base Colour` | Select a saturated colour | The statue albedo tint changes. |
| `PBR Metallic` | Move from 0 to 1 | Diffuse response reduces and environment/direct specular response increases. |
| `PBR Roughness` | Move from 0.04 to 1 | Specular response changes from tight to broad. |
| `PBR Light Intensity` | Move from 0 to 250000 | Direct point-light contribution changes with no legacy-light API dependency. |
| `PBR Environment` | Switch Cool / Warm placeholder | IBL contribution changes colour. |

Use the controls to record at least one reference image for each tone-map operator. Recommended filenames are:

- `doc/reference-images/pbr-statue-aces.png`
- `doc/reference-images/pbr-statue-reinhard.png`

Reference images are intentionally not committed until captured from a target graphics configuration.

## Pipeline regression check

1. Select `Default` in **Render Pipeline** and confirm the application continues rendering without errors.
2. Select `PBR` again and confirm the statue returns without being hidden.
3. Resize the window and confirm the PBR scene target is recreated and presentation still fills the window.

## Transparency check for authored assets

For a PBR material with `alphaMode` `MASK`, verify pixels below `alphaCutoff` are discarded and depth is written for remaining pixels. For `BLEND`, verify it renders after opaque/masked PBR geometry with source-alpha blending and no depth writes. The statue itself is authored as `OPAQUE`.

## Known coverage gaps

The current demo validates the statue, factor controls, direct light, placeholder IBL, HDR presentation, and pipeline switching. A dedicated sphere grid, texture-backed normal-map asset, emissive asset, alpha-mask asset, and alpha-blend asset remain follow-up demo content.
