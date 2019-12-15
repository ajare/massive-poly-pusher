#version 150

@@Texture tex = sampler2D

@@In uv = vec2
@@In colour = vec4

@@Out colour = vec4

void main()
{
	@Out(colour) = texture(@Texture(tex), @In(uv)) * @In(colour);
}
