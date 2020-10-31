@@Version

## Texture
@@Texture(sampler2D TEX1);
@@Texture(sampler2D TEX2);
##

void main()
{
## Texture
    @Out(vec4 COLOUR) = mix(texture(@Texture(TEX1), @In(TEXCOORDS).xy), texture(@Texture(TEX2), @In(TEXCOORDS).xy), (@In(NORMAL).y + 1.0) / 2.0);
## Else
	@Out(vec4 COLOUR) = @In(COLOUR);
##
}