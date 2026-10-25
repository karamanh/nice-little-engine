#include "renderer_3d.h"
#include "render_texture.h"

#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>

namespace nle
{
    renderer_3d::renderer_3d(ref<window_glfw> render_target)
        : m_render_target(render_target)
    {
        glClearColor(m_clear_color.r, m_clear_color.g, m_clear_color.b, 1.0f);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        render_target->render_3d() = [this](){
            this->main_routine();
        };
    }

    renderer_3d::~renderer_3d()
    {
    }

    void renderer_3d::set_current_scene(ref<scene_3d> scene)
    {
        m_current_scene = scene;
    }

    ref<scene_3d> renderer_3d::current_scene()
    {
        return m_current_scene;
    }

    class bloom& renderer_3d::bloom()
    {
        return m_bloom;
    }

    ref<window_glfw> renderer_3d::render_target()
    {
        return m_render_target;
    }

    void renderer_3d::set_render_layer_attribute(enum render_layer layer, const struct render_layer_attribute& attribute)
    {
        m_render_layer_attributes[layer] = attribute;
    }

    render_layer_attribute renderer_3d::render_layer_attribute_of(enum render_layer layer)
    {
        return m_render_layer_attributes[layer];
    }

    const opengl_backend::frame_statistics& renderer_3d::statistics() const
    {
        return m_opengl_backend.statistics();
    }

    float renderer_3d::delta_time() const
    {
        return m_delta_time;
    }

    void renderer_3d::set_clear_color(const glm::vec3& color)
    {
        m_clear_color = color;
        glClearColor(color.r, color.g, color.b, 1.0f);
    }

    glm::vec3 renderer_3d::clear_color() const
    {
        return m_clear_color;
    }

    render_context renderer_3d::build_render_context(ref<scene_3d> scene)
    {
        render_context context;

        auto camera = scene->camera();

        context.resolution = scene->target_resolution();
        const float aspect_ratio = context.resolution.y > 0.0f
            ? context.resolution.x / context.resolution.y
            : 1.0f;

        context.projection = glm::perspective(camera->field_of_view(), aspect_ratio, camera->near(), camera->far());
        context.view = camera->view_matrix();
        context.eye_position = camera->position();
        context.time = m_time;
        context.delta_time = m_delta_time;

        // Worked out before the lights, because which lights are worth
        // uploading is the same question as what the camera can see.
        context.view_frustum = frustum::of(context.projection * context.view);

        context.directional_light = scene->light()->to_directional_light_data();
        context.point_lights = scene->collect_point_lights(context.eye_position,
                                                           context.view_frustum);

        context.fog = scene->fog();
        context.fog.banks = banks_in_sight(context.fog, context.eye_position, context.view_frustum);

        return context;
    }

    std::vector<fog_bank_data> renderer_3d::banks_in_sight(const fog_data& fog, const glm::vec3& eye,
                                                           const frustum& view)
    {
        // Past this a bank is not worth its cost: it is a smudge at the
        // bottom of the view at best, and every pixel would still pay for it.
        // Nearer still where the distance fog has already hidden everything.
        constexpr float FARTHEST = 600.0f;
        const float reach = fog.enabled ? std::min(FARTHEST, fog.far_distance) : FARTHEST;

        std::vector<std::pair<float, fog_bank_data>> kept;

        for(fog_bank_data bank : fog.banks)
        {
            if(!bank.enabled || bank.density <= 0.0f)
            {
                continue;
            }

            // How far to its nearest edge, along the ground; nought from
            // inside it, ring or not.
            const float across = glm::length(glm::vec2(eye.x - bank.centre.x, eye.z - bank.centre.z));
            const float to_edge = std::max(0.0f, across - bank.radius);

            if(to_edge >= reach)
            {
                continue;
            }

            // And whether it is anywhere in front of the camera.
            const float half_tall = (bank.top - bank.bottom) * 0.5f;
            const glm::vec3 middle(bank.centre.x, bank.centre.y + bank.bottom + half_tall, bank.centre.z);

            if(!view.holds(middle, std::sqrt(bank.radius * bank.radius + half_tall * half_tall)))
            {
                continue;
            }

            // Thinning in over the last quarter of the way, so one does not
            // appear all at once as you walk towards it.
            const float fade = std::clamp((reach - to_edge) / (reach * 0.25f), 0.0f, 1.0f);
            bank.density *= fade * fade * (3.0f - 2.0f * fade);

            kept.emplace_back(across, bank);
        }

        // Farthest first: the shaders lay them over the picture in order, so
        // the nearest must come last to be in front.
        std::sort(kept.begin(), kept.end(), [](const auto& a, const auto& b) { return a.first > b.first; });

        if(kept.size() > static_cast<size_t>(MAX_FOG_BANKS))
        {
            kept.erase(kept.begin(), kept.end() - MAX_FOG_BANKS);
        }

        std::vector<fog_bank_data> banks;
        banks.reserve(kept.size());

        for(auto& [far, bank] : kept)
        {
            banks.push_back(bank);
        }

        return banks;
    }

