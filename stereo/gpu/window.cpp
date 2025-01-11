#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif // __EMSCRIPTEN__

#include <stereo/gpu/window.h>
#include <webgpu/glfw3webgpu.h>


namespace stereo {

Window::Window(wgpu::Instance instance, wgpu::Device device, vec2u dims):
    instance(instance),
    device(device),
    queue(device.getQueue()),
    dims(dims)
{
    instance.reference();
    device.reference();
    init();
}

Window::~Window() {
    release();
    if (instance) instance.release();
    if (device)   device.release();
    if (queue)    queue.release();
    if (surface)  surface.release();
    if (window)   glfwDestroyWindow(window);
}

void Window::init() {
    int w = dims.x;
    int h = dims.y;
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // todo: allow resize
    window  = glfwCreateWindow(dims.x / 2, dims.y / 2, "WGPU App", nullptr, nullptr);
    surface = glfwGetWGPUSurface(instance, window);
    wgpu::RequestAdapterOptions opts;
    opts.compatibleSurface = surface;
    wgpu::Adapter adapter = instance.requestAdapter(opts);
    wgpu::SurfaceConfiguration config;
    wgpu::SurfaceCapabilities caps;
    surface.getCapabilities(adapter, &caps);

    config.width   = w;
    config.height  = h;
    config.usage   = wgpu::TextureUsage::RenderAttachment;
    surface_format = caps.formats[0];
    config.format  = surface_format;

    config.viewFormatCount = 0;
    config.viewFormats     = nullptr;
    config.device          = device;
    config.presentMode     = wgpu::PresentMode::Fifo;
    config.alphaMode       = wgpu::CompositeAlphaMode::Auto;

    surface.configure(config);
    adapter.release();
}

void Window::release() {
    if (surface) surface.unconfigure();
}


wgpu::TextureView Window::next_target() {
    // Get the surface texture
    wgpu::SurfaceTexture surface_tex;
    surface.getCurrentTexture(&surface_tex);
    if (surface_tex.status != wgpu::SurfaceGetCurrentTextureStatus::Success) {
        return nullptr;
    }
    wgpu::Texture texture = surface_tex.texture;

    // Create a view for this surface texture
    wgpu::TextureViewDescriptor view_desc;
    view_desc.label           = "Surface texture view";
    view_desc.format          = texture.getFormat();
    view_desc.dimension       = wgpu::TextureViewDimension::_2D;
    view_desc.baseMipLevel    = 0;
    view_desc.mipLevelCount   = 1;
    view_desc.baseArrayLayer  = 0;
    view_desc.arrayLayerCount = 1;
    view_desc.aspect = wgpu::TextureAspect::All;
    return texture.createView(view_desc);
}

}  // namespace stereo
