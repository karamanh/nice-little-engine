#include "render_object_3d.h"

namespace nle
{
    render_object_3d::render_object_3d(const std::string &id)
        : object_3d(id)
    {
        m_visible = true;
        m_render_layer = render_layer::_0;
        m_render_mode = render_mode::fill;
        m_primitive_type = primitive_type::triangles;
    }

    render_object_3d::~render_object_3d()
    {
    }

    void render_object_3d::set_render_mode(enum render_mode rm)
    {
        m_render_mode = rm;
        for(auto ro : m_render_objects)
        {
            ro->set_render_mode(rm);
        }
    }

    render_mode render_object_3d::render_mode() const
    {
        return m_render_mode;
    }

    void render_object_3d::set_render_layer(enum render_layer rl)
    {
        m_render_layer = rl;
        for(auto ro : m_render_objects)
        {
            ro->set_render_layer(rl);
        }
    }

    enum render_layer render_object_3d::render_layer() const
    {
        return m_render_layer;
    }

    void render_object_3d::set_primitive_type(enum primitive_type pt)
    {
        m_primitive_type = pt;
        for(auto ro : m_render_objects)
        {
            ro->set_primitive_type(pt);
        }
    }

    primitive_type render_object_3d::primitive_type()
    {
        return m_primitive_type;
    }

    void render_object_3d::set_visible(bool visible)
    {
        m_visible = visible;
        for(auto ro : m_render_objects)
        {
            ro->set_visible(visible);
        }
    }

    bool render_object_3d::visible() const
    {
        return m_visible;
    }

    void render_object_3d::set_shader(ref<class shader> shader)
    {
        m_shader = shader;
        for(auto ro : m_render_objects)
        {
            ro->set_shader(m_shader);
        }
    }

    ref<class shader> render_object_3d::shader()
    {
        return m_shader;
    }

    void render_object_3d::set_material_override(ref<class material> material_override)
    {
        m_material_override = material_override;
    }

    ref<class material> render_object_3d::material_override()
    {
        return m_material_override;
    }

    void render_object_3d::set_sheen(float amount, const glm::vec3& colour, float tint)
    {
        m_sheen = amount > 0.0f ? amount : 0.0f;
        m_sheen_colour = colour;
        m_sheen_tint = tint < 0.0f ? 0.0f : (tint > 1.0f ? 1.0f : tint);
    }

    float render_object_3d::sheen_tint() const
    {
        return m_sheen_tint;
    }

    void render_object_3d::set_fog_cap(float cap)
    {
        m_fog_cap = cap < 0.0f ? 0.0f : (cap > 1.0f ? 1.0f : cap);
    }

    float render_object_3d::fog_cap() const
    {
        return m_fog_cap;
    }

    float render_object_3d::sheen() const
    {
        return m_sheen;
    }

    const glm::vec3& render_object_3d::sheen_colour() const
    {
        return m_sheen_colour;
    }

    void render_object_3d::set_glow_shell(float width, const glm::vec3& colour)
    {
        m_glow_shell_width = width > 0.0f ? width : 0.0f;
        m_glow_shell_colour = colour;
    }

    float render_object_3d::glow_shell_width() const
    {
        return m_glow_shell_width;
    }

    const glm::vec3& render_object_3d::glow_shell_colour() const
    {
        return m_glow_shell_colour;
    }

    void render_object_3d::set_outline(float width, const glm::vec3& colour)
    {
        m_outline_width = width > 0.0f ? width : 0.0f;
        m_outline_colour = colour;
    }

    float render_object_3d::outline_width() const
    {
        return m_outline_width;
    }

    const glm::vec3& render_object_3d::outline_colour() const
    {
        return m_outline_colour;
    }

    nlohmann::json render_object_3d::to_json() const
    {
        auto j = object_3d::to_json();
        j["render_layer"] = static_cast<int>(m_render_layer);
        j["render_mode"] = static_cast<int>(m_render_mode);
        return j;
    }

    void render_object_3d::from_json(const nlohmann::json &j)
    {
        object_3d::from_json(j);
        m_render_layer = static_cast<enum render_layer>(j["render_layer"]);
        m_render_mode = static_cast<enum render_mode>(j["render_mode"]);
    }

    void render_object_3d::set_scene(ref<class render_object_3d> scene)
    {
        m_scene = scene;
        for(auto ro : m_render_objects)
        {
            ro->set_scene(scene);
        }
    }

    ref<class render_object_3d> render_object_3d::scene()
    {
        return m_scene;
    }

    std::set<ref<class render_object_3d>, std::owner_less<ref<class render_object_3d>>> render_object_3d::render_objects()
    {
        return m_render_objects;
    }

    void render_object_3d::add_child(ref<object_3d> child)
    {
        object_3d::add_child(child);

        if(auto ro = std::dynamic_pointer_cast<render_object_3d>(child))
        {
            if(m_render_objects.insert(ro).second)
            {
                ro->set_scene(m_scene);
                ro->set_shader(m_shader);
                ro->set_visible(m_visible);
                ro->set_render_layer(m_render_layer);
                ro->set_primitive_type(m_primitive_type);
            }
        }
    }
    
    void render_object_3d::delete_child(ref<object_3d> child)
    {
        if(auto ro = std::dynamic_pointer_cast<render_object_3d>(child))
        {
            m_render_objects.erase(ro);
        }
        object_3d::delete_child(child);
    }
} // namespace nle
