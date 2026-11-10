/**
 * @file texture.h
 * @author Hasan Karaman (hk@hasankaraman.dev)
 * @brief 
 * @version 0.1
 * @date 2024-02-12
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#pragma once

#include "gl.h"

#include <string>

namespace nle
{

/**
 * @brief How a picture is sampled when it is smaller on screen than on disk.
 *
 * crisp keeps one texel one texel. It is what a palette belongs in: the
 * character packs paint a whole model from a picture a few dozen pixels
 * across, where the skin and the robe are neighbouring texels, and anything
 * that blends between texels blends the robe onto the face. Which is exactly
 * what happened the day this was not a choice.
 *
 * smooth uses the mipmaps, and is what anything tiled across a landscape
 * wants: with a texture repeating two hundred times, one screen pixel covers
 * many texels, and picking one of them at random is a surface that boils as
 * the camera moves.
 */
enum class texture_filter
{
    crisp,
    smooth
};

class texture
{
public:
    texture(const std::string& path, bool flip = true,
            texture_filter filtering = texture_filter::crisp);

    texture(const uint8_t *blob, size_t size, bool flip = true);

    /**
     * @brief Wraps pixels that are already decoded.
     *
     * glTF hands back decoded images rather than encoded files, so there is
     * nothing for stb to do. @p channels may be 1, 2, 3 or 4.
     */
    texture(const uint8_t *pixels, int width, int height, int channels, bool flip = false,
            texture_filter filtering = texture_filter::crisp);

    virtual ~texture();

    uint32_t id() const;

    /// The picture's own size, for anything that has to keep its shape.
    int width() const;
    int height() const;
    void use(uint8_t unit = 0) const;
    void unuse(uint8_t unit = 0) const;
private:
    uint32_t m_id = 0U;
    int m_width;
    int m_height;
    int m_bit_depth;

    texture_filter m_filtering = texture_filter::crisp;

    void load_from_file(const std::string& path, bool flip);
    void load_from_memory(const unsigned char *blob, size_t size, bool flip);
    void load_from_pixels(const unsigned char *pixels, int width, int height, int channels, bool flip);

    /// Applies the wrap/filter parameters and uploads to the bound texture.
    void upload(const unsigned char *pixels, int format);
};

} // namespace nle
