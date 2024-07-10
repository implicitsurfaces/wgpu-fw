#pragma once

#include <opencv2/opencv.hpp>
#include <stereo/defs.h>
#include <stereo/gpu/mip_generator.h>

namespace stereo {

using CaptureRef = std::shared_ptr<cv::VideoCapture>;

inline vec2ui capture_res(CaptureRef capture) {
    unsigned int w = capture->get(cv::CAP_PROP_FRAME_WIDTH);
    unsigned int h = capture->get(cv::CAP_PROP_FRAME_HEIGHT);
    return vec2ui{w, h};
}

struct FrameSource {
    CaptureRef    source;
    vec2ui        res;
    wgpu::Texture src_texture = nullptr;
    MipTexture    mip;

private:
    
    std::vector<uint8_t> _capture_buffer;
    
public:
    
    FrameSource();
    
    FrameSource(
        CaptureRef source,
        MipGenerator& mip_gen,
        std::vector<uint8_t>& capture_buffer
    );
    
    ~FrameSource();
    
    bool capture();
    
private:
    
    wgpu::Texture _create_texture(wgpu::Device device);
};

} // namespace stereo
