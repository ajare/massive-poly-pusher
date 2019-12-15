#version 150

@@Uniform light = vec3

@@Texture tex = sampler2D

varying vec3 vPos;
varying vec3 vNormal;
varying vec2 vUv;
varying vec4 vColour;

out vec4 outColour;

void main()
{
	vec3 l = normalize(@Uniform(light) - vPos);   
	float d = max(dot(vNormal,l), 0.0);  
	d = clamp(d, 0.0, 1.0); 

	vec3 shadedColour = vColour.xyz * d;
	outColour = texture(@Texture(tex), vUv) * vec4(shadedColour, 1.0);
}
