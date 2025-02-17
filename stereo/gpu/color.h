#pragma once

#include <stereo/defs.h>

namespace stereo {
   
enum struct ColorSpace {
    Linear_sRGB,
    Gamma_sRGB,
    Oklab,
};

inline vec3 linear_srgb_to_oklab(vec3 c) {
    float l = 0.4122214708f * c.r + 0.5363325363f * c.g + 0.0514459929f * c.b;
    float m = 0.2119034982f * c.r + 0.6806995451f * c.g + 0.1073969566f * c.b;
    float s = 0.0883024619f * c.r + 0.2817188376f * c.g + 0.6299787005f * c.b;

    float l_ = std::cbrtf(l);
    float m_ = std::cbrtf(m);
    float s_ = std::cbrtf(s);

    return {
        0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_,
        1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_,
        0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_,
    };
}

inline vec3 oklab_to_linear_srgb(vec3 c) {
    float L = c.x;
    float a = c.y;
    float b = c.z;
    float l_ = L + 0.3963377774f * a + 0.2158037573f * b;
    float m_ = L - 0.1055613458f * a - 0.0638541728f * b;
    float s_ = L - 0.0894841775f * a - 1.2914855480f * b;

    float l = l_ * l_ * l_;
    float m = m_ * m_ * m_;
    float s = s_ * s_ * s_;

    return {
		+4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
		-1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
		-0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s,
    };
}

/// Convert a linear color to a gamma-corrected color.
inline vec3 linear_to_gamma_encode(vec3 c, float γ) {
    return std::pow(c, 1.f / γ);
}

/// Apply the gamma correction to obtain a linear color.
inline vec3 gamma_to_linear_decode(vec3 c, float γ) {
    return std::pow(c, γ);
}

} // namespace stereo
