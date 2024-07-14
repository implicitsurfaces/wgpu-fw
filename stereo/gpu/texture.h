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
    
    wgpu::Device  device  = nullptr;
    wgpu::Texture texture = nullptr;
    MipSetting    mip_setting;
    std::vector<wgpu::TextureView> views;
    
    Texture(wgpu::Texture texture, wgpu::Device device, MipSetting mip_level = {});
    Texture();
    Texture(const Texture&);
    Texture(Texture&&);
    ~Texture();
    
    Texture& operator=(const Texture&);
    Texture& operator=(Texture&&);
    
    int32_t num_mip_levels();
    range1i mip_range();
    
    wgpu::TextureView view_for_mip(int32_t level);
    wgpu::TextureView view_for_mip(MipLevels level);

private:
    
    void _init();
    void _release();
    
};

}  // namespace stereo
