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
      Events:
        - trigger: spawn
          action: audio
          payload: 100
        - trigger: age
          age: 0.5
          action: gameplayCallback
          payload: 42
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
        distortion: true
        distortionStrength: 0.02
        maximumDrawDistance: 250
        minimumProjectedSize: 1.5
```

Supported spawn shapes are `point`, `line`, `box`, `sphere`, `hemisphere`, `disc`, and `cone`. Curves may also contain `Alpha`, `VelocityMultiplier`, `Drag`, `RotationSpeed`, and `EmissiveIntensity` blocks.

An optional `Mesh` block selects real-geometry particles:

```yaml
      Mesh:
        model: Models/Rock
        material: Materials/Rock # optional; embedded mesh materials are the default
```

`model` names a `Model` resource and every mesh in that model is rendered with GPU instancing; optional `material` overrides each mesh's embedded `Material`. The particle's world position, scalar rotation, uniform spawn/size scale, and velocity are consumed directly by the material's 3D vertex program. Rotation is around the velocity direction (local Y while stationary), which gives moving rocks, casings, debris, fragments, and leaves a stable tumbling axis. Mesh particles use `MPP.ParticleMeshScene`, depth testing, and ordinary material shading; they never enter a billboard blend-class draw. Material programs must use the standard `@MMatrix` and `@MCPMatrix` 3D transform contract.

Supported billboard values are `cameraFacing`, `screenAligned`, `cylindrical`, `axisLocked`, `velocityAligned`, and `velocityStretched`. `velocityStretched` keeps the particle's authored width while lengthening it along its camera-projected velocity; stationary particles retain a square fallback and particle rotation is ignored so the long axis remains velocity-aligned.

Supported blend classes are `additive`, `alpha`, and `weightedOit`. Conventional alpha appearances can opt into exact GPU back-to-front sorting with `depthSort: true`; it defaults to `false`. Additive and `weightedOit` appearances never run the sorting path even if the flag is present. Use `weightedOit` for dense smoke, dust, steam, and atmospheric particles where stable order-independent compositing is preferable to exact sorting.

`distortion: true` writes the billboard to the authored `MPP.ParticleDistortion` pass in addition to its ordinary blend-class pass. `distortionStrength` is the maximum normalized-screen UV offset and defaults to `0.02`; zero disables output. A texture's RG channels encode the signed offset direction (`[0,1]` maps to `[-1,1]`) and alpha masks it. An appearance without a texture uses an outward radial direction, which is convenient for shockwaves. Composite the additive `RG16F`/`RG32F` buffer with `MPP.ParticleDistortionComposite` before tone mapping (and normally before bloom).

`maximumDrawDistance` is measured in world units and `minimumProjectedSize` is a particle diameter in pixels. Both default to zero, which disables that culling test. Frustum culling always applies. Live particle effects can also be hidden without stopping simulation through `ParticleSystem::setEffectVisible` or assigned visibility flags through `setEffectVisibilityFlags`.

`Gravity`, `Drag`, `Noise`, `CurlNoise`, `Turbulence`, `VectorField`, and `Collision` are named optional blocks, not a sequence. Their evaluation order is fixed by the engine and cannot be authored. `maximumParticleCount` at effect level must exactly equal the sum of all emitter-template values; enforcement at runtime remains per emitter template.

Noise, curl noise, turbulence, and vector fields share `frequency`, `strength`, `scroll`, and `timeScale` controls. Curl noise computes a divergence-free force from the built-in 3D noise texture. Turbulence folds and combines 1–8 noise octaves; `lacunarity` controls frequency growth and `gain` controls amplitude decay. `ParticleSystem::setVectorField` installs the arbitrary RGB 3D texture shared by vector-field behaviour modules. RGB values map from `[0,1]` to `[-1,1]`; clear it with `clearVectorField`. A missing field makes only the `VectorField` module inert.

Collision `sources` is a comma-separated combination of `screenSpace`, `analytical`, and `signedDistanceField`; an omitted value defaults to `analytical`. Responses are `bounce`, `slide`, `stop`, `kill`, and `spawnSecondaryEffect`. Restitution is used by bounce, friction is clamped to `[0,1]`, `radiusScale` multiplies the particle's base size, and `screenSpaceThickness` limits how far behind sampled geometry a depth collision can be recovered. The spawn-secondary response stops the particle and retains the one-frame `ParticleFlag::CollisionEvent` and `ParticleFlag::SpawnSecondaryEffect` markers. An authored collision event rule selects what is created on first contact.

Screen-space collision samples the last completed depth image supplied to an `MPP.ParticleScene` or `MPP.ParticleWeightedOit` pass, so the first rendered frame has no screen collision and subsequent simulation remains once-per-frame before graph execution. The pass's `DEPTH` input must therefore be a sampled, resolved depth texture. Analytical world colliders are supplied with `ParticleSystem::setColliders`; supported `ParticleColliderShape` values are plane, sphere, box, and capsule. A plane uses `first.xyz`/`first.w` as normal/distance, a sphere uses `first.xyz`/`first.w` as centre/radius, a box uses `first.xyz`, `second.xyz`, and `third.xyzw` as centre/half-extents/orientation quaternion, and a capsule uses `first.xyz`/`first.w` and `second.xyz` as endpoints/radius.

Install one optional 3D signed-distance texture with `ParticleSystem::setSignedDistanceField`. Its transform maps world coordinates to `[0,1]^3`; red stores signed distance as `(red - isoValue) * distanceScale`. Sampling outside the texture domain does not collide. Use linear filtering and clamp-to-edge wrapping for stable gradients.

## Particle lighting

Lighting is authored per emitter template and is disabled by default:

```yaml
      Lighting:
        proxyLight: true
        lightInjection: true
        volumetricLighting: true
        colour: 1 0.35 0.08
        intensity: 8
        range: 3
        volumetricIntensity: 0.2
