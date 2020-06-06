@@Version

void main()
{
	@Out(NORMAL) = normalize(@NormalMatrix * @In(NORMAL).xyz);

	vec4 vertPos = @MCPMatrix * vec4(@In(POSITION), 1);
	@Out(POSITION) = vertPos.xyz;
	
	gl_Position = vertPos;
}
