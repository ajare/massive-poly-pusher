#pragma once

#include <string>

/*
 * Default 3d program.
 *
 */
const std::string VertexShader3dTemplate =
R"(
@@Version

void main()
{
    @Out(vec3 FRAGPOSITION) = @Vec3(@MMatrix * @Vec4(@In(POSITION)));
    @Out(vec3 NORMAL) = normalize(@NormalMatrix * @Vec3(@In(NORMAL)));
    @Out(vec2 TEXCOORDS) = @In(TEXCOORDS);
    @Out(vec4 COLOUR) = @In(COLOUR);

    gl_Position = @MCPMatrix * @Vec4(@In(POSITION));
}
)";

const std::string FragmentShader3dTemplate =
R"(
@@Version

@@Uniform(float GAMMA);
## Texture
@@Texture(sampler2D TEX1);
##
@@Texture(sampler2DShadow SHADOW_MAP);

layout(std140, binding = 2) uniform ShadowFrame
{
    mat4 LIGHT_VIEW_PROJECTION;
    vec4 MAP_TEXEL_SIZE_AND_RADIUS;
    vec4 BIAS_AND_ENABLED;
};

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
    float strength = 0.5;
    float exponent = 32;

    vec3 r = reflect(-l, n);
    float spec = pow(max(dot(v, r), 0.0), exponent);
    return strength * spec;
}

float directionalShadowVisibility(vec3 worldPosition, vec3 normal, vec3 lightDirection)
{
    if (BIAS_AND_ENABLED.z < 0.5)
    {
        return 1.0;
    }

    vec4 shadowPosition = LIGHT_VIEW_PROJECTION * vec4(worldPosition, 1.0);
    shadowPosition.xyz /= max(shadowPosition.w, 0.00001);
    vec3 shadowCoords = shadowPosition.xyz * 0.5 + 0.5;
    if (shadowCoords.x <= 0.0 || shadowCoords.x >= 1.0 || shadowCoords.y <= 0.0 || shadowCoords.y >= 1.0 ||
        shadowCoords.z <= 0.0 || shadowCoords.z >= 1.0)
    {
        return 1.0;
    }

    float nDotL = max(dot(normal, lightDirection), 0.0);
    float bias = BIAS_AND_ENABLED.x + BIAS_AND_ENABLED.y * (1.0 - nDotL);
    vec3 compareCoords = vec3(shadowCoords.xy, shadowCoords.z - bias);
    if (MAP_TEXEL_SIZE_AND_RADIUS.w < 0.5)
    {
        return texture(@Texture(SHADOW_MAP), compareCoords);
    }

    float visibility = 0.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            vec2 offset = vec2(float(x), float(y)) * MAP_TEXEL_SIZE_AND_RADIUS.xy * MAP_TEXEL_SIZE_AND_RADIUS.z;
            visibility += texture(@Texture(SHADOW_MAP), vec3(compareCoords.xy + offset, compareCoords.z));
        }
    }
    return visibility / 9.0;
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
        float specular = phong(viewDir, normalDir, lightDir);
        
        float shadow = i == 0 ? directionalShadowVisibility(@In(FRAGPOSITION), normalDir, lightDir) : 1.0;
        colourContrib += @Uniform(LIGHTS[i]).colour * (diffuse + specular) * shadow;
    }

    vec4 shadedColour = vec4(colourContrib, 1.0) * @In(COLOUR);

## Texture
    @Out(vec4 COLOUR) = texture(@Texture(TEX1), @In(TEXCOORDS).xy) * shadedColour;
## Else
    @Out(vec4 COLOUR) = shadedColour;
##
	@Out(COLOUR).rgb = pow(@Out(COLOUR).rgb, vec3(1.0 / @Uniform(GAMMA)));
}
)";

/*
 * Fullscreen 2d shader.
 *
 */
const std::string VertexShaderFullscreenTemplate =
R"(
@@Version

void main()
{
	vec4 transVertex = @MCPMatrix * @Vec4(@In(POSITION));
	vec2 centredPos = vec2(transVertex.x - @HalfWindowSize.x, transVertex.y - @HalfWindowSize.y);

	@Out(vec2 TEXCOORDS) = @In(TEXCOORDS);
	gl_Position = vec4(centredPos / @HalfWindowSize, 0, 1);
}
)";

// Don't apply gamma correction as this is used to render framebuffers which have
// already been gamma-corrected.
// Depth-only programs used by generic shadow domains. They depend only on
// position, so meshes from PBR and non-PBR materials share the same caster path.
const std::string VertexShaderShadowDepthTemplate =
R"(
@@Version

