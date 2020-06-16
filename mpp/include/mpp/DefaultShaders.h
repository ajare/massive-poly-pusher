#pragma once

/*
 * Default 3d shader.
 *
 */
const std::string VertexShader3dTemplate =
R"(
@@Version

@@In pos = vec3
## Normal
@@In normal = vec3
## TexCoords2
@@Passthrough uv = vec2
## Alpha
@@Passthrough colour = float
## RGB
@@Passthrough colour = vec3
## RGBA
@@Passthrough colour = vec4
## Normal
@@Out normal = vec3
## 

void main()
{
## Normal
	@Out(normal) = normalize(@NormalMatrix * @In(normal));
##
	vec4 vertPos = @MCPMatrix * vec4(@In(pos), 1);
	gl_Position = vertPos;
}
)";

const std::string VertexShader3dTemplate2 =
R"(
@@Version

void main()
{
    @Out(vec3 NORMAL) = normalize(@NormalMatrix * @In(NORMAL).xyz);

    vec4 vertPos = @MCPMatrix * vec4(@In(POSITION), 1);

    @Out(vec2 TEXCOORDS) = @In(TEXCOORDS);
    @Out(vec4 COLOUR) = @In(COLOUR);

    gl_Position = vertPos;
}
)";

const std::string FragmentShader3dTemplate =
R"(
@@Version

## Texture
@@Texture tex = sampler2D
## TexCoords2
@@In uv = vec2
## Alpha
@@In colour = float
## RGB
@@In colour = vec3
## RGBA
@@In colour = vec4
## Normal
@@In normal = vec3
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
##

## Texture
	@Out(colour) = texture(@Texture(tex), @In(uv)) * colour;
## Else
	@Out(colour) = colour;
##
}
)";

const std::string FragmentShader3dTemplate2 = 
R"(
@@Version

@@Texture(sampler2D, tex)

void main()
{
    vec3 shadedColour = @In(COLOUR).xyz;
    @Out(vec4 COLOUR) = texture(@Texture(tex), @In(TEXCOORDS).xy) * vec4(shadedColour, 1.0);
}
)";

/*
 * Fullscreen 2d shader.
 *
 */
const std::string VertexShaderFullscreenTemplate =
R"(
@@Version

@@In pos = vec2
@@Passthrough uv = vec2

void main()
{
	vec4 transVertex = @MCPMatrix * vec4(@In(pos), 0, 1);
	vec2 centredPos = vec2(transVertex.x - @HalfWindowSize.x, transVertex.y - @HalfWindowSize.y);
	gl_Position = vec4(centredPos / @HalfWindowSize, 0, 1);
}
)";

const std::string FragmentShaderFullscreenTemplate =
R"(
@@Version

## Diffuse
@@Uniform diffuse = vec4
##

@@Texture tex = sampler2D

@@In uv = vec2
@@Out colour = vec4

void main()
{
## Diffuse
	@Out(colour) = texture(@Texture(tex), @In(uv)) * @Uniform(diffuse);
## Else
	@Out(colour) = texture(@Texture(tex), @In(uv));
##
}
)";

/*
 * Text shader.
 *
 */
const std::string VertexShaderTextTemplate =
R"(
@@Version

@@In pos = vec2
## Points
@@Passthrough uvs = vec4
## Else
@@Passthrough uvs = vec2
##

## RGBA
@@Passthrough colour = vec4
##

void main()
{
	vec4 transVertex = @MCPMatrix * vec4(@In(pos), 0, 1);
	vec2 centredPos = transVertex.xy - @HalfWindowSize;
## Points
	centredPos += gl_PointSize / 2.0; // Rendering as points, which generate vertices from centre, so offset to get bottom-left.
##
	gl_Position = vec4(centredPos / @HalfWindowSize, 0, 1);
}
)";

const std::string FragmentShaderTextTemplate =
R"(
@@Version

@@Uniform colour = vec4
@@Texture tex = sampler2D

## Points
@@In uvs = vec4
## Else
@@In uvs = vec2
##

## RGBA
@@In colour = vec4
##

@@Out colour = vec4

void main()
{
## Points
	vec2 uv = mix(@In(uvs).xy, @In(uvs).zw, vec2(gl_PointCoord.x, 1.0 - gl_PointCoord.y));
	@Out(colour) = texture(@Texture(tex), uv) * @Uniform(colour);
## Else
	@Out(colour) = texture(@Texture(tex), @In(uvs)) * @Uniform(colour);
##

## RGBA
	@Out(colour) *= @In(colour);
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
@@Texture tex = sampler2D
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
	@Out(colour) = texture(@Texture(tex), tc) * colour;
## Else
	@Out(colour) = colour;
##
}
)";