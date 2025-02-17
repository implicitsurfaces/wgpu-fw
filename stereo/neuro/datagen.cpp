#include <random>
#include <stereo/gpu/color.h>
#include <stereo/neuro/datagen.h>

namespace stereo {

ExampleGen::ExampleGen(SimpleRender* renderer, pcg64 rng):
    rng(rng),
    noise(rng),
    mip_gen(std::make_shared<MipGenerator>(renderer->device())),
    white_tex(renderer->white_tex()),
    black_tex(renderer->black_tex()),
    grey_tex(renderer->grey_tex()) {}

ModelRef ExampleGen::add_scene(SimpleRender* renderer) {
    std::cout << "generating scene... ";
    std::flush(std::cout);
    ModelRef model = std::make_shared<Model>();
    MaterialRef material = create_material(1024, true); // xxx alpha always on
    MaterialId material_id = renderer->add_material(material);
    add_surface(model, 256, material_id);
    renderer->add_model(model);
    std::cout << "done." << std::endl;
    return model;
}

size_t ExampleGen::add_surface(ModelRef model, size_t width, MaterialId material_id) {
    Grid2d<float> grid({width, width});
    random_heightfield<1>(grid);
    size_t v0 = model->verts.size();
    size_t n = width * width;
    model->verts.reserve(model->verts.size() + n);
    range h_range = range::empty;
    for (size_t y = 0; y < grid.dims.y; ++y) {
        for (size_t x = 0; x < grid.dims.x; ++x) {
            // compute normal
            vec3 n;
            float xx = static_cast<float>(x) / grid.dims.x;
            float yy = static_cast<float>(y) / grid.dims.y;
            if (x < grid.dims.x - 1 and y < grid.dims.y - 1) {
                float dx = 1. / grid.dims.x;
                float dy = 1. / grid.dims.y;
                vec3 a = {xx,      yy,      grid(x,     y    )};
                vec3 b = {xx + dx, yy,      grid(x + 1, y    )};
                vec3 c = {xx,      yy + dy, grid(x,     y + 1)};
                n = (b - a).cross(c - a).unit();
            } else {
                // xxx: todo: do something better for the edges
                n = {0,0,1};
            }
            // compute UV
            vec2 uv = {
                x / (float) grid.dims.x,
                y / (float) grid.dims.y
            };
            float h = grid(x, y);
            h_range |= h;
            model->verts.push_back({
                .p  = {xx, yy, h},
                .n  = n,
                .uv = uv,
            });
        }
    }
    
    // add triangle indices
    size_t i0 = model->indices.size();
    for (size_t y = 0; y < grid.dims.y - 1; ++y) {
        for (size_t x = 0; x < grid.dims.x - 1; ++x) {
            gpu_size_t i0 = v0 + y * grid.dims.x + x;
            gpu_size_t i1 = i0 + 1;
            gpu_size_t i2 = i0 + grid.dims.x;
            gpu_size_t i3 = i2 + 1;
            model->indices.insert(model->indices.end(), {i0, i1, i2, i1, i3, i2});
        }
    }
    
    // add a prim for the surface
    float w = static_cast<float>(width);
    model->prims.push_back({
        .index_range = {(gpu_size_t) i0, (gpu_size_t) (model->indices.size() - 1)},
        .geo_type = PrimitiveType::Triangles,
        .material_id = material_id,
        .obj_bounds = {range2::from_center({w / 2, w / 2}, {w, w}), h_range}
    });
    return model->prims.size() - 1;
}

template <typename Generator>
static AffineTransform<float,1> pick_interval(Generator& rng, float power) {
    std::uniform_real_distribution<float> uniform(0., 1.);
    float a = uniform(rng);
    float b = uniform(rng);
    if (power != 1.f) {
        a = std::pow(a, power);
        b = std::pow(b, power);
    }
    return geom::remap_transform(range::from_corners(a, b));
}

// maps a distribution inside the unit box [0,1]^3 to a new distribution
template <typename Generator>
static AffineTransform<float,3> pick_Lab_diffuse_distribution(Generator& rng) {
    std::uniform_real_distribution<float> uniform(0., 1.);
    std::normal_distribution<float> normal(0., 1.);
    SimpleMatrix<float,3,3> mx;
    for (index_t i = 0; i < 9; ++i) {
        mx.data_begin()[i] = normal(rng);
    }
    AffineTransform<float,3> xf = geom::scale(1.f, 0.125f, 0.125f) // |a,b| should be < 0.5
        * geom::transformation(mx);
    // vector from the un-transformed unit sphere that maps to the positive L-axis:
    vec3 L_post = xf * (vec3(1,0,0) / xf).unit();
    float L = std::abs(L_post.x);
    range L_target_range = range::from_corners(
        0.8 * std::sqrt(uniform(rng)),
        0.8 * std::sqrt(uniform(rng))
    );
    // put the result L range into [0,1]
    xf = geom::unmap_transform(range(-L, L)   * range2(0., 1.)) * xf;
    // put L in [0, 1] into the target range
    xf = geom::remap_transform(L_target_range * range2(0., 1.)) * xf;
    // center the distribution on the origin and return:
    return xf * geom::translation(vec3(-0.5));
}

template <typename Generator>
static vec3 pick_random_diffuse(Generator& rng) {
    float brighten = 0.5;
    return oklab_to_linear_srgb(
        vec3(1. - brighten, 0, 0) * (pick_Lab_diffuse_distribution(rng) * (
            // distribution into the (0,1) box
            0.125 * geom::random_gaussian<float,3>(rng) + vec3(0.5)
        )) + vec3(brighten, 0, 0)
    );
}

MaterialRef ExampleGen::create_material(gpu_size_t width, bool has_alpha) {
    // todo: pick a bias and gain for each texture.
    //   provide transform for the channel values, which takes a vector in
    //   (0,1) to the final space
    // todo: re-enable alpha
    std::normal_distribution<float> normal(0., 1.);
    return std::make_shared<Material>(Material {
        .diffuse   = create_texture<3>(width, ColorSpace::Oklab, pick_Lab_diffuse_distribution(rng)),
        .specular  = create_texture<1>(width, ColorSpace::Linear_sRGB, pick_interval(rng, 1.)),
        .emission  = black_tex,
        .roughness = create_texture<1>(width, ColorSpace::Linear_sRGB, pick_interval(rng, 2.)),
        .metallic  = black_tex,
        .alpha     = has_alpha ? create_texture<1>(width, ColorSpace::Linear_sRGB) : white_tex,
        .normal    = grey_tex,
        
        // xxx todo: more random params
        // xxx todo: turn alpha back on
        .coeffs = {
            .k_diff  = pick_random_diffuse(rng),
            .k_spec  = {vec3(0.5f) + 0.25 * normal(rng)},
            .k_emit  = vec3(0.),
            .k_rough = 1.f,
            .k_metal = 0.f,
            .alpha   = 1.f,
        },
    });
}


template <index_t N>
TextureRef ExampleGen::create_texture(
        gpu_size_t width,
        ColorSpace src_color_space,
        const AffineTransform<float,N>& color_xf)
{
    wgpu::Device device = mip_gen->get_device();
    uint64_t ww = width;
    Grid2d<VecType<float,N>> grid({width, width});
    TextureRef tex = std::make_shared<Texture>(Texture {
        device,
        {width, width},
        wgpu::TextureFormat::RGBA8UnormSrgb,
        "generated texture",
        // extra flags for mip generation
        wgpu::TextureUsage::CopySrc |
        wgpu::TextureUsage::CopyDst |
        wgpu::TextureUsage::TextureBinding,
    });
    
    // populate the grid
    random_heightfield<N>(grid);
    
    // compute the range of values in the grid
    range value_range = range::empty;
    for (size_t i = 0; i < width * width; ++i) {
        for (size_t c = 0; c < N; ++c) {
            value_range |= coord(grid.buf[i], c);
        }
    }
    
    // convert and copy the data to the texture buffer
    std::vector<uint8_t> data (width * width * 4);
    for (size_t y = 0; y < grid.dims.y; ++y) {
        for (size_t x = 0; x < grid.dims.x; ++x) {
            VecType<float,N> v = grid(x, y);
            // normalize contrast to [0,1]^N, then apply the color transform
            v = color_xf * value_range.unmap(v);
            
            // up-convert to vec4
            vec4 write_val;
            for (size_t c = 0; c < 4; ++c) {
                size_t c_i = std::min<size_t>(c, N - 1);
                write_val[c] = coord(v, c_i);
                if constexpr (N == 3) {
                    // if we only generate 3 channels, alpha gets 1.0
                    if (c == 3) write_val[c] = 1.;
                }
            }
            
            // convert color to srgb
            switch (src_color_space) {
                case ColorSpace::Oklab: {
                    range  l_range = { 0.66, 1.00};
                    range ab_range = {-0.10, 0.10};
                    write_val = {
                        oklab_to_linear_srgb(write_val.template resized<3>()),
                        write_val.a
                    };
                } [[fallthrough]];
                case ColorSpace::Linear_sRGB: {
                    write_val = {
                        linear_to_gamma_encode(write_val.template resized<3>(), 2.2),
                        write_val.a 
                    };
                } break;
                case ColorSpace::Gamma_sRGB: {
                    // do nothing
                } break;
            }
            
            // write to data
            write_val = range4({0.}, {1.}).clip(write_val);
            size_t i = 4 * (y * width + x);
            data[i + 0] = static_cast<uint8_t>(write_val.r * 255);
            data[i + 1] = static_cast<uint8_t>(write_val.g * 255);
            data[i + 2] = static_cast<uint8_t>(write_val.b * 255);
            data[i + 3] = static_cast<uint8_t>(write_val.a * 255);
        }
    }
    tex->submit_write(data.data());
    generate_mips(mip_gen, *tex);
    return tex;
}


template <index_t N>
void ExampleGen::random_heightfield(Grid2d<VecType<float,N>>& grid) {
    std::uniform_int_distribution<int> octave_rng(
        1,
        std::max(1, std::bit_width(std::max(grid.dims.x, grid.dims.y)))
    );
    std::normal_distribution<double> gauss(0, 1);
    
    grid.fill(VecType<float,N>{0.f});
    
    size_t octaves = octave_rng(rng);
    for (size_t i = 0; i < octaves; ++i) {
        // random weight for this octave
        double gain = gauss(rng);
        // random translation for this octave (different offset per channel)
        vec2d tx[N];
        for (size_t n = 0; n < N; ++n) {
            tx[n] = {250 * gauss(rng), 250 * gauss(rng)};
        }
        // random 2D transform for this octave
        mat2d xf {
            {gauss(rng), gauss(rng)},
            {gauss(rng), gauss(rng)},
        };
        double s = 1 << i;
        for (size_t y = 0; y < grid.dims.y; ++y) {
            for (size_t x = 0; x < grid.dims.x; ++x) {
                double xx = x / (double) grid.dims.x;
                double yy = y / (double) grid.dims.y;
                vec2d   p = s * xf * vec2d(xx, yy);
                VecType<float,N> v;
                for (size_t n = 0; n < N; ++n) {
                    coord(v,n) = 0.5 * gain * noise(p + tx[n]) / s;
                }
                grid(x, y) += v;
            }
        }
    }
}

} // namespace stereo
