#pragma once

namespace mpp
{
	// Initialises only GPU-owned allocation state. In particular the CPU never
	// manufactures free particle indices or counter values.
	inline char const* ParticlePoolInitialiseComputeShader = R"MPP(#version 430

layout(local_size_x = MPP_PARTICLE_WORK_GROUP_SIZE) in;

layout(std430, binding = 1) restrict writeonly buffer ParticleFreeIndices
{
    uint FREE_INDICES[];
};

layout(std430, binding = 4) restrict buffer ParticleCounters
{
    uint FREE_COUNT;
    uint ACTIVE_COUNT_A;
    uint ACTIVE_COUNT_B;
    uint DROPPED_SPAWN_COUNT;
    uint SPAWNED_COUNT;
    uint KILLED_COUNT;
    uint RENDERED_COUNT;
    uint CULLED_COUNT;
    uint LIVE_COUNTS[];
};

uniform uint POOL_CAPACITY;
uniform uint TEMPLATE_CAPACITY;

void main()
{
    uint index = gl_GlobalInvocationID.x;
    if (index < POOL_CAPACITY)
        FREE_INDICES[index] = index;
    if (index < TEMPLATE_CAPACITY)
        LIVE_COUNTS[index] = 0u;

    if (index == 0u)
    {
        FREE_COUNT = POOL_CAPACITY;
        ACTIVE_COUNT_A = 0u;
        ACTIVE_COUNT_B = 0u;
        DROPPED_SPAWN_COUNT = 0u;
        SPAWNED_COUNT = 0u;
        KILLED_COUNT = 0u;
        RENDERED_COUNT = 0u;
        CULLED_COUNT = 0u;
    }
}
)MPP";

	// Resets only the per-frame diagnostic counters. This dispatch exists only
	// while statistics are enabled and never touches the allocation counters.
	inline char const* ParticleStatisticsPrepareComputeShader = R"MPP(#version 430

layout(local_size_x = 1) in;

layout(std430, binding = 4) restrict buffer ParticleCounters
{
    uint FREE_COUNT;
    uint ACTIVE_COUNT_A;
    uint ACTIVE_COUNT_B;
    uint DROPPED_SPAWN_COUNT;
    uint SPAWNED_COUNT;
    uint KILLED_COUNT;
    uint RENDERED_COUNT;
    uint CULLED_COUNT;
    uint LIVE_COUNTS[];
};

uniform uint ACTIVE_LIST_INDEX;
uniform uint REQUESTED_SPAWN_COUNT;

void main()
{
    DROPPED_SPAWN_COUNT = 0u;
    SPAWNED_COUNT = REQUESTED_SPAWN_COUNT;
    KILLED_COUNT = ACTIVE_LIST_INDEX == 0u ? ACTIVE_COUNT_A : ACTIVE_COUNT_B;
    RENDERED_COUNT = 0u;
    CULLED_COUNT = 0u;
}
)MPP";

	// One work group consumes one spawn command. Each lane handles a strided
	// subset of that command, so dispatch cost follows requested spawns rather
	// than pool capacity. Allocation and budget reservations use CAS loops: an
	// exhausted pool can never underflow FREE_COUNT or duplicate an index.
	inline char const* ParticleSpawnComputeShader = R"MPP(#version 430

layout(local_size_x = MPP_PARTICLE_WORK_GROUP_SIZE) in;

struct ParticleRecord
{
    vec4 positionAge;
    vec4 velocityLifetime;
    uint packedColour;
    float baseSize;
    float rotation;
    float angularVelocity;
    uint emitterIndex;
    uint seed;
    uint flags;
    uint padding;
};

struct EmitterSimData
{
    mat4 transform;
    vec4 shapeParameters;
    vec4 initialVelocityMin;
    vec4 initialVelocityMax;
    vec4 colourMin;
    vec4 colourMax;
    vec4 lifetimeSizeRanges;
    vec4 rotationRanges;
    uvec4 shapeSeedModulesBudget;
    uvec4 emissionState;
    vec4 emissionRateAndPadding;
    vec4 parameterMultipliers0;
    vec4 parameterMultipliers1;
    vec4 gravityAndDrag;
    vec4 noiseFrequencyStrength;
    vec4 noiseScrollAndTimeScale;
};

struct SpawnCommand
{
    uint emitterIndex;
    uint count;
    uint randomSeed;
    uint spawnCounter;
};

layout(std430, binding = 0) restrict writeonly buffer ParticlePool
{
    ParticleRecord PARTICLES[];
};
layout(std430, binding = 1) restrict buffer ParticleFreeIndices
{
    uint FREE_INDICES[];
};
layout(std430, binding = 2) restrict writeonly buffer ParticleActiveIndicesA
{
    uint ACTIVE_INDICES_A[];
};
layout(std430, binding = 3) restrict writeonly buffer ParticleActiveIndicesB
{
    uint ACTIVE_INDICES_B[];
};
layout(std430, binding = 4) restrict buffer ParticleCounters
{
    uint FREE_COUNT;
    uint ACTIVE_COUNT_A;
    uint ACTIVE_COUNT_B;
    uint DROPPED_SPAWN_COUNT;
    uint SPAWNED_COUNT;
    uint KILLED_COUNT;
    uint RENDERED_COUNT;
    uint CULLED_COUNT;
    uint LIVE_COUNTS[];
};
layout(std430, binding = 5) restrict readonly buffer ParticleEmitters
{
    EmitterSimData EMITTERS[];
};
layout(std430, binding = 7) restrict readonly buffer ParticleSpawnCommands
{
    SpawnCommand SPAWN_COMMANDS[];
};
uniform uint EMITTER_COUNT;
uniform uint TEMPLATE_COUNT;
uniform uint SPAWN_COMMAND_OFFSET;
uniform uint ACTIVE_LIST_INDEX;

