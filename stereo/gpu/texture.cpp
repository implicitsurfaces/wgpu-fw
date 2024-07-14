#include <stereo/gpu/texture.h>

namespace stereo {

Texture::Texture() {};

Texture::Texture(wgpu::Texture texture, wgpu::Device device, MipSetting mip) :
    device(device),
    texture(texture),
    mip_setting(mip)
{
    device.reference();
    texture.reference();
    _init();
}

Texture::Texture(const Texture& other) :
    device(other.device),
    texture(other.texture),
    mip_setting(other.mip_setting),
    views(other.views)
{
    device.reference();
    texture.reference();
    for (auto view : views) {
        view.reference();
    }
}

Texture::Texture(Texture&& other) :
    device(other.device),
    texture(other.texture),
    mip_setting(other.mip_setting),
    views(std::move(other.views))
{
    other.device  = nullptr;
    other.texture = nullptr;
}

Texture::~Texture() {
    _release();
}

Texture& Texture::operator=(Texture&& other) {
    std::swap(device, other.device);
    std::swap(texture, other.texture);
    std::swap(mip_setting, other.mip_setting);
    std::swap(views, other.views);
    return *this;
}

Texture& Texture::operator=(const Texture& other) {
    _release();
    device    = other.device;
    texture   = other.texture;
    mip_setting = other.mip_setting;
    views     = other.views;
    device.reference();
    texture.reference();
    for (auto view : views) {
        view.reference();
    }
    return *this;
}

range1i Texture::mip_range() {
    if (std::holds_alternative<MipLevels>(mip_setting.range)) {
        return range1i(0, texture.getMipLevelCount() - 1);
    } else if (std::holds_alternative<int32_t>(mip_setting.range)) {
        int32_t mip = std::get<int32_t>(mip_setting.range);
        return range1i(mip, mip);
    } else if (std::holds_alternative<range1i>(mip_setting.range)) {
        range1i r = std::get<range1i>(mip_setting.range);
        return r & range1i(0, texture.getMipLevelCount() - 1);
    }
    return range1i::empty;
}

int32_t Texture::num_mip_levels() {
    if (std::holds_alternative<MipLevels>(mip_setting.range)) {
        return texture.getMipLevelCount();
    } else if (std::holds_alternative<int32_t>(mip_setting.range)) {
        return std::get<int32_t>(mip_setting.range);
    } else if (std::holds_alternative<range1i>(mip_setting.range)) {
        return std::max(mip_range().dimensions(), 0);
    }
    return 0;
}

void Texture::_init() {
    bool all_mips = std::holds_alternative<MipLevels>(mip_setting.range)
        and std::get<MipLevels>(mip_setting.range) == MipLevels::All;
    range1i range = mip_range();
    bool multi_view = mip_setting.view_type == MipViews::Multiple;
    
    // create the neccesary views of the texture
    int32_t mip_levels = std::max(range.dimensions(), 0);
    wgpu::TextureViewDescriptor view_descriptor;
    view_descriptor.aspect          = wgpu::TextureAspect::All;
    view_descriptor.baseArrayLayer  = range.lo;
    view_descriptor.arrayLayerCount = 1;
    view_descriptor.dimension       = wgpu::TextureViewDimension::_2D;
    view_descriptor.format          = texture.getFormat();
    view_descriptor.mipLevelCount   = multi_view ? 1 : mip_levels;
    
    views.reserve(mip_levels);
    range1i view_range = multi_view ? range : range1i(0, 0);
    for (size_t level = view_range.lo; level <= view_range.hi; ++level) {
        std::string mip_label = all_mips ? std::to_string(level) : "[all]";
        std::string label = "frame source view (mip = " + mip_label + ")";
        view_descriptor.label = label.c_str();
        view_descriptor.baseMipLevel = level;
        views.push_back(texture.createView(view_descriptor));
    }
}

void Texture::_release() {
    for (auto view : views) {
        view.release();
    }
    views.clear();
    if (texture) texture.release();
    if (device)  device.release();
}

wgpu::TextureView Texture::view_for_mip(int32_t level) {
    if (std::holds_alternative<range1i>(mip_setting.range)) {
        range1i mip_range = std::get<range1i>(mip_setting.range);
        if (mip_range.contains(level)) {
            return views[level - mip_range.lo];
        }
    } else if (level == 0 or mip_setting.view_type == MipViews::Multiple) {
        return views[level];
    }
    return nullptr;
}

wgpu::TextureView Texture::view_for_mip(MipLevels level) {
    if (mip_setting.view_type == MipViews::Multiple) {
        return views[0];
    }
    return nullptr;
}

}  // namespace stereo
