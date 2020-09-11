#pragma once

/*
 * Default 3d program.
 *
 */
const std::string VertexShader3dTemplate =
R"(
@@Version

void main()
{
    @Out(vec3 NORMAL) = normalize(@NormalMatrix * @Vec3(@In(NORMAL)));

    vec4 vertPos = @MCPMatrix * @Vec4(@In(POSITION));

    @Out(vec2 TEXCOORDS) = @In(TEXCOORDS);
    @Out(vec4 COLOUR) = @In(COLOUR);

    gl_Position = vertPos;
}
)";

const std::string FragmentShader3dTemplate =
R"(
@@Version

@@Texture(sampler2D TEX1)

void main()
{
    vec3 shadedColour = @In(COLOUR).xyz;
    @Out(vec4 COLOUR) = texture(@Texture(TEX1), @In(TEXCOORDS).xy) * vec4(shadedColour, 1.0);
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

const std::string FragmentShaderFullscreenTemplate =
R"(
@@Version

@@Uniform(vec4 DIFFUSE)
@@Texture(sampler2D TEX1)

void main()
{
	@Out(vec4 COLOUR) = texture(@Texture(TEX1), @In(TEXCOORDS)) * @Uniform(DIFFUSE);
}
)";

/*
 * Text shader.
 *
 */
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
}
)";

const std::string FragmentShaderTextTemplate =
R"(
@@Version

@@Uniform(vec4 COLOUR);
@@Texture(sampler2D TEX1)

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
	vec2 d = normalize(@In(POSITION).zw);
	@Out(mat4 TEXROTATION) =
	mat4(d.x, d.y, 0.0, 0.0,
		-d.y, d.x, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0);
## Triangles&Rotation
	vec2 d = normalize(@In(POSITION).zw);
	mat4 rotationMatrix =
	mat4(d.x, d.y, 0.0, 0.0,
		-d.y, d.x, 0.0, 0.0,
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
	vec4 transCentroid = @MCPMatrix * vec4(@In(TEXCOORDS).zw, 0, 1);
	vec2 offset = transVertex.xy - transCentroid.xy;
	offset = vec2(rotationMatrix * vec4(offset, 0.0, 1.0));
	transVertex.xy = transCentroid.xy + offset.xy;
## Else
	vec4 transVertex = @MCPMatrix * vec4(@In(POSITION).xy, 0, 1);
##
	vec2 centredPos = vec2(transVertex.x - @HalfWindowSize.x, transVertex.y - @HalfWindowSize.y);
	gl_Position = vec4(centredPos / @HalfWindowSize, 0, 1);
}
)";

const std::string FragmentShader2dTemplate =
R"(
@@Version

## Diffuse
@@Uniform(vec4 DIFFUSE);
## Texture
@@Texture(sampler2D TEX1)
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

## Points&!Rotation&!TexCoords
	// Use gl_PointCoord
	vec2 tc = gl_PointCoord;
## TexCoords2
	// Use supplied texture coords for a single image
	vec2 tc = @In(TEXCOORDS);
## Triangles&Rotation
	// Use first part of texcoords
	vec2 tc = vec2(@In(TEXCOORDS));
## Points&!Rotation&TexCoords4
	// Use supplied texture coords for an atlas
	vec2 tc = mix(@In(TEXCOORDS).st, @In(TEXCOORDS).pq, vec2(gl_PointCoord.x, 1.0 - gl_PointCoord.y));
## Points&Rotation&!Atlas
	// Rotate a single image
	const vec2 offset = vec2(0.5, 0.5);
	vec2 tc = vec2(gl_PointCoord.x, 1.0 - gl_PointCoord.y);
	tc -= offset;
	tc = vec2(@In(TEXROTATION) * vec4(tc, 0.0, 1.0));
	tc += offset;
## Points&Rotation&Atlas
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
    @Out(vec4 SIZES) = vec4(@In(POSITION).zw, @In(SIZES).xy);
## Else
	@Out(vec4 SIZES) = vec4(0.0, 0.0, @In(SIZES).xy);
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

## Diffuse
@@Uniform(vec4 DIFFUSE);
##

void main()
{
	vec4 borderColour = @In(BORDERCOLOUR);
	vec4 innerColour = @In(INNERCOLOUR);
	vec2 sizes = vec2(@In(SIZES).zw);

## Diffuse
	borderColour *= @Uniform(DIFFUSE);
	innerColour *= @Uniform(DIFFUSE);
##

## Points
	vec2 tc = gl_PointCoord;
## Else
	vec2 tc = @In(SIZES).xy;
##
	tc -= 0.5;

	float outerBorder = 0.25;
	float innerBorder = (sizes.x - sizes.y) / (sizes.x * 4);

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
}
)";