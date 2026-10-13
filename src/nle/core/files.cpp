#include "files.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>

namespace nle::files
{

namespace
{
    std::mutex g_mutex;
    std::shared_ptr<source> g_source;

    std::shared_ptr<source> current()
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        return g_source;
    }
}

void set_source(std::shared_ptr<source> from)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_source = std::move(from);
}

std::optional<std::string> read(const std::string& path)
{
    if(auto from = current())
    {
        return from->read(path);
    }

    return read_disk(path);
}

bool exists(const std::string& path)
{
    if(auto from = current())
    {
        return from->exists(path);
    }

    std::error_code error;
    return std::filesystem::is_regular_file(path, error);
}

std::optional<std::string> read_disk(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);

    if(!in)
    {
        return std::nullopt;
    }

    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

} // namespace nle::files