```

`proxyLight` exposes one point-light representation for each live Emitter through `ParticleSystem::getProxyLights`; its position follows the Emitter transform. `lightInjection` opts that proxy into the renderer's PBR light array. Scene-authored lights have priority, and proxies fill only the remaining slots in the fixed eight-light budget in emitter-index order. It therefore never creates a dynamic light per Particle. `lightInjection` requires `proxyLight: true`.

`volumetricLighting` opts the same Emitter-sized sphere into the additive `MPP.ParticleVolumetricLighting` graph pass. The pass integrates inscattered radiance through the sphere, clips the path against optional `DEPTH`, and writes HDR colour plus an optional emissive/bloom output. `volumetricIntensity` scales this contribution independently of direct lighting; `colour`, `intensity`, and positive world-space `range` are shared. Generated graph pipelines include this pass. Authored graphs should use additive one/one blending, disable depth writes, and place it after opaque depth is available.

Stopped, hidden, destroyed, and retired Emitters contribute neither proxy nor volumetric lighting. `ParticleParameter::EmissiveScale` scales both direct and volumetric intensity. Particle count never affects proxy count or the volumetric draw count.

## Particle events

An emitter template's optional `Events` list contains repeated `Event` records. `trigger` is `spawn`, `death`, `collision`, or `age`. Spawn fires only after successful GPU allocation, death covers both lifetime expiry and collision kill, collision fires on first contact rather than every frame of sustained contact, and age fires once when the particle crosses the required non-negative `age` in seconds. An age of zero fires with the successful spawn.

Actions are `secondaryParticleBurst`, `decal`, `audio`, `light`, and `gameplayCallback`. A secondary particle burst requires `targetEmitter` (an emitter-template name in this particle effect) and a positive `count`. It allocates and initializes particles directly on the GPU at the event's world position; no particle count, event, or spawn command crosses the CPU. Configure a target used only by events with `Spawn.enabled: false`; its normal template budget still clamps all live particles. Destroying a target emitter invalidates inbound events immediately, including queued work, rather than allowing a reused emitter slot to receive them. Secondary-burst target graphs must be acyclic, and a same-frame spawn-triggered chain may be no deeper than eight stages. This is intra-effect event routing, not the child particle effect asset composition deferred to #38.

The other actions carry an optional unsigned `payload` interpreted by application code. Callback records include event position and age, particle velocity and lifetime, a collision contact normal (zero for other triggers), and the generational source-emitter identity. Register handlers with `ParticleSystem::setEventCallback` for `Decal`, `Audio`, `Light`, or `GameplayCallback`. The first handler lazily enables a four-slot staging/fence ring; completed GPU event batches are polled with zero timeout after at least two frames, and a busy ring drops a readback sample instead of stalling rendering. Clearing the last handler removes all event readback. Callbacks run from the next `ParticleSystem::simulate` that finds a completed batch. A `Light` action remains a notification for application-owned transient work; authored emitter-level proxy lights and volumetric contributions use the `Lighting` block instead.
