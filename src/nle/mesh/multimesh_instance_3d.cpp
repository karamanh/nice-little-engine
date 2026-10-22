#include "multimesh_instance_3d.h"

#include <algorithm>
#include <cmath>

namespace nle
{

    multimesh_instance_3d::multimesh_instance_3d(ref<class multimesh_3d> multimesh)
    {
        m_multimesh = multimesh;

        if(m_multimesh)
        {
            for(auto m3d : m_multimesh->meshes())
            {
                add_child(make_ref<mesh_instance_3d>(m3d));
            }
        }

        update();
    }

    multimesh_instance_3d::~multimesh_instance_3d()
    {
    }

    ref<class multimesh_3d> multimesh_instance_3d::multimesh()
    {
        return m_multimesh;
    }

    float multimesh_instance_3d::bounding_radius() const
    {
        if(!m_multimesh)
        {
            return 0.0f;
        }

        // Every part measured from this object's own origin, so the sphere
        // covers the whole of what is drawn rather than the largest single
        // piece of it.
        float furthest = 0.0f;

        for(const auto& one : m_multimesh->meshes())
        {
            if(one)
            {
                furthest = std::max(furthest, one->bounding_radius());
            }
        }

        const glm::vec3 size = scale();
        const float most = std::max({ std::abs(size.x), std::abs(size.y), std::abs(size.z) });

        // A little room to spare. What is measured is the shape the model
        // was saved in, and a model that animates leaves that shape -- an
        // arm swings out, a wing opens -- so the sphere is drawn wider than
        // the pose it was taken from.
        constexpr float ROOM_TO_MOVE = 1.35f;

        return furthest * most * ROOM_TO_MOVE;
    }

    void multimesh_instance_3d::render(render_command_buffer& command_buffer, const render_context& context)
    {
        for(auto ro : render_objects())
        {
            // The whole of it polished alike: the pieces take the sheen that
            // was asked of the model.
            ro->set_sheen(this->sheen(), this->sheen_colour(), this->sheen_tint());
            ro->set_fog_cap(this->fog_cap());
            ro->render(command_buffer, context);
        }
    }

    void multimesh_instance_3d::set_material_override(ref<class material> material_override)
    {
        render_object_3d::set_material_override(material_override);

        for(auto ro : render_objects())
        {
            ro->set_material_override(material_override);
        }
    }

    bool multimesh_instance_3d::see_through()
    {
        const auto& used = this->material_override();
        return used && used->dissolve() < 1.0f;
    }

    void multimesh_instance_3d::update()
    {
        object_3d::update();
        /// TODO: update model matrix
    }

} // namespace nle