uint hashValue(uint value)
{
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

float randomScalar(inout uint state)
{
    state = hashValue(state + 0x9e3779b9u);
    return float(state) * (1.0 / 4294967296.0);
}

vec3 randomUnitVector(inout uint state)
{
    float z = randomScalar(state) * 2.0 - 1.0;
    float angle = randomScalar(state) * 6.28318530718;
    float radius = sqrt(max(0.0, 1.0 - z * z));
    return vec3(cos(angle) * radius, z, sin(angle) * radius);
}

vec3 sampleShape(uint shape, vec4 parameters, inout uint state)
{
    if (shape == 0u) // point
        return vec3(0.0);
    if (shape == 1u) // line, centred on the emitter
        return parameters.xyz * (randomScalar(state) * 2.0 - 1.0);
    if (shape == 2u) // box volume
        return parameters.xyz * vec3(
            randomScalar(state) * 2.0 - 1.0,
            randomScalar(state) * 2.0 - 1.0,
            randomScalar(state) * 2.0 - 1.0);
    if (shape == 3u) // sphere volume
        return randomUnitVector(state) * (parameters.x * pow(randomScalar(state), 1.0 / 3.0));
    if (shape == 4u) // hemisphere volume, local +Y
    {
        vec3 direction = randomUnitVector(state);
        direction.y = abs(direction.y);
        return direction * (parameters.x * pow(randomScalar(state), 1.0 / 3.0));
    }
    if (shape == 5u) // disc area, local XZ
    {
        float angle = randomScalar(state) * 6.28318530718;
        float radius = parameters.x * sqrt(randomScalar(state));
        return vec3(cos(angle) * radius, 0.0, sin(angle) * radius);
    }
    // Cone volume, apex at the origin and axis along local +Y.
    float heightFraction = pow(randomScalar(state), 1.0 / 3.0);
    float angle = randomScalar(state) * 6.28318530718;
    float radius = parameters.x * heightFraction * sqrt(randomScalar(state));
    return vec3(cos(angle) * radius, parameters.y * heightFraction, sin(angle) * radius);
}

bool reserveTemplateParticle(uint templateIndex, uint budget)
{
    uint observed = LIVE_COUNTS[templateIndex];
    while (observed < budget)
    {
        uint previous = atomicCompSwap(LIVE_COUNTS[templateIndex], observed, observed + 1u);
        if (previous == observed) return true;
        observed = previous;
    }
    return false;
}

bool popFreeIndex(out uint particleIndex)
{
    uint observed = FREE_COUNT;
    while (observed > 0u)
    {
        uint previous = atomicCompSwap(FREE_COUNT, observed, observed - 1u);
        if (previous == observed)
        {
            particleIndex = FREE_INDICES[observed - 1u];
            return true;
        }
        observed = previous;
    }
    return false;
}

void appendActiveIndex(uint particleIndex)
{
    uint destination;
    if (ACTIVE_LIST_INDEX == 0u)
    {
        destination = atomicAdd(ACTIVE_COUNT_A, 1u);
        ACTIVE_INDICES_A[destination] = particleIndex;
    }
    else
    {
        destination = atomicAdd(ACTIVE_COUNT_B, 1u);
        ACTIVE_INDICES_B[destination] = particleIndex;
    }
}

void main()
{
    SpawnCommand command = SPAWN_COMMANDS[SPAWN_COMMAND_OFFSET + gl_WorkGroupID.x];
    if (command.emitterIndex >= EMITTER_COUNT)
    {
        for (uint ordinal = gl_LocalInvocationID.x; ordinal < command.count; ordinal += gl_WorkGroupSize.x)
            atomicAdd(DROPPED_SPAWN_COUNT, 1u);
        return;
    }

    EmitterSimData emitter = EMITTERS[command.emitterIndex];
    if (emitter.emissionState.y == 0u) return;

    uint templateIndex = emitter.emissionState.w;
    if (templateIndex >= TEMPLATE_COUNT)
    {
        for (uint ordinal = gl_LocalInvocationID.x; ordinal < command.count; ordinal += gl_WorkGroupSize.x)
            atomicAdd(DROPPED_SPAWN_COUNT, 1u);
        return;
    }

    uint budget = emitter.shapeSeedModulesBudget.w;
    for (uint ordinal = gl_LocalInvocationID.x; ordinal < command.count; ordinal += gl_WorkGroupSize.x)
    {
        if (!reserveTemplateParticle(templateIndex, budget))
        {
            atomicAdd(DROPPED_SPAWN_COUNT, 1u);
            continue;
        }

        uint particleIndex;
        if (!popFreeIndex(particleIndex))
        {
            atomicAdd(LIVE_COUNTS[templateIndex], 0xffffffffu);
            atomicAdd(DROPPED_SPAWN_COUNT, 1u);
            continue;
        }

        uint seed = hashValue(emitter.shapeSeedModulesBudget.y ^ command.randomSeed);
        seed = hashValue(seed ^ particleIndex);
        seed = hashValue(seed ^ (command.spawnCounter + ordinal));
        uint randomState = seed;

        vec3 localPosition = sampleShape(emitter.shapeSeedModulesBudget.x, emitter.shapeParameters, randomState);
        vec3 velocityMix = vec3(randomScalar(randomState), randomScalar(randomState), randomScalar(randomState));
        vec3 localVelocity = mix(emitter.initialVelocityMin.xyz, emitter.initialVelocityMax.xyz, velocityMix);
        localVelocity *= emitter.parameterMultipliers0.z;

        float lifetime = mix(emitter.lifetimeSizeRanges.x, emitter.lifetimeSizeRanges.y, randomScalar(randomState));
        lifetime *= emitter.parameterMultipliers0.w;
        float size = mix(emitter.lifetimeSizeRanges.z, emitter.lifetimeSizeRanges.w, randomScalar(randomState));
        size *= emitter.parameterMultipliers0.y;
        float rotation = mix(emitter.rotationRanges.x, emitter.rotationRanges.y, randomScalar(randomState));
        float angularVelocity = mix(emitter.rotationRanges.z, emitter.rotationRanges.w, randomScalar(randomState));
        vec4 colour = mix(emitter.colourMin, emitter.colourMax, vec4(
            randomScalar(randomState), randomScalar(randomState), randomScalar(randomState), randomScalar(randomState)));
        colour.a *= emitter.parameterMultipliers1.x;

        ParticleRecord particle;
        particle.positionAge = vec4((emitter.transform * vec4(localPosition, 1.0)).xyz, 0.0);
        particle.velocityLifetime = vec4(mat3(emitter.transform) * localVelocity, lifetime);
        particle.packedColour = packUnorm4x8(clamp(colour, 0.0, 1.0));
        particle.baseSize = size;
        particle.rotation = rotation;
        particle.angularVelocity = angularVelocity;
        particle.emitterIndex = command.emitterIndex;
        particle.seed = seed;
        particle.flags = 0u;
        particle.padding = 0u;
        PARTICLES[particleIndex] = particle;
        appendActiveIndex(particleIndex);
    }
}
)MPP";

	// Converts the GPU-owned active count into an indirect compute command and
	// clears only the destination state. This constant-cost dispatch is what lets
	// the simulation kernel itself run over the active list rather than capacity.
	inline char const* ParticleSimulationPrepareComputeShader = R"MPP(#version 430

layout(local_size_x = 1) in;

layout(std430, binding = 4) restrict buffer ParticleCounters
{
    uint FREE_COUNT;
    uint ACTIVE_COUNT_A;
    uint ACTIVE_COUNT_B;
    uint DROPPED_SPAWN_COUNT;
    uint SPAWNED_COUNT;
    uint KILLED_COUNT;
    uint RENDERED_COUNT;
    uint CULLED_COUNT;
    uint LIVE_COUNTS[];
};
layout(std430, binding = 7) restrict writeonly buffer ParticleSimulationDispatchCommand
{
    uint DISPATCH_COMMAND[];
};

uniform uint ACTIVE_LIST_INDEX;

void main()
{
    uint activeCount = ACTIVE_LIST_INDEX == 0u ? ACTIVE_COUNT_A : ACTIVE_COUNT_B;
    if (ACTIVE_LIST_INDEX == 0u)
        ACTIVE_COUNT_B = 0u;
    else
        ACTIVE_COUNT_A = 0u;

    DISPATCH_COMMAND[0] = (activeCount + MPP_PARTICLE_WORK_GROUP_SIZE - 1u) / MPP_PARTICLE_WORK_GROUP_SIZE;
    DISPATCH_COMMAND[1] = 1u;
    DISPATCH_COMMAND[2] = 1u;
}
)MPP";

	// The one simulation kernel. Behaviour modules branch on the emitter mask at
	// runtime (ADR 0006); no module define permutations exist. Survivors append to
	// the opposite list while dead slots return directly to the GPU free stack.
	inline char const* ParticleSimulationComputeShader = R"MPP(#version 430

layout(local_size_x = MPP_PARTICLE_WORK_GROUP_SIZE) in;

const uint PARTICLE_MODULE_GRAVITY = 1u << 0u;
const uint PARTICLE_MODULE_DRAG = 1u << 1u;
const uint PARTICLE_MODULE_NOISE = 1u << 2u;

struct ParticleRecord
{
    vec4 positionAge;
    vec4 velocityLifetime;
    uint packedColour;
    float baseSize;
    float rotation;
    float angularVelocity;
    uint emitterIndex;
    uint seed;
    uint flags;
    uint padding;
};

struct EmitterSimData
{
    mat4 transform;
    vec4 shapeParameters;
    vec4 initialVelocityMin;
    vec4 initialVelocityMax;
    vec4 colourMin;
    vec4 colourMax;
    vec4 lifetimeSizeRanges;
    vec4 rotationRanges;
    uvec4 shapeSeedModulesBudget;
    uvec4 emissionState;
    vec4 emissionRateAndPadding;
    vec4 parameterMultipliers0;
    vec4 parameterMultipliers1;
    vec4 gravityAndDrag;
    vec4 noiseFrequencyStrength;
    vec4 noiseScrollAndTimeScale;
};

