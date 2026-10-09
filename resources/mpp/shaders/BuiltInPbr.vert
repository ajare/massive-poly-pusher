@@Version

void main()
{
    vec3 worldPosition = @Vec3(@MMatrix * @Vec4(@In(POSITION)));
    vec3 worldNormal = normalize(@NormalMatrix * @Vec3(@In(NORMAL)));
    vec3 worldTangent = normalize(@NormalMatrix * @Vec3(@In(TANGENT)));

    @Out(vec3 WORLD_POSITION) = worldPosition;
    @Out(vec3 NORMAL) = worldNormal;
    @Out(vec4 TANGENT) = vec4(worldTangent, @In(TANGENT).w);
    @Out(vec2 TEXCOORDS) = @In(TEXCOORDS);

    gl_Position = @MCPMatrix * @Vec4(@In(POSITION));
}
