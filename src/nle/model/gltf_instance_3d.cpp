#include "gltf_instance_3d.h"

#include "../renderer/surface_uniforms.h"

#include <algorithm>
#include <cmath>

namespace nle
{

gltf_instance_3d::gltf_instance_3d(ref<model_gltf> model)
    : multimesh_instance_3d(model ? model->multimesh() : make_ref<multimesh_3d>()),
      m_model(model)
{
    if(m_model)
    {
        m_animator = make_ref<class animator>(m_model->skeleton(), m_model->animations());
    }
}

gltf_instance_3d::~gltf_instance_3d()
{
}

ref<class animator> gltf_instance_3d::animator()
{
    return m_animator;
}

ref<model_gltf> gltf_instance_3d::model() const
{
    return m_model;
}

void gltf_instance_3d::set_auto_advance(bool auto_advance)
{
    m_auto_advance = auto_advance;
}

bool gltf_instance_3d::auto_advance() const
{
    return m_auto_advance;
}

int gltf_instance_3d::node_index(const std::string& name) const
{
    if(!m_model || !m_model->skeleton())
    {
        return -1;
    }

    return m_model->skeleton()->find_node(name);
}

void gltf_instance_3d::set_node_visible(const std::string& name, bool visible)
{
    const int node = node_index(name);

    if(node < 0)
    {
        return;
    }

    if(visible)
    {
        m_hidden_nodes.erase(node);
    }
    else
    {
        m_hidden_nodes.insert(node);
    }
}

bool gltf_instance_3d::node_visible(int node) const
{
    return m_hidden_nodes.find(node) == m_hidden_nodes.end();
}

float gltf_instance_3d::bounding_radius() const
{
    if(!m_model || !m_animator)
    {
        return 0.0f;
    }

    float furthest = 0.0f;

    for(const auto& primitive : m_model->primitives())
    {
        if(!primitive.mesh)
        {
            continue;
        }

        const float own = primitive.mesh->bounding_radius();

        if(primitive.skin >= 0)
        {
            // A skinned piece is posed entirely by its joints, which work in
            // the model's own space, so it is already measured from the
            // right origin.
            furthest = std::max(furthest, own);
            continue;
        }

        // An unskinned piece is drawn through its node, so how far it reaches
        // is how far the node stands from the origin plus how big the piece
        // is once that node has scaled it.
        const glm::mat4& node = m_animator->node_world_matrix(primitive.node);

        const float stretch = std::max({ glm::length(glm::vec3(node[0])),
                                         glm::length(glm::vec3(node[1])),
                                         glm::length(glm::vec3(node[2])) });

        furthest = std::max(furthest, glm::length(glm::vec3(node[3])) + own * stretch);
    }

    const glm::vec3 size = scale();
    const float most = std::max({ std::abs(size.x), std::abs(size.y), std::abs(size.z) });

    // Room to spare, because a skin moves its vertices away from the pose
    // they were saved in and nothing here has looked at where they went.
    constexpr float ROOM_TO_MOVE = 1.35f;

    return furthest * most * ROOM_TO_MOVE;
}

void gltf_instance_3d::render(render_command_buffer& command_buffer, const render_context& context)
{
    if(!m_model || !m_animator)
    {
        return;
    }

    advance(context);

    command_buffer.set_polygon_mode(GL_FRONT_AND_BACK, static_cast<GLenum>(render_mode()));
    command_buffer.use_shader(this->shader());

    const glm::mat4 instance_transform = this->transform_matrix();
    const auto& override_material = this->material_override();

    // A model is usually split into several primitives that all share one
    // skin -- this character is 15 primitives over a single 62-joint skin --
    // and the palette uniform persists between draws, so it only has to be
    // uploaded when the skin actually changes.
    int uploaded_skin = -1;

    for(const auto& primitive : m_model->primitives())
    {
        if(!primitive.mesh)
        {
            continue;
        }

        // Hidden geometry is skipped here rather than earlier, so the pose is
        // still computed and anything hung off the node still follows it.
        if(!node_visible(primitive.node))
        {
            continue;
        }

        record_surface_uniforms(command_buffer, context, primitive.mesh->texture(),
                                override_material ? override_material : primitive.mesh->material());

        if(primitive.skin >= 0)
        {
            const auto& palette = m_animator->joint_matrices(static_cast<size_t>(primitive.skin));

            if(!palette.empty())
            {
                // glTF poses a skinned mesh entirely through its joints, so the
                // node's own transform is not part of the model matrix here.
                command_buffer.set_uniform("u_skinning_enabled", 1);

                if(primitive.skin != uploaded_skin)
                {
                    command_buffer.set_uniform("u_joint_matrices", palette);
                    uploaded_skin = primitive.skin;
                }

                command_buffer.set_uniform("u_model", instance_transform);
            }
            else
            {
                command_buffer.set_uniform("u_skinning_enabled", 0);
                command_buffer.set_uniform("u_model", instance_transform);
            }
        }
        else
        {
            command_buffer.set_uniform("u_skinning_enabled", 0);
            command_buffer.set_uniform("u_model",
                                       instance_transform * m_animator->node_world_matrix(primitive.node));
        }

        // Translucent as a whole when an override says so: blended, and not
        // writing depth, or a see-through body hides what is behind it as
        // well as a solid one would.
        const bool faint = override_material && override_material->dissolve() < 1.0f;

        if(faint)
        {
            command_buffer.set_blending(override_material->blending());
            command_buffer.set_depth_mask(false);
        }

        // As far as fog may hide it. Set after the material's own, which is
        // what each draw starts from, so nothing needs putting back.
        if(this->fog_cap() < 1.0f)
        {
            const auto& used = override_material ? override_material : primitive.mesh->material();
            const float material_limit = used ? used->fog_limit() : 1.0f;
            command_buffer.set_uniform("u_material.fog_limit", std::min(material_limit, this->fog_cap()));
        }

        // Its sheen, for these draws alone.
        const bool polished = this->sheen() > 0.0f;

        if(polished)
        {
            command_buffer.set_uniform("u_sheen", glm::vec4(this->sheen_colour(), this->sheen()));
            command_buffer.set_uniform("u_sheen_tint", this->sheen_tint());
        }

        command_buffer.draw_elements(static_cast<GLenum>(this->primitive_type()),
                                     primitive.mesh->vao(),
                                     primitive.mesh->ebo(),
                                     primitive.mesh->indices().size());

        if(polished)
        {
            command_buffer.set_uniform("u_sheen", glm::vec4(0.0f));
            command_buffer.set_uniform("u_sheen_tint", 0.0f);
        }

        if(faint)
        {
            command_buffer.set_depth_mask(true);
            command_buffer.set_blending(blend_mode::none);
        }
    }
}

void gltf_instance_3d::advance(const render_context& context)
{
    if(m_auto_advance && context.time != m_advanced_at)
    {
        m_animator->update(context.delta_time);
        m_advanced_at = context.time;
    }
}

const void* gltf_instance_3d::copies_key()
{
    if(!m_model || !m_animator || this->material_override() || this->sheen() > 0.0f
       || this->fog_cap() < 1.0f || !m_hidden_nodes.empty() || render_mode() != nle::render_mode::fill
       || this->primitive_type() != nle::primitive_type::triangles)
    {
        return nullptr;
    }

    for(const auto& primitive : m_model->primitives())
    {
        if(primitive.skin >= 0)
        {
            return nullptr;
        }
    }

    return m_model.get();
}

void gltf_instance_3d::render_copies(render_command_buffer& command_buffer, const render_context& context,
                                     const std::vector<render_object_3d*>& copies)
{
    // The renderer only hands over copies of this model; each moves its own
    // pose on, which is how a windmill's sails turn while it is one of many.
    std::vector<gltf_instance_3d*> placed;
    placed.reserve(copies.size());

    for(auto* one : copies)
    {
        auto* copy = static_cast<gltf_instance_3d*>(one);
        copy->advance(context);
        placed.push_back(copy);
    }

    command_buffer.set_polygon_mode(GL_FRONT_AND_BACK, static_cast<GLenum>(nle::render_mode::fill));
    command_buffer.use_shader(this->shader());
    command_buffer.set_uniform("u_skinning_enabled", 0);
    command_buffer.set_uniform("u_instanced", 1);

    for(const auto& primitive : m_model->primitives())
    {
        if(!primitive.mesh)
        {
            continue;
        }

        std::vector<glm::mat4> transforms;
        transforms.reserve(placed.size());

        for(auto* copy : placed)
        {
            transforms.push_back(copy->transform_matrix() * copy->m_animator->node_world_matrix(primitive.node));
        }

        record_surface_uniforms(command_buffer, context, primitive.mesh->texture(), primitive.mesh->material());

        command_buffer.draw_copies(static_cast<GLenum>(nle::primitive_type::triangles), primitive.mesh->vao(),
                                   primitive.mesh->ebo(), primitive.mesh->indices().size(),
                                   std::move(transforms));
    }

    // Back as every other draw expects it.
    command_buffer.set_uniform("u_instanced", 0);
}

bool gltf_instance_3d::see_through()
{
    const auto& override_material = this->material_override();

    return override_material && override_material->dissolve() < 1.0f;
}

} // namespace nle