layout(std430, binding = 0) restrict buffer ParticlePool
{
    ParticleRecord PARTICLES[];
};
layout(std430, binding = 1) restrict buffer ParticleFreeIndices
{
    uint FREE_INDICES[];
};
layout(std430, binding = 2) restrict buffer ParticleActiveIndicesA
{
    uint ACTIVE_INDICES_A[];
};
layout(std430, binding = 3) restrict buffer ParticleActiveIndicesB
{
    uint ACTIVE_INDICES_B[];
};
layout(std430, binding = 4) restrict buffer ParticleCounters
{
    uint FREE_COUNT;
    uint ACTIVE_COUNT_A;
    uint ACTIVE_COUNT_B;
    uint DROPPED_SPAWN_COUNT;
    uint SPAWNED_COUNT;
    uint KILLED_COUNT;
    uint RENDERED_COUNT;
    uint CULLED_COUNT;
    uint LIVE_COUNTS[];
};
layout(std430, binding = 5) restrict readonly buffer ParticleEmitters
{
    EmitterSimData EMITTERS[];
};
layout(binding = 0) uniform sampler3D NOISE_TEXTURE;
uniform uint ACTIVE_LIST_INDEX;
uniform uint EMITTER_COUNT;
uniform uint TEMPLATE_COUNT;
uniform float DELTA_SECONDS;
uniform float SIMULATION_SECONDS;

void killParticle(uint particleIndex, uint emitterIndex)
{
    uint freeDestination = atomicAdd(FREE_COUNT, 1u);
    FREE_INDICES[freeDestination] = particleIndex;
    if (emitterIndex < EMITTER_COUNT)
    {
        uint templateIndex = EMITTERS[emitterIndex].emissionState.w;
        if (templateIndex < TEMPLATE_COUNT)
            atomicAdd(LIVE_COUNTS[templateIndex], 0xffffffffu);
    }
}

void appendSurvivor(uint particleIndex)
{
    if (ACTIVE_LIST_INDEX == 0u)
    {
        uint destination = atomicAdd(ACTIVE_COUNT_B, 1u);
        ACTIVE_INDICES_B[destination] = particleIndex;
    }
    else
    {
        uint destination = atomicAdd(ACTIVE_COUNT_A, 1u);
        ACTIVE_INDICES_A[destination] = particleIndex;
    }
}

void main()
{
    uint sourceCount = ACTIVE_LIST_INDEX == 0u ? ACTIVE_COUNT_A : ACTIVE_COUNT_B;
    uint activeOrdinal = gl_GlobalInvocationID.x;
    if (activeOrdinal >= sourceCount) return;

    uint particleIndex = ACTIVE_LIST_INDEX == 0u ? ACTIVE_INDICES_A[activeOrdinal] : ACTIVE_INDICES_B[activeOrdinal];
    ParticleRecord particle = PARTICLES[particleIndex];
    if (particle.emitterIndex >= EMITTER_COUNT)
    {
        killParticle(particleIndex, particle.emitterIndex);
        return;
    }

    particle.positionAge.w += DELTA_SECONDS;
    if (particle.positionAge.w >= particle.velocityLifetime.w)
    {
        killParticle(particleIndex, particle.emitterIndex);
        return;
    }

    EmitterSimData emitter = EMITTERS[particle.emitterIndex];
    uint modules = emitter.shapeSeedModulesBudget.z;
    vec3 velocity = particle.velocityLifetime.xyz;

    if ((modules & PARTICLE_MODULE_GRAVITY) != 0u)
        velocity += emitter.gravityAndDrag.xyz * DELTA_SECONDS;

    if ((modules & PARTICLE_MODULE_NOISE) != 0u)
    {
        vec3 samplePosition = particle.positionAge.xyz * emitter.noiseFrequencyStrength.xyz;
        samplePosition += emitter.noiseScrollAndTimeScale.xyz * (SIMULATION_SECONDS * emitter.noiseScrollAndTimeScale.w);
        vec3 noiseForce = textureLod(NOISE_TEXTURE, samplePosition, 0.0).xyz * 2.0 - 1.0;
        velocity += noiseForce * (emitter.noiseFrequencyStrength.w * DELTA_SECONDS);
    }

    if ((modules & PARTICLE_MODULE_DRAG) != 0u)
        velocity *= max(0.0, 1.0 - max(0.0, emitter.gravityAndDrag.w) * DELTA_SECONDS);

    particle.velocityLifetime.xyz = velocity;
    particle.positionAge.xyz += velocity * DELTA_SECONDS;
    particle.rotation += particle.angularVelocity * DELTA_SECONDS;
    PARTICLES[particleIndex] = particle;
    appendSurvivor(particleIndex);
}
)MPP";

	// Clears the per-template counts and scatter cursors, and turns the current
	// GPU-owned active count into the exact indirect dispatch used by both count
	// and scatter. No count crosses the CPU boundary.
	inline char const* ParticleCompactionPrepareComputeShader = R"MPP(#version 430

layout(local_size_x = MPP_PARTICLE_WORK_GROUP_SIZE) in;

layout(std430, binding = 4) restrict buffer ParticleCounters
{
    uint FREE_COUNT;
    uint ACTIVE_COUNT_A;
    uint ACTIVE_COUNT_B;
    uint DROPPED_SPAWN_COUNT;
    uint SPAWNED_COUNT;
    uint KILLED_COUNT;
    uint RENDERED_COUNT;
    uint CULLED_COUNT;
    uint TEMPLATE_COUNTS[];
};
layout(std430, binding = 6) restrict buffer ParticleCompactionScratch
{
    uint COMPACTION_VALUES[];
};
layout(std430, binding = 7) restrict writeonly buffer ParticleCompactionDispatchCommand
{
    uint DISPATCH_COMMAND[];
};

uniform uint ACTIVE_LIST_INDEX;
uniform uint TEMPLATE_COUNT;
uniform uint TEMPLATE_CAPACITY;

void main()
{
    uint templateIndex = gl_GlobalInvocationID.x;
    if (templateIndex < TEMPLATE_COUNT)
    {
        TEMPLATE_COUNTS[templateIndex] = 0u;
        COMPACTION_VALUES[templateIndex] = 0u;
        COMPACTION_VALUES[TEMPLATE_CAPACITY + templateIndex] = 0u;
        COMPACTION_VALUES[TEMPLATE_CAPACITY * 2u + templateIndex] = 0u;
    }

    if (templateIndex == 0u)
    {
        uint activeCount = ACTIVE_LIST_INDEX == 0u ? ACTIVE_COUNT_A : ACTIVE_COUNT_B;
        DISPATCH_COMMAND[0] = (activeCount + MPP_PARTICLE_WORK_GROUP_SIZE - 1u) / MPP_PARTICLE_WORK_GROUP_SIZE;
        DISPATCH_COMMAND[1] = 1u;
        DISPATCH_COMMAND[2] = 1u;
    }
}
)MPP";

	// Counts every survivor for template budgets, and visible survivors for draw
	// compaction. Visibility remains GPU-only and is evaluated again by scatter;
	// no separate culling pass or CPU result is introduced.
	inline char const* ParticleCompactionCountComputeShader = R"MPP(#version 430

layout(local_size_x = MPP_PARTICLE_WORK_GROUP_SIZE) in;
layout(std140, binding = 3) uniform CameraFrame
{
    mat4 VIEW_MATRIX;
    mat4 PROJECTION_MATRIX;
    mat4 INVERSE_PROJECTION_MATRIX;
    vec4 VIEWPORT_SIZE;
    vec4 NEAR_FAR_TIME;
};

struct ParticleRecord { vec4 positionAge; vec4 velocityLifetime; uint packedColour; float baseSize; float rotation; float angularVelocity; uint emitterIndex; uint seed; uint flags; uint padding; };
struct EmitterSimData { mat4 transform; vec4 shapeParameters; vec4 initialVelocityMin; vec4 initialVelocityMax; vec4 colourMin; vec4 colourMax; vec4 lifetimeSizeRanges; vec4 rotationRanges; uvec4 shapeSeedModulesBudget; uvec4 emissionState; vec4 emissionRateAndPadding; vec4 parameterMultipliers0; vec4 parameterMultipliers1; vec4 gravityAndDrag; vec4 noiseFrequencyStrength; vec4 noiseScrollAndTimeScale; };
struct TemplateRenderData { uvec4 textureAndAtlas; vec4 tintAndAlpha; vec4 appearance; uvec4 modes; vec4 culling; uvec4 sorting; };

