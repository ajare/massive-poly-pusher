@@Version

@@Uniform light = vec3

@@Texture tex = sampler2D

@@In uv = vec2
@@In colour = vec4
@@In pos = vec3
@@In normal = vec3

@@Out colour = vec4

void main()
{
	vec3 l = normalize(@Uniform(light) - @In(pos));   
	float d = max(dot(@In(normal),l), 0.0);  
	d = clamp(d, 0.0, 1.0); 

	vec3 shadedColour = @In(colour).xyz * d;
	@Out(colour) = texture(@Texture(tex), @In(uv).xy) * vec4(shadedColour, 1.0);
}
