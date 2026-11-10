#version 330 core


uniform vec3 eyePosition;

in vec3     io_position;
in float    io_render_scale;
out vec4    io_color;

const float epsilon = 0.0001;
float line_thickness = 0.0005;

float log_n(float base, float value)
{
    return log(value) / log(base);
}

void main()
{
    float scale = io_render_scale;

    // in glsl, base of log(x) is e
    // if we need log(x) with base n we need to do:
    // log(x) / log(n);

    float cell_size = 10.0;
    float a = log_n(cell_size, scale);

    if((pow(cell_size, a) - scale) < epsilon)
    {
        cell_size *= floor(a);
    }
    else
    {
        cell_size *= pow(cell_size, floor(a) + 1.0);
    }

    float dx = mod(abs(io_position.x), cell_size);
    float dz = mod(abs(io_position.z), cell_size);
    line_thickness *= scale;

    if((dx < line_thickness) || (dz < line_thickness))
    {
        if((abs(io_position.x) < line_thickness))
        {
            io_color = vec4(1.0, 0.0, 0.0, 1.0);
        }
        else if((abs(io_position.z) < line_thickness))
        {
            io_color = vec4(0.0, 1.0, 0.0, 1.0);
        }
        else
        {
            io_color = vec4(0.5, 0.5, 0.5, 1.0);
        }
    }
    else
    {
        discard;
    }

    // const float n = 10.0;
    // const float log_n = log(n);
    // float a = log(scale) / log_n;
    // float cell_size = 1.0;

    // if((pow(n, a) - scale) < epsilon)
    // {
    //     cell_size *= floor(a);
    // }
    // else
    // {
    //     cell_size *= pow(n, floor(a) + 1);
    // }

    // float dx = mod(abs(io_position.x), cell_size);
    // float dz = mod(abs(io_position.z), cell_size);
    // line_thickness *= io_render_scale;

    // if((dx < line_thickness) || (dz < line_thickness))
    // {
    //     if((abs(io_position.x) < line_thickness))
    //     {
    //         io_color = vec4(1.0, 0.0, 0.0, 1.0);
    //     }
    //     else if((abs(io_position.z) < line_thickness))
    //     {
    //         io_color = vec4(0.0, 1.0, 0.0, 1.0);
    //     }
    //     else
    //     {
    //         io_color = vec4(0.5, 0.5, 0.5, 1.0);
    //     }
    // }
    // else
    // {
    //     discard;
    // }
}