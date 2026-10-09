#version 430

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
flat in uvec2 PARTICLE_TEXTURE_LAYER_FLAGS;
flat in float PARTICLE_SOFT_DISTANCE;
flat in float PARTICLE_DISTORTION_STRENGTH;
uniform sampler2D SCENE_DEPTH;
uniform sampler2DArray PARTICLE_ALBEDO_ARRAY;
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

vec3 srgbToLinear(vec3 encoded)
{
    bvec3 usePower = greaterThan(encoded, vec3(0.04045));
    vec3 low = encoded / 12.92;
    vec3 high = pow((encoded + 0.055) / 1.055, vec3(2.4));
    return mix(low, high, usePower);
}

void main()
{
    bool hasAlbedo = (PARTICLE_TEXTURE_LAYER_FLAGS.y & 1u) != 0u;
    vec4 albedo = hasAlbedo ?
        texture(PARTICLE_ALBEDO_ARRAY, vec3(PARTICLE_UV, float(PARTICLE_TEXTURE_LAYER_FLAGS.x))) : vec4(1.0);
    if ((PARTICLE_TEXTURE_LAYER_FLAGS.y & 2u) != 0u) albedo.rgb = srgbToLinear(albedo.rgb);
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
#if MPP_PARTICLE_DISTORTION
    // RG stores a normalized-screen UV offset. Authored textures use their RG
    // channels as a signed vector; an omitted texture gets a radial shockwave/
    // heat-haze fallback from the billboard centre. Alpha remains the mask.
    vec2 direction = hasAlbedo ? albedo.rg * 2.0 - 1.0 :
        normalize(PARTICLE_CORNER * 2.0 - 1.0 + vec2(1.0e-6));
    FRAGMENT_COLOUR = vec4(direction * PARTICLE_DISTORTION_STRENGTH * colour.a, 0.0, 0.0);
    FRAGMENT_BLOOM = vec4(0.0);
#elif MPP_PARTICLE_WEIGHTED_OIT
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
