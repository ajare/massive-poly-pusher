@@Version

void main()
{
    @Out(vec3 NORMAL) = normalize(@NormalMatrix * @Vec3(@In(NORMAL)));

    vec4 vertPos = @MCPMatrix * @Vec4(@In(POSITION));

    @Out(vec2 TEXCOORDS) = @In(TEXCOORDS);

    gl_Position = vertPos;
}