layout(std430, binding = 0) restrict readonly buffer ParticlePool { ParticleRecord PARTICLES[]; };
layout(std430, binding = 2) restrict readonly buffer ParticleActiveIndicesA { uint ACTIVE_INDICES_A[]; };
layout(std430, binding = 3) restrict readonly buffer ParticleActiveIndicesB { uint ACTIVE_INDICES_B[]; };
layout(std430, binding = 4) restrict buffer ParticleCounters { uint FREE_COUNT; uint ACTIVE_COUNT_A; uint ACTIVE_COUNT_B; uint DROPPED_SPAWN_COUNT; uint SPAWNED_COUNT; uint KILLED_COUNT; uint RENDERED_COUNT; uint CULLED_COUNT; uint TEMPLATE_COUNTS[]; };
layout(std430, binding = 5) restrict readonly buffer ParticleEmitters { EmitterSimData EMITTERS[]; };
layout(std430, binding = 6) restrict buffer ParticleCompactionScratch { uint COMPACTION_VALUES[]; };
layout(std430, binding = 7) restrict readonly buffer ParticleTemplates { TemplateRenderData TEMPLATES[]; };

uniform uint ACTIVE_LIST_INDEX;
uniform uint EMITTER_COUNT;
uniform uint TEMPLATE_COUNT;
uniform uint TEMPLATE_CAPACITY;

bool outsidePlane(vec4 plane, vec3 centre, float radius)
{
    return dot(plane, vec4(centre, 1.0)) < -radius * length(plane.xyz);
}

bool particleVisible(ParticleRecord particle, EmitterSimData emitter, TemplateRenderData appearance)
{
    const uint EFFECT_VISIBLE = 1u << 0u;
    if ((uint(emitter.emissionRateAndPadding.y) & EFFECT_VISIBLE) == 0u) return false;

    vec3 viewPosition = (VIEW_MATRIX * vec4(particle.positionAge.xyz, 1.0)).xyz;
    float radius = abs(particle.baseSize);
    if (appearance.modes.z == 5u) radius *= 1.0 + length(particle.velocityLifetime.xyz);
    vec4 row0 = vec4(PROJECTION_MATRIX[0][0], PROJECTION_MATRIX[1][0], PROJECTION_MATRIX[2][0], PROJECTION_MATRIX[3][0]);
    vec4 row1 = vec4(PROJECTION_MATRIX[0][1], PROJECTION_MATRIX[1][1], PROJECTION_MATRIX[2][1], PROJECTION_MATRIX[3][1]);
    vec4 row2 = vec4(PROJECTION_MATRIX[0][2], PROJECTION_MATRIX[1][2], PROJECTION_MATRIX[2][2], PROJECTION_MATRIX[3][2]);
    vec4 row3 = vec4(PROJECTION_MATRIX[0][3], PROJECTION_MATRIX[1][3], PROJECTION_MATRIX[2][3], PROJECTION_MATRIX[3][3]);
    if (outsidePlane(row3 + row0, viewPosition, radius) || outsidePlane(row3 - row0, viewPosition, radius) ||
        outsidePlane(row3 + row1, viewPosition, radius) || outsidePlane(row3 - row1, viewPosition, radius) ||
        outsidePlane(row3 + row2, viewPosition, radius) || outsidePlane(row3 - row2, viewPosition, radius)) return false;

    if (appearance.culling.x > 0.0 && length(viewPosition) - radius > appearance.culling.x) return false;
    if (appearance.culling.y > 0.0)
    {
        vec4 clip = PROJECTION_MATRIX * vec4(viewPosition, 1.0);
        float divisor = abs(PROJECTION_MATRIX[2][3]) > 0.5 ? max(abs(clip.w), 1.0e-6) : 1.0;
        vec2 radiusPixels = radius * vec2(abs(PROJECTION_MATRIX[0][0]), abs(PROJECTION_MATRIX[1][1])) *
            VIEWPORT_SIZE.xy * 0.5 / divisor;
        if (2.0 * max(radiusPixels.x, radiusPixels.y) < appearance.culling.y) return false;
    }
    return true;
}

void main()
{
    uint activeCount = ACTIVE_LIST_INDEX == 0u ? ACTIVE_COUNT_A : ACTIVE_COUNT_B;
    uint activeOrdinal = gl_GlobalInvocationID.x;
    if (activeOrdinal >= activeCount) return;
    uint particleIndex = ACTIVE_LIST_INDEX == 0u ? ACTIVE_INDICES_A[activeOrdinal] : ACTIVE_INDICES_B[activeOrdinal];
    ParticleRecord particle = PARTICLES[particleIndex];
    if (particle.emitterIndex >= EMITTER_COUNT) return;
    EmitterSimData emitter = EMITTERS[particle.emitterIndex];
    uint templateIndex = emitter.emissionState.w;
    if (templateIndex >= TEMPLATE_COUNT) return;
    atomicAdd(TEMPLATE_COUNTS[templateIndex], 1u);
    if (particleVisible(particle, emitter, TEMPLATES[templateIndex]))
        atomicAdd(COMPACTION_VALUES[TEMPLATE_CAPACITY * 2u + templateIndex], 1u);
}
)MPP";

	// A single invocation is intentional: template tables are capped at 4096,
	// making this constant-sized scan small while avoiding a multi-level scan and
	// still keeping every value GPU-resident. It also authors every indirect draw
	// command, including empty templates.
	inline char const* ParticleCompactionPrefixComputeShader = R"MPP(#version 430

layout(local_size_x = 1) in;

layout(std430, binding = 4) restrict buffer ParticleCounters
{
    uint FREE_COUNT;
    uint ACTIVE_COUNT_A;
    uint ACTIVE_COUNT_B;
    uint DROPPED_SPAWN_COUNT;
    uint SPAWNED_COUNT;
    uint KILLED_COUNT;
    uint RENDERED_COUNT;
    uint CULLED_COUNT;
    uint TEMPLATE_COUNTS[];
};
layout(std430, binding = 6) restrict buffer ParticleCompactionScratch
{
    uint COMPACTION_VALUES[];
};
layout(std430, binding = 7) restrict writeonly buffer ParticleIndirectCommands
{
    uint INDIRECT_COMMANDS[];
};

uniform uint TEMPLATE_COUNT;
uniform uint TEMPLATE_CAPACITY;

void main()
{
    uint activeOffset = 0u;
    uint visibleOffset = 0u;
    for (uint templateIndex = 0u; templateIndex < TEMPLATE_COUNT; ++templateIndex)
    {
        uint activeCount = TEMPLATE_COUNTS[templateIndex];
        uint visibleCount = COMPACTION_VALUES[TEMPLATE_CAPACITY * 2u + templateIndex];
        COMPACTION_VALUES[templateIndex] = visibleOffset;
        uint commandOffset = templateIndex * 4u;
        INDIRECT_COMMANDS[commandOffset] = 4u;
        INDIRECT_COMMANDS[commandOffset + 1u] = visibleCount;
        INDIRECT_COMMANDS[commandOffset + 2u] = visibleOffset * 4u;
        INDIRECT_COMMANDS[commandOffset + 3u] = templateIndex;
        activeOffset += activeCount;
        visibleOffset += visibleCount;
    }
    // Statistics prepare stores the requested spawn count and starting active
    // count in these fields. Visibility never affects lifetime or spawn budgets.
    uint spawned = SPAWNED_COUNT >= DROPPED_SPAWN_COUNT ? SPAWNED_COUNT - DROPPED_SPAWN_COUNT : 0u;
    uint startAndSpawned = KILLED_COUNT + spawned;
    SPAWNED_COUNT = spawned;
    KILLED_COUNT = startAndSpawned >= activeOffset ? startAndSpawned - activeOffset : 0u;
    RENDERED_COUNT = visibleOffset;
    CULLED_COUNT = activeOffset >= visibleOffset ? activeOffset - visibleOffset : 0u;
}
)MPP";

	// Scatters each survivor into its template's prefix-summed range. Atomic
	// cursors make order within a range unspecified, but ranges themselves are
	// exact and non-overlapping.
	inline char const* ParticleCompactionScatterComputeShader = R"MPP(#version 430

