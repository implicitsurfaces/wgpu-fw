#include <stereo/gpu/frame_source.h>

namespace stereo {

FrameSource::FrameSource() {}

FrameSource::FrameSource(
    CaptureRef source,
    MipGenerator& mip_gen,
    std::vector<uint8_t>& capture_buffer):
        source(source),
        res(capture_res(source)),
        src_texture(_create_texture(mip_gen.get_device())),
        mip(mip_gen.create_mip_texture(src_texture)),
        _capture_buffer(capture_buffer) {}

wgpu::Texture FrameSource::_create_texture(wgpu::Device device) {
    const wgpu::TextureFormat format = wgpu::TextureFormat::BGRA8Unorm;
    // create the texture 
    wgpu::TextureDescriptor desc;
    desc.dimension       = wgpu::TextureDimension::_2D;
    desc.format          = format;
    desc.size            = {res.x, res.y, 1};
    desc.mipLevelCount   = std::bit_width(std::max(res.x, res.y));
    desc.sampleCount     = 1;
    desc.viewFormatCount = 0;
    desc.viewFormats     = nullptr;
    desc.usage           = 
        wgpu::TextureUsage::TextureBinding | // to read the texture from a shader
        wgpu::TextureUsage::StorageBinding | // to write the texture from a shader
        wgpu::TextureUsage::CopyDst;         // to upload the input data
    desc.label = "frame source texture";
    
    return device.createTexture(desc);
}

FrameSource::~FrameSource() {
    if (src_texture) {
        src_texture.destroy();
        src_texture.release();
    }
}

bool FrameSource::capture() {
    constexpr size_t n_channels = 4;
    vec2ui new_cap_dims = capture_res(source);
    if (new_cap_dims != res) {
        res = new_cap_dims;
        // xxx: re-specify the texture!!
        std::abort();
    }
    // wrap the backing buffer + fill it with capture data
    _capture_buffer.reserve(n_channels * res.x * res.y);
    cv::Mat frame {
        (int) new_cap_dims.y,
        (int) new_cap_dims.x,
        CV_8UC4,
        _capture_buffer.data()
    };
    bool did_read = source->read(frame);
    if (did_read) {
        wgpu::ImageCopyTexture dst_texture;
        dst_texture.texture  = src_texture;
        dst_texture.origin   = { 0, 0, 0 };
        dst_texture.aspect   = wgpu::TextureAspect::All;
        dst_texture.mipLevel = 0;
        
        wgpu::TextureDataLayout src_layout;
        src_layout.offset       = 0;
        src_layout.bytesPerRow  = n_channels * sizeof(uint8_t) * res.x;
        src_layout.rowsPerImage = res.y;
        
        // upload the new data to the texture
        wgpu::Queue queue = mip.device.getQueue();
        queue.writeTexture(
            dst_texture,
            _capture_buffer.data(),
            (size_t) n_channels * res.x * res.y,
            src_layout,
            {res.x, res.y, 1}
        );
        queue.release();
        // generate mipmaps from the new data
        mip.generate();
    }
    return did_read;
}

} // namespace stereo
