@@Version

@@Uniform light = vec3

@@In uv = vec2
@@In colour = vec4
@@In pos = vec3
@@In normal = vec3

@@Out colour = vec4

void main()
{
	@Out(colour) = vec4(@In(normal), 1.0);
}
