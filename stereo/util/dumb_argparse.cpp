#include <stereo/util/dumb_argparse.h>

namespace stereo {

std::optional<size_t> find_cmd_option(int argc, char** argv, std::string_view option) {
    auto it = std::find(argv, argv + argc, option);
    if (it == argv + argc) return std::nullopt;
    return std::distance(argv, it);
}

std::optional<uint32_t> get_option_u32(int argc, char** argv, std::string_view option) {
    auto i = find_cmd_option(argc, argv, option);
    if (i) {
        size_t index = *i + 1;
        if (index < argc) {
            try {
                return std::stoi(argv[index]);
            } catch (std::invalid_argument& e) {
                std::cerr << "Invalid integer argument for option `" << option << "`" << std::endl;
                return std::nullopt;
            }
        } else {
            std::cerr << "Missing integer argument for option `" << option << "`" << std::endl;
            return std::nullopt;
        }
    } else {
        return std::nullopt;
    }
}

std::optional<float> get_option_f32(int argc, char**argv, std::string_view option) {
    auto i = find_cmd_option(argc, argv, option);
    if (i) {
        size_t index = *i + 1;
        if (index < argc) {
            try {
                return std::stof(argv[index]);
            } catch (std::invalid_argument& e) {
                std::cerr << "Invalid float argument for option `" << option << "`" << std::endl;
                return std::nullopt;
            }
        } else {
            std::cerr << "Missing float argument for option `" << option << "`" << std::endl;
            return std::nullopt;
        }
    } else {
        return std::nullopt;
    }
}

}  // namespace stereo