    bool renderer_3d::is_visible(const ref<render_object_3d>& ro, const glm::vec3& eye,
                                 const frustum& view)
    {
        if(!ro->visible())
        {
            return false;
        }

        const auto& attribute = m_render_layer_attributes[ro->render_layer()];
        if(!attribute.visible)
        {
            return false;
        }

        if(glm::distance(ro->position(), eye) >= attribute.render_distance)
        {
            return false;
        }

        // And whether the camera is pointing at it at all. A render distance
        // on its own keeps a ball around the eye, and most of a ball is
        // behind you: at any one moment the screen holds a wedge of it, and
        // the rest was drawn so that nobody could see it.
        //
        // Something that does not know its own size says nothing here and is
        // drawn. The terrain is the case that matters -- it is one object
        // the size of the map, and there is no answer to "is the ground on
        // screen" other than yes.
        const float radius = ro->bounding_radius();

        return radius <= 0.0f || view.holds(ro->position(), radius);
    }

    class shadow_map& renderer_3d::shadows()
    {
        return m_shadows;
    }

    void renderer_3d::set_depth_shader(ref<class shader> shader)
    {
        m_depth_shader = std::move(shader);
    }

    void renderer_3d::set_outline_shader(ref<class shader> shader)
    {
        m_outline_shader = std::move(shader);
    }

    void renderer_3d::render_scene(ref<scene_3d> scene, bool with_shadows)
    {
        render_context context = build_render_context(scene);

        // What the sun can see, drawn first. Everything is recorded exactly
        // as it would be for the lit pass -- same objects, same skinning,
        // same model matrices -- and drawn with one shader that keeps only
        // depth, so a character's shadow is the shape the character is
        // actually in rather than the shape of its bind pose.
        const bool casting = with_shadows && m_shadows.enabled() && m_depth_shader
                          && context.directional_light.enabled;

        if(casting)
        {
            context.light_space = m_shadows.aim(context.eye_position,
                                                context.directional_light.direction);
            context.shadows_enabled = true;

            if(m_shadows.begin())
            {
                render_context from_the_sun = context;

                // The light's box, as one matrix. The depth shader still
                // multiplies projection by view, so one of them is enough
                // and the other is nothing.
                from_the_sun.projection = context.light_space;
                from_the_sun.view = glm::mat4(1.0f);

                m_shadow_commands.clear();

                // Only what is inside the light's box. Drawing the whole
                // level here cost more than the lit pass did -- it ignored
                // every render distance the game had set and redrew four
                // hundred props a frame at map resolution.
                //
                // The test is against the eye rather than the camera's
                // frustum, because something behind you is exactly what
                // puts a shadow in front of you.
                const float reach = m_shadows.distance();

                for(auto ro : scene->render_objects())
                {
                    if(!ro->visible() || !ro->casts_shadow())
                    {
                        continue;
                    }

                    if(glm::distance(ro->position(), context.eye_position) > reach)
                    {
                        continue;
                    }

                    ro->render(m_shadow_commands, from_the_sun);
                }

                // The light's own view and projection, which are uploaded
                // when a shader is bound and so have to be in place before
                // anything is drawn. Without this the depth map was the
                // scene from the camera, which casts no shadows anybody
                // would recognise.
                m_opengl_backend.begin_frame(from_the_sun);

                // Each with its own shader, as it has always been drawn: the
                // depth shader was asked for here, but the backend did not
                // honour forcing until outlines needed it, and turning it on
                // for this pass as well would change every shadow in the game
                // -- particles, which are laid out differently, among them.
                m_opengl_backend.execute_commands(m_shadow_commands);

                m_shadows.end(m_render_target->width(), m_render_target->height());
            }
            else
            {
                context.shadows_enabled = false;
            }
        }

        if(context.shadows_enabled)
        {
            m_shadows.bind_for_reading(context.shadow_texture_unit);
        }

        m_command_buffer.clear();
        m_opengl_backend.begin_frame(context);

        if(scene->sky())
        {
            scene->sky()->set_position(context.eye_position);
            scene->sky()->render(m_command_buffer, context);
        }

        // Solid things first, then the see-through ones.
        //
        // Something see-through does not write depth, and must not: it would
        // hide what is behind it. The cost is that anything opaque drawn
        // afterwards paints straight over it, and the order these arrive in
        // is the order of their addresses -- so whether a portal or a spell's
        // circle survived the frame depended on where the allocator had put
        // the ground. Travelling to another level shuffled that, which is why
        // they went missing on arriving somewhere rather than at any
        // particular place.
        std::vector<ref<render_object_3d>> see_through;

        for(auto ro : scene->render_objects())
        {
            if(!is_visible(ro, context.eye_position, context.view_frustum))
            {
                continue;
            }

            if(ro->see_through())
            {
                see_through.push_back(ro);
                continue;
            }

            ro->render(m_command_buffer, context);
        }

        // Far to near among themselves, so two that overlap blend in the
        // order the eye expects rather than the order they were made in.
        std::sort(see_through.begin(), see_through.end(),
                  [&](const ref<render_object_3d>& a, const ref<render_object_3d>& b) {
                      return glm::distance(a->position(), context.eye_position)
                           > glm::distance(b->position(), context.eye_position);
                  });

        for(auto& ro : see_through)
        {
            ro->render(m_command_buffer, context);
        }

        // Execute all commands through OpenGL backend
        m_opengl_backend.execute_commands(m_command_buffer);

        draw_glow_shells(scene, context);
        draw_outlines(scene, context);
    }

