#include <stereo/gpu/texture.h>

namespace stereo {

// todo: this does not handle edges correctly yet

struct MipGenerator;

struct MipTexture {
    
    Texture texture;
    std::vector<wgpu::BindGroup> bind_groups;
    MipGenerator* generator  = nullptr;
    
    friend struct MipGenerator;
    
private:
    
    MipTexture(wgpu::Texture texture, MipGenerator& gen);
    
    void _init();
    void _release();

public:
    
    MipTexture();
    MipTexture(const MipTexture&);
    MipTexture(MipTexture&&);
    ~MipTexture();
    
    MipTexture& operator=(const MipTexture&);
    MipTexture& operator=(MipTexture&&);
    
    void generate();
};

struct MipGenerator {
private:
    
    wgpu::Device          _device            = nullptr;
    wgpu::BindGroupLayout _bind_group_layout = nullptr;
    wgpu::ComputePipeline _pipeline          = nullptr; // this has the shader
    
    friend struct MipTexture;
    
    void _init();
    void _release();
    
public:
    
    MipGenerator(wgpu::Device device);
    ~MipGenerator();
    
    MipTexture create_mip_texture(wgpu::Texture texture);
    wgpu::Device get_device();
};

} // namespace stereo
