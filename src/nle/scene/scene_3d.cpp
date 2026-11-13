#include "scene_3d.h"

#include <algorithm>

namespace nle
{

scene_3d::scene_3d()
{
    m_default_camera = make_ref<class camera>();
    m_default_light = make_ref<class light>();
}

scene_3d::~scene_3d()
{
    // A copy: deleting takes them out of the set being walked.
    const auto all = render_objects();

    for(const auto& ro : all)
    {
        delete_child(ro);
    }
}

void scene_3d::set_camera(ref<class camera> camera)
{
    m_camera = camera;
}

ref<class camera> scene_3d::camera()
{
    return m_camera == nullptr ? m_default_camera : m_camera;
}

ref<class camera> scene_3d::default_camera()
{
    return m_default_camera;
}

void scene_3d::set_light(ref<class light> light)
{
    m_light = light;
}

ref<class light> scene_3d::light()
{
    return m_light == nullptr ? m_default_light : m_light;
}

void scene_3d::add_point_light(ref<class point_light> light)
{
    if(!light)
    {
        return;
    }

    if(std::find(m_point_lights.begin(), m_point_lights.end(), light) == m_point_lights.end())
    {
        m_point_lights.push_back(light);
    }
}

void scene_3d::remove_point_light(ref<class point_light> light)
{
    m_point_lights.erase(std::remove(m_point_lights.begin(), m_point_lights.end(), light),
                         m_point_lights.end());
}

const std::vector<ref<class point_light>>& scene_3d::point_lights() const
{
    return m_point_lights;
}

std::vector<point_light_data> scene_3d::collect_point_lights(const glm::vec3& eye,
                                                             const frustum& view) const
{
    std::vector<std::pair<float, point_light_data>> candidates;
    candidates.reserve(m_point_lights.size());

    for(const auto& pl : m_point_lights)
    {
        if(!pl || !pl->enabled())
        {
            continue;
        }

        // Nothing it could light is on screen. The shader would work this
        // out too -- it refuses any fragment past the range -- but only
        // after the light had taken one of the eight slots there are and
        // been tested against every fragment in the frame.
        if(!view.holds(pl->position(), pl->range()))
        {
            continue;
        }

        candidates.emplace_back(glm::distance(pl->position(), eye), pl->to_point_light_data());
    }

    /// Nearest lights win the limited slots. Sorting by distance to the eye
    /// rather than to each object is an approximation, but it keeps the set
    /// stable across the frame, which avoids lights popping between draws.
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    if(candidates.size() > static_cast<size_t>(MAX_POINT_LIGHTS))
    {
        candidates.resize(static_cast<size_t>(MAX_POINT_LIGHTS));
    }

    std::vector<point_light_data> result;
    result.reserve(candidates.size());
    for(auto& candidate : candidates)
    {
        result.push_back(candidate.second);
    }

    return result;
}

void scene_3d::set_sky(ref<class sky> sky)
{
    m_sky = sky;
    m_sky->set_scene(shared_from_this());
    m_sky->set_shader(this->shader());
}

ref<class sky> scene_3d::sky()
{
    return m_sky;
}

void scene_3d::set_fog(const fog_data& fog)
{
    m_fog = fog;
}

const fog_data& scene_3d::fog() const
{
    return m_fog;
}

void scene_3d::render(render_command_buffer& command_buffer, const render_context& context)
{
    for(const auto& it : render_objects())
    {
        it->render(command_buffer, context);
    }
}

void scene_3d::add_child(ref<object_3d> child)
{
    render_object_3d::add_child(child);
    
    /// register as render object (if it's a render object)
    if(auto ro = std::dynamic_pointer_cast<render_object_3d>(child))
    {
        auto sp = shared_from_this();
        ro->set_scene(sp);
    }

    /// lights are not render objects, but the renderer still needs to find them.
    if(auto pl = std::dynamic_pointer_cast<class point_light>(child))
    {
        add_point_light(pl);
    }
}

void scene_3d::delete_child(ref<object_3d> child)
{
    /// first we unregister render object (if it's a render object)
    auto ro = std::dynamic_pointer_cast<render_object_3d>(child);
    if(ro)
    {
        ro->m_scene.reset();
    }

    if(auto pl = std::dynamic_pointer_cast<class point_light>(child))
    {
        remove_point_light(pl);
    }

    render_object_3d::delete_child(child);
}

nlohmann::json scene_3d::to_json() const
{
    auto j = object_3d::to_json();
    throw std::runtime_error("nlohmann::json scene_3d::to_json() not fully implemented");
    return j;
}

void scene_3d::from_json(const nlohmann::json &j)
{
    object_3d::from_json(j);
    throw std::runtime_error("nlohmann::json scene_3d::from_json() not fully implemented");
}

void scene_3d::set_target_resolution(glm::vec2 resolution)
{
    m_target_resolution = resolution;
}

glm::vec2 scene_3d::target_resolution() const
{
    return m_target_resolution;
}
} // namespace nle
