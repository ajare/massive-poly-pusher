@@Version

@@Texture(sampler2D TEX1);
@@Texture(sampler2D TEX2);
@@Texture(samplerCube ENVIRONMENT);

void main()
{
    vec3 baseColour = texture(@Texture(TEX1), @In(TEXCOORDS)).rgb;
    vec3 detailColour = texture(@Texture(TEX2), @In(TEXCOORDS)).rgb;
    vec3 environment = texture(@Texture(ENVIRONMENT), normalize(@In(NORMAL))).rgb;

    // Keep every sampler active while Milestone 2 validates dynamic bindings.
    @Out(vec4 COLOUR) = vec4(baseColour * 0.94 + detailColour * 0.03 + environment * 0.03, 1.0);
}
