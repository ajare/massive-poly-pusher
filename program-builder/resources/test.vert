/*
TODO:
- Non-float types for out vars, eg ivec2, bvec2, dvec2, etc
  - This may force incompatibility.  If a meshspec declares an in attribute
    as unnormalised integer, and this gets passed to an out variable, how does
	the out variable know its type (which should be int/ivec)?
	- During compilation, do these conversions:
	  
	  Example code: @Out(vec4 COLOUR) = @In(COLOUR)
	  
	  In          Out        Code
	  float       float      _out_colour = _in_colour
	  float       int        _out_colour = int(_in_colour)
	  int(norm)   int        _out_colour = float(_in_colour)
	  int(unorm)  int        _out_colour = _in_colour
	  int(norm)   float      _out_colour = _in_colour
	  int(unorm)  float      _out_colour = float(_in_colour)
	  
	  Where there are casts, emit a warning specific to the conversion,
	  describing the potential issues.
	  This should be done during token replacement/code generation.

- Do uniforms: @@Uniform(vec4 LIGHT) and @Uniform(LIGHT) etc.
- Add attributes in and do token replacement
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
