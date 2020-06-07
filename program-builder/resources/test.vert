/*
For vertex shader, in declarations aren't required as we generate them all from the mesh spec
Out declarations are required, with type, as we cannot easily deduce the type
For passthrough variables, these must be explicitly passed through in the code

For following shaders, in declarations aren't required, as we use the previous stage's out declarations
Out declarations are required as with vertex shader

*/

@@Version

@@In(POSITION) = vec4
@@Out(NORMAL) = vec3

void main()
{
	@Out(NORMAL) = normalize(@NormalMatrix * @In(NORMAL).xyz);

	vec4 vertPos = @MCPMatrix * vec4(@In(POSITION), 1);
	@Out(POSITION) = vertPos.xyz;
	
	gl_Position = vertPos;
}
