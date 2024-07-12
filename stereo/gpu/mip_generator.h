#include <stereo/defs.h>

namespace stereo {

// todo: this does not handle edges correctly yet

struct MipGenerator;

struct MipTexture {
    
    wgpu::Device                   device     = nullptr;
    wgpu::Texture                  texture    = nullptr;
    wgpu::BindGroup                bind_group = nullptr;
    MipGenerator*                  generator  = nullptr;
    std::vector<wgpu::TextureView> views;
    
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
    wgpu::ShaderModule _load_shader_module();
    
public:
    
    MipGenerator(wgpu::Device device);
    ~MipGenerator();
    
    MipTexture create_mip_texture(wgpu::Texture texture);
    wgpu::Device get_device();
};

} // namespace stereo
