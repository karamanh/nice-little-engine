/**
 * @file files.h
 * @brief Where the engine's loaders get their files from.
 *
 * Every picture, model and piece of music the engine loads is found and read
 * through here rather than on disk directly, so a game can keep its files
 * however it likes -- in an archive, under other names, changed in a way
 * only it can undo -- without any loader knowing. Nothing set, it is the disk.
 */

#pragma once

#include <memory>
#include <optional>
#include <string>

namespace nle::files
{

/// What the loaders' files come from, if not the disk.
class source
{
public:
    virtual ~source() = default;

    /// The whole of the file at @p path, or nothing if there is none.
    virtual std::optional<std::string> read(const std::string& path) = 0;

    /// Whether there is a file at @p path.
    virtual bool exists(const std::string& path) = 0;
};

/**
 * @brief Has every later lookup go through @p from instead of the disk.
 *
 * Set once, before anything is loaded. Null puts the disk back.
 */
void set_source(std::shared_ptr<source> from);

/// The whole of a file: from the source if one is set, else from disk.
std::optional<std::string> read(const std::string& path);

/// Whether there is a file: in the source if one is set, else on disk.
bool exists(const std::string& path);

/// The whole of a file straight from disk, whatever source is set.
std::optional<std::string> read_disk(const std::string& path);

} // namespace nle::files
