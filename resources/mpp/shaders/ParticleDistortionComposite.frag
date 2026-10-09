
@@Version

@@Texture(sampler2D SCENE);
@@Texture(sampler2D DISTORTION);

void main()
{
    vec2 uv = @In(TEXCOORDS);
    vec2 offset = texture(@Texture(DISTORTION), uv).rg;
    // Clamp at the viewport edge rather than depending on an authored scene
    // sampler's wrap mode, which may legitimately be repeat for another pass.
    @Out(vec4 COLOUR) = texture(@Texture(SCENE), clamp(uv + offset, vec2(0.0), vec2(1.0)));
}