layout(local_size_x = MPP_PARTICLE_WORK_GROUP_SIZE) in;
layout(std140, binding = 3) uniform CameraFrame
{
    mat4 VIEW_MATRIX;
    mat4 PROJECTION_MATRIX;
    mat4 INVERSE_PROJECTION_MATRIX;
    vec4 VIEWPORT_SIZE;
    vec4 NEAR_FAR_TIME;
};

struct ParticleRecord
{
    vec4 positionAge;
    vec4 velocityLifetime;
    uint packedColour;
    float baseSize;
    float rotation;
    float angularVelocity;
    uint emitterIndex;
    uint seed;
    uint flags;
    uint padding;
};

struct EmitterSimData
{
    mat4 transform;
    vec4 shapeParameters;
    vec4 initialVelocityMin;
    vec4 initialVelocityMax;
    vec4 colourMin;
    vec4 colourMax;
    vec4 lifetimeSizeRanges;
    vec4 rotationRanges;
    uvec4 shapeSeedModulesBudget;
    uvec4 emissionState;
    vec4 emissionRateAndPadding;
    vec4 parameterMultipliers0;
    vec4 parameterMultipliers1;
    vec4 gravityAndDrag;
    vec4 noiseFrequencyStrength;
    vec4 noiseScrollAndTimeScale;
};

struct TemplateRenderData
{
    uvec4 textureAndAtlas;
    vec4 tintAndAlpha;
    vec4 appearance;
    uvec4 modes;
    vec4 culling;
    uvec4 sorting;
};

layout(std430, binding = 0) restrict readonly buffer ParticlePool
{
    ParticleRecord PARTICLES[];
};
layout(std430, binding = 1) restrict writeonly buffer ParticleRenderIndices
{
    uint RENDER_INDICES[];
};
layout(std430, binding = 2) restrict readonly buffer ParticleActiveIndicesA
{
    uint ACTIVE_INDICES_A[];
};
layout(std430, binding = 3) restrict readonly buffer ParticleActiveIndicesB
{
    uint ACTIVE_INDICES_B[];
};
layout(std430, binding = 4) restrict readonly buffer ParticleCounters
{
    uint FREE_COUNT;
    uint ACTIVE_COUNT_A;
    uint ACTIVE_COUNT_B;
    uint DROPPED_SPAWN_COUNT;
    uint SPAWNED_COUNT;
    uint KILLED_COUNT;
    uint RENDERED_COUNT;
    uint CULLED_COUNT;
    uint TEMPLATE_COUNTS[];
};
layout(std430, binding = 5) restrict readonly buffer ParticleEmitters
{
    EmitterSimData EMITTERS[];
};
layout(std430, binding = 6) restrict buffer ParticleCompactionScratch
{
    uint COMPACTION_VALUES[];
};
layout(std430, binding = 7) restrict readonly buffer ParticleTemplates
{
    TemplateRenderData TEMPLATES[];
};

uniform uint ACTIVE_LIST_INDEX;
uniform uint EMITTER_COUNT;
uniform uint TEMPLATE_COUNT;
uniform uint TEMPLATE_CAPACITY;

bool outsidePlane(vec4 plane, vec3 centre, float radius)
{
    return dot(plane, vec4(centre, 1.0)) < -radius * length(plane.xyz);
}

bool particleVisible(ParticleRecord particle, EmitterSimData emitter, TemplateRenderData appearance)
{
    const uint EFFECT_VISIBLE = 1u << 0u;
    if ((uint(emitter.emissionRateAndPadding.y) & EFFECT_VISIBLE) == 0u) return false;
    vec3 viewPosition = (VIEW_MATRIX * vec4(particle.positionAge.xyz, 1.0)).xyz;
    float radius = abs(particle.baseSize);
    if (appearance.modes.z == 5u) radius *= 1.0 + length(particle.velocityLifetime.xyz);
    vec4 row0 = vec4(PROJECTION_MATRIX[0][0], PROJECTION_MATRIX[1][0], PROJECTION_MATRIX[2][0], PROJECTION_MATRIX[3][0]);
    vec4 row1 = vec4(PROJECTION_MATRIX[0][1], PROJECTION_MATRIX[1][1], PROJECTION_MATRIX[2][1], PROJECTION_MATRIX[3][1]);
    vec4 row2 = vec4(PROJECTION_MATRIX[0][2], PROJECTION_MATRIX[1][2], PROJECTION_MATRIX[2][2], PROJECTION_MATRIX[3][2]);
    vec4 row3 = vec4(PROJECTION_MATRIX[0][3], PROJECTION_MATRIX[1][3], PROJECTION_MATRIX[2][3], PROJECTION_MATRIX[3][3]);
    if (outsidePlane(row3 + row0, viewPosition, radius) || outsidePlane(row3 - row0, viewPosition, radius) ||
        outsidePlane(row3 + row1, viewPosition, radius) || outsidePlane(row3 - row1, viewPosition, radius) ||
        outsidePlane(row3 + row2, viewPosition, radius) || outsidePlane(row3 - row2, viewPosition, radius)) return false;
    if (appearance.culling.x > 0.0 && length(viewPosition) - radius > appearance.culling.x) return false;
    if (appearance.culling.y > 0.0)
    {
        vec4 clip = PROJECTION_MATRIX * vec4(viewPosition, 1.0);
        float divisor = abs(PROJECTION_MATRIX[2][3]) > 0.5 ? max(abs(clip.w), 1.0e-6) : 1.0;
        vec2 radiusPixels = radius * vec2(abs(PROJECTION_MATRIX[0][0]), abs(PROJECTION_MATRIX[1][1])) *
            VIEWPORT_SIZE.xy * 0.5 / divisor;
        if (2.0 * max(radiusPixels.x, radiusPixels.y) < appearance.culling.y) return false;
    }
    return true;
}

void main()
{
    uint activeCount = ACTIVE_LIST_INDEX == 0u ? ACTIVE_COUNT_A : ACTIVE_COUNT_B;
    uint activeOrdinal = gl_GlobalInvocationID.x;
    if (activeOrdinal >= activeCount) return;

    uint particleIndex = ACTIVE_LIST_INDEX == 0u ? ACTIVE_INDICES_A[activeOrdinal] : ACTIVE_INDICES_B[activeOrdinal];
    ParticleRecord particle = PARTICLES[particleIndex];
    if (particle.emitterIndex >= EMITTER_COUNT) return;
    EmitterSimData emitter = EMITTERS[particle.emitterIndex];
    uint templateIndex = emitter.emissionState.w;
    if (templateIndex >= TEMPLATE_COUNT || !particleVisible(particle, emitter, TEMPLATES[templateIndex])) return;

    uint cursor = atomicAdd(COMPACTION_VALUES[TEMPLATE_CAPACITY + templateIndex], 1u);
    RENDER_INDICES[COMPACTION_VALUES[templateIndex] + cursor] = particleIndex;
}
)MPP";

	// Reads one GPU-authored indirect draw command and turns its instance count
	// into the indirect work size shared by all radix stages for that appearance.
	inline char const* ParticleSortPrepareComputeShader = R"MPP(#version 430

layout(local_size_x = 1) in;
layout(std430, binding = 0) restrict readonly buffer ParticleIndirectCommands { uint INDIRECT_COMMANDS[]; };
layout(std430, binding = 1) restrict writeonly buffer ParticleSortDispatchCommand { uint DISPATCH_COMMAND[]; };
uniform uint TEMPLATE_INDEX;

void main()
{
    uint count = INDIRECT_COMMANDS[TEMPLATE_INDEX * 4u + 1u];
    DISPATCH_COMMAND[0] = (count + MPP_PARTICLE_WORK_GROUP_SIZE - 1u) / MPP_PARTICLE_WORK_GROUP_SIZE;
    DISPATCH_COMMAND[1] = 1u;
    DISPATCH_COMMAND[2] = 1u;
}
)MPP";

	// Generates an IEEE-754 depth key and particle-index pair for one opted-in
	// alpha appearance. Complementing the monotonic float key makes the ascending
	// radix result render far-to-near.
	inline char const* ParticleSortKeyComputeShader = R"MPP(#version 430

