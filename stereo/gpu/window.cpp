#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif // __EMSCRIPTEN__

#include <stereo/gpu/window.h>
#include <webgpu/glfw3webgpu.h>


namespace stereo {

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

void Window::_rebuild_depth_buffer() {
    depth_buffer = Texture {
        device,
        surface_dims,
        wgpu::TextureFormat::Depth24Plus,
        "depth buffer",
        wgpu::TextureUsage::RenderAttachment,
        1 // only base mip level
    },
    depth_view = _get_depth_view(depth_buffer);
}

Window::Window(wgpu::Instance instance, wgpu::Device device, vec2ui dims):
    instance(instance),
    device(device),
    queue(device.getQueue()),
    window_dims(dims / 2),
    surface_dims(dims),
    input_manager(this)
{
    instance.reference();
    device.reference();
    
    int w = window_dims.x;
    int h = window_dims.y;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // todo: allow resize
    window  = glfwCreateWindow(window_dims.x, window_dims.y, "WGPU App", nullptr, nullptr);
    surface = glfwGetWGPUSurface(instance, window);
    
    configure_surface(dims);
    
    glfwSetWindowUserPointer(window, this);
}

Window::~Window() {
    if (window) glfwSetWindowUserPointer(window, nullptr);
    if (depth_view) depth_view.release();
    if (surface)  {
        surface.unconfigure();
        surface.release();
    }
    if (queue)    queue.release();
    if (device)   device.release();
    if (instance) instance.release();
    if (window)   glfwDestroyWindow(window);
}

void Window::configure_surface(vec2ui dims) {
    wgpu::RequestAdapterOptions opts;
    opts.compatibleSurface = surface;
    wgpu::Adapter adapter = instance.requestAdapter(opts);
    wgpu::SurfaceCapabilities caps;
    surface.getCapabilities(adapter, &caps);
    
    wgpu::SurfaceConfiguration config;
    config.width   = surface_dims.x;
    config.height  = surface_dims.y;
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
    
    _rebuild_depth_buffer();
}

vec2d Window::convert_window_coordinates(
            vec2d coords,
            CoordSpace from_coord,
            CoordSpace to_coord) const
{
    // lo: bottom left | hi: top right
    range2d r[2];
    for (int i = 0; i < 2; ++i) {
        switch (i == 0 ? from_coord : to_coord) {
            case CoordSpace::WINDOW_COORDS:
                r[i] = range2d(
                    // bottom of the frame is +y
                    {0, (double) window_dims.y}, 
                    {(double) window_dims.x, 0});
                break;
            case CoordSpace::BUFFER_COORDS:
                r[i] = range2d(
                    // top of the frame is +y
                    {0, 0},
                    {(double) surface_dims.x, (double) surface_dims.y});
                break;
            case CoordSpace::ASPECT_COORDS: {
                double h = window_dims.y / (double) window_dims.x;
                r[i] = range2d({-1, -h / 2}, {1, h / 2});
                break;
            }
            case CoordSpace::NDC_COORDS:
                r[i] = range2d({-1, -1}, {1, 1});
                break;
        }
    }
    return r[1].remap(r[0].unmap(coords));
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
