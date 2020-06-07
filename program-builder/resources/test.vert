/*
For vertex shader, in declarations aren't required as we generate them all from the mesh spec

Work backwards from last shader: find the in variables it uses and generate a typeless declaration, eg
for fragment shader.

layout(location=0) in <type> POSITION;
layout(location=1) in <type> NORMAL;
layout(location=2) in <type> TEXCOORDS;
layout(location=3) in <type> COLOUR;

For all in/out declarations (except in for vertex shader), locations should be in order of POSITION, NORMAL, TEXCOORDS, COLOUR as used.

For fragment shader, check that only one @Out variable is used: this is assumed to be the colour and will
be a vec4, so based on the name generate:

layout(location=0) out vec4 <NAME>

Then, working backwards, we generate the out declarations of the previous stage: eg vertex shader

layout(location=0) out <type> POSITION;
layout(location=1) out <type> NORMAL;
layout(location=2) out <type> TEXCOORDS;
layout(location=3) out <type> COLOUR;

And then the in declarations of the vertex shader are generated - with type - from the mesh specification, eg:

layout(location=0) in vec3 POSITION;
layout(location=1) in vec3 NORMAL;
layout(location=2) in vec2 TEXCOORDS;
layout(location=3) in vec4 COLOUR;

Then, we need to check, going forwards from vertex shader, that the following are satisfied:
- Are all out-variables assigned to?
  - Error if not: we require explicit pass-through
- Are all in-variables used?
  - Warning if not.  It may be that they are not used in this stage but are in a later one, but then they should
    be explicitly passed through.
  
Then we just need to determine the types.  For this we probably need to declare out variable types.  And if we do
this, this acts as our out variable declaration, so we support multiple out vars for fragment shader, and know the
in var types of the next stage.  Or could declare type in usage, eg:

@Out(vec4 COLOUR) = @In(COLOUR);

This is better.  In vars we don't need to know the type as they are coming in, but out vars we are creating, so need
to define.  If we use the out var again then we don't need to specify the type, so @Out(COLOUR) works if we've already
done @Out(vec4 COLOUR).
*/

@@Version

void main()
{
	@Out(vec3 NORMAL) = normalize(@NormalMatrix * @In(NORMAL).xyz);

	vec4 vertPos = @MCPMatrix * vec4(@In(POSITION), 1);
	
	@Out(vec3 POSITION) = vertPos.xyz;
	@Out(POSITION) = vertPos.xyz;
	@Out(vec2 TEXCOORDS) = @In(TEXCOORDS);
	@Out(vec4 COLOUR) = @In(COLOUR);
	
	gl_Position = vertPos;
}
