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

@@Texture(sampler2D TEX)

void main()
{
    vec3 shadedColour = @In(COLOUR).xyz;
    @Out(vec4 COLOUR) = texture(@Texture(TEX), @In(TEXCOORDS).xy) * vec4(shadedColour, 1.0);
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
@@Texture(sampler2D TEX)

void main()
{
	@Out(vec4 COLOUR) = texture(@Texture(TEX), @In(TEXCOORDS)) * @Uniform(DIFFUSE);
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
##
    @Out(vec4 TEXCOORDS) = @In(TEXCOORDS);

## Colours
	@Out(vec4 COLOUR) = @In(COLOURS);
##
	gl_Position = vec4(centredPos / @HalfWindowSize, 0, 1);
}
)";

const std::string FragmentShaderTextTemplate =
R"(
@@Version

@@Uniform(vec4 COLOUR);
@@Texture(sampler2D TEX)

void main()
{
## Points
	vec2 uv = mix(@In(TEXCOORDS).xy, @In(TEXCOORDS).zw, vec2(gl_PointCoord.x, 1.0 - gl_PointCoord.y));
	@Out(vec4 COLOUR) = texture(@Texture(TEX), uv) * @Uniform(COLOUR);
## Else
	@Out(vec4 COLOUR) = texture(@Texture(TEX), @In(TEXCOORDS)) * @Uniform(COLOUR);
##

## Colours
	@Out(COLOUR) *= @In(COLOURS);
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

## Position2
@@In pos = vec2
## Position4
@@In pos = vec4
## TexCoords2
@@Passthrough uv = vec2
## TexCoords4
@@Passthrough uv = vec4
## Alpha
@@Passthrough colour = float
## RGB
@@Passthrough colour = vec3
## RGBA
@@Passthrough colour = vec4
## Rotation
@@Out texRotation = mat4
##

void main()
{
## Rotation
	vec2 d = normalize(@In(pos).zw);
	@Out(texRotation) =
	mat4(d.y, d.x, 0.0, 0.0,
		-d.x, d.y, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0);
##

	vec4 transVertex = @MCPMatrix * vec4(@In(pos).xy, 0, 1);
	vec2 centredPos = vec2(transVertex.x - @HalfWindowSize.x, transVertex.y - @HalfWindowSize.y);
	gl_Position = vec4(centredPos / @HalfWindowSize, 0, 1);
}
)";

const std::string FragmentShader2dTemplate =
R"(
@@Version

## Diffuse
@@Uniform diffuse = vec4
## Texture
@@Texture TEX = sampler2D
## TexCoords2
@@In uv = vec2
## TexCoords4
@@In uv = vec4
## Alpha
@@In colour = float
## RGB
@@In colour = vec3
## RGBA
@@In colour = vec4
## Rotation
@@In texRotation = mat4
##
@@Out colour = vec4

void main()
{
	vec4 colour = vec4(1.0, 1.0, 1.0, 1.0);
## Alpha
	colour = vec4(1.0, 1.0, 1.0, @In(colour));
## RGB
	colour = vec4(@In(colour), 1.0);
## RGBA
	colour = @In(colour);
## Diffuse
    colour *= @Uniform(diffuse);
##

## Points&Rotation&!TexCoords2&!TexCoords4
	vec2 tc = vec2(@In(texRotation) * vec4(gl_PointCoord.x, 1.0 - gl_PointCoord.y, 0.0, 1.0));
## Points&!Rotation&!TexCoords2&!TexCoords4
	vec2 tc = gl_PointCoord;
## TexCoords2
	vec2 tc = @In(uv);
## Points&TexCoords4&!Rotation
	vec2 tc = mix(@In(uv).st, @In(uv).pq, vec2(gl_PointCoord.x, 1.0 - gl_PointCoord.y));
## Points&TexCoords4&Rotation
	vec2 tc = vec2(gl_PointCoord.x, 1.0 - gl_PointCoord.y);
	const vec2 offset = vec2(0.5,0.5);
	tc -= offset;
	tc = vec2(@In(texRotation) * vec4(tc, 0, 1));
	tc += offset;
	tc = vec2(mix(@In(uv).s, @In(uv).p, tc.s), mix(@In(uv).t, @In(uv).q, tc.t));
## Texture
	@Out(colour) = texture(@Texture(TEX), tc) * colour;
## Else
	@Out(colour) = colour;
##
}
)";