layout(local_size_x = MPP_PARTICLE_WORK_GROUP_SIZE) in;
layout(std140, binding = 3) uniform CameraFrame
{
    mat4 VIEW_MATRIX;
    mat4 PROJECTION_MATRIX;
    mat4 INVERSE_PROJECTION_MATRIX;
    vec4 VIEWPORT_SIZE;
    vec4 NEAR_FAR_TIME;
};
struct ParticleRecord { vec4 positionAge; vec4 velocityLifetime; uint packedColour; float baseSize; float rotation; float angularVelocity; uint emitterIndex; uint seed; uint flags; uint padding; };
layout(std430, binding = 0) restrict readonly buffer ParticlePool { ParticleRecord PARTICLES[]; };
layout(std430, binding = 1) restrict readonly buffer ParticleRenderIndices { uint RENDER_INDICES[]; };
layout(std430, binding = 2) restrict readonly buffer ParticleIndirectCommands { uint INDIRECT_COMMANDS[]; };
layout(std430, binding = 3) restrict writeonly buffer ParticleSortRecords { uvec2 SORT_RECORDS[]; };
uniform uint TEMPLATE_INDEX;

uint descendingFloatKey(float value)
{
    uint bits = floatBitsToUint(value);
    uint ascending = (bits & 0x80000000u) != 0u ? ~bits : bits ^ 0x80000000u;
    return ~ascending;
}

void main()
{
    uint command = TEMPLATE_INDEX * 4u;
    uint count = INDIRECT_COMMANDS[command + 1u];
    uint localOrdinal = gl_GlobalInvocationID.x;
    if (localOrdinal >= count) return;
    uint rangeOffset = INDIRECT_COMMANDS[command + 2u] / 4u;
    uint ordinal = rangeOffset + localOrdinal;
    uint particleIndex = RENDER_INDICES[ordinal];
    float viewDepth = -(VIEW_MATRIX * vec4(PARTICLES[particleIndex].positionAge.xyz, 1.0)).z;
    SORT_RECORDS[ordinal] = uvec2(descendingFloatKey(viewDepth), particleIndex);
}
)MPP";

	// One histogram belongs to each work group. Reusing the same scratch from one
	// appearance and nibble to the next avoids both clears and capacity-sized work
	// for additive appearances.
	inline char const* ParticleRadixHistogramComputeShader = R"MPP(#version 430

layout(local_size_x = MPP_PARTICLE_WORK_GROUP_SIZE) in;
layout(std430, binding = 0) restrict readonly buffer ParticleSortRecords { uvec2 SORT_RECORDS[]; };
layout(std430, binding = 1) restrict readonly buffer ParticleIndirectCommands { uint INDIRECT_COMMANDS[]; };
layout(std430, binding = 2) restrict writeonly buffer ParticleRadixHistogram { uint GROUP_BUCKETS[]; };
uniform uint TEMPLATE_INDEX;
uniform uint RADIX_SHIFT;
shared uint LOCAL_BUCKETS[16];

void main()
{
    for (uint bucket = gl_LocalInvocationID.x; bucket < 16u; bucket += gl_WorkGroupSize.x)
        LOCAL_BUCKETS[bucket] = 0u;
    barrier();
    uint command = TEMPLATE_INDEX * 4u;
    uint count = INDIRECT_COMMANDS[command + 1u];
    uint localOrdinal = gl_GlobalInvocationID.x;
    if (localOrdinal < count)
    {
        uint rangeOffset = INDIRECT_COMMANDS[command + 2u] / 4u;
        uint digit = (SORT_RECORDS[rangeOffset + localOrdinal].x >> RADIX_SHIFT) & 15u;
        atomicAdd(LOCAL_BUCKETS[digit], 1u);
    }
    barrier();
    for (uint bucket = gl_LocalInvocationID.x; bucket < 16u; bucket += gl_WorkGroupSize.x)
        GROUP_BUCKETS[gl_WorkGroupID.x * 16u + bucket] = LOCAL_BUCKETS[bucket];
}
)MPP";

	// The capped pool has at most 16,384 groups. A single deterministic scan is
	// small relative to sorting the particles and supplies stable global offsets
	// without subgroup extensions or a CPU-visible count.
	inline char const* ParticleRadixPrefixComputeShader = R"MPP(#version 430

layout(local_size_x = 1) in;
layout(std430, binding = 0) restrict readonly buffer ParticleIndirectCommands { uint INDIRECT_COMMANDS[]; };
layout(std430, binding = 1) restrict buffer ParticleRadixHistogram { uint GROUP_BUCKETS[]; };
uniform uint TEMPLATE_INDEX;

void main()
{
    uint count = INDIRECT_COMMANDS[TEMPLATE_INDEX * 4u + 1u];
    uint groupCount = (count + MPP_PARTICLE_WORK_GROUP_SIZE - 1u) / MPP_PARTICLE_WORK_GROUP_SIZE;
    uint bucketBase = 0u;
    for (uint bucket = 0u; bucket < 16u; ++bucket)
    {
        uint running = 0u;
        for (uint group = 0u; group < groupCount; ++group)
        {
            uint address = group * 16u + bucket;
            uint groupCountForBucket = GROUP_BUCKETS[address];
            GROUP_BUCKETS[address] = bucketBase + running;
            running += groupCountForBucket;
        }
        bucketBase += running;
    }
}
)MPP";

	// Stable scatter is the defining radix property. Each lane computes its rank
	// among earlier lanes with the same nibble; the prefix stage contributes every
	// earlier work group's count. Eight four-bit passes cover the complete key.
	inline char const* ParticleRadixScatterComputeShader = R"MPP(#version 430

layout(local_size_x = MPP_PARTICLE_WORK_GROUP_SIZE) in;
layout(std430, binding = 0) restrict readonly buffer ParticleSortInput { uvec2 INPUT_RECORDS[]; };
layout(std430, binding = 1) restrict writeonly buffer ParticleSortOutput { uvec2 OUTPUT_RECORDS[]; };
layout(std430, binding = 2) restrict readonly buffer ParticleIndirectCommands { uint INDIRECT_COMMANDS[]; };
layout(std430, binding = 3) restrict readonly buffer ParticleRadixHistogram { uint GROUP_BUCKETS[]; };
uniform uint TEMPLATE_INDEX;
uniform uint RADIX_SHIFT;
shared uint LOCAL_DIGITS[MPP_PARTICLE_WORK_GROUP_SIZE];

void main()
{
    uint command = TEMPLATE_INDEX * 4u;
    uint count = INDIRECT_COMMANDS[command + 1u];
    uint localOrdinal = gl_GlobalInvocationID.x;
    uint lane = gl_LocalInvocationID.x;
    uint rangeOffset = INDIRECT_COMMANDS[command + 2u] / 4u;
    uvec2 record = uvec2(0u);
    uint digit = 16u;
    if (localOrdinal < count)
    {
        record = INPUT_RECORDS[rangeOffset + localOrdinal];
        digit = (record.x >> RADIX_SHIFT) & 15u;
    }
    LOCAL_DIGITS[lane] = digit;
    barrier();
    if (localOrdinal >= count) return;
    uint localRank = 0u;
    for (uint earlier = 0u; earlier < lane; ++earlier)
        if (LOCAL_DIGITS[earlier] == digit) ++localRank;
    uint destination = GROUP_BUCKETS[gl_WorkGroupID.x * 16u + digit] + localRank;
    OUTPUT_RECORDS[rangeOffset + destination] = record;
}
)MPP";

	inline char const* ParticleSortFinalizeComputeShader = R"MPP(#version 430

layout(local_size_x = MPP_PARTICLE_WORK_GROUP_SIZE) in;
layout(std430, binding = 0) restrict readonly buffer ParticleSortRecords { uvec2 SORT_RECORDS[]; };
layout(std430, binding = 1) restrict writeonly buffer ParticleRenderIndices { uint RENDER_INDICES[]; };
layout(std430, binding = 2) restrict readonly buffer ParticleIndirectCommands { uint INDIRECT_COMMANDS[]; };
uniform uint TEMPLATE_INDEX;

