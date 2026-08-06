@@Version

// Raw/legacy use keeps the historical complete contract. New materials receive
// an explicit define block after @@Version before parser/build.
#ifndef PBR_SPEC_LEGACY_FULL_CONTRACT
#define PBR_SPEC_LEGACY_FULL_CONTRACT 1
#define PBR_SPEC_BASE_COLOUR_MAP 1
#define PBR_SPEC_METALLIC 1
#define PBR_SPEC_ROUGHNESS 1
#define PBR_SPEC_METALLIC_ROUGHNESS_MAP 1
#define PBR_SPEC_NORMAL_MAP 1
#define PBR_SPEC_OCCLUSION 1
#define PBR_SPEC_EMISSIVE 1
#define PBR_SPEC_ALPHA_MASK 0
#define PBR_SPEC_ALPHA_BLEND 0
#define PBR_SPEC_DOUBLE_SIDED 0
#endif

@@Uniform(vec4 PBR_BASE_COLOUR_FACTOR);
@@Uniform(float PBR_EXT_LIGHTING_SCALE);
#if PBR_SPEC_METALLIC || PBR_SPEC_LEGACY_FULL_CONTRACT
@@Uniform(float PBR_METALLIC_FACTOR);
#endif
#if PBR_SPEC_ROUGHNESS || PBR_SPEC_LEGACY_FULL_CONTRACT
@@Uniform(float PBR_ROUGHNESS_FACTOR);
#endif
#if PBR_SPEC_EMISSIVE || PBR_SPEC_LEGACY_FULL_CONTRACT
@@Uniform(vec3 PBR_EMISSIVE_FACTOR);
#endif
#if PBR_SPEC_NORMAL_MAP || PBR_SPEC_LEGACY_FULL_CONTRACT
@@Uniform(float PBR_NORMAL_SCALE);
#endif
#if PBR_SPEC_OCCLUSION || PBR_SPEC_LEGACY_FULL_CONTRACT
@@Uniform(float PBR_OCCLUSION_STRENGTH);
#endif
#if PBR_SPEC_LEGACY_FULL_CONTRACT
@@Uniform(int PBR_ALPHA_MODE);
@@Uniform(int PBR_DOUBLE_SIDED);
#endif
#if PBR_SPEC_ALPHA_MASK || PBR_SPEC_LEGACY_FULL_CONTRACT
@@Uniform(float PBR_ALPHA_CUTOFF);
#endif

#if PBR_SPEC_BASE_COLOUR_MAP || PBR_SPEC_LEGACY_FULL_CONTRACT
@@Texture(sampler2D PBR_BASE_COLOUR_MAP);
#endif
#if PBR_SPEC_METALLIC_ROUGHNESS_MAP || PBR_SPEC_LEGACY_FULL_CONTRACT
@@Texture(sampler2D PBR_METALLIC_ROUGHNESS_MAP);
#endif
#if PBR_SPEC_NORMAL_MAP || PBR_SPEC_LEGACY_FULL_CONTRACT
@@Texture(sampler2D PBR_NORMAL_MAP);
#endif
#if PBR_SPEC_OCCLUSION || PBR_SPEC_LEGACY_FULL_CONTRACT
@@Texture(sampler2D PBR_OCCLUSION_MAP);
#endif
#if PBR_SPEC_EMISSIVE || PBR_SPEC_LEGACY_FULL_CONTRACT
@@Texture(sampler2D PBR_EMISSIVE_MAP);
#endif
@@Texture(samplerCube PBR_IRRADIANCE_MAP);
@@Texture(samplerCube PBR_PREFILTERED_SPECULAR_MAP);
@@Texture(sampler2D PBR_BRDF_LUT);
@@Texture(sampler2DShadow SHADOW_MAP);

layout(std140, binding = 2) uniform ShadowFrame
{
    mat4 LIGHT_VIEW_PROJECTION;
    vec4 MAP_TEXEL_SIZE_AND_RADIUS;
    vec4 BIAS_AND_ENABLED;
};

struct PbrLight
{
    vec4 colourIntensity;
    vec4 positionRange;
    vec4 directionType;
};

layout(std140, binding = 1) uniform PbrLights
{
    vec4 AMBIENT_AND_COUNT;
    PbrLight LIGHTS[8];
};

const float PI = 3.14159265359;

float distributionGgx(vec3 normal, vec3 halfway, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float nDotH = max(dot(normal, halfway), 0.0);
    float nDotH2 = nDotH * nDotH;
    float denominator = nDotH2 * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denominator * denominator, 0.000001);
}

float geometrySchlickGgx(float nDotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return nDotV / max(nDotV * (1.0 - k) + k, 0.000001);
}

float geometrySmith(vec3 normal, vec3 viewDirection, vec3 lightDirection, float roughness)
{
    return geometrySchlickGgx(max(dot(normal, viewDirection), 0.0), roughness) *
        geometrySchlickGgx(max(dot(normal, lightDirection), 0.0), roughness);
}

