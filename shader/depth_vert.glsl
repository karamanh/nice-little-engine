#version 330 core

// The depth pass. Everything the lit vertex shader does to put a vertex
// where it belongs, and nothing it does to work out how it should look --
// this pass keeps only how far away things were.
//
// Skinning is here because a character's shadow has to be the shape the
// character is actually in. Without it every animated thing casts the
// shadow of its bind pose, standing with its arms out.

#define NLE_MAX_JOINTS 128

uniform mat4 u_model;
uniform mat4 u_projection;
uniform mat4 u_view;

uniform int u_skinning_enabled;
uniform mat4 u_joint_matrices[NLE_MAX_JOINTS];

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 color;
layout (location = 3) in vec2 texture_coordinates;
layout (location = 4) in vec4 joints;
layout (location = 5) in vec4 weights;

void main()
{
    vec4 local = vec4(position, 1.0);

    if (u_skinning_enabled == 1)
    {
        mat4 skin = weights.x * u_joint_matrices[int(joints.x)]
                  + weights.y * u_joint_matrices[int(joints.y)]
                  + weights.z * u_joint_matrices[int(joints.z)]
                  + weights.w * u_joint_matrices[int(joints.w)];

        local = skin * local;
    }

    gl_Position = u_projection * u_view * u_model * local;
}
