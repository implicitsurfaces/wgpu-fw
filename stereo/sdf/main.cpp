#include "webgpu/wgpu.h"

#include <geomc/shape/Sphere.h>

#include <stereo/gpu/window.h>
#include <stereo/sdf/sdf_eval.h>

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

int main(int argc, char** argv) {
    wgpu::Instance instance = wgpu::createInstance(wgpu::InstanceDescriptor{});
    if (not instance) {
        std::cerr << "Could not acquire a WebGPU instance." << std::endl;
        std::abort();
    }
    wgpu::Device device = _create_device(instance);
    
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
        
        output = sdf_eval.evaluate(expr, input, 1, ParamVariation::VaryDerivative, output);
        
        WGPUWrappedSubmissionIndex idx;
        wgpuDevicePoll(device, false, &idx);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Time: " << dt << "ms" << std::endl;
}
