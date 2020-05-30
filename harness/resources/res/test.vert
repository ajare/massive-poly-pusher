@@Version

@@In pos = vec3
@@In normal = vec4

@@Passthrough uv = vec2
@@Passthrough colour = vec4

@@Out pos = vec3
@@Out normal = vec3

void main()
{
	@Out(normal) = normalize(@NormalMatrix * @In(normal).xyz);

	vec4 vertPos = @MCPMatrix * vec4(@In(pos), 1);
	@Out(pos) = vertPos.xyz;
	
	gl_Position = vertPos;
}