    void renderer_3d::draw_glow_shells(const ref<scene_3d>& scene, const render_context& context)
    {
        if(!m_outline_shader)
        {
            return;
        }

        bool any = false;

        for(auto ro : scene->render_objects())
        {
            if(ro->glow_shell_width() <= 0.0f || !is_visible(ro, context.eye_position, context.view_frustum))
            {
                continue;
            }

            if(!any)
            {
                // Light added, not painted: behind what is in front, never
                // hiding anything, and only its far side -- the inverted hull
                // -- so it rings the edge rather than tinting the face.
                any = true;
                glEnable(GL_DEPTH_TEST);
                glDepthMask(GL_FALSE);
                glEnable(GL_BLEND);
                glBlendFunc(GL_ONE, GL_ONE);
                glEnable(GL_CULL_FACE);
                glCullFace(GL_FRONT);
            }

            m_outline_commands.clear();
            ro->render(m_outline_commands, context);

            m_outline_shader->use();

            if(const int at = m_outline_shader->uniform_location("u_outline_width"); at != -1)
            {
                glUniform1f(at, ro->glow_shell_width());
            }

            if(const int at = m_outline_shader->uniform_location("u_outline_colour"); at != -1)
            {
                const glm::vec3& c = ro->glow_shell_colour();
                glUniform3f(at, c.r, c.g, c.b);
            }

            m_opengl_backend.invalidate_state_cache();
            m_opengl_backend.force_shader(m_outline_shader);
            m_opengl_backend.execute_commands(m_outline_commands);
            m_opengl_backend.force_shader(nullptr);
        }

        if(any)
        {
            glCullFace(GL_BACK);
            glDisable(GL_CULL_FACE);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            m_opengl_backend.invalidate_state_cache();
        }
    }

