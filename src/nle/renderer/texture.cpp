#include "texture.h"

#include "../core/files.h"
#include "../core/utils.h"
#include "../../vendor/stb_image.h"

#include <cstring>
#include <vector>

namespace nle
{
    texture::texture(const std::string &path, bool flip, texture_filter filtering)
        : m_filtering(filtering)
    {
        load_from_file(path, flip);
    }

    texture::texture(const uint8_t *blob, size_t size, bool flip)
    {
        load_from_memory(blob, size, flip);
    }

    texture::texture(const uint8_t *pixels, int width, int height, int channels, bool flip,
                     texture_filter filtering)
        : m_filtering(filtering)
    {
        load_from_pixels(pixels, width, height, channels, flip);
    }

    texture::~texture()
    {
        glDeleteTextures(1, &m_id);
    }

    uint32_t texture::id() const
    {
        return m_id;
    }

    void texture::use(uint8_t unit) const
    {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, m_id);
    }

    void texture::unuse(uint8_t unit) const
    {
        glBindTexture(GL_TEXTURE_2D + unit, 0);
    }

    void texture::load_from_file(const std::string &path, bool flip)
    {
        if(m_id != 0)
            return;
        
        const auto bytes = files::read(path);

        if (!bytes || bytes->empty())
        {
            utils::prerror("Texture::load_from_file(): nothing here", path);
            return;
        }

        stbi_set_flip_vertically_on_load(flip);

        unsigned char *data = stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(bytes->data()),
                                                    static_cast<int>(bytes->size()), &m_width, &m_height,
                                                    &m_bit_depth, STBI_rgb_alpha);

        // The flag is stb's, not this texture's: one switch for the whole
        // program. Left on, it flipped whatever stb decoded next -- which
        // includes every glTF image, since tinygltf decodes through the same
        // stb -- so a model loaded after any picture came out with its
        // texture upside down and its UVs pointing at the wrong islands.
        stbi_set_flip_vertically_on_load(false);

        if (!data)
        {
            utils::prerror("Texture::load_from_file(): nothing here", path);
            return;
        }
        
        upload(data, GL_RGBA);

        stbi_image_free(data);
    }

    void texture::load_from_memory(const unsigned char *blob, size_t size, bool flip)
    {
        if(m_id != 0 || blob == nullptr || size == 0)
        {
            return;
        }

        stbi_set_flip_vertically_on_load(flip);

        unsigned char *data = stbi_load_from_memory(blob, size, &m_width, &m_height, &m_bit_depth, STBI_rgb_alpha);

        // Off again at once, for the reason given in load_from_file.
        stbi_set_flip_vertically_on_load(false);

        if(!data)
        {
            utils::prerror("Texture::load_from_memory(): error loading from memory");
            return;
        }
        
        upload(data, GL_RGBA);

        stbi_image_free(data);
    }

    void texture::load_from_pixels(const unsigned char *pixels, int width, int height, int channels, bool flip)
    {
        if(m_id != 0 || pixels == nullptr || width <= 0 || height <= 0)
        {
            return;
        }

        int format = GL_RGBA;
        switch(channels)
        {
            case 1: format = GL_RED; break;
            case 2: format = GL_RG; break;
            case 3: format = GL_RGB; break;
            case 4: format = GL_RGBA; break;
            default:
                utils::prerror("texture::load_from_pixels(): unsupported channel count", channels);
                return;
        }

        m_width = width;
        m_height = height;
        m_bit_depth = channels;

        std::vector<unsigned char> flipped;
        if(flip)
        {
            // stb's flip-on-load does not apply here; the rows are already decoded.
            const size_t stride = static_cast<size_t>(width) * static_cast<size_t>(channels);
            flipped.resize(stride * static_cast<size_t>(height));

            for(int y = 0; y < height; ++y)
            {
                std::memcpy(flipped.data() + stride * static_cast<size_t>(y),
                            pixels + stride * static_cast<size_t>(height - 1 - y),
                            stride);
            }

            pixels = flipped.data();
        }

        // rows of 1/2/3-channel images are not 4-byte aligned.
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        upload(pixels, format);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }

    int texture::width() const
    {
        return m_width;
    }

    int texture::height() const
    {
        return m_height;
    }

    void texture::upload(const unsigned char *pixels, int format)
    {
        glGenTextures(1, &m_id);
        glBindTexture(GL_TEXTURE_2D, m_id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        // Only where it was asked for. Turning this on for everything fixed
        // the ground boiling in the distance and broke every character in the
        // game: they are painted from a palette a few dozen pixels across,
        // and blending between its texels paints the robe onto the face.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        m_filtering == texture_filter::smooth ? GL_LINEAR_MIPMAP_LINEAR
                                                              : GL_NEAREST);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexImage2D(GL_TEXTURE_2D, 0, format, m_width, m_height, 0, format, GL_UNSIGNED_BYTE, pixels);
        glGenerateMipmap(GL_TEXTURE_2D);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

} // namespace nle
