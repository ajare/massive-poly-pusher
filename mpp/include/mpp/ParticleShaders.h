#pragma once

namespace mpp
{
	// The vertical slice's simulation kernel. It is deliberately trivial: the
	// pool, free list, spawn shapes and behaviour modules arrive later. What it
	// does establish is the shape everything else is built on -- particle state
	// written straight into a shader storage buffer, and the draw's indirect
	// arguments written on the GPU, with no readback anywhere.
	inline char const* ParticleSimulationComputeShader = R"MPP(#version 430

layout(local_size_x = MPP_PARTICLE_WORK_GROUP_SIZE) in;

// xyz is the world position, w is the billboard half-extent.
layout(std430, binding = 0) restrict writeonly buffer ParticlePool
{
    vec4 PARTICLES[];
};

// One glDrawArraysIndirect command: count, instanceCount, first, baseInstance.
layout(std430, binding = 1) restrict writeonly buffer ParticleIndirectCommands
{
    uint INDIRECT_COMMAND[];
};

uniform uint PARTICLE_COUNT;
uniform float ELAPSED_SECONDS;

void main()
{
    uint particle = gl_GlobalInvocationID.x;
    if (particle >= PARTICLE_COUNT) return;

    // A golden-angle spiral: cheap, evenly distributed, and entirely derived
    // from the invocation index, so it needs no state carried between frames.
    float index = float(particle);
    float angle = index * 2.39996323 + ELAPSED_SECONDS * 0.35;
    float radius = 0.25 + 1.75 * fract(index * 0.61803399);
    float height = sin(index * 0.7 + ELAPSED_SECONDS) * 0.75;

    PARTICLES[particle] = vec4(cos(angle) * radius, height, sin(angle) * radius, 0.04);

    // The draw arguments are GPU-authored, which is the point of the slice: the
    // CPU never learns how many particles survived.
    if (particle == 0u)
    {
        INDIRECT_COMMAND[0] = 4u;             // vertices in one triangle-strip quad
        INDIRECT_COMMAND[1] = PARTICLE_COUNT; // instances
        INDIRECT_COMMAND[2] = 0u;             // first vertex
        INDIRECT_COMMAND[3] = 0u;             // base instance
    }
}
)MPP";

	// Attribute-less instanced billboards: the quad comes from gl_VertexID, the
	// particle from gl_InstanceID, and the instance count from the indirect
	// command. There is no vertex buffer and no Mesh anywhere on this path.
	inline char const* ParticleDrawVertexShader = R"MPP(#version 430

layout(std140, binding = 3) uniform CameraFrame
{
    mat4 VIEW_MATRIX;
    mat4 PROJECTION_MATRIX;
    mat4 INVERSE_PROJECTION_MATRIX;
    vec4 VIEWPORT_SIZE;
    vec4 NEAR_FAR_TIME;
};

layout(std430, binding = 0) restrict readonly buffer ParticlePool
{
    vec4 PARTICLES[];
};

out vec2 PARTICLE_CORNER;
out vec3 PARTICLE_TINT;

void main()
{
    vec4 particle = PARTICLES[gl_InstanceID];

    // 0,0  1,0  0,1  1,1 -- the triangle strip winding for one quad.
    vec2 corner = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1));
    PARTICLE_CORNER = corner;

    // Camera-facing by construction: the quad is expanded in view space, so the
    // basis needs no per-particle work. The other billboard modes differ only in
    // how that basis is built.
    vec3 viewPosition = (VIEW_MATRIX * vec4(particle.xyz, 1.0)).xyz;
    viewPosition.xy += (corner * 2.0 - 1.0) * particle.w;

    float hue = fract(float(gl_InstanceID) * 0.61803399);
    PARTICLE_TINT = 0.45 + 0.55 * cos(6.28318531 * (hue + vec3(0.0, 0.33, 0.67)));

    gl_Position = PROJECTION_MATRIX * vec4(viewPosition, 1.0);
}
)MPP";

	// Untextured for the slice; textures, atlases and curve LUTs arrive with the
	// particle appearance. MPP_PARTICLE_BLEND_ADDITIVE is the blend-class seam:
	// per ADR 0006 the draw is specialised by #define rather than branched.
	inline char const* ParticleDrawFragmentShader = R"MPP(#version 430

in vec2 PARTICLE_CORNER;
in vec3 PARTICLE_TINT;

layout(location = 0) out vec4 FRAGMENT_COLOUR;

void main()
{
    float edge = length(PARTICLE_CORNER * 2.0 - 1.0);
    float coverage = 1.0 - smoothstep(0.75, 1.0, edge);

#if MPP_PARTICLE_BLEND_ADDITIVE
    // Additive particles carry their coverage in the colour, and contribute
    // nothing to alpha: the scene target's coverage must not be disturbed.
    FRAGMENT_COLOUR = vec4(PARTICLE_TINT * coverage, 0.0);
#else
    FRAGMENT_COLOUR = vec4(PARTICLE_TINT, coverage);
#endif
}
)MPP";
}
