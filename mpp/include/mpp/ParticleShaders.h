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
    }
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

	// Counts the globally interleaved survivor list by emitter template. These are
	// the same counters used by spawn-budget reservations and later copied by the
	// asynchronous statistics path.
	inline char const* ParticleCompactionCountComputeShader = R"MPP(#version 430

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

layout(std430, binding = 0) restrict readonly buffer ParticlePool
{
    ParticleRecord PARTICLES[];
};
layout(std430, binding = 2) restrict readonly buffer ParticleActiveIndicesA
{
    uint ACTIVE_INDICES_A[];
};
layout(std430, binding = 3) restrict readonly buffer ParticleActiveIndicesB
{
    uint ACTIVE_INDICES_B[];
};
layout(std430, binding = 4) restrict buffer ParticleCounters
{
    uint FREE_COUNT;
    uint ACTIVE_COUNT_A;
    uint ACTIVE_COUNT_B;
    uint DROPPED_SPAWN_COUNT;
    uint TEMPLATE_COUNTS[];
};
layout(std430, binding = 5) restrict readonly buffer ParticleEmitters
{
    EmitterSimData EMITTERS[];
};

uniform uint ACTIVE_LIST_INDEX;
uniform uint EMITTER_COUNT;
uniform uint TEMPLATE_COUNT;

void main()
{
    uint activeCount = ACTIVE_LIST_INDEX == 0u ? ACTIVE_COUNT_A : ACTIVE_COUNT_B;
    uint activeOrdinal = gl_GlobalInvocationID.x;
    if (activeOrdinal >= activeCount) return;

    uint particleIndex = ACTIVE_LIST_INDEX == 0u ? ACTIVE_INDICES_A[activeOrdinal] : ACTIVE_INDICES_B[activeOrdinal];
    uint emitterIndex = PARTICLES[particleIndex].emitterIndex;
    if (emitterIndex >= EMITTER_COUNT) return;
    uint templateIndex = EMITTERS[emitterIndex].emissionState.w;
    if (templateIndex < TEMPLATE_COUNT)
        atomicAdd(TEMPLATE_COUNTS[templateIndex], 1u);
}
)MPP";

	// A single invocation is intentional: template tables are capped at 4096,
	// making this constant-sized scan small while avoiding a multi-level scan and
	// still keeping every value GPU-resident. It also authors every indirect draw
	// command, including empty templates.
	inline char const* ParticleCompactionPrefixComputeShader = R"MPP(#version 430

layout(local_size_x = 1) in;

layout(std430, binding = 4) restrict readonly buffer ParticleCounters
{
    uint FREE_COUNT;
    uint ACTIVE_COUNT_A;
    uint ACTIVE_COUNT_B;
    uint DROPPED_SPAWN_COUNT;
    uint TEMPLATE_COUNTS[];
};
layout(std430, binding = 6) restrict writeonly buffer ParticleCompactionScratch
{
    uint COMPACTION_VALUES[];
};
layout(std430, binding = 7) restrict writeonly buffer ParticleIndirectCommands
{
    uint INDIRECT_COMMANDS[];
};

uniform uint TEMPLATE_COUNT;

void main()
{
    uint offset = 0u;
    for (uint templateIndex = 0u; templateIndex < TEMPLATE_COUNT; ++templateIndex)
    {
        uint count = TEMPLATE_COUNTS[templateIndex];
        COMPACTION_VALUES[templateIndex] = offset;
        uint commandOffset = templateIndex * 4u;
        INDIRECT_COMMANDS[commandOffset] = 4u;
        INDIRECT_COMMANDS[commandOffset + 1u] = count;
        INDIRECT_COMMANDS[commandOffset + 2u] = offset * 4u;
        INDIRECT_COMMANDS[commandOffset + 3u] = templateIndex;
        offset += count;
    }
}
)MPP";

	// Scatters each survivor into its template's prefix-summed range. Atomic
	// cursors make order within a range unspecified, but ranges themselves are
	// exact and non-overlapping.
	inline char const* ParticleCompactionScatterComputeShader = R"MPP(#version 430

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

