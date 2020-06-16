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

- Do uniforms: @@Uniform(vec4 LIGHT) and @Uniform(LIGHT) etc. [DONE]
- Add attributes in and do token replacement [DONE]
- Uniforms should be before attributes
- Do @Vec2 / @Vec3 / @Vec4 to cast vectors (and scalars, eg Colour1) to right type
  Eg, if position is a vec3, then:
  @Vec4(@In(POSITION))
  would translate to vec4(_mpp_i_POSITION, 1)
  - Use padding values based on component, eg X,Y,0,1 for position, X,Y,Z,1 for normal, U,V,0,0 for texcoords and 1 for padding any colour
  - Need to be able to shorten as well, eg:
    @Vec2(@In(POSITION))
	would translate to vec2(_mpp_i_POSITION.xy)
	
  To implement this, first do replacement of normal attributes/uniforms, eg so end up with:
  @Vec4(_mpp_i_POSITION)
  Then search for regex @Vec[2-4]\s*\(
  Then recursively search from that point onwards, counting ( and ) and stopping when we reach 0
*/

@@Version

void main()
{
	@Out(vec3 NORMAL) = normalize(@NormalMatrix * @In(NORMAL).xyz);

	vec4 vertPos = @MCPMatrix * @Vec4(@In(POSITION));
	
	@Out(vec3 POSITION) = vertPos.xyz;
	@Out(POSITION) = vertPos.xyz;
	@Out(vec2 TEXCOORDS) = @In(TEXCOORDS);
	@Out(vec4 COLOUR) = @In(COLOUR);
	
	gl_Position = vertPos;
}
