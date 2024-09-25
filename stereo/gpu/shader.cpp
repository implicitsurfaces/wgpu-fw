#include <stereo/gpu/shader.h>

namespace stereo {

/**
 * Load the contents of a file located at `fpath` and return the contents as an array of char.
 * The returned array will be `nullptr` if the read was unsuccessful.
 */
static std::string read_file(const char* fpath) {
    // attempt to open the requested file
    auto f = fopen(fpath, "rb");
    if (f == nullptr) {
        std::cerr << "Could not load file `" << fpath << "` : ";
        std::cerr << std::strerror(errno) << "\n";
        return nullptr;
    }
    // compute how large the file is by seeking to the end
    fseek(f, 0, SEEK_END);
    auto n_bytes = ftell(f);
    rewind(f);
    
    std::string s;
    s.resize(n_bytes + 1);
    size_t n_read = fread(s.data(), 1, n_bytes, f);
    if (n_read != n_bytes) {
        std::cerr << "Incomplete file read (" << n_read << "/" << n_bytes << "): ";
        std::cerr << std::strerror(errno) << "\n";
        s.resize(0);
    }
    s[n_bytes] = '\0'; // mandatory null termination
    return s;
}

wgpu::ShaderModule shader_from_file(wgpu::Device device, const char* fpath) {
    std::string src = read_file(fpath);
    if (src.empty()) {
        std::cerr << "empty shader file" << std::endl;
        return nullptr;
    }
    return shader_from_str(device, src.c_str(), fpath);
}

wgpu::ShaderModule shader_from_str(
        wgpu::Device device,
        const char* source,
        const char* label)
{
    wgpu::ShaderModuleWGSLDescriptor shader_code {};
	shader_code.chain.next = nullptr;
	shader_code.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
	shader_code.code = source;
    
	wgpu::ShaderModuleDescriptor module_desc {};
	module_desc.nextInChain = &shader_code.chain;
	module_desc.hintCount = 0;
	module_desc.hints = nullptr;
    module_desc.label = label;
	wgpu::ShaderModule shader_module = device.createShaderModule(module_desc);
    
    if (not shader_module) {
        std::cerr << "Error creating shader module" << std::endl;
        std::abort();
    }
    
    return shader_module;
}

wgpu::ComputePipeline create_pipeline(
        wgpu::Device          device,
        wgpu::ShaderModule    shader,
        std::initializer_list<wgpu::BindGroupLayout> layouts,
        const char*           label,
        std::initializer_list<wgpu::ConstantEntry> constants)
{
    wgpu::PipelineLayoutDescriptor pld;
    pld.bindGroupLayoutCount = layouts.size();
    pld.bindGroupLayouts     = (WGPUBindGroupLayout*) layouts.begin();
    wgpu::PipelineLayout pl  = device.createPipelineLayout(pld);
    
    wgpu::ComputePipelineDescriptor cpd;
    cpd.compute.entryPoint    = "main";
    cpd.compute.module        = shader;
    cpd.compute.constantCount = 0;
    cpd.layout                = pl;
    cpd.label                 = label;
    
    if (constants.size() > 0) {
        cpd.compute.constantCount = constants.size();
        cpd.compute.constants     = constants.begin();
    }
    return device.createComputePipeline(cpd);
}

wgpu::BindGroupLayoutEntry sampler_layout(gpu_size_t binding) {
    wgpu::BindGroupLayoutEntry entry = wgpu::Default;
    entry.binding      = binding;
    entry.visibility   = wgpu::ShaderStage::Compute;
    entry.sampler.type = wgpu::SamplerBindingType::Filtering;
    return entry;
}

wgpu::BindGroupLayoutEntry texture_layout(gpu_size_t binding) {
    wgpu::BindGroupLayoutEntry entry = wgpu::Default;
    entry.binding               = binding;
    entry.visibility            = wgpu::ShaderStage::Compute;
    entry.texture.sampleType    = wgpu::TextureSampleType::Float;
    entry.texture.viewDimension = wgpu::TextureViewDimension::_2D;
    return entry;
}

wgpu::BindGroupEntry sampler_entry(gpu_size_t binding, wgpu::Sampler sampler) {
    wgpu::BindGroupEntry entry = wgpu::Default;
    entry.binding = binding;
    entry.sampler = sampler;
    return entry;
}

}  // namespace stereo
