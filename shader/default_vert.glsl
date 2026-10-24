#version 330

#define NLE_MAX_JOINTS 64

out vec4 io_vertex_color;
out vec2 io_texture_coordinates;
out vec3 io_normal;
out vec3 io_frag_position;
out vec4 io_light_space_position;

uniform mat4 u_model;
uniform mat4 u_projection;
uniform mat4 u_view;
uniform mat4 u_light_space;

// Skinning. u_joint_matrices holds, for the skin this draw belongs to,
// joint_world_matrix * inverse_bind_matrix for every joint.
uniform int u_skinning_enabled;

uniform float u_waves;
uniform float u_time;

uniform mat4 u_joint_matrices[NLE_MAX_JOINTS];

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 color;
layout (location = 3) in vec2 texture;
layout (location = 4) in vec4 joints;
layout (location = 5) in vec4 weights;

int joint_index(float raw)
{
    // Clamped so malformed weights can never index outside the array.
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

// The swell on water: three long waves crossing at angles, so it never
// lines up into rows. The same here and in default_frag.glsl, which shades by
// its slopes. Height about one either way; u_waves scales it.
vec3 wave_at(vec2 at, float time)
{
    const vec2 D1 = vec2(0.80, 0.60);
    const vec2 D2 = vec2(-0.45, 0.89);
    const vec2 D3 = vec2(0.97, -0.24);
    const float K1 = 0.21;
    const float K2 = 0.33;
    const float K3 = 0.52;

    float p1 = dot(D1, at) * K1 + time * 0.6;
    float p2 = dot(D2, at) * K2 + time * 0.85;
    float p3 = dot(D3, at) * K3 + time * 1.1;

    float height = 0.5 * sin(p1) + 0.3 * sin(p2) + 0.2 * sin(p3);

    // And how fast that changes along x and along z.
    vec2 slope = 0.5 * K1 * cos(p1) * D1 + 0.3 * K2 * cos(p2) * D2 + 0.2 * K3 * cos(p3) * D3;

    return vec3(height, slope);
}

void main() {
    mat4 model = u_model;

    if (u_skinning_enabled == 1)
    {
        model = model * skin_matrix();
    }

    vec4 world_position = model * vec4(position, 1.0);

    if (u_waves > 0.0)
    {
        world_position.y += u_waves * wave_at(world_position.xz, u_time).x;
    }

    gl_Position = u_projection * u_view * world_position;
    io_texture_coordinates = texture;
    io_normal = mat3(transpose(inverse(model))) * normal;
    io_frag_position = world_position.xyz;

    // Where this ends up in the depth map the sun drew. Worked out here
    // rather than in the fragment shader because it is a matrix multiply
    // per vertex instead of one per pixel, and interpolating the result is
    // exactly the same answer.
    io_light_space_position = u_light_space * world_position;
    io_vertex_color = vec4(color, 1.0f);
}
