#version 150

attribute vec3 pos;
attribute vec3 normal;
attribute vec2 uv;
attribute vec4 colour;

varying vec3 vPos;
varying vec3 vNormal;
varying vec2 vUv;
varying vec4 vColour;

void main()
{
	vec4 vertPos = @MCPMatrix * vec4(pos, 1);
	
    vPos = pos;
    vNormal = normalize(@NormalMatrix * normal);
    vUv = uv;
    vColour = colour;
    
	gl_Position = vertPos;
}
