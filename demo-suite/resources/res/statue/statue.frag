@@Version

## Texture
@@Texture(sampler2D TEX1)
##

void main()
{
## Texture
    vec3 shadedColour = @In(COLOUR).xyz;
    @Out(vec4 COLOUR) = texture(@Texture(TEX1), @In(TEXCOORDS).xy) * vec4(shadedColour, 1.0);
## Else
	@Out(vec4 COLOUR) = @In(COLOUR);
##
}