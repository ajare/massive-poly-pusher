@@Version

@@Texture(sampler2D TEX1);

void main()
{
    vec4 pos = @Vec4(@In(POSITION));
    vec2 heightCoords = vec2((pos.x / 1024.0) + 0.5, (pos.z / 1024.0) + 0.5);
    pos.y += texture(@Texture(TEX1), heightCoords).r * 100;
    
    // Need to recalculate normal.  This can be done by sampling nearby pixels from
    // the texture, though it will create seams at the edges.  The proper way would be
    // to store the normal in the texture.  So, use 16-bit RGBA texture, with half-floats
    // for the normal in RGB, and the height in Alpha.
    
    @Out(vec3 FRAGPOSITION) = @Vec3(@MMatrix * pos);
    @Out(vec3 NORMAL) = normalize(@NormalMatrix * @Vec3(@In(NORMAL)));
    @Out(vec2 TEXCOORDS) = @In(TEXCOORDS);
    @Out(vec4 COLOUR) = @In(COLOUR);

    gl_Position = @MCPMatrix * pos;
}