# Shadow validation

## MPP point-shadow matrix

Use the deterministic `GpuTestPointQuality` fixture in `runRenderGraphGpuTests()` as
the automated baseline, then capture the same point domain in RenderDoc.

1. Start with axis-oriented opaque boxes around the point light, a `MASK` cutout, and
   a `BLEND` receiving plane. Confirm opaque and mask casters contribute depth while
   blend receives without casting.
2. Inspect `+X`, `-X`, `+Y`, `-Y`, `+Z`, and `-Z` cubemap faces. Their depth must
   match the axis caster, with no missing face or face-edge discontinuity.
3. Toggle hard and 3x3 PCF and change PCF radius. The edge softens without moving or
   disconnecting at a cubemap seam. Change constant and normal bias from zero (acne)
   through the stable value to excessive (peter-panning).
4. Test a receiver near the light-range boundary. Shadow visibility fades from
   `fadeStartNormalized * range` to fully lit at range, rather than popping.
5. Record domain diagnostics after the first render: it reports a rendered dirty map
   and six face passes. Render a stationary frame: it reports reuse and zero face
   passes. Move the light, modify a caster, and call explicit invalidation in turn:
   each makes exactly one six-face update.
6. Exercise a point-capability/allocation failure. The domain is disabled with one
   warning, the colour frame remains valid and directly lit, and an unshadowed
   pipeline remains unaffected.
7. In RenderDoc, verify the six `Pass: PointShadow [domain] Face ...` events precede
   colour work once for all pipelines joined to that domain. A clean capture contains
   no point-face events. This is the reference process-flow evidence for one shared
   cubemap.

## Boolean World: Player Torch

The Player Torch is the permanently equipped point light. Game configuration is under
`Configuration/Video/Shadows` in every Launcher `Game.yaml`:

```yaml
Shadows:
  Enabled: true
  FaceResolution: 1024
  Range: 192
  NearPlane: 0.25
  ConstantBias: 0.0008
  NormalBias: 0.0025
  Filter: pcf
  FilterRadius: 1
  FadeStart: 0.9
```

`FaceResolution` is per cubemap face. The Game maps this configuration to the single
`BooleanWorld.PlayerTorch` domain and every render-scale/AA variant joins it. F1 opens
session-only Player Torch diagnostics: it can override enabled state and adjust range,
biases, filter, PCF radius, and fade start. It displays (but does not edit) configured
face resolution. Leaving F1 or the game does not write `Game.yaml`; change that file to
persist settings. A fallback indicator means MPP rejected cubemap support/allocation,
not that configuration was intentionally disabled.

Manual Game/editor checklist:

- Move: the Player Torch and shadow move with the player; turn in place and verify the
  debug torch offset follows yaw while the cubemap is invalidated only by the changed
  light position.
- Stand still: diagnostics/process flow reuse the cached cubemap. Trigger a World
  generation commit and verify the Game explicitly invalidates it.
- Check the F1 debug offset, a close wall, and floor/ceiling/wall surfaces for acne and
  peter-panning; run both 2D and 3D horizontal material modes.
- Cross cubemap seams and range fade. Check opaque, masked, and transparent policy:
  transparent surfaces receive; masked surfaces cast their cutoff; blended surfaces do
  not cast.
- Compare the editor player-view preview with Game using the Player proxy as the
  Player Torch position. Edit/commit world geometry and verify the preview invalidates
  and redraws its one shared domain.
- Force/observe the hardware fallback. The scene remains directly lit, with no stale
  shadow texture or failed frame.

Run the MPP render-graph/material/scene/shadow GPU suites together with affected
Launcher, Game, render, and editor tests before accepting a change. Image comparison is
still manual; retain dirty/clean RenderDoc captures with the six-face event sequence.
