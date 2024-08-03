#pragma once

#include <stereo/defs.h>

namespace stereo {

// mip views have three options:
// - all levels in one view
// - a specific mip level
// - views for a range of mip levels

/// Should the mip levels be stored as one view, or multiple views?
enum struct MipViews {
    Single,
    Multiple
};

/// All possible mip levels should be represented
enum struct MipLevels {
    All
};

/// What range of mip levels should be represented?
using MipRange = std::variant<MipLevels, int32_t, range1i>;

/// How should mip levels be covered?
struct MipSetting {
    MipViews view_type = MipViews::Single;
    MipRange range     = MipLevels::All;
};

struct Texture {
private:    
    wgpu::Device  _device  = nullptr;
    wgpu::Texture _texture = nullptr;
    range1i       _mip_range;
    std::vector<wgpu::TextureView> _mip_views;
    wgpu::TextureView _full_view;

public:
    Texture(wgpu::Texture texture, wgpu::Device device, range1i mip_range=range1i::full);
    Texture();
    Texture(const Texture&);
    Texture(Texture&&);
    ~Texture();
    
    Texture& operator=(const Texture&);
    Texture& operator=(Texture&&);
    
    int32_t num_mip_levels();
    range1i mip_range();
    Texture clone(
        std::string_view label,
        std::optional<wgpu::TextureUsageFlags> usage=std::nullopt,
        std::optional<range1i> mip_range=std::nullopt
    );
    
    wgpu::TextureView view_for_mip(int32_t level);
    wgpu::TextureView view();
    wgpu::Texture     texture() const;
    wgpu::Device      device() const;

private:
    
    void _init();
    void _release();
    
};

}  // namespace stereo
