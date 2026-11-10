#include "shadow_map.h"

#include "../core/utils.h"

#include "gl.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace nle
{

shadow_map::shadow_map() = default;

shadow_map::~shadow_map()
{
    drop_resources();
}

void shadow_map::set_enabled(bool enabled)
{
    m_enabled = enabled;

    // Given up as soon as they are switched off, since a depth texture of
    // four megabytes is not worth keeping for a setting somebody turned off.
    if(!m_enabled)
    {
        drop_resources();
    }
}

bool shadow_map::enabled() const
{
    return m_enabled;
}

void shadow_map::set_resolution(int pixels)
{
    const int wanted = std::clamp(pixels, 256, 4096);

    if(wanted == m_resolution)
    {
        return;
    }

    m_resolution = wanted;

    // Rebuilt at the new size on the next frame that wants one.
    drop_resources();
}

int shadow_map::resolution() const
{
    return m_resolution;
}

void shadow_map::set_distance(float units)
{
    m_distance = std::max(10.0f, units);
}

float shadow_map::distance() const
{
    return m_distance;
}

bool shadow_map::ensure_resources()
{
    if(m_framebuffer != 0 && m_built_at == m_resolution)
    {
        return true;
    }

    drop_resources();

    glGenFramebuffers(1, &m_framebuffer);
    glGenTextures(1, &m_depth);

    glBindTexture(GL_TEXTURE_2D, m_depth);
    // Unsigned integers, which is what OpenGL ES insists a 24 bit depth is
    // given as -- even with nothing to give.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_resolution, m_resolution, 0,
                 GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Clamped to a border of "as far away as possible", so that anything
    // outside the box is lit rather than pitch black. Without this the world
    // past the shadow distance goes dark, which is a great deal worse than
    // having no shadows out there. OpenGL ES has no border, so there the
    // shader says it instead.
#ifdef NLE_GLES
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#else
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    const float border[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
#endif

    // Compared rather than read, so the sampler does the depth test itself
    // and returns how much of the fragment is lit.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depth, 0);

    // Depth only: there is no colour in this pass and saying so is what
    // makes it cheap.
    const GLenum no_colour = GL_NONE;
    glDrawBuffers(1, &no_colour);
    glReadBuffer(GL_NONE);

    const GLenum state = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if(state != GL_FRAMEBUFFER_COMPLETE)
    {
        utils::prerror("shadow_map: incomplete framebuffer");
        drop_resources();
        return false;
    }

    m_built_at = m_resolution;

    return true;
}

void shadow_map::drop_resources()
{
    if(m_depth != 0)
    {
        glDeleteTextures(1, &m_depth);
        m_depth = 0;
    }

    if(m_framebuffer != 0)
    {
        glDeleteFramebuffers(1, &m_framebuffer);
        m_framebuffer = 0;
    }

    m_built_at = 0;
}

glm::mat4 shadow_map::aim(const glm::vec3& eye, const glm::vec3& light_direction)
{
    glm::vec3 towards = light_direction;

    if(glm::length(towards) < 0.0001f)
    {
        towards = glm::vec3(0.0f, -1.0f, 0.0f);
    }

    towards = glm::normalize(towards);

    // Centred a little ahead of nothing in particular: the eye itself, since
    // that is what anybody is looking at. The box reaches m_distance in
    // every direction, so half of it is behind the camera -- which is what
    // casts the shadow of something standing behind you onto the ground in
    // front of you.
    const glm::vec3 centre = eye;
    const glm::vec3 from = centre - towards * m_distance;

    // Any up will do so long as it is not the light's own direction.
    const glm::vec3 up = std::fabs(towards.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f)
                                                      : glm::vec3(0.0f, 1.0f, 0.0f);

    const glm::mat4 view = glm::lookAt(from, centre, up);

    const glm::mat4 projection = glm::ortho(-m_distance, m_distance,
                                            -m_distance, m_distance,
                                            0.1f, m_distance * 2.5f);

    m_light_space = projection * view;

    return m_light_space;
}

bool shadow_map::begin()
{
    if(!m_enabled || !ensure_resources())
    {
        return false;
    }

    // Whatever was being drawn into before this, so it can be put back.
    // Bloom renders the scene into a target of its own, and binding the
    // window afterwards sent the whole scene somewhere bloom then painted
    // an empty buffer over -- a grey screen with the interface still on it.
    m_was_bound = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_was_bound);

    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glViewport(0, 0, m_resolution, m_resolution);
    glClear(GL_DEPTH_BUFFER_BIT);

    // Back faces only. A shadow cast by the far side of a thing floats free
    // of the thing itself, which hides the stripes that otherwise appear on
    // every surface facing the light.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    return true;
}

void shadow_map::end(int window_width, int window_height)
{
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<unsigned int>(m_was_bound));
    glViewport(0, 0, window_width, window_height);
}

void shadow_map::bind_for_reading(int texture_unit) const
{
    if(m_depth == 0)
    {
        return;
    }

    glActiveTexture(GL_TEXTURE0 + texture_unit);
    glBindTexture(GL_TEXTURE_2D, m_depth);
}

unsigned int shadow_map::texture() const
{
    return m_depth;
}

} // namespace nle
