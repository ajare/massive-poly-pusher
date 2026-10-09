#version 430

struct VolumetricLightingData
{
    vec4 positionAndRange;
    vec4 colourAndIntensity;
    vec4 volumetricAndPadding;
    uvec4 flagsAndPadding;
};

layout(std430, binding = 0) restrict readonly buffer ParticleVolumetricLighting
{
    VolumetricLightingData LIGHTING[];
};

layout(std140, binding = 3) uniform CameraFrame
{
    mat4 VIEW_MATRIX;
    mat4 PROJECTION_MATRIX;
    mat4 INVERSE_PROJECTION_MATRIX;
    vec4 VIEWPORT_SIZE;
    vec4 NEAR_FAR_TIME;
};

out vec2 VOLUME_LOCAL;
flat out vec4 VOLUME_COLOUR_INTENSITY;
flat out vec2 VOLUME_DEPTH_RANGE;
flat out float VOLUME_INTENSITY;

void main()
{
    VolumetricLightingData lighting = LIGHTING[gl_InstanceID];
    uint flags = lighting.flagsAndPadding.x;
    float range = lighting.positionAndRange.w;
    float intensity = lighting.volumetricAndPadding.x;
    if ((flags & 4u) == 0u || (flags & 8u) == 0u || range <= 0.0 || intensity <= 0.0)
    {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        return;
    }

    const vec2 corners[4] = vec2[4](
        vec2(-1.0, -1.0), vec2(1.0, -1.0),
        vec2(-1.0,  1.0), vec2(1.0,  1.0));
    vec2 corner = corners[gl_VertexID];
    vec3 centreView = (VIEW_MATRIX * vec4(lighting.positionAndRange.xyz, 1.0)).xyz;
    vec3 billboardView = centreView + vec3(corner * range, 0.0);
    VOLUME_LOCAL = corner;
    VOLUME_COLOUR_INTENSITY = lighting.colourAndIntensity;
    VOLUME_DEPTH_RANGE = vec2(-centreView.z, range);
    VOLUME_INTENSITY = intensity;
    gl_Position = PROJECTION_MATRIX * vec4(billboardView, 1.0);
}
