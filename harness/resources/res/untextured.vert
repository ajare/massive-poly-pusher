#version 150

uniform mat4 u_projection;
uniform mat4 u_camera;
uniform mat4 u_model;
uniform mat3 u_normalMatrix;

in vec3 vin_pos3;
in vec3 vin_normal3;
in vec2 vin_uv2;
in vec4 vin_colour4;

out vec3 vout_normal3;
out vec2 vout_uv2;
out vec4 vout_colour4;

void main() 
{
	vout_uv2 = vin_uv2;
	vout_colour4 = vin_colour4;
	
	vout_normal3 = normalize(u_normalMatrix * vin_normal3);
	gl_Position =  u_projection * u_camera * u_model * vec4(vin_pos3, 1);
}