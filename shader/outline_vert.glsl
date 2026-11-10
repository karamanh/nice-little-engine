#version 330 core

// The outline pass. The thing is drawn twice with this: once as it is, into
// the stencil only, and once with every vertex pushed out along its normal,
// where the stencil says it was not -- which leaves a rim round its edge.
//
// Pushed by a share of the distance to the eye rather than a fixed amount,
// so the rim is about the same few pixels whether the thing is at your feet
// or across the field. Skinned the same way the lit pass skins, or a
// running monster would be outlined in its bind pose.

#define NLE_MAX_JOINTS 64

uniform mat4 u_model;
uniform mat4 u_projection;
uniform mat4 u_view;
uniform vec3 u_eye_position;

uniform int u_skinning_enabled;
uniform mat4 u_joint_matrices[NLE_MAX_JOINTS];

uniform float u_outline_width;

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 color;
layout (location = 3) in vec2 texture_coordinates;
layout (location = 4) in vec4 joints;
layout (location = 5) in vec4 weights;

int joint_index(float raw)
{
    return int(clamp(raw, 0.0, float(NLE_MAX_JOINTS - 1)));
}

mat4 skin_matrix()
{
    float total = weights.x + weights.y + weights.z + weights.w;

    if (total <= 0.0)
    {
        return mat4(1.0);
    }

    mat4 skin = weights.x * u_joint_matrices[joint_index(joints.x)]
              + weights.y * u_joint_matrices[joint_index(joints.y)]
              + weights.z * u_joint_matrices[joint_index(joints.z)]
              + weights.w * u_joint_matrices[joint_index(joints.w)];

    return skin / total;
}

void main()
{
    mat4 model = u_model;

    if (u_skinning_enabled == 1)
    {
        model = model * skin_matrix();
    }

    vec4 world_position = model * vec4(position, 1.0);
    vec3 world_normal = mat3(transpose(inverse(model))) * normal;

    float length_of = length(world_normal);
    vec3 outward = length_of > 0.0 ? world_normal / length_of : vec3(0.0);

    float away = distance(world_position.xyz, u_eye_position);

    world_position.xyz += outward * u_outline_width * away;

    gl_Position = u_projection * u_view * world_position;
}
