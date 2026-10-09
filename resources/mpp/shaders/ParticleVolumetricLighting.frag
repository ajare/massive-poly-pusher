#version 430

layout(std140, binding = 3) uniform CameraFrame
{
    mat4 VIEW_MATRIX;
    mat4 PROJECTION_MATRIX;
    mat4 INVERSE_PROJECTION_MATRIX;
    vec4 VIEWPORT_SIZE;
    vec4 NEAR_FAR_TIME;
};

in vec2 VOLUME_LOCAL;
flat in vec4 VOLUME_COLOUR_INTENSITY;
flat in vec2 VOLUME_DEPTH_RANGE;
flat in float VOLUME_INTENSITY;
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
    float radiusSquared = dot(VOLUME_LOCAL, VOLUME_LOCAL);
    if (radiusSquared >= 1.0) discard;
    float halfPath = VOLUME_DEPTH_RANGE.y * sqrt(max(1.0 - radiusSquared, 0.0));
    float nearDepth = max(VOLUME_DEPTH_RANGE.x - halfPath, 0.0);
    float farDepth = VOLUME_DEPTH_RANGE.x + halfPath;
    float visibleFraction = 1.0;
    if (HAS_SCENE_DEPTH != 0)
    {
        vec2 uv = gl_FragCoord.xy / max(VIEWPORT_SIZE.xy, vec2(1.0));
        float sampledDepth = texture(SCENE_DEPTH, uv).r;
        if (sampledDepth < 1.0)
            visibleFraction = clamp((linearViewDepth(sampledDepth) - nearDepth) /
                max(farDepth - nearDepth, 0.000001), 0.0, 1.0);
    }
    float integratedPath = 2.0 * sqrt(max(1.0 - radiusSquared, 0.0));
    vec3 radiance = VOLUME_COLOUR_INTENSITY.rgb * VOLUME_COLOUR_INTENSITY.a *
        VOLUME_INTENSITY * integratedPath * visibleFraction;
    FRAGMENT_COLOUR = vec4(radiance, 0.0);
    FRAGMENT_BLOOM = vec4(radiance, 0.0);
}
