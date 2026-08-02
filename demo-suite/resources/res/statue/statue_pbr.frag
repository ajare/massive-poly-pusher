@@Version

@@Uniform(vec4 PBR_BASE_COLOUR_FACTOR);
@@Uniform(float PBR_METALLIC_FACTOR);
@@Uniform(float PBR_ROUGHNESS_FACTOR);
@@Uniform(vec3 PBR_EMISSIVE_FACTOR);
@@Uniform(float PBR_NORMAL_SCALE);
@@Uniform(float PBR_OCCLUSION_STRENGTH);
@@Uniform(int PBR_ALPHA_MODE);
@@Uniform(float PBR_ALPHA_CUTOFF);
@@Uniform(int PBR_DOUBLE_SIDED);

@@Texture(sampler2D PBR_BASE_COLOUR_MAP);
@@Texture(sampler2D PBR_METALLIC_ROUGHNESS_MAP);
@@Texture(sampler2D PBR_NORMAL_MAP);
@@Texture(sampler2D PBR_OCCLUSION_MAP);
@@Texture(sampler2D PBR_EMISSIVE_MAP);
@@Texture(samplerCube PBR_IRRADIANCE_MAP);
@@Texture(samplerCube PBR_PREFILTERED_SPECULAR_MAP);
@@Texture(sampler2D PBR_BRDF_LUT);

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

void main()
{
    vec4 baseSample = texture(@Texture(PBR_BASE_COLOUR_MAP), @In(TEXCOORDS));
    vec4 baseColour = baseSample * @Uniform(PBR_BASE_COLOUR_FACTOR);
    if (@Uniform(PBR_ALPHA_MODE) == 1 && baseColour.a < @Uniform(PBR_ALPHA_CUTOFF))
    {
        discard;
    }

    vec3 normal = normalize(@In(NORMAL));
    vec4 tangentInput = @In(TANGENT);
    vec3 tangent = normalize(tangentInput.xyz - normal * dot(normal, tangentInput.xyz));
    vec3 bitangent = normalize(cross(normal, tangent)) * tangentInput.w;
    vec3 normalSample = texture(@Texture(PBR_NORMAL_MAP), @In(TEXCOORDS)).xyz * 2.0 - 1.0;
    normalSample.xy *= @Uniform(PBR_NORMAL_SCALE);
    normal = normalize(mat3(tangent, bitangent, normal) * normalSample);
    if (@Uniform(PBR_DOUBLE_SIDED) != 0 && !gl_FrontFacing)
    {
        normal = -normal;
    }

    vec3 metallicRoughness = texture(@Texture(PBR_METALLIC_ROUGHNESS_MAP), @In(TEXCOORDS)).rgb;
    float metallic = clamp(metallicRoughness.b * @Uniform(PBR_METALLIC_FACTOR), 0.0, 1.0);
    float roughness = clamp(metallicRoughness.g * @Uniform(PBR_ROUGHNESS_FACTOR), 0.04, 1.0);
    float occlusion = mix(1.0, texture(@Texture(PBR_OCCLUSION_MAP), @In(TEXCOORDS)).r, @Uniform(PBR_OCCLUSION_STRENGTH));
    vec3 emissive = texture(@Texture(PBR_EMISSIVE_MAP), @In(TEXCOORDS)).rgb * @Uniform(PBR_EMISSIVE_FACTOR);

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
        direct += (kD * baseColour.rgb / PI + specular) * radiance * nDotL;
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

    @Out(vec4 COLOUR) = vec4(ambient + direct + emissive, baseColour.a);
}