void main()
{
    uint command = TEMPLATE_INDEX * 4u;
    uint count = INDIRECT_COMMANDS[command + 1u];
    uint localOrdinal = gl_GlobalInvocationID.x;
    if (localOrdinal >= count) return;
    uint rangeOffset = INDIRECT_COMMANDS[command + 2u] / 4u;
    RENDER_INDICES[rangeOffset + localOrdinal] = SORT_RECORDS[rangeOffset + localOrdinal].y;
}
)MPP";

	// Attribute-less instanced billboards. Basis-only modes share square
	// expansion; velocity-stretched mode supplies independent half-extents.
	inline char const* ParticleDrawVertexShader = R"MPP(#version 430

layout(std140, binding = 3) uniform CameraFrame
{
    mat4 VIEW_MATRIX;
    mat4 PROJECTION_MATRIX;
    mat4 INVERSE_PROJECTION_MATRIX;
    vec4 VIEWPORT_SIZE;
    vec4 NEAR_FAR_TIME;
};

const uint BILLBOARD_CAMERA_FACING = 0u;
const uint BILLBOARD_SCREEN_ALIGNED = 1u;
const uint BILLBOARD_CYLINDRICAL = 2u;
const uint BILLBOARD_AXIS_LOCKED = 3u;
const uint BILLBOARD_VELOCITY_ALIGNED = 4u;
const uint BILLBOARD_VELOCITY_STRETCHED = 5u;
const uint ANIMATION_PLAYBACK_MASK = 0xffu;
const uint ANIMATION_FRAME_OVER_LIFE = 1u;
const uint ANIMATION_FIXED_RATE = 2u;
const uint ANIMATION_RANDOM_START = 1u << 8u;

struct ParticleRecord
{
    vec4 positionAge;
    vec4 velocityLifetime;
    uint packedColour;
    float baseSize;
    float rotation;
    float angularVelocity;
    uint emitterIndex;
    uint seed;
    uint flags;
    uint padding;
};

struct EmitterSimData
{
    mat4 transform;
    vec4 shapeParameters;
    vec4 initialVelocityMin;
    vec4 initialVelocityMax;
    vec4 colourMin;
    vec4 colourMax;
    vec4 lifetimeSizeRanges;
    vec4 rotationRanges;
    uvec4 shapeSeedModulesBudget;
    uvec4 emissionState;
    vec4 emissionRateAndPadding;
    vec4 parameterMultipliers0;
    vec4 parameterMultipliers1;
    vec4 gravityAndDrag;
    vec4 noiseFrequencyStrength;
    vec4 noiseScrollAndTimeScale;
};

struct TemplateRenderData
{
    uvec4 textureAndAtlas;
    vec4 tintAndAlpha;
    vec4 appearance;
    uvec4 modes;
    vec4 culling;
    uvec4 sorting;
};

layout(std430, binding = 0) restrict readonly buffer ParticlePool { ParticleRecord PARTICLES[]; };
layout(std430, binding = 1) restrict readonly buffer ParticleRenderIndices { uint RENDER_INDICES[]; };
layout(std430, binding = 5) restrict readonly buffer ParticleEmitters { EmitterSimData EMITTERS[]; };
layout(std430, binding = 6) restrict readonly buffer ParticleTemplates { TemplateRenderData TEMPLATES[]; };
uniform sampler2D PARTICLE_CURVE_LUT;

out vec2 PARTICLE_UV;
out vec2 PARTICLE_CORNER;
out vec4 PARTICLE_TINT;
out float PARTICLE_VIEW_DEPTH;
flat out uvec2 PARTICLE_TEXTURE_HANDLE;
flat out float PARTICLE_SOFT_DISTANCE;

vec3 safeNormal(vec3 value, vec3 fallback)
{
    float magnitudeSquared = dot(value, value);
    return magnitudeSquared > 1e-10 ? value * inversesqrt(magnitudeSquared) : fallback;
}

vec4 sampleCurveRow(float normalizedLife, float row)
{
    vec2 dimensions = vec2(textureSize(PARTICLE_CURVE_LUT, 0));
    vec2 texel = vec2(clamp(normalizedLife, 0.0, 1.0) * (dimensions.x - 1.0) + 0.5, row + 0.5);
    return texture(PARTICLE_CURVE_LUT, texel / dimensions);
}

void billboardBasis(ParticleRecord particle, EmitterSimData emitter, uint mode,
    vec3 cameraPosition, vec3 screenRight, vec3 screenUp, vec3 viewForward,
    out vec3 right, out vec3 up)
{
    vec3 toCamera = safeNormal(cameraPosition - particle.positionAge.xyz, -viewForward);
    if (mode == BILLBOARD_SCREEN_ALIGNED)
    {
        right = screenRight;
        up = screenUp;
    }
    else if (mode == BILLBOARD_CYLINDRICAL)
    {
        up = vec3(0.0, 1.0, 0.0);
        right = safeNormal(cross(up, toCamera), screenRight);
    }
    else if (mode == BILLBOARD_AXIS_LOCKED)
    {
        up = safeNormal(mat3(emitter.transform) * vec3(0.0, 1.0, 0.0), screenUp);
        right = safeNormal(cross(up, toCamera), screenRight);
    }
    else if (mode == BILLBOARD_VELOCITY_ALIGNED || mode == BILLBOARD_VELOCITY_STRETCHED)
    {
        vec3 projectedVelocity = particle.velocityLifetime.xyz - viewForward * dot(particle.velocityLifetime.xyz, viewForward);
        up = safeNormal(projectedVelocity, screenUp);
        right = safeNormal(cross(viewForward, up), screenRight);
    }
    else
    {
        right = safeNormal(cross(screenUp, toCamera), screenRight);
        up = safeNormal(cross(toCamera, right), screenUp);
    }
}

vec2 particleHalfExtents(ParticleRecord particle, uint mode, vec3 viewForward, float size)
{
    if (mode != BILLBOARD_VELOCITY_STRETCHED) return vec2(size);
    vec3 projectedVelocity = particle.velocityLifetime.xyz - viewForward * dot(particle.velocityLifetime.xyz, viewForward);
    // Screen-plane speed contributes a dimensionless stretch ratio: stationary
    // particles remain square and faster motion lengthens only the velocity axis.
    return vec2(size, size * (1.0 + length(projectedVelocity)));
}

vec3 expandParticleQuad(vec3 centre, vec3 right, vec3 up, vec2 corner,
    vec2 halfExtents, float rotation)
{
    vec2 offset = corner * 2.0 - 1.0;
    float sineRotation = sin(rotation);
    float cosineRotation = cos(rotation);
    offset = mat2(cosineRotation, sineRotation, -sineRotation, cosineRotation) * offset;
    return centre + right * offset.x * halfExtents.x + up * offset.y * halfExtents.y;
}

uint flipbookFrame(ParticleRecord particle, TemplateRenderData appearance)
{
    uint frameCount = max(1u, min(appearance.modes.x,
        max(1u, appearance.textureAndAtlas.z) * max(1u, appearance.textureAndAtlas.w)));
    uint animation = appearance.modes.y;
    uint frame = 0u;
    uint playback = animation & ANIMATION_PLAYBACK_MASK;
    if (playback == ANIMATION_FRAME_OVER_LIFE && particle.velocityLifetime.w > 0.0)
    {
        float life = clamp(particle.positionAge.w / particle.velocityLifetime.w, 0.0, 0.99999994);
        frame = min(uint(life * float(frameCount)), frameCount - 1u);
    }
    else if (playback == ANIMATION_FIXED_RATE)
        frame = uint(max(0.0, floor(particle.positionAge.w * max(0.0, appearance.appearance.z)))) % frameCount;
    if ((animation & ANIMATION_RANDOM_START) != 0u)
        frame = (frame + particle.seed % frameCount) % frameCount;
    return frame;
}

