#version 430

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
    vec4 curlNoiseFrequencyStrength;
    vec4 curlNoiseScrollAndTimeScale;
    vec4 turbulenceFrequencyStrength;
    vec4 turbulenceScrollAndTimeScale;
    vec4 turbulenceOctavesLacunarityGain;
    vec4 vectorFieldFrequencyStrength;
    vec4 vectorFieldScrollAndTimeScale;
    uvec4 collisionConfiguration;
    vec4 collisionParameters;
    uvec4 eventRange;
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
flat out uvec2 PARTICLE_TEXTURE_LAYER_FLAGS;
flat out float PARTICLE_SOFT_DISTANCE;
flat out float PARTICLE_DISTORTION_STRENGTH;

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
    PARTICLE_TEXTURE_LAYER_FLAGS = appearance.textureAndAtlas.xy;
    PARTICLE_SOFT_DISTANCE = max(0.0, appearance.appearance.y);
    PARTICLE_DISTORTION_STRENGTH = appearance.culling.w;
    gl_Position = PROJECTION_MATRIX * VIEW_MATRIX * vec4(worldPosition, 1.0);
}
