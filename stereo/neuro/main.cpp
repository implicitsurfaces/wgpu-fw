#include "webgpu/webgpu.hpp"
#include "webgpu/wgpu.h"

#include <geomc/shape/Transformed.h>
#include <geomc/shape/Intersect.h>
#include <geomc/function/Utils.h>

#include <numbers>
#include <stereo/gpu/window.h>
#include <stereo/gpu/simple_render.h>
#include <stereo/util/load_model.h>
#include <stereo/neuro/datagen.h>

using namespace stereo;

bool g_app_error = false;

void _uncaptured_error(WGPUErrorType type, char const* message, void* p_userdata) {
    std::cerr << "Device error: type " << type << std::endl;
    if (message) {
        std::cerr << "  " << message << std::endl;
    }
    g_app_error = true;
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
    dd.label = "Training sample GPU Device";
    dd.requiredLimits = &limits;
    dd.uncapturedErrorCallbackInfo = err_cb;
    wgpu::Device device = adapter.requestDevice(dd);
    return device;
}

static ModelRef _add_bnuuy(SimpleRender* renderer) {
    ModelRef bnuuy = std::make_shared<Model>();
    range1i prim_range = add_model(*bnuuy, "resource/obj/bnuuy.obj");
    if (prim_range.is_empty()) {
        std::cerr << "Failed to load model" << std::endl;
        return nullptr;
    }
    renderer->add_model(bnuuy);
    return bnuuy;
}

bool g_should_regen = false;

int main(int argc, char** argv) {
    glfwInit();
    wgpu::Instance instance = wgpu::createInstance(wgpu::Default);
    wgpu::Device device = _create_device(instance);
    SimpleRender renderer{device};
    
    ExampleGen gen {
        &renderer,
        std::seed_seq {
            0x8fe23474c3601a68ULL,
            0x0cbeacca45d9e5f8ULL,
            0x6ddaeb4ef19a7a52ULL,
            0x2039d4ea797e2103ULL,
        }
    };
    
    ModelRef model = gen.add_scene(&renderer);
    range3 model_box = model->compute_vertex_bounds();
    double r = model_box.dimensions().max();
    vec3 center = model_box.center();
    
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
    
    // being rendering
    vec3d up = {0, 0, 1};
    double theta = 0.;
    while (not glfwWindowShouldClose(window.window) and not g_app_error) {
        glfwPollEvents();
        
        if (g_should_regen) {
            g_should_regen = false;
            renderer.clear_models();
            gen.add_scene(&renderer);
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
