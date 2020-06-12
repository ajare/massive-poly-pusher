/*
TODO:
- Check if out vars with no type have already been declared.
- Non-float types for out vars, eg ivec2, bvec2, dvec2, etc
  - This may force incompatibility.  If a meshspec declares an in attribute
    as unnormalised integer, and this gets passed to an out variable, how does
	the out variable know its type (which should be int/ivec)?
- Vertex shader in attributes may need to match out attributes, eg
  in the case of passthrough.  But we can't catch mismatches here,
  too hard.  Or is it?
- Check for unused in attributes
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