vec3 fresnelSchlick(float cosine, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(1.0 - cosine, 5.0);
}

vec3 fresnelSchlickRoughness(float cosine, vec3 f0, float roughness)
{
    return f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(1.0 - cosine, 5.0);
}

float directionalShadowVisibility(vec3 worldPosition, vec3 normal, vec3 lightDirection)
{
    if (BIAS_AND_ENABLED.z < 0.5)
    {
        return 1.0;
    }

    vec4 shadowPosition = LIGHT_VIEW_PROJECTION * vec4(worldPosition, 1.0);
    shadowPosition.xyz /= max(shadowPosition.w, 0.00001);
    vec3 shadowCoords = shadowPosition.xyz * 0.5 + 0.5;
    if (shadowCoords.x <= 0.0 || shadowCoords.x >= 1.0 || shadowCoords.y <= 0.0 || shadowCoords.y >= 1.0 ||
        shadowCoords.z <= 0.0 || shadowCoords.z >= 1.0)
    {
        return 1.0;
    }

    float nDotL = max(dot(normal, lightDirection), 0.0);
    float bias = BIAS_AND_ENABLED.x + BIAS_AND_ENABLED.y * (1.0 - nDotL);
    vec3 compareCoords = vec3(shadowCoords.xy, shadowCoords.z - bias);
    if (MAP_TEXEL_SIZE_AND_RADIUS.w < 0.5)
    {
        return texture(@Texture(SHADOW_MAP), compareCoords);
    }

    float visibility = 0.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            vec2 offset = vec2(float(x), float(y)) * MAP_TEXEL_SIZE_AND_RADIUS.xy * MAP_TEXEL_SIZE_AND_RADIUS.z;
            visibility += texture(@Texture(SHADOW_MAP), vec3(compareCoords.xy + offset, compareCoords.z));
        }
    }
    return visibility / 9.0;
}

