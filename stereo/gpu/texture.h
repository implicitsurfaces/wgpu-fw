#pragma once

#include <stereo/defs.h>

namespace stereo {

struct Texture {
private:    
    wgpu::Device  _device  = nullptr;
    wgpu::Texture _texture = nullptr;
    range1i       _mip_range;
    std::vector<wgpu::TextureView> _mip_views;
    wgpu::TextureView _full_view;

public:
    Texture(wgpu::Texture texture, wgpu::Device device, range1i mip_range=range1i::full);
    Texture(
        wgpu::Device device,
        vec2ui size,
        wgpu::TextureFormat format,
        const char* label="texture"
    );
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
    gpu_size_t        width();
    gpu_size_t        height();
    
    void submit_write(uint8_t* data, gpu_size_t bytes_per_channel=4);

private:
    
    void _init();
    void _release();
    
};

}  // namespace stereo
