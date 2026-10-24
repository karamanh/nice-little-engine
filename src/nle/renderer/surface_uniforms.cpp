#include "surface_uniforms.h"

#include "material.h"
#include "texture.h"

namespace nle
{

void record_surface_uniforms(render_command_buffer& command_buffer,
                             const render_context& context,
                             const ref<class texture>& texture,
                             const ref<class material>& material)
{
    if(texture)
    {
        command_buffer.set_uniform("u_texture_enabled", 1);
        command_buffer.use_texture(texture);
    }
    else
    {
        command_buffer.set_uniform("u_texture_enabled", 0);
    }

    bool accept_light = true;

    if(material)
    {
        command_buffer.set_uniform("u_material.ambient", material->ambient());
        command_buffer.set_uniform("u_material.diffuse", material->diffuse());
        command_buffer.set_uniform("u_material.specular", material->specular());
        command_buffer.set_uniform("u_material.shininess", material->shininess());
        command_buffer.set_uniform("u_material.dissolve", material->dissolve());
        command_buffer.set_uniform("u_material.accept_light", static_cast<int>(material->accept_light()));
        command_buffer.set_uniform("u_material.fog_limit", material->fog_limit());
        command_buffer.set_uniform("u_waves", material->waves());
        accept_light = material->accept_light();
    }
    else
    {
        // Uniforms belong to the program, and the last draw may have left
        // its own limit behind.
        command_buffer.set_uniform("u_material.fog_limit", 1.0f);
        command_buffer.set_uniform("u_waves", 0.0f);
    }

    // Only whether to consult the lights is per-draw; their values are frame
    // constants uploaded by the backend.
    command_buffer.set_uniform("u_lighting_enabled",
                               (accept_light && context.directional_light.enabled) ? 1 : 0);
    command_buffer.set_uniform("u_point_lighting_enabled", accept_light ? 1 : 0);

    // Not ground. The terrain says otherwise for itself, after this; every
    // other surface has to say it, because uniforms belong to the program and
    // the program is shared, so a value left behind by the last draw is a
    // value this one inherits.
    command_buffer.set_uniform("u_terrain_extent", 0.0f);

    // What the sun could see, and where to look it up. Sent per surface
    // like everything else here, so that a shader without these simply
    // drops them as it drops any uniform it has no use for.
    command_buffer.set_uniform("u_light_space", context.light_space);
    command_buffer.set_uniform("u_shadows_enabled",
                               (accept_light && context.shadows_enabled) ? 1 : 0);
    command_buffer.set_uniform("u_shadow_map", context.shadow_texture_unit);
    command_buffer.set_uniform("u_shadow_softness", context.shadow_softness);
}

} // namespace nle