void main()
{
#if PBR_SPEC_BASE_COLOUR_MAP || PBR_SPEC_LEGACY_FULL_CONTRACT
    vec4 baseColour = texture(@Texture(PBR_BASE_COLOUR_MAP), @In(TEXCOORDS)) * @Uniform(PBR_BASE_COLOUR_FACTOR);
#else
    vec4 baseColour = @Uniform(PBR_BASE_COLOUR_FACTOR);
#endif
#if PBR_SPEC_LEGACY_FULL_CONTRACT
    if (@Uniform(PBR_ALPHA_MODE) == 1 && baseColour.a < @Uniform(PBR_ALPHA_CUTOFF)) discard;
#elif PBR_SPEC_ALPHA_MASK
    if (baseColour.a < @Uniform(PBR_ALPHA_CUTOFF)) discard;
#endif

    vec3 normal = normalize(@In(NORMAL));
#if PBR_SPEC_NORMAL_MAP || PBR_SPEC_LEGACY_FULL_CONTRACT
    vec4 tangentInput = @In(TANGENT);
    vec3 tangent = normalize(tangentInput.xyz - normal * dot(normal, tangentInput.xyz));
    vec3 bitangent = normalize(cross(normal, tangent)) * tangentInput.w;
    vec3 normalSample = texture(@Texture(PBR_NORMAL_MAP), @In(TEXCOORDS)).xyz * 2.0 - 1.0;
    normalSample.xy *= @Uniform(PBR_NORMAL_SCALE);
    normal = normalize(mat3(tangent, bitangent, normal) * normalSample);
#endif
#if PBR_SPEC_LEGACY_FULL_CONTRACT
    if (@Uniform(PBR_DOUBLE_SIDED) != 0 && !gl_FrontFacing) normal = -normal;
#elif PBR_SPEC_DOUBLE_SIDED
    if (!gl_FrontFacing) normal = -normal;
#endif

#if PBR_SPEC_METALLIC_ROUGHNESS_MAP || PBR_SPEC_LEGACY_FULL_CONTRACT
    vec3 metallicRoughness = texture(@Texture(PBR_METALLIC_ROUGHNESS_MAP), @In(TEXCOORDS)).rgb;
#endif
#if PBR_SPEC_LEGACY_FULL_CONTRACT
    float metallic = clamp(metallicRoughness.b * @Uniform(PBR_METALLIC_FACTOR), 0.0, 1.0);
    float roughness = clamp(metallicRoughness.g * @Uniform(PBR_ROUGHNESS_FACTOR), 0.04, 1.0);
#elif PBR_SPEC_METALLIC
#if PBR_SPEC_METALLIC_ROUGHNESS_MAP
    float metallic = clamp(metallicRoughness.b * @Uniform(PBR_METALLIC_FACTOR), 0.0, 1.0);
#else
    float metallic = clamp(@Uniform(PBR_METALLIC_FACTOR), 0.0, 1.0);
#endif
#else
    const float metallic = 0.0;
#endif
#if !PBR_SPEC_LEGACY_FULL_CONTRACT
#if PBR_SPEC_ROUGHNESS
#if PBR_SPEC_METALLIC_ROUGHNESS_MAP
    float roughness = clamp(metallicRoughness.g * @Uniform(PBR_ROUGHNESS_FACTOR), 0.04, 1.0);
#else
    float roughness = clamp(@Uniform(PBR_ROUGHNESS_FACTOR), 0.04, 1.0);
#endif
#else
    const float roughness = 0.04;
#endif
#endif
#if PBR_SPEC_OCCLUSION || PBR_SPEC_LEGACY_FULL_CONTRACT
    float occlusion = mix(1.0, texture(@Texture(PBR_OCCLUSION_MAP), @In(TEXCOORDS)).r, @Uniform(PBR_OCCLUSION_STRENGTH));
#else
    const float occlusion = 1.0;
#endif
#if PBR_SPEC_EMISSIVE || PBR_SPEC_LEGACY_FULL_CONTRACT
    vec3 emissive = texture(@Texture(PBR_EMISSIVE_MAP), @In(TEXCOORDS)).rgb * @Uniform(PBR_EMISSIVE_FACTOR);
#else
    const vec3 emissive = vec3(0.0);
#endif

    vec3 viewDirection = normalize(@ViewPos - @In(WORLD_POSITION));
    vec3 f0 = mix(vec3(0.04), baseColour.rgb, metallic);
    vec3 direct = vec3(0.0);
    int lightCount = int(AMBIENT_AND_COUNT.a);
    for (int i = 0; i < lightCount; ++i)
    {
        PbrLight light = LIGHTS[i];
        float isPoint = light.directionType.w;
        vec3 toLight = light.positionRange.xyz - @In(WORLD_POSITION);
        float distanceToLight = max(length(toLight), 0.0001);
        vec3 pointDirection = toLight / distanceToLight;
        vec3 directionalDirection = normalize(-light.directionType.xyz);
        vec3 lightDirection = mix(directionalDirection, pointDirection, isPoint);
        float attenuation = mix(1.0, 1.0 / (distanceToLight * distanceToLight), isPoint);
        if (isPoint > 0.5 && light.positionRange.w > 0.0 && distanceToLight > light.positionRange.w)
        {
            attenuation = 0.0;
        }

        vec3 halfway = normalize(viewDirection + lightDirection);
        float ndf = distributionGgx(normal, halfway, roughness);
        float geometry = geometrySmith(normal, viewDirection, lightDirection, roughness);
        vec3 fresnel = fresnelSchlick(max(dot(halfway, viewDirection), 0.0), f0);
        vec3 specular = ndf * geometry * fresnel /
            max(4.0 * max(dot(normal, viewDirection), 0.0) * max(dot(normal, lightDirection), 0.0), 0.0001);
        vec3 kS = fresnel;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
        float nDotL = max(dot(normal, lightDirection), 0.0);
        vec3 radiance = light.colourIntensity.rgb * light.colourIntensity.a * attenuation;
        float shadow = isPoint > 0.5 ? 1.0 : directionalShadowVisibility(@In(WORLD_POSITION), normal, lightDirection);
        direct += (kD * baseColour.rgb / PI + specular) * radiance * nDotL * shadow;
    }

    float nDotV = max(dot(normal, viewDirection), 0.0);
    vec3 fresnel = fresnelSchlickRoughness(nDotV, f0, roughness);
    vec3 kS = fresnel;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    vec3 irradiance = texture(@Texture(PBR_IRRADIANCE_MAP), normal).rgb;
    vec3 diffuse = irradiance * baseColour.rgb;
    vec3 reflection = reflect(-viewDirection, normal);
    vec3 prefiltered = textureLod(@Texture(PBR_PREFILTERED_SPECULAR_MAP), reflection, roughness * 4.0).rgb;
    vec2 brdf = texture(@Texture(PBR_BRDF_LUT), vec2(nDotV, roughness)).rg;
    vec3 specular = prefiltered * (fresnel * brdf.x + brdf.y);
    vec3 ambient = (kD * diffuse + specular) * occlusion + AMBIENT_AND_COUNT.rgb * baseColour.rgb;

    // Opaque and masked materials must not inherit an undefined/blended
    // framebuffer alpha. Only Blend materials expose the authored alpha.
#if PBR_SPEC_LEGACY_FULL_CONTRACT
    float outputAlpha = @Uniform(PBR_ALPHA_MODE) == 2 ? baseColour.a : 1.0;
#elif PBR_SPEC_ALPHA_BLEND
    float outputAlpha = baseColour.a;
#else
    const float outputAlpha = 1.0;
#endif
    @Out(vec4 COLOUR) = vec4(ambient + direct * @Uniform(PBR_EXT_LIGHTING_SCALE) + emissive, outputAlpha);
    // Location 1 is ignored by the single-target manual PBR path. GraphPBR
    // attaches it as an HDR bloom mask, so authored emissive is isolated from
    // otherwise bright direct/albedo lighting.
    @Out(vec4 BLOOM_MASK) = vec4(emissive, 1.0);
}
