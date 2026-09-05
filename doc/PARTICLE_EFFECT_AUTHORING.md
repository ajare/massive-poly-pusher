# Particle effect authoring

Particle effects are reusable assets stored as `*.particle.yaml`. They describe emitter templates only; live emitter handles and effect transforms are created by `ParticleSystem` and are never stored in the asset.

```yaml
ParticleEffect:
  version: 1
  name: Smoke
  maximumParticleCount: 1024
  Emitters:
    - name: Plume
      maximumParticleCount: 1024
      transform: 1 0 0 0  0 1 0 0  0 0 1 0  0 0 0 1
      Spawn:
        shape: cone
        shapeParameters: 1 2 0.5 0
        mode: continuous
        rate: 60
        lifetime: 1 3
        size: 0.2 0.5
      Behaviours:
        Gravity:
          acceleration: 0 -1 0
        Drag:
          coefficient: 0.2
        Noise:
          frequency: 1 1 1
          strength: 0.5
          scroll: 0 0.2 0
          timeScale: 1
        CurlNoise:
          frequency: 0.5 0.5 0.5
          strength: 1.5
          scroll: 0 0.1 0
          timeScale: 1
        Turbulence:
          frequency: 1 1 1
          strength: 0.75
          scroll: 0 0.2 0
          timeScale: 1
          octaves: 4
          lacunarity: 2
          gain: 0.5
        VectorField:
          frequency: 1 1 1
          strength: 2
          scroll: 0 0 0
          timeScale: 1
        Collision:
          sources: screenSpace,analytical,signedDistanceField
          response: bounce
          restitution: 0.5
          friction: 0.1
          radiusScale: 0.5
          screenSpaceThickness: 0.2
      Curves:
        Size:
          default: 1
          Keys:
            - { time: 0, value: 0.25 }
            - { time: 1, value: 2 }
        Colour:
          default: 1 1 1
          Keys:
            - { time: 0, colour: 1 0.5 0.1 }
            - { time: 1, colour: 0.1 0.1 0.1 }
      Appearance:
        texture: Textures/Smoke
        tint: 1 1 1
        alpha: 0.8
        atlasColumns: 4
        atlasRows: 4
        frameCount: 16
        animation: frameOverLife
        randomStart: true
        billboard: cameraFacing
        blendClass: alpha
        depthSort: true
        maximumDrawDistance: 250
        minimumProjectedSize: 1.5
```

Supported spawn shapes are `point`, `line`, `box`, `sphere`, `hemisphere`, `disc`, and `cone`. Curves may also contain `Alpha`, `VelocityMultiplier`, `Drag`, `RotationSpeed`, and `EmissiveIntensity` blocks.

Supported billboard values are `cameraFacing`, `screenAligned`, `cylindrical`, `axisLocked`, `velocityAligned`, and `velocityStretched`. `velocityStretched` keeps the particle's authored width while lengthening it along its camera-projected velocity; stationary particles retain a square fallback and particle rotation is ignored so the long axis remains velocity-aligned.

Supported blend classes are `additive`, `alpha`, and `weightedOit`. Conventional alpha appearances can opt into exact GPU back-to-front sorting with `depthSort: true`; it defaults to `false`. Additive and `weightedOit` appearances never run the sorting path even if the flag is present. Use `weightedOit` for dense smoke, dust, steam, and atmospheric particles where stable order-independent compositing is preferable to exact sorting.

`maximumDrawDistance` is measured in world units and `minimumProjectedSize` is a particle diameter in pixels. Both default to zero, which disables that culling test. Frustum culling always applies. Live particle effects can also be hidden without stopping simulation through `ParticleSystem::setEffectVisible` or assigned visibility flags through `setEffectVisibilityFlags`.

`Gravity`, `Drag`, `Noise`, `CurlNoise`, `Turbulence`, `VectorField`, and `Collision` are named optional blocks, not a sequence. Their evaluation order is fixed by the engine and cannot be authored. `maximumParticleCount` at effect level must exactly equal the sum of all emitter-template values; enforcement at runtime remains per emitter template.

Noise, curl noise, turbulence, and vector fields share `frequency`, `strength`, `scroll`, and `timeScale` controls. Curl noise computes a divergence-free force from the built-in 3D noise texture. Turbulence folds and combines 1–8 noise octaves; `lacunarity` controls frequency growth and `gain` controls amplitude decay. `ParticleSystem::setVectorField` installs the arbitrary RGB 3D texture shared by vector-field behaviour modules. RGB values map from `[0,1]` to `[-1,1]`; clear it with `clearVectorField`. A missing field makes only the `VectorField` module inert.

Collision `sources` is a comma-separated combination of `screenSpace`, `analytical`, and `signedDistanceField`; an omitted value defaults to `analytical`. Responses are `bounce`, `slide`, `stop`, `kill`, and `spawnSecondaryEffect`. Restitution is used by bounce, friction is clamped to `[0,1]`, `radiusScale` multiplies the particle's base size, and `screenSpaceThickness` limits how far behind sampled geometry a depth collision can be recovered. The spawn-secondary response sets `ParticleFlag::CollisionEvent` and `ParticleFlag::SpawnSecondaryEffect` on first contact; consuming that GPU event to create child work belongs to the secondary-effects feature.

Screen-space collision samples the last completed depth image supplied to an `MPP.ParticleScene` or `MPP.ParticleWeightedOit` pass, so the first rendered frame has no screen collision and subsequent simulation remains once-per-frame before graph execution. The pass's `DEPTH` input must therefore be a sampled, resolved depth texture. Analytical world colliders are supplied with `ParticleSystem::setColliders`; supported `ParticleColliderShape` values are plane, sphere, box, and capsule. A plane uses `first.xyz`/`first.w` as normal/distance, a sphere uses `first.xyz`/`first.w` as centre/radius, a box uses `first.xyz`, `second.xyz`, and `third.xyzw` as centre/half-extents/orientation quaternion, and a capsule uses `first.xyz`/`first.w` and `second.xyz` as endpoints/radius.

Install one optional 3D signed-distance texture with `ParticleSystem::setSignedDistanceField`. Its transform maps world coordinates to `[0,1]^3`; red stores signed distance as `(red - isoValue) * distanceScale`. Sampling outside the texture domain does not collide. Use linear filtering and clamp-to-edge wrapping for stable gradients.
