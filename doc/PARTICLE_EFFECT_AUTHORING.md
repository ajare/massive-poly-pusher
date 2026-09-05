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
```

Supported spawn shapes are `point`, `line`, `box`, `sphere`, `hemisphere`, `disc`, and `cone`. Curves may also contain `Alpha`, `VelocityMultiplier`, `Drag`, `RotationSpeed`, and `EmissiveIntensity` blocks.

`Gravity`, `Drag`, and `Noise` are named optional blocks, not a sequence. Their evaluation order is fixed by the engine and cannot be authored. `maximumParticleCount` at effect level must exactly equal the sum of all emitter-template values; enforcement at runtime remains per emitter template.