    void renderer_3d::draw_outlines(const ref<scene_3d>& scene, const render_context& context)
    {
        if(!m_outline_shader)
        {
            return;
        }

        auto set_uniforms = [&](float width, const glm::vec3& c) {
            m_outline_shader->use();

            if(const int at = m_outline_shader->uniform_location("u_outline_width"); at != -1)
            {
                glUniform1f(at, width);
            }

            if(const int at = m_outline_shader->uniform_location("u_outline_colour"); at != -1)
            {
                glUniform3f(at, c.r, c.g, c.b);
            }
        };

        auto draw_with_outline_shader = [&]() {
            // Bound behind the backend's back just now, so it must not trust
            // what it thinks is bound.
            m_opengl_backend.invalidate_state_cache();
            m_opengl_backend.force_shader(m_outline_shader);
            m_opengl_backend.execute_commands(m_outline_commands);
            m_opengl_backend.force_shader(nullptr);
        };

        bool any = false;

        for(auto ro : scene->render_objects())
        {
            if(ro->outline_width() <= 0.0f || !is_visible(ro, context.eye_position, context.view_frustum))
            {
                continue;
            }

            any = true;

            m_outline_commands.clear();
            ro->render(m_outline_commands, context);

            // First the shape itself, into the stencil and nowhere else:
            // every pixel it covers, hidden or not, is marked.
            glClear(GL_STENCIL_BUFFER_BIT);
            glEnable(GL_STENCIL_TEST);
            glStencilMask(0xFF);
            glStencilFunc(GL_ALWAYS, 1, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glDisable(GL_DEPTH_TEST);

            set_uniforms(0.0f, ro->outline_colour());
            draw_with_outline_shader();

            // Then the same shape swollen, everywhere it was not: what is
            // left is a rim exactly round its edge, whatever its normals do.
            // Depth-tested, so a wall in front hides the rim as it hides the
            // thing itself.
            glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
            glStencilMask(0x00);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glEnable(GL_DEPTH_TEST);

            set_uniforms(ro->outline_width(), ro->outline_colour());
            draw_with_outline_shader();
        }

        if(any)
        {
            glStencilMask(0xFF);
            glDisable(GL_STENCIL_TEST);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glEnable(GL_DEPTH_TEST);
            m_opengl_backend.invalidate_state_cache();
        }
    }

    void renderer_3d::render_to(ref<scene_3d> scene, render_texture& target, const glm::vec4& clear)
    {
        if(!scene || target.framebuffer() == 0)
        {
            return;
        }

        GLint was_bound = 0;
        GLint viewport[4] = { 0, 0, 0, 0 };
        GLfloat was_clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        GLboolean depth_mask = GL_TRUE;

        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &was_bound);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, was_clear);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);

        GLint blend[4] = { GL_ONE, GL_ZERO, GL_ONE, GL_ZERO };
        glGetIntegerv(GL_BLEND_SRC_RGB, &blend[0]);
        glGetIntegerv(GL_BLEND_DST_RGB, &blend[1]);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend[2]);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blend[3]);

        const GLboolean blending = glIsEnabled(GL_BLEND);
        const GLboolean depth_test = glIsEnabled(GL_DEPTH_TEST);
        const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);

        glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer());
        glViewport(0, 0, target.width(), target.height());

        // The state the scene's own drawing assumes it starts from; the
        // interface in the middle of whose frame this may be has its own.
        glDisable(GL_BLEND);
        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);

        glClearColor(clear.r, clear.g, clear.b, clear.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        scene->set_target_resolution(glm::vec2(target.width(), target.height()));
        render_scene(scene, false);

        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(was_bound));
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glClearColor(was_clear[0], was_clear[1], was_clear[2], was_clear[3]);
        glDepthMask(depth_mask);

        if(blending) { glEnable(GL_BLEND); } else { glDisable(GL_BLEND); }
        if(depth_test) { glEnable(GL_DEPTH_TEST); } else { glDisable(GL_DEPTH_TEST); }
        if(scissor) { glEnable(GL_SCISSOR_TEST); } else { glDisable(GL_SCISSOR_TEST); }

        // The blend function too: see-through things change it.
        glBlendFuncSeparate(blend[0], blend[1], blend[2], blend[3]);
    }

    void renderer_3d::main_routine()
    {
        const int64_t now_us = m_clock.elapsed_time_us();
        m_delta_time = static_cast<float>(now_us - m_last_frame_us) / 1000000.0f;
        m_last_frame_us = now_us;
        m_time = static_cast<float>(now_us) / 1000000.0f;

        const int width = m_render_target->width();
        const int height = m_render_target->height();

        // The scene goes into bloom's own target when it is on, and straight
        // to the window when it is not. Either way it is cleared once, here.
        if(!m_bloom.begin(width, height))
        {
            glViewport(0, 0, width, height);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        if(m_current_scene)
        {
            m_current_scene->set_target_resolution(glm::vec2(width, height));
            render_scene(m_current_scene);
        }

        // Nothing if bloom did not begin, so the interface drawn after this
        // lands on the window either way.
        m_bloom.end();

        // glfw specific functions like glfwSwapBuffers are called
        // from window_glfw::display().
        // we only have gl specific function calls here.
    }

} // namespace nle
