#include <stereo/gpu/texture.h>

namespace stereo {

Texture::Texture() {};

Texture::Texture(wgpu::Texture texture, wgpu::Device device, range1i mip_range):
    _device(device),
    _texture(texture),
    _mip_range(mip_range)
{
    device.reference();
    texture.reference();
    _init();
}

Texture::Texture(const Texture& other) :
    _device(other._device),
    _texture(other._texture),
    _mip_range(other._mip_range),
    _mip_views(other._mip_views),
    _full_view(other._full_view)
{
    _device.reference();
    _texture.reference();
    _full_view.reference();
    for (auto view : _mip_views) {
        view.reference();
    }
}

Texture::Texture(Texture&& other) :
    _device(other._device),
    _texture(other._texture),
    _mip_range(other._mip_range),
    _mip_views(std::move(other._mip_views)),
    _full_view(other._full_view)
{
    other._device    = nullptr;
    other._texture   = nullptr;
    other._full_view = nullptr;
}

Texture::~Texture() {
    _release();
}

Texture& Texture::operator=(Texture&& other) {
    std::swap(_device, other._device);
    std::swap(_texture, other._texture);
    std::swap(_mip_range, other._mip_range);
    std::swap(_mip_views, other._mip_views);
    std::swap(_full_view, other._full_view);
    return *this;
}

Texture& Texture::operator=(const Texture& other) {
    _release();
    _device    = other._device;
    _texture   = other._texture;
    _mip_views = other._mip_views;
    _mip_range = other._mip_range;
    _full_view = other._full_view;
    _device.reference();
    _texture.reference();
    _full_view.reference();
    for (auto view : _mip_views) {
        view.reference();
    }
    return *this;
}

range1i Texture::mip_range() {
    return range1i(0, _texture.getMipLevelCount() - 1) & _mip_range;
}

int32_t Texture::num_mip_levels() {
    return mip_range().dimensions();
}

void Texture::_init() {
    // create the neccesary views of the texture
    range1i view_range = mip_range();
    int32_t n_levels = view_range.dimensions();
    wgpu::TextureViewDescriptor view_descriptor;
    view_descriptor.aspect          = wgpu::TextureAspect::All;
    view_descriptor.baseArrayLayer  = 0;
    view_descriptor.arrayLayerCount = 1;
    view_descriptor.dimension       = wgpu::TextureViewDimension::_2D;
    view_descriptor.format          = _texture.getFormat();
    view_descriptor.mipLevelCount   = 1;
    
    _mip_views.reserve(n_levels);
    for (size_t level = view_range.lo; level <= view_range.hi; ++level) {
        std::string label = "frame source view (mip = " + std::to_string(level) + ")";
        view_descriptor.label = label.c_str();
        view_descriptor.baseMipLevel = level;
        _mip_views.push_back(_texture.createView(view_descriptor));
    }
    
    view_descriptor.baseMipLevel  = view_range.lo;
    view_descriptor.mipLevelCount = n_levels;
    view_descriptor.label = "texture view (all mip levels)";
    _full_view = _texture.createView(view_descriptor);
}

void Texture::_release() {
    for (auto view : _mip_views) {
        view.release();
    }
    _mip_views.clear();
    if (_texture)   _texture.release();
    if (_device)    _device.release();
    if (_full_view) _full_view.release();
}

Texture Texture::clone(
        std::string_view label,
        std::optional<wgpu::TextureUsageFlags> usage,
        std::optional<range1i> mip_range) {
    wgpu::TextureDescriptor desc = {};
    desc.size.width    = _texture.getWidth();
    desc.size.height   = _texture.getHeight();
    desc.size.depthOrArrayLayers = _texture.getDepthOrArrayLayers();
    desc.mipLevelCount = _texture.getMipLevelCount();
    desc.sampleCount   = _texture.getSampleCount();
    desc.dimension     = _texture.getDimension();
    desc.format        = _texture.getFormat();
    desc.usage         = usage.value_or(_texture.getUsage());
    desc.label         = label.data();
    return {
        _device.createTexture(desc),
        _device,
        mip_range.value_or(_mip_range)
    };
}

wgpu::TextureView Texture::view_for_mip(int32_t level) {
    range1i range = mip_range();
    int32_t i = range.clip(level);
    return _mip_views[i - range.lo];
}

wgpu::TextureView Texture::view() {
    return _full_view;
}

wgpu::Texture Texture::texture() const {
    return _texture;
}

wgpu::Device Texture::device() const {
    return _device;
}

}  // namespace stereo
