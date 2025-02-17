#include "GLFW/glfw3.h"
#include "webgpu/wgpu.h"
#include <thread>

#include <geomc/shape/Sphere.h>

#include <stereo/gpu/window.h>
#include <stereo/sdf/visualize_sdf.h>

using namespace stereo;

using dualf = Dual<float>;
using sphere3e = Sphere<dualf,3>;

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
    wgpu::RequestAdapterOptions options;
    options.compatibleSurface = nullptr; // only compute for now.
    wgpu::Adapter adapter = instance.requestAdapter(options);

    // not needed for now...?
    // wgpu::SupportedLimits supported_limits;
    // adapter.getLimits(&supported_limits);

    // increase any limits if needed
    wgpu::RequiredLimits limits = wgpu::Default;
    limits.limits.maxBindGroups = 6;

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
    dd.label = "SDF GPU Compute Device";
    dd.requiredLimits = &limits;
    dd.uncapturedErrorCallbackInfo = err_cb;
    wgpu::Device device = adapter.requestDevice(dd);
    return device;
}

auto g_finish = std::chrono::high_resolution_clock::now();
bool g_done = false;

void run_compute(wgpu::Device device) {
    // sdf context
    gpu_size_t w = 1024;
    gpu_size_t n_samples = w * w;
    SdfEvaluator sdf_eval {device};
    SdfNodeRef<float> sdf = std::make_shared<SdfUnion<float>>(
        std::make_shared<SdfSphere<float>>(sphere3({-1., 0., 0.}, 1.)),
        std::make_shared<SdfSphere<float>>(sphere3({ 1., 0., 0.}, 1.))
    );
    SdfGpuExpr expr {sdf_eval, sdf};
    SdfInput input {sdf_eval, n_samples, n_samples};
    SdfOutputRef output;
    
    // populate input (pixel center grid on [-2, 2]^2)
    std::vector<vec3gpu> sample_pts {n_samples};
    for (gpu_size_t y = 0; y < w; ++y) {
        float t = (y + 0.5) / w * 2.f - 1.f;
        for (gpu_size_t x = 0; x < w; ++x) {
            float s = (x + 0.5) / w * 2.f - 1.f;
            sample_pts[y * w + x] = vec3gpu {2 * s, 2 * t, 0};
        }
    }
    input.write_samples_x(sample_pts.data());
    
    int c = 0;
    auto start = std::chrono::high_resolution_clock::now();
    while (not g_app_error and c++ < 100) {
        glfwPollEvents();
        
        if (c == 100) {
            std::cout << "last cycle\n";
            wgpuQueueOnSubmittedWorkDone(
                device.getQueue(),
                [](WGPUQueueWorkDoneStatus status, WGPU_NULLABLE void* userdata) {
                    g_finish = std::chrono::high_resolution_clock::now();
                    g_done = true;
                    std::cout << "Work done: " << status << std::endl;
                },
                nullptr
            );
        }
        output = sdf_eval.evaluate(expr, input, 1, ParamVariation::VaryDerivative, output);
        
        WGPUWrappedSubmissionIndex idx;
        wgpuDevicePoll(device, false, &idx);
    }
    // auto end = std::chrono::high_resolution_clock::now();
    while (not g_done) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(g_finish - start).count();
    std::cout << "Time: " << dt << "ms" << std::endl;
}

void run_visualizer(wgpu::Device device, wgpu::Instance instance) {
    SdfNodeRef<dualf> sdf = std::make_shared<SdfUnion<dualf>>(
        std::make_shared<SdfSphere<dualf>>(sphere3e({-1., 0., 0.}, 1.)),
        std::make_shared<SdfSphere<dualf>>(sphere3e({ 1., 0., 0.}, 1.))
    );
    glfwInit();
    VisualizeSdf sdf_vis {instance, device, sdf};
    auto last_frame_end = std::chrono::high_resolution_clock::now();
    while (not glfwWindowShouldClose(sdf_vis.window()->window) and not g_app_error) {
        glfwPollEvents();
        
        sdf_vis.do_frame();
        
        // allow for async events to be processed
        WGPUWrappedSubmissionIndex idx;
        wgpuDevicePoll(device, false, &idx);
        
        // track framerate
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - last_frame_end).count();
        std::cout << "fps: " << 1000. / elapsed_ms << "\r" << std::flush;
        last_frame_end = now;
    }
    std::cout << (g_app_error ? "aborted" : "finished") << std::endl;
}

int main(int argc, char** argv) {
    wgpu::Instance instance = wgpu::createInstance(wgpu::InstanceDescriptor{});
    if (not instance) {
        std::cerr << "Could not acquire a WebGPU instance." << std::endl;
        std::abort();
    }
    wgpu::Device device = _create_device(instance);
    run_visualizer(device, instance);
    
}
