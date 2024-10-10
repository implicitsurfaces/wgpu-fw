#pragma once

#include <stereo/defs.h>

namespace stereo {

std::optional<size_t>   find_cmd_option(int argc, char** argv, std::string_view option);
std::optional<uint32_t> get_option_u32(int argc, char** argv, std::string_view option);
std::optional<float>    get_option_f32(int argc, char**argv, std::string_view option);

template <typename T>
std::optional<T> get_choice(
    int argc,
    char** argv,
    std::string_view option,
    std::initializer_list<std::pair<std::string_view, T>> choices)
{
    auto i = find_cmd_option(argc, argv, option);
    if (i) {
        size_t index = *i + 1;
        if (index < argc) {
            auto choice = std::string_view(argv[index]);
            for (auto& [name, value] : choices) {
                if (choice == name) return value;
            }
            std::cerr << "Invalid choice for option `" << option << "`" << std::endl;
            return std::nullopt;
        } else {
            std::cerr << "Missing choice argument for option `" << option << "`" << std::endl;
            return std::nullopt;
        }
    } else {
        return std::nullopt;
    }
}

}  // namespace stereo
