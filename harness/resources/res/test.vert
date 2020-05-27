@@Version

@@In pos = ivec4
@@In normal = vec3

@@Passthrough uv = vec2
@@Passthrough colour = vec3

@@Out pos = vec3
@@Out normal = vec3

void main()
{
	@Out(normal) = normalize(@NormalMatrix * @In(normal));

	vec4 vertPos = @MCPMatrix * @In(pos);
	@Out(pos) = vertPos.xyz;
	
	gl_Position = vertPos;
}