void main()
{
    gl_Position = @MCPMatrix * @Vec4(@In(POSITION));
}
)";

const std::string FragmentShaderShadowDepthTemplate =
R"(
@@Version

void main()
{
}
)";

const std::string FragmentShaderFullscreenTemplate =
R"(
@@Version

@@Uniform(float GAMMA);
@@Uniform(vec4 DIFFUSE);
@@Texture(sampler2D TEX1);

void main()
{
	@Out(vec4 COLOUR) = texture(@Texture(TEX1), @In(TEXCOORDS)) * @Uniform(DIFFUSE);
}
)";

// Temporary HDR presentation shader used by the opt-in PBR pipeline. Surface
// shading remains replaceable while the PBR material model is introduced.
const std::string FragmentShaderToneMapTemplate =
R"(
@@Version

@@Uniform(float EXPOSURE);
@@Uniform(float GAMMA);
@@Uniform(int TONE_MAP_OPERATOR);
@@Texture(sampler2D TEX1);

void main()
{
	vec3 colour = texture(@Texture(TEX1), @In(TEXCOORDS)).rgb * @Uniform(EXPOSURE);
	if (@Uniform(TONE_MAP_OPERATOR) == 0)
	{
		colour = colour / (colour + vec3(1.0));
	}
	else
	{
		// Narkowicz ACES filmic approximation.
		colour = clamp((colour * (2.51 * colour + 0.03)) / (colour * (2.43 * colour + 0.59) + 0.14), 0.0, 1.0);
	}
	@Out(vec4 COLOUR) = vec4(pow(colour, vec3(1.0 / @Uniform(GAMMA))), 1.0);
}
)";

/*
 * Text shader.
 *
 */
/*
 * Pipeline-owned bloom shaders. Bloom is image-space and therefore works for
 * any material rendered into the pipeline scene target.
 */
const std::string FragmentShaderBloomExtractTemplate =
R"(
@@Version

@@Uniform(float THRESHOLD);
@@Texture(sampler2D TEX1);

void main()
{
    vec3 colour = texture(@Texture(TEX1), @In(TEXCOORDS)).rgb;
    @Out(vec4 COLOUR) = vec4(max(colour - vec3(@Uniform(THRESHOLD)), vec3(0.0)), 1.0);
}
)";

const std::string FragmentShaderBloomBlurTemplate =
R"(
@@Version

@@Uniform(vec2 DIRECTION);
@@Texture(sampler2D TEX1);

void main()
{
    vec2 texel = 1.0 / vec2(textureSize(@Texture(TEX1), 0));
    vec2 offset = @Uniform(DIRECTION) * texel;
    vec3 colour = texture(@Texture(TEX1), @In(TEXCOORDS)).rgb * 0.227027;
    colour += texture(@Texture(TEX1), @In(TEXCOORDS) + offset * 1.384615).rgb * 0.316216;
    colour += texture(@Texture(TEX1), @In(TEXCOORDS) - offset * 1.384615).rgb * 0.316216;
    colour += texture(@Texture(TEX1), @In(TEXCOORDS) + offset * 3.230769).rgb * 0.070270;
    colour += texture(@Texture(TEX1), @In(TEXCOORDS) - offset * 3.230769).rgb * 0.070270;
    @Out(vec4 COLOUR) = vec4(colour, 1.0);
}
)";

const std::string FragmentShaderBloomCombineTemplate =
R"(
@@Version

@@Uniform(float INTENSITY);
@@Texture(sampler2D SCENE);
@@Texture(sampler2D BLOOM);

void main()
{
    vec3 scene = texture(@Texture(SCENE), @In(TEXCOORDS)).rgb;
    vec3 bloom = texture(@Texture(BLOOM), @In(TEXCOORDS)).rgb;
    @Out(vec4 COLOUR) = vec4(scene + bloom * @Uniform(INTENSITY), 1.0);
}
)";

const std::string VertexShaderTextTemplate =
R"(
@@Version

void main()
{
	vec4 transVertex = @MCPMatrix * @Vec4(@In(POSITION));

	vec2 centredPos = transVertex.xy - @HalfWindowSize;

## Points
	centredPos += gl_PointSize / 2.0;
	@Out(vec4 TEXCOORDS) = @In(TEXCOORDS);
## Else
    @Out(vec2 TEXCOORDS) = @In(TEXCOORDS);
##

## Colours
	@Out(vec4 COLOUR) = @In(COLOUR);
##
	gl_Position = vec4(centredPos / @HalfWindowSize, 0, 1);
## Points
	gl_PointSize = @PointSize;
##
}
)";

