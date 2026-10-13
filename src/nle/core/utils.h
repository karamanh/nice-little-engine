#pragma once

#include "files.h"

#include <array>
#include <random>
#include <string>
#include <fstream>
#include <sstream>
#include <cstring>
#include <iostream>
#include <algorithm>

// C++20 throughout, so the standard one on every compiler. The old test for
// which filesystem a GCC had read glibc's <features.h>, which only glibc has.
#include <filesystem>
namespace fs = std::filesystem;

namespace nle::utils
{
    extern std::ostream& output_stream;

    template <typename arg_type, typename... args_type>
    inline void print(arg_type &&arg, args_type &&...args)
    {
        output_stream << "\x1b[32m" << std::forward<arg_type>(arg);
        ((output_stream << ',' << std::forward<args_type>(args)), ...);
        output_stream << "\x1b[0m\n";
    }

    template <typename arg_type, typename... args_type>
    inline void prdebug(arg_type &&arg, args_type &&...args)
    {
        output_stream << "\x1b[33m" << std::forward<arg_type>(arg);
        ((output_stream << ',' << std::forward<args_type>(args)), ...);
        output_stream << "\x1b[0m\n";
    }

    template <typename arg_type, typename... args_type>
    inline void prerror(arg_type &&arg, args_type &&...args)
    {
        output_stream << "\x1b[31m" << std::forward<arg_type>(arg);
        ((output_stream << ',' << std::forward<args_type>(args)), ...);
        output_stream << "\x1b[0m\n";
    }

    const constexpr unsigned int UID_LEN = 16;

    template <typename T = std::mt19937>
    inline auto random_generator() -> T
    {
        auto constexpr seed_bytes = sizeof(typename T::result_type) * T::state_size;
        auto constexpr seed_len = seed_bytes / sizeof(std::seed_seq::result_type);
        auto seed = std::array<std::seed_seq::result_type, seed_len>();
        auto dev = std::random_device();
        std::generate_n(std::begin(seed), seed_len, std::ref(dev));
        auto seed_seq = std::seed_seq(std::begin(seed), std::end(seed));
        return T{seed_seq};
    }

    inline std::string generate_random_alphanumeric_string(std::size_t len = UID_LEN)
    {
        static constexpr auto chars =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        thread_local auto rng = random_generator<>();
        auto dist = std::uniform_int_distribution{{}, std::strlen(chars) - 1};
        auto result = std::string(len, '\0');
        std::generate_n(std::begin(result), len, [&]()
                        { return chars[dist(rng)]; });
        return result;
    }

    /// The whole of a file, empty if there is none. Through nle::files, so
    /// shaders come from wherever the game keeps its files.
    inline std::string load_file(const std::string& path)
    {
        return files::read(path).value_or(std::string());
    }

} // namespace nle