uniform uint ACTIVE_LIST_INDEX;
uniform uint EMITTER_COUNT;
uniform uint TEMPLATE_COUNT;
uniform uint TEMPLATE_CAPACITY;

void main()
{
    uint activeCount = ACTIVE_LIST_INDEX == 0u ? ACTIVE_COUNT_A : ACTIVE_COUNT_B;
    uint activeOrdinal = gl_GlobalInvocationID.x;
    if (activeOrdinal >= activeCount) return;

    uint particleIndex = ACTIVE_LIST_INDEX == 0u ? ACTIVE_INDICES_A[activeOrdinal] : ACTIVE_INDICES_B[activeOrdinal];
    uint emitterIndex = PARTICLES[particleIndex].emitterIndex;
    if (emitterIndex >= EMITTER_COUNT) return;
    uint templateIndex = EMITTERS[emitterIndex].emissionState.w;
    if (templateIndex >= TEMPLATE_COUNT) return;

    uint cursor = atomicAdd(COMPACTION_VALUES[TEMPLATE_CAPACITY + templateIndex], 1u);
    RENDER_INDICES[COMPACTION_VALUES[templateIndex] + cursor] = particleIndex;
}
)MPP";

	// Attribute-less instanced billboards: only the compact render index list is
	// traversed. The full fixed-capacity pool is never used as a dispatch or draw
	// range.
	inline char const* ParticleDrawVertexShader = R"MPP(#version 430

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

layout(std430, binding = 0) restrict readonly buffer ParticlePool
{
    ParticleRecord PARTICLES[];
};
layout(std430, binding = 1) restrict readonly buffer ParticleRenderIndices
{
    uint RENDER_INDICES[];
};

out vec2 PARTICLE_CORNER;
out vec4 PARTICLE_TINT;

void main()
{
    // Indirect command.first is the template range offset multiplied by four.
    // The low two bits remain the attribute-less quad corner while the upper
    // bits select this command's contiguous run in the render index list.
    uint renderOrdinal = (uint(gl_VertexID) >> 2u) + uint(gl_InstanceID);
    uint particleIndex = RENDER_INDICES[renderOrdinal];
    ParticleRecord particle = PARTICLES[particleIndex];

    vec2 corner = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1));
    PARTICLE_CORNER = corner;
    PARTICLE_TINT = unpackUnorm4x8(particle.packedColour);

    vec3 viewPosition = (VIEW_MATRIX * vec4(particle.positionAge.xyz, 1.0)).xyz;
    vec2 billboardOffset = corner * 2.0 - 1.0;
    float sineRotation = sin(particle.rotation);
    float cosineRotation = cos(particle.rotation);
    billboardOffset = mat2(cosineRotation, sineRotation, -sineRotation, cosineRotation) * billboardOffset;
    viewPosition.xy += billboardOffset * particle.baseSize;
    gl_Position = PROJECTION_MATRIX * vec4(viewPosition, 1.0);
}
)MPP";

	inline char const* ParticleDrawFragmentShader = R"MPP(#version 430

in vec2 PARTICLE_CORNER;
in vec4 PARTICLE_TINT;

layout(location = 0) out vec4 FRAGMENT_COLOUR;

void main()
{
    float edge = length(PARTICLE_CORNER * 2.0 - 1.0);
    float coverage = 1.0 - smoothstep(0.75, 1.0, edge);

#if MPP_PARTICLE_BLEND_ADDITIVE
    FRAGMENT_COLOUR = vec4(PARTICLE_TINT.rgb * PARTICLE_TINT.a * coverage, 0.0);
#else
    FRAGMENT_COLOUR = vec4(PARTICLE_TINT.rgb, PARTICLE_TINT.a * coverage);
#endif
}
)MPP";
}