const std::string FragmentShaderTextTemplate =
R"(
@@Version

@@Uniform(float GAMMA);
@@Uniform(vec4 COLOUR);
@@Texture(sampler2D TEX1);

void main()
{
## Points
	vec2 uv = mix(@In(TEXCOORDS).xy, @In(TEXCOORDS).zw, vec2(gl_PointCoord.x, 1.0 - gl_PointCoord.y));
	@Out(vec4 COLOUR) = texture(@Texture(TEX1), uv) * @Uniform(COLOUR);
## Else
	@Out(vec4 COLOUR) = texture(@Texture(TEX1), @In(TEXCOORDS)) * @Uniform(COLOUR);
##

## Colours
	@Out(COLOUR) *= @In(COLOUR);
##

	@Out(COLOUR).rgb = pow(@Out(COLOUR).rgb, vec3(1.0 / @Uniform(GAMMA)));
}
)";

/*
 * Basic 2d shader.
 *
 */
const std::string VertexShader2dTemplate =
R"(
@@Version

void main()
{
## Points&Rotation
	vec2 d = normalize(@In(ROTATION).xy);
	@Out(mat4 TEXROTATION) =
	mat4(d.y, d.x, 0.0, 0.0,
		-d.x, d.y, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0);
## Triangles&Rotation
	vec2 d = normalize(@In(ROTATION).xy);
	mat4 rotationMatrix =
	mat4(d.y, d.x, 0.0, 0.0,
		-d.x, d.y, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0);
## Colour
	@Out(vec4 COLOUR) = @Vec4(@In(COLOUR));
## TexCoords2
	@Out(vec2 TEXCOORDS) = @In(TEXCOORDS);
## TexCoords4
	@Out(vec4 TEXCOORDS) = @In(TEXCOORDS);
## Triangles&Rotation
	// Rotate around TEXCOORDS.zw
	vec4 transVertex = @MCPMatrix * vec4(@In(POSITION).xy, 0, 1);
	vec4 transCentroid = @MCPMatrix * vec4(@In(POSITION).zw, 0, 1);
	vec2 offset = transVertex.xy - transCentroid.xy;
	offset = vec2(rotationMatrix * vec4(offset, 0.0, 1.0));
	transVertex.xy = transCentroid.xy + offset.xy;
## Else
	vec4 transVertex = @MCPMatrix * vec4(@In(POSITION).xy, 0, 1);
##
	transVertex.y = @HalfWindowSize.y * 2 - transVertex.y;
	vec2 centredPos = vec2(transVertex.x - @HalfWindowSize.x, transVertex.y - @HalfWindowSize.y);
	gl_Position = vec4(centredPos / @HalfWindowSize, 0, 1);
## Points
	gl_PointSize = @PointSize;
##
}
)";

const std::string FragmentShader2dTemplate =
R"(
@@Version

@@Uniform(float GAMMA);
## Diffuse
@@Uniform(vec4 DIFFUSE);
## Texture
@@Texture(sampler2D TEX1);
##

void main()
{
## Colour
	vec4 colour = @Vec4(@In(COLOUR));
## Else
	vec4 colour = vec4(1.0, 1.0, 1.0, 1.0);
##
## Diffuse
    colour *= @Uniform(DIFFUSE);
##

## !Points&Texture
	vec2 tc = @In(TEXCOORDS).st;
## Points&!Rotation&Texture&!Atlas
	// Use gl_PointCoord
	vec2 tc = gl_PointCoord;
## Points&!Rotation&Texture&Atlas
	// Use supplied texture coords for an atlas
	vec2 tc = mix(@In(TEXCOORDS).st, @In(TEXCOORDS).pq, vec2(gl_PointCoord.x, 1.0 - gl_PointCoord.y));

## Points&Rotation&Texture&!Atlas
	// Rotate a single image
	const vec2 offset = vec2(0.5, 0.5);
	vec2 tc = vec2(gl_PointCoord.x, 1.0 - gl_PointCoord.y);
	tc -= offset;
	tc = vec2(@In(TEXROTATION) * vec4(tc, 0.0, 1.0));
	tc += offset;
## Points&Rotation&Texture&Atlas
	// Rotate an image within an atlas
	const vec2 offset = vec2(0.5, 0.5);
	vec2 tc = vec2(gl_PointCoord.x, 1.0 - gl_PointCoord.y);
	tc -= offset;
	tc = vec2(@In(TEXROTATION) * vec4(tc, 0.0, 1.0));
	tc += offset;
	tc = vec2(mix(@In(TEXCOORDS).s, @In(TEXCOORDS).p, tc.s), mix(@In(TEXCOORDS).t, @In(TEXCOORDS).q, tc.t));
## Texture
	@Out(vec4 COLOUR) = texture(@Texture(TEX1), tc) * colour;
## Else
	@Out(vec4 COLOUR) = colour;
##

	@Out(COLOUR).rgb = pow(@Out(COLOUR).rgb, vec3(1.0 / @Uniform(GAMMA)));
}
)";

