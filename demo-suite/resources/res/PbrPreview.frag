@@Version

@@Texture(sampler2D TEX1);
@@Texture(sampler2D TEX2);
@@Texture(samplerCube ENVIRONMENT);

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

void main()
{
    vec3 baseColour = texture(@Texture(TEX1), @In(TEXCOORDS)).rgb;
    vec3 detailColour = texture(@Texture(TEX2), @In(TEXCOORDS)).rgb;
    vec3 environment = texture(@Texture(ENVIRONMENT), normalize(@In(NORMAL))).rgb;
    vec3 normal = normalize(@In(NORMAL));

    vec3 directLighting = AMBIENT_AND_COUNT.rgb;
    int lightCount = int(AMBIENT_AND_COUNT.a);
    for (int i = 0; i < lightCount; ++i)
    {
        PbrLight light = LIGHTS[i];
        float isPoint = light.directionType.w;
        vec3 toLight = light.positionRange.xyz - @In(FRAGPOSITION);
        float distanceToLight = max(length(toLight), 0.0001);
        vec3 pointDirection = toLight / distanceToLight;
        vec3 directionalDirection = normalize(-light.directionType.xyz);
        vec3 lightDirection = mix(directionalDirection, pointDirection, isPoint);
        float attenuation = mix(1.0, 1.0 / (distanceToLight * distanceToLight), isPoint);
        if (isPoint > 0.5 && light.positionRange.w > 0.0 && distanceToLight > light.positionRange.w)
        {
            attenuation = 0.0;
        }
        directLighting += light.colourIntensity.rgb * light.colourIntensity.a * max(dot(normal, lightDirection), 0.0) * attenuation;
    }

    // Keep every sampler active while the PBR material and IBL contracts are
    // introduced. Milestone 5 replaces this preview equation with GGX PBR.
    vec3 albedo = baseColour * 0.94 + detailColour * 0.03 + environment * 0.03;
    @Out(vec4 COLOUR) = vec4(albedo * directLighting, 1.0);
}
