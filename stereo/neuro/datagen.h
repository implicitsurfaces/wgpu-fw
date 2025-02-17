#pragma once

#include <pcg_random.hpp>

#include <geomc/function/PerlinNoise.h>
#include <stereo/gpu/simple_render.h>
#include <stereo/gpu/texture.h>
#include <stereo/gpu/mip_generator.h>
#include <stereo/gpu/material.h>
#include <stereo/gpu/color.h>
#include <stereo/gpu/simple_render.h>
#include <stereo/util/grid.h>

namespace stereo {

struct SceneExample {
    Camera<double> cam_a;
    Camera<double> cam_b;
};

struct ExampleGen {
    pcg64                 rng;
    PerlinNoise<double,2> noise;
    MipGeneratorRef       mip_gen;
    TextureRef            white_tex;
    TextureRef            black_tex;
    TextureRef            grey_tex;
    
    
    ExampleGen(SimpleRender* renderer, pcg64 rng);
    
    ModelRef add_scene(SimpleRender* renderer);
    
    size_t add_surface(ModelRef model, size_t width, MaterialId material_id=0);
    
    /// Fill a grid with a smooth, random heightfield having N channels
    template <index_t N>
    void random_heightfield(Grid2d<VecType<float,N>>& grid);
    
    /**
     * @brief Generate a square texture, with N the number of channels
     *
     * The color space is the space in which to generate the random signal. The
     * target color space is always unorm sRGB.
     */
    template <index_t N>
    TextureRef create_texture(
        gpu_size_t width,
        ColorSpace src_color_space=ColorSpace::Gamma_sRGB,
        const AffineTransform<float, N>& color_xf={}
    );
    
    MaterialRef create_material(gpu_size_t width, bool has_alpha);
};

} // namespace stereo