void main()
{
    // command.first is rangeOffset * 4: its low bits select a quad corner and
    // its upper bits retain the compact render-list offset.
    uint renderOrdinal = (uint(gl_VertexID) >> 2u) + uint(gl_InstanceID);
    ParticleRecord particle = PARTICLES[RENDER_INDICES[renderOrdinal]];
    EmitterSimData emitter = EMITTERS[particle.emitterIndex];
    TemplateRenderData appearance = TEMPLATES[emitter.emissionState.w];

    vec2 corner = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1));
    mat3 inverseViewRotation = transpose(mat3(VIEW_MATRIX));
    vec3 screenRight = safeNormal(inverseViewRotation[0], vec3(1.0, 0.0, 0.0));
    vec3 screenUp = safeNormal(inverseViewRotation[1], vec3(0.0, 1.0, 0.0));
    vec3 viewForward = safeNormal(-inverseViewRotation[2], vec3(0.0, 0.0, -1.0));
    vec3 cameraPosition = inverseViewRotation * -VIEW_MATRIX[3].xyz;
    float normalizedLife = particle.velocityLifetime.w > 0.0 ?
        clamp(particle.positionAge.w / particle.velocityLifetime.w, 0.0, 1.0) : 0.0;
    float rowOffset = appearance.appearance.w;
    vec4 scalarCurves0 = sampleCurveRow(normalizedLife, rowOffset);
    vec4 scalarCurves1 = sampleCurveRow(normalizedLife, rowOffset + 1.0);
    vec3 colourOverLife = sampleCurveRow(normalizedLife, rowOffset + 2.0).rgb;

    vec3 right;
    vec3 up;
    billboardBasis(particle, emitter, appearance.modes.z, cameraPosition,
        screenRight, screenUp, viewForward, right, up);
    float size = particle.baseSize * scalarCurves0.x;
    vec2 halfExtents = particleHalfExtents(particle, appearance.modes.z, viewForward, size);
    // Rotation would turn the long axis away from projected velocity, so the
    // stretched mode deliberately treats velocity as its complete orientation.
    float rotation = appearance.modes.z == BILLBOARD_VELOCITY_STRETCHED ? 0.0 : particle.rotation;
    vec3 worldPosition = expandParticleQuad(particle.positionAge.xyz, right, up,
        corner, halfExtents, rotation);

    uint columns = max(1u, appearance.textureAndAtlas.z);
    uint rows = max(1u, appearance.textureAndAtlas.w);
    uint frame = flipbookFrame(particle, appearance);
    vec2 atlasCell = vec2(float(frame % columns), float(frame / columns));
    PARTICLE_UV = (atlasCell + corner) / vec2(float(columns), float(rows));
    PARTICLE_CORNER = corner;
    PARTICLE_TINT = unpackUnorm4x8(particle.packedColour) * appearance.tintAndAlpha;
    PARTICLE_TINT.rgb *= colourOverLife * appearance.appearance.x * emitter.parameterMultipliers1.y * scalarCurves1.y;
    PARTICLE_TINT.a *= scalarCurves0.y * emitter.parameterMultipliers1.x;
    PARTICLE_VIEW_DEPTH = -(VIEW_MATRIX * vec4(particle.positionAge.xyz, 1.0)).z;
    PARTICLE_TEXTURE_HANDLE = appearance.textureAndAtlas.xy;
    PARTICLE_SOFT_DISTANCE = max(0.0, appearance.appearance.y);
    gl_Position = PROJECTION_MATRIX * VIEW_MATRIX * vec4(worldPosition, 1.0);
}
)MPP";

	inline char const* ParticleDrawFragmentShader = R"MPP(#version 430

#if MPP_PARTICLE_BINDLESS_TEXTURES
#extension GL_ARB_bindless_texture : require
#endif

layout(std140, binding = 3) uniform CameraFrame
{
    mat4 VIEW_MATRIX;
    mat4 PROJECTION_MATRIX;
    mat4 INVERSE_PROJECTION_MATRIX;
    vec4 VIEWPORT_SIZE;
    vec4 NEAR_FAR_TIME;
};

in vec2 PARTICLE_UV;
in vec2 PARTICLE_CORNER;
in vec4 PARTICLE_TINT;
in float PARTICLE_VIEW_DEPTH;
flat in uvec2 PARTICLE_TEXTURE_HANDLE;
flat in float PARTICLE_SOFT_DISTANCE;
uniform sampler2D SCENE_DEPTH;
uniform int HAS_SCENE_DEPTH;
layout(location = 0) out vec4 FRAGMENT_COLOUR;
layout(location = 1) out vec4 FRAGMENT_BLOOM;

float linearViewDepth(float depth)
{
    float nearPlane = NEAR_FAR_TIME.x;
    float farPlane = NEAR_FAR_TIME.y;
    float ndc = depth * 2.0 - 1.0;
    return (2.0 * nearPlane * farPlane) /
        max(farPlane + nearPlane - ndc * (farPlane - nearPlane), 0.000001);
}

void main()
{
    vec4 albedo = vec4(1.0);
#if MPP_PARTICLE_BINDLESS_TEXTURES
    if (any(notEqual(PARTICLE_TEXTURE_HANDLE, uvec2(0u))))
        albedo = texture(sampler2D(PARTICLE_TEXTURE_HANDLE), PARTICLE_UV);
#endif
    float edge = length(PARTICLE_CORNER * 2.0 - 1.0);
    float coverage = 1.0 - smoothstep(0.75, 1.0, edge);
    float softFade = 1.0;
    if (HAS_SCENE_DEPTH != 0 && PARTICLE_SOFT_DISTANCE > 0.0)
    {
        vec2 depthUv = gl_FragCoord.xy / max(VIEWPORT_SIZE.xy, vec2(1.0));
        float sceneDepth = texture(SCENE_DEPTH, depthUv).r;
        if (sceneDepth < 1.0)
            softFade = clamp((linearViewDepth(sceneDepth) - PARTICLE_VIEW_DEPTH) /
                PARTICLE_SOFT_DISTANCE, 0.0, 1.0);
    }
    vec4 colour = PARTICLE_TINT * albedo;
    colour.a *= coverage * softFade;
#if MPP_PARTICLE_WEIGHTED_OIT
    float alpha = clamp(colour.a, 0.0, 1.0);
    // McGuire/Bavoil weighted blended OIT: favour opaque, near fragments while
    // keeping every operation commutative. Revealage is stored as optical depth
    // so both attachments can use the same authored additive blend state.
    float alphaWeight = pow(min(1.0, alpha * 10.0) + 0.01, 3.0);
    float depthWeight = pow(1.0 - gl_FragCoord.z * 0.9, 3.0);
    float weight = clamp(alphaWeight * 1.0e3 * depthWeight, 1.0e-2, 3.0e3);
    FRAGMENT_COLOUR = vec4(colour.rgb * alpha, alpha) * weight;
    FRAGMENT_BLOOM = vec4(-log(max(1.0 - alpha, 1.0e-5)), 0.0, 0.0, 0.0);
#else
    FRAGMENT_COLOUR = colour;
    FRAGMENT_BLOOM = vec4(colour.rgb, colour.a);
#endif
}
)MPP";

	// Composite the weighted average over the scene. The second accumulation
	// texture contains summed optical depth, whose exponential is mathematically
	// the same order-independent revealage product used by the two-blend-function
	// formulation, without requiring per-attachment blend state.
	inline char const* ParticleWeightedOitResolveFragmentShader = R"MPP(
@@Version

@@Uniform(int HAS_BLOOM);
@@Texture(sampler2D SCENE);
@@Texture(sampler2D ACCUMULATION);
@@Texture(sampler2D OPTICAL_DEPTH);
@@Texture(sampler2D BLOOM);

void main()
{
    vec2 uv = @In(TEXCOORDS);
    vec4 scene = texture(@Texture(SCENE), uv);
    vec4 accumulation = texture(@Texture(ACCUMULATION), uv);
    float opticalDepth = max(texture(@Texture(OPTICAL_DEPTH), uv).r, 0.0);
    float transmittance = exp(-opticalDepth);
    float opacity = 1.0 - transmittance;
    vec3 weightedColour = accumulation.rgb / max(accumulation.a, 1.0e-5);
    vec3 particleColour = accumulation.a > 1.0e-5 ? weightedColour : vec3(0.0);

    @Out(vec4 COLOUR) = vec4(
        particleColour * opacity + scene.rgb * transmittance,
        opacity + scene.a * transmittance);

    vec4 bloom = @Uniform(HAS_BLOOM) != 0 ? texture(@Texture(BLOOM), uv) : vec4(0.0);
    @Out(vec4 BLOOM_MASK) = vec4(
        particleColour * opacity + bloom.rgb * transmittance,
        opacity + bloom.a * transmittance);
}
)MPP";
}
