#version 150

uniform sampler2D u_texture0;
uniform vec4 u_colour;

in vec3 vout_normal3;
in vec2 vout_uv2;
in vec4 vout_colour4;

out vec4 fout_colour4;

void main()
{
	fout_colour4 = texture(u_texture0, vout_uv2) * vout_colour4 * u_colour * vec4(vout_normal3.xyz, 1);
}
