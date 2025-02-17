#pragma once

#include <stereo/gpu/texture.h>

namespace stereo {

struct OverlayColor;

struct MaterialCoeffs {
    alignas(16) vec3 k_diff  = vec3(1.);
    alignas(16) vec3 k_spec  = vec3(1.);
    alignas(16) vec3 k_emit  = vec3(0.);
    float            k_rough = 1.;
    float            k_metal = 1.;
    float            alpha   = 1.;
    
    MaterialCoeffs operator*(const MaterialCoeffs& other) const {
        return MaterialCoeffs {
            (vec3) k_diff * (vec3) other.k_diff,
            (vec3) k_spec * (vec3) other.k_spec,
            (vec3) k_emit * (vec3) other.k_emit,
            k_rough * other.k_rough,
            k_metal * other.k_metal,
            alpha   * other.alpha,
        };
    }
};

struct Material {
    
    TextureRef diffuse;
    TextureRef specular;
    TextureRef emission;
    TextureRef roughness;
    TextureRef metallic;
    TextureRef alpha;
    TextureRef normal;
    
    MaterialCoeffs coeffs;
    
};

using MaterialRef = std::shared_ptr<Material>;

struct OverlayColor {
    std::optional<vec4> tint     = std::nullopt;
    std::optional<vec4> additive = std::nullopt;
    std::optional<vec4> over     = std::nullopt;
    
    /**
        * @brief Apply another overlay color to this one, replacing
        * the settings in this color with those in the other which
        * are specified.
        */
    OverlayColor operator|(const OverlayColor& other) const {
        OverlayColor out = *this;
        if (other.tint)     out.tint     = other.tint;
        if (other.additive) out.additive = other.additive;
        if (other.over)     out.over     = other.over;
        return out;
    }
    
    static OverlayColor none() {
        return OverlayColor {
            vec4{1.f},
            vec4{0.f},
            vec4{0.f}
        };
    }
};


}
