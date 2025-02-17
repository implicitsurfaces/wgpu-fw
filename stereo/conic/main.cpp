#include "webgpu/webgpu.hpp"
#include "webgpu/wgpu.h"

#include <pcg_random.hpp>

#include <geomc/shape/Transformed.h>
#include <geomc/shape/Intersect.h>
#include <geomc/function/Utils.h>
#include <geomc/random/SampleVector.h>

#include <numbers>
#include <stereo/gpu/window.h>
#include <stereo/gpu/simple_render.h>
#include <stereo/util/load_model.h>
#include <stereo/conic/conic_vis.h>
#include <vector>

using namespace stereo;

bool g_app_error = false;

void _uncaptured_error(WGPUErrorType type, char const* message, void* p_userdata) {
    std::cerr << "Device error: type " << type << std::endl;
    if (message) {
        std::cerr << "  " << message << std::endl;
    }
    g_app_error = true;
}

template <typename T, typename G>
static SimpleMatrix<T,3,3> _random_matrix(G& rng) {
    return {
        geom::random_gaussian<T,3>(rng),
        geom::random_gaussian<T,3>(rng),
        geom::random_gaussian<T,3>(rng),
    };
}

static wgpu::Device _create_device(wgpu::Instance instance) {
    wgpu::DeviceDescriptor dd;
    wgpu::Adapter adapter = instance.requestAdapter(wgpu::Default);

    // not needed for now...?
    // wgpu::SupportedLimits supported_limits;
    // adapter.getLimits(&supported_limits);

    // increase any limits if needed
    wgpu::RequiredLimits limits = wgpu::Default;
    // limits.limits.maxBindGroups = 6;

    wgpu::UncapturedErrorCallbackInfo err_cb;
    err_cb.callback = _uncaptured_error;
    err_cb.userdata = (void*) nullptr;
    err_cb.nextInChain = nullptr;

    // request extra features
    // capture video is BGRA; not enabled by default; request it
    wgpu::FeatureName features[] = {
        wgpu::FeatureName::BGRA8UnormStorage,
        wgpu::FeatureName::ShaderF16,
    };
    dd.requiredFeatureCount = 2;
    dd.requiredFeatures = reinterpret_cast<const WGPUFeatureName*>(features);
    dd.label = "Conic visualizer GPU Device";
    dd.requiredLimits = &limits;
    dd.uncapturedErrorCallbackInfo = err_cb;
    wgpu::Device device = adapter.requestDevice(dd);
    return device;
}

static wgpu::TextureView _get_depth_view(Texture& tex) {
    wgpu::TextureViewDescriptor depthTextureViewDesc;
    depthTextureViewDesc.aspect = wgpu::TextureAspect::DepthOnly;
    depthTextureViewDesc.baseArrayLayer  = 0;
    depthTextureViewDesc.arrayLayerCount = 1;
    depthTextureViewDesc.baseMipLevel    = 0;
    depthTextureViewDesc.mipLevelCount   = 1;
    depthTextureViewDesc.dimension = wgpu::TextureViewDimension::_2D;
    depthTextureViewDesc.format = wgpu::TextureFormat::Depth24Plus;
    return tex.texture().createView(depthTextureViewDesc);
}

bool g_should_regen = false;

static ModelRef _add_random_conic(SimpleRender& renderer) {
    static pcg64 rng {
        std::seed_seq{
            0x1f8f8d2142ebd391, 0x173d2e8cd314f04b,
            0x0f3e37ecb7cae79a, 0x2be2af423c846b4d
        }
    };
    constexpr double tau = 2. * std::numbers::pi;
    
    // create models
    ConicVisualizer vis;
    Conic<float> c_a;
    c_a = -1 * _random_matrix<float>(rng) * c_a;
    ModelRef section_model = std::make_shared<Model>();
    ModelRef cone_model    = std::make_shared<Model>();
    range angle_range = {0, 0.875 *  tau};
    
    // add the base cone
    vis.add_conic_section(
        section_model,
        c_a,
        128,    // segments
        0.02f , // line thickness
        angle_range
    );
    vis.build_cone(
        cone_model,
        c_a,
        128,
        {0, 3.}, // height range
        angle_range
    );
    vis.build_cone(
        cone_model,
        c_a,
        128,
        {-3., 0.}, // height range
        angle_range
    );
    
    // add a section
    Conic<float> c_b = c_a.slice({0.1, -0.5, 1});
    size_t slice_id = vis.add_conic_section(
        section_model,
        c_b,
        128,
        0.02f,
        angle_range
    );
    
    mat3 ca_to_cb = c_a.M_inv * c_b.M_fwd;
    section_model->prims[slice_id].obj_to_world = geom::transformation(ca_to_cb);
    
    ModelId id = renderer.add_model(section_model);
    renderer.add_model(cone_model);
    renderer.set_model_overlay(
        id,
        { .tint = vec4(1, .125, .125, 1) }
    );
    return section_model;
}

int main(int argc, char** argv) {
    glfwInit();
    wgpu::Instance instance = wgpu::createInstance(wgpu::Default);
    wgpu::Device device = _create_device(instance);
    SimpleRender renderer {device};
    constexpr double tau = 2. * std::numbers::pi;
    
    ModelRef model   = _add_random_conic(renderer);    
    range3 model_box = model->compute_coarse_bounds();
    double r         = model_box.dimensions().max();
    vec3 center      = model_box.center();
    
    float res = 1.5;
    Window window {
        instance,
        device, 
        vec2ui {(uint32_t) (res * 1920), (uint32_t) (res * 1080)}
    };
    
    Camera<double> cam;
    cam.set_perspective(75, 1920 / 1080., r / 100., r * 20.);
    
    // set up key response
    auto key_resp = [](GLFWwindow* window, int key, int scancode, int action, int mods) {

        if (key == GLFW_KEY_SPACE and action == GLFW_PRESS) {
            // toggle run state
            g_should_regen = true;
        } else if ((key == GLFW_KEY_Q or key == GLFW_KEY_ESCAPE) and action == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    };

    glfwSetKeyCallback(window.window, key_resp);
    
    vec3d up = {0, 0, 1};
    double theta = 0.;
    while (not glfwWindowShouldClose(window.window) and not g_app_error) {
        glfwPollEvents();
        
        if (g_should_regen) {
            g_should_regen = false;
            renderer.clear_models();
            model = _add_random_conic(renderer);
        }
        cam.orbit(
            (vec3d) model_box.center(),
            r * 1.5,
            geom::radians(22.5),
            theta,
            up
        );
        
        float exposure = 0.;
        wgpu::TextureView backbuffer = window.next_target();
        renderer.render(cam, exposure, backbuffer, window.depth_view);
        window.surface.present();
        
        // allow for async events to be processed
        WGPUWrappedSubmissionIndex idx;
        wgpuDevicePoll(device, false, &idx);
        theta += 2. * std::numbers::pi / 360.;
    }
}
