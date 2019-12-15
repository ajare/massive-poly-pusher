#version 150

@@Uniform halfWindowSize = vec2

@@In pos = vec2
@@Passthrough uv = vec2
@@Passthrough colour = vec4

void main()
{
	vec4 transVertex = @MCPMatrix * vec4(@In(pos), 0, 1);
	vec2 centredPos = vec2(transVertex.x - @Uniform(halfWindowSize).x, transVertex.y - @Uniform(halfWindowSize).y);
	
	gl_Position = vec4(centredPos / @Uniform(halfWindowSize), 0, 1);
	gl_PointSize = 2;
}

