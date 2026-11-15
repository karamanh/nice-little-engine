#version 330

// A soft cloud made of solid shapes: each puff is thick where it is looked
// at square on and fades to nothing at its rim, so a handful of low rounded
// shapes read as one body of cloud with no edge to it. No pictures, no
// lights and no stepping through anything -- it costs what it covers of the
// screen and nothing anywhere else. Drawn with default_vert.glsl.

in vec4 io_vertex_color;
in vec3 io_normal;
in vec3 io_frag_position;

out vec4 io_color;

struct Material
{
    // How much of it there is at its thickest, nought to one.
    float dissolve;

    // What colour the light of the hour makes it; the puffs' own shading is
    // in their vertices.
    vec3 diffuse;
};

uniform Material u_material;
uniform vec3 u_eye_position;

void main()
{
    vec3 normal = normalize(io_normal);
    vec3 towards = normalize(u_eye_position - io_frag_position);

    // Either side of a puff: seen from inside it, it is still cloud.
    float facing = abs(dot(normal, towards));
    // Gone a little before the outline: a shape made of flat faces has an
    // outline with corners, and they must not show.
    float soft = smoothstep(0.2, 0.85, facing);

    io_color = vec4(io_vertex_color.rgb * u_material.diffuse, soft * soft * u_material.dissolve);
}
