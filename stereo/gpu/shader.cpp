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
    
	wgpu::ShaderModuleDescriptor shader_module {};
	shader_module.nextInChain = &shader_code.chain;
	shader_module.hintCount = 0;
	shader_module.hints = nullptr;
    shader_module.label = label;
	return device.createShaderModule(shader_module);
}

}  // namespace stereo
