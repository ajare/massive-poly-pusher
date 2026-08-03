# Soft Shadow Validation

Use DemoSuite with **Render Pipeline** set to `PBR`. The statue is the PBR caster/receiver and the visible flat grid is a legacy-lit receiver in the same `MainDirectionalShadow` domain.

## Checks

1. Enable **Shadows Enabled**. The statue should self-shadow and cast onto the grid.
2. Select **Hard (1 tap)**, then **Soft (3x3 PCF)**. The soft setting should broaden the shadow edge without changing the light direction.
3. Change **Shadow Filter Radius**. The edge should become softer as the radius increases.
4. Change **Shadow Resolution** from 512 to 2048. The map should recreate without errors; fine details should become less aliased.
5. Change **Shadow Extent**. A smaller extent increases detail but may clip distant casters; pixels outside the map remain lit.
6. Set both bias controls to zero to expose self-shadow acne, then restore defaults. Increase bias excessively to observe peter-panning. Defaults should be stable for the statue scale.
7. Change **Shadow Light Direction**. The PBR directional light and shadow projection should move together.
8. Switch the render pipeline to `Default`. It intentionally has no shadow domain and should remain unshadowed.
9. Switch back to `PBR` and verify the shadow map returns without resource, framebuffer, shader, or OpenGL errors.

## Recommended captures

Capture these manually from the target graphics configuration:

- `doc/reference-images/shadow-pbr-to-legacy-hard.png`
- `doc/reference-images/shadow-pbr-to-legacy-pcf.png`
- `doc/reference-images/shadow-bias-acne.png`
- `doc/reference-images/shadow-bias-peter-panning.png`
- `doc/reference-images/shadow-default-unshadowed.png`

## Known gaps

No depth-map preview, automated image comparison, PBR alpha-mask casting, generic caster metadata, point/spot lights, cascades, or contact-hardening shadows are implemented yet.
