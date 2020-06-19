@@Version

void main()
{
	@Out(vec3 NORMAL) = normalize(@NormalMatrix * @In(NORMAL).xyz);

	vec4 vertPos = @MCPMatrix * @Vec4(@In(POSITION).xy);
	
	@Out(vec3 POSITION) = vertPos.xyz;
	@Out(POSITION) = vertPos.xyz;
	@Out(vec2 TEXCOORDS) = @In(TEXCOORDS);
	@Out(vec4 COLOUR) = @In(COLOUR);
	
	gl_Position = vertPos;
}
