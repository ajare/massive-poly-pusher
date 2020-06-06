@@Version

@@Uniform light = vec3

@@Texture tex = sampler2D

void main()
{
	vec3 l = normalize(@Uniform(light) - @In(POSITION));   
	float d = max(dot(@In(NORMAL),l), 0.0);  
	d = clamp(d, 0.0, 1.0); 

	vec3 shadedColour = @In(COLOUR).xyz * d;
	@Out(COLOUR) = texture(@Texture(tex), @In(TEXCOORDS).xy) * vec4(shadedColour, 1.0);
}
