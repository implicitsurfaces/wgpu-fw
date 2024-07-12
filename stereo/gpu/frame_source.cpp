#include <stereo/gpu/frame_source.h>

namespace stereo {

FrameSource::FrameSource() {}

FrameSource::FrameSource(const FrameSource& other):
    source     (other.source),
    res        (other.res),
    src_texture(other.src_texture),
    mip        (other.mip)
{
    src_texture.reference();
}

FrameSource::FrameSource(FrameSource&& other):
    source     (std::move(other.source)),
    res        (std::move(other.res)),
    src_texture(std::move(other.src_texture)),
    mip        (std::move(other.mip))
{
    other.src_texture = nullptr;
}

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
    wgpu::TextureFormat format = wgpu::TextureFormat::BGRA8Unorm;
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

FrameSource& FrameSource::operator=(const FrameSource& other) {
    source = other.source;
    res    = other.res;
    wgpu::Texture other_tex = other.src_texture;
    other_tex.reference();
    src_texture.release();
    src_texture = other_tex;
    mip    = other.mip;
    return *this;
}

FrameSource& FrameSource::operator=(FrameSource&& other) {
    std::swap(source,      other.source);
    std::swap(res,         other.res);
    std::swap(src_texture, other.src_texture);
    std::swap(mip,         other.mip);
    return *this;
}

static bool _debug_image(vec2ui res, std::vector<uint8_t>& data) {
    const int n_channels = 4;
    data.resize(res.x * res.y * n_channels);
    for (int y = 0; y < res.y; ++y) {
        for (int x = 0; x < res.x; ++x) {
            int    px = y * res.x + x;
            int     b = px * n_channels;
            float   s = x / float(res.x);
            float   t = y / float(res.y);
            float   m = std::sin(50. * s * M_PI) * std::cos(25. * t * M_PI);
            uint8_t v = 255 * (0.5 + 0.5 * m);
            data[b + 0] = v;     // b
            data[b + 1] = v / 2; // g
            data[b + 2] = v / 4; // r
            data[b + 3] = 255;   // a
        }
    }
    return true;
}

bool FrameSource::capture() {
    vec2ui new_cap_dims = capture_res(source);
    if (new_cap_dims != res) {
        res = new_cap_dims;
        // xxx: re-specify the texture!!
        std::abort();
    }
    // wrap the backing buffer + fill it with capture data
    size_t pixels = res.x * res.y;
    if (pixels == 0) return false;
    _capture_buffer.resize(4 * pixels);
    cv::Mat frame {
        (int) res.y,
        (int) res.x,
        CV_8UC3,
        _capture_buffer.data()
    };
    bool did_read = source->read(frame);
    // the capture data is three channel. but webgpu only supports four channel.
    // repack the data.
    for (int64_t i = pixels - 1; i >= 0; --i) {
        size_t src = 3 * i;
        size_t dst = 4 * i;
        _capture_buffer[dst + 0] = _capture_buffer[src + 0];
        _capture_buffer[dst + 1] = _capture_buffer[src + 1];
        _capture_buffer[dst + 2] = _capture_buffer[src + 2];
        _capture_buffer[dst + 3] = 0xFF;
    }
    // bool did_read = _debug_image(res, _capture_buffer);
    if (did_read) {
        wgpu::ImageCopyTexture dst_texture;
        dst_texture.texture  = src_texture;
        dst_texture.origin   = { 0, 0, 0 };
        dst_texture.aspect   = wgpu::TextureAspect::All;
        dst_texture.mipLevel = 0;
        
        wgpu::TextureDataLayout src_layout;
        src_layout.offset       = 0;
        src_layout.bytesPerRow  = 4 * sizeof(uint8_t) * res.x;
        src_layout.rowsPerImage = res.y;
        
        // upload the new data to the texture
        wgpu::Queue queue = mip.device.getQueue();
        queue.writeTexture(
            dst_texture,
            _capture_buffer.data(),
            4 * pixels,
            src_layout,
            {res.x, res.y, 1}
        );
        queue.release();
        // generate mipmaps from the new data
        mip.generate();
    } else {
        std::cerr << "NO READ :(" << std::endl;
    }
    return did_read;
}

} // namespace stereo
