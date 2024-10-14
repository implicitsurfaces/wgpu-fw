#pragma once

#include <stereo/gpu/mip_generator.h>

namespace stereo {

struct Image {
    std::shared_ptr<uint8_t[]> data = nullptr;
    vec2u size     = {0, 0};
    int   channels =  0;
};

Image load_jpeg(std::string_view filename);

Texture load_texture(std::string_view filename, MipGeneratorRef mip_generator);

}  // namespace stereo
