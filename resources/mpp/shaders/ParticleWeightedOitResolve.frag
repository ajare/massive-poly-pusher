
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
