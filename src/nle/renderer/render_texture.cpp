#include "render_texture.h"

#include "../core/utils.h"

#include "gl.h"

#include <algorithm>

namespace nle
{

render_texture::render_texture()
{
}

render_texture::~render_texture()
{
    destroy();
}

void render_texture::destroy()
{
    if(m_depth != 0)
    {
        glDeleteRenderbuffers(1, &m_depth);
    }

    if(m_color != 0)
    {
        glDeleteTextures(1, &m_color);
    }

    if(m_framebuffer != 0)
    {
        glDeleteFramebuffers(1, &m_framebuffer);
    }

    m_framebuffer = 0;
    m_color = 0;
    m_depth = 0;
    m_width = 0;
    m_height = 0;
}

bool render_texture::resize(int width, int height)
{
    width = std::max(width, 1);
    height = std::max(height, 1);

    if(m_framebuffer != 0 && width == m_width && height == m_height)
    {
        return true;
    }

    destroy();

    GLint was_bound = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &was_bound);

    glGenFramebuffers(1, &m_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

    glGenTextures(1, &m_color);
    glBindTexture(GL_TEXTURE_2D, m_color);

    // Eight bits a channel, with alpha: this is shown rather than processed
    // further, and the alpha is what lets it stand over whatever is behind.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_color, 0);

    glGenRenderbuffers(1, &m_depth);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depth);
    // With a stencil, which outlines are masked with.
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depth);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(was_bound));

    if(status != GL_FRAMEBUFFER_COMPLETE)
    {
        utils::prerror("render_texture: framebuffer incomplete", static_cast<int>(status));
        destroy();
        return false;
    }

    m_width = width;
    m_height = height;

    return true;
}

unsigned int render_texture::framebuffer() const
{
    return m_framebuffer;
}

unsigned int render_texture::texture() const
{
    return m_color;
}

int render_texture::width() const
{
    return m_width;
}

int render_texture::height() const
{
    return m_height;
}

} // namespace nle
