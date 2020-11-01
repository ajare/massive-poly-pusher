@@Version

@@Uniform(float SPEC_EXPONENT);
@@Uniform(float SPEC_STRENGTH);

## Texture
@@Texture(sampler2D TEX1);
##

struct Light
{
    vec3 position;
    vec3 colour;
};

layout(std140, binding = 0) uniform Block
{
	@@Uniform(vec3 AMBIENT);
	@@Uniform(Light LIGHTS[2]);
	@@Uniform(int NUM_LIGHTS);
};

float lambert(vec3 n, vec3 l)
{
    float result = dot(n, l);
    return max(result, 0.0);
}

float phong(vec3 v, vec3 n, vec3 l)
{
    vec3 r = reflect(-l, n);
    float spec = pow(max(dot(v, r), 0.0), @Uniform(SPEC_EXPONENT));
    return spec * @Uniform(SPEC_STRENGTH);
}

float blinn_phong(vec3 v, vec3 n, vec3 l)
{
    vec3 h = normalize(l + v);
    return pow(max(dot(n, h), 0.0), @Uniform(SPEC_EXPONENT));
}

void main()
{
    vec3 normalDir = @In(NORMAL);
    vec3 viewDir = normalize(@ViewPos - @In(FRAGPOSITION));

    vec3 colourContrib = @Uniform(AMBIENT);

	for (int i = 0; i < @Uniform(NUM_LIGHTS); i++)
	{
        vec3 lightDir = normalize(@Uniform(LIGHTS[i]).position - @In(FRAGPOSITION));

        // Lighting model
        float diffuse = lambert(normalDir, lightDir);
        //float specular = phong(viewDir, normalDir, lightDir);
		float specular = blinn_phong(viewDir, normalDir, lightDir);
        
        colourContrib += @Uniform(LIGHTS[i]).colour * (diffuse + specular);
	}

    vec4 shadedColour = vec4(colourContrib, 1.0) * @In(COLOUR);

## Texture
    @Out(vec4 COLOUR) = texture(@Texture(TEX1), @In(TEXCOORDS).xy) * shadedColour;
## Else
    @Out(vec4 COLOUR) = shadedColour;
##

    // Gamma correction
	//float gamma = 2.2;
	//@Out(COLOUR).rgb = pow(@Out(COLOUR).rgb, vec3(1.0/gamma));
}