/*
 * Circle
 *
 */

const std::string VertexShader2dCircle =
R"(
@@Version

void main()
{
## Triangles
    @Out(vec4 OPTIONS) = vec4(@In(POSITION).zw, @In(OPTIONS).xy);
## Else
	@Out(vec4 OPTIONS) = vec4(0.0, 0.0, @In(OPTIONS).xy);
##

	@Out(vec4 BORDERCOLOUR) = @In(BORDERCOLOUR);
	@Out(vec4 INNERCOLOUR) = @Vec4(@In(INNERCOLOUR));

	vec4 transVertex = @MCPMatrix * vec4(@In(POSITION).xy, 0, 1);
	vec2 centredPos = vec2(transVertex.x - @HalfWindowSize.x, transVertex.y - @HalfWindowSize.y);
	gl_Position = vec4(centredPos / @HalfWindowSize, 0, 1);
}
)";

const std::string FragmentShader2dCircle =
R"(
@@Version

@@Uniform(float GAMMA);
## Diffuse
@@Uniform(vec4 DIFFUSE);
##

void main()
{
	vec4 borderColour = @In(BORDERCOLOUR);
	vec4 innerColour = @In(INNERCOLOUR);
	vec2 options = vec2(@In(OPTIONS).zw);

## Diffuse
	borderColour *= @Uniform(DIFFUSE);
	innerColour *= @Uniform(DIFFUSE);
##

## Points
	vec2 tc = gl_PointCoord;
## Else
	vec2 tc = @In(OPTIONS).xy;
##
	tc -= 0.5;

	float outerBorder = 0.25;
	float innerBorder = (options.x - options.y) / (options.x * 4);

	float d = dot(tc, tc);
	if (d <= innerBorder)
	{
		@Out(vec4 COLOUR) = innerColour;
	}
	else if (d <= outerBorder)
	{
		@Out(COLOUR) = borderColour;
	}
	else
	{
		@Out(COLOUR) = vec4(0.0);
	}

	@Out(COLOUR).rgb = pow(@Out(COLOUR).rgb, vec3(1.0 / @Uniform(GAMMA)));
}
)";

const std::string FragmentShader2dCircleAntialiased =
R"(
@@Version

@@Uniform(float GAMMA);
## Diffuse
@@Uniform(vec4 DIFFUSE);
##

void main()
{
	vec4 borderColour = @In(BORDERCOLOUR);
	vec4 innerColour = @In(INNERCOLOUR);
	vec2 options = vec2(@In(OPTIONS).zw);

## Diffuse
	borderColour *= @Uniform(DIFFUSE);
	innerColour *= @Uniform(DIFFUSE);
##

## Points
	vec2 tc = gl_PointCoord;
## Else
	vec2 tc = @In(OPTIONS).xy;
##
	tc -= 0.5;

	float outerBorder = 0.25;
	float innerBorder = (options.x - options.y) / (options.x * 4);
	float alphaBorder = outerBorder - (outerBorder * 2 / options.x);

	float d = dot(tc, tc);
	if (d <= innerBorder)
	{
		@Out(vec4 COLOUR) = innerColour;
	}
	else if (d <= outerBorder)
	{
		if (d < alphaBorder)
		{
			@Out(COLOUR) = borderColour;
		}
		else
		{
			float blend = 1.0 - (d - alphaBorder) / (outerBorder - alphaBorder);
			@Out(COLOUR) = vec4(borderColour.xyz, blend);
		}
	}
	else
	{
		@Out(COLOUR) = vec4(0.0);
	}

	@Out(COLOUR).rgb = pow(@Out(COLOUR).rgb, vec3(1.0 / @Uniform(GAMMA)));
}
)";