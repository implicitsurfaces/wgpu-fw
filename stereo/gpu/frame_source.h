#pragma once

#include <opencv2/opencv.hpp>

#include <stereo/gpu/mip_generator.h>
#include <stereo/gpu/filter.h>

namespace stereo {

using CaptureRef = std::shared_ptr<cv::VideoCapture>;

inline vec2ui capture_res(CaptureRef capture) {
    unsigned int w = capture->get(cv::CAP_PROP_FRAME_WIDTH);
    unsigned int h = capture->get(cv::CAP_PROP_FRAME_HEIGHT);
    return vec2ui{w, h};
}

struct FrameSource {
    CaptureRef      source;
    vec2ui          res;
    wgpu::Texture   src_texture = nullptr;
    MipTexture      mip;
    FilteredTexture filtered;

private:
    
    std::vector<uint8_t> _capture_buffer;
    
public:
    
    FrameSource();
    FrameSource(const FrameSource& other);
    FrameSource(FrameSource&& other);
    
    FrameSource(
        CaptureRef source,
        MipGenerator& mip_gen,
        Filter3x3& filter,
        std::vector<uint8_t>& capture_buffer
    );
    
    ~FrameSource();
    
    FrameSource& operator=(const FrameSource& other);
    FrameSource& operator=(FrameSource&& other);
    
    bool capture();
    
private:
    
    wgpu::Texture _create_texture(wgpu::Device device);
};

} // namespace stereo
