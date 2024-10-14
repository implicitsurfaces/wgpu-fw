/**
 * This file is part of the "Learn WebGPU for C++" book.
 *   https://github.com/eliemichel/LearnWebGPU
 *
 * MIT License
 * Copyright (c) 2022-2023 Elie Michel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <stereo/defs.h>

#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <array>

#include "compute.h"

using namespace wgpu;

uint32_t max_mip_level(const Extent3D& textureSize) {
    return std::bit_width(std::max(textureSize.width, textureSize.height));
}

bool Application::init() {
    m_bufferSize = 64 * sizeof(float);
    if (!init_device()) return false;
    init_bind_group_layout();
    init_compute_pipeline();
    init_texture();
    init_texture_views();
    return true;
}

void Application::finish() {
    terminate_texture_views();
    terminate_texture();
    terminate_compute_pipeline();
    terminate_bind_group_layout();
    terminate_device();
}

bool Application::init_device() {
    // Create instance
    m_instance = createInstance(InstanceDescriptor{});
    if (!m_instance) {
        std::cerr << "Could not initialize WebGPU!" << std::endl;
        return false;
    }

    // Create surface and adapter
    std::cout << "Requesting adapter..." << std::endl;
    RequestAdapterOptions adapterOpts{};
    adapterOpts.compatibleSurface = nullptr;
    Adapter adapter = m_instance.requestAdapter(adapterOpts);
    std::cout << "Got adapter: " << adapter << std::endl;

    std::cout << "Requesting device..." << std::endl;
    SupportedLimits supportedLimits;
    adapter.getLimits(&supportedLimits);
    RequiredLimits requiredLimits = wgpu::Default;
    /*
    requiredLimits.limits.maxBindGroups = 2;
    requiredLimits.limits.maxUniformBuffersPerShaderStage = 2;
    requiredLimits.limits.maxUniformBufferBindingSize = 16 * 4 * sizeof(float);
    requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;
    requiredLimits.limits.maxBufferSize = m_bufferSize;
    requiredLimits.limits.maxTextureDimension1D = 4096;
    requiredLimits.limits.maxTextureDimension2D = 4096;
    requiredLimits.limits.maxTextureDimension3D = 4096;
    requiredLimits.limits.maxTextureArrayLayers = 1;
    requiredLimits.limits.maxSampledTexturesPerShaderStage = 3;
    requiredLimits.limits.maxSamplersPerShaderStage = 1;
    requiredLimits.limits.maxVertexBufferArrayStride = 68;
    requiredLimits.limits.maxInterStageShaderComponents = 17;
    requiredLimits.limits.maxStorageBuffersPerShaderStage = 2;
    requiredLimits.limits.maxComputeWorkgroupSizeX = 8;
    requiredLimits.limits.maxComputeWorkgroupSizeY = 8;
    requiredLimits.limits.maxComputeWorkgroupSizeZ = 1;
    requiredLimits.limits.maxComputeInvocationsPerWorkgroup = 64;
    requiredLimits.limits.maxComputeWorkgroupsPerDimension = 2;
    requiredLimits.limits.maxStorageBufferBindingSize = m_bufferSize;
    requiredLimits.limits.maxStorageTexturesPerShaderStage = 1;
    */

    // Create device
    DeviceDescriptor deviceDesc {};
    deviceDesc.label = "GPU compute device";
    deviceDesc.requiredFeatureCount = 0;
    deviceDesc.requiredLimits = &requiredLimits;
    deviceDesc.defaultQueue.label = "Default dispatch queue";
    m_device = adapter.requestDevice(deviceDesc);
    std::cout << "Got device: " << m_device << std::endl;

    // Add an error callback for more debug info
    // xxx: this also went away??
    // m_uncapturedErrorCallback = m_device.setUncapturedErrorCallback([](ErrorType type, char const* message) {
    //     std::cout << "Device error: type " << type;
    //     if (message) std::cout << " (message: " << message << ")";
    //     std::cout << std::endl;
    // });
    /*
    // xxx dunno why this method can't be found??
    m_deviceLostCallback = m_device.setDeviceLostCallback([](DeviceLostReason reason, char const* message) {
        std::cout << "Device lost: reason " << reason;
        if (message) std::cout << " (message: " << message << ")";
        std::cout << std::endl;
    });
    */

#ifdef WEBGPU_BACKEND_WGPU
    m_device.getQueue().submit(0, nullptr);
#else
    m_instance.processEvents();
#endif

    return true;
}

void Application::terminate_device() {
    wgpuDeviceRelease(m_device);
    wgpuInstanceRelease(m_instance);
}

void Application::init_texture() {
    // Load image data
    int width, height, channels;
    // xxx: load image
    uint8_t* pixelData = nullptr;
    if (nullptr == pixelData) throw std::runtime_error("Could not load input texture!");
    Extent3D textureSize = { (uint32_t)width, (uint32_t)height, 1 };

    // Create texture
    TextureDescriptor textureDesc;
    textureDesc.dimension       = TextureDimension::_2D;
    textureDesc.format          = TextureFormat::RGBA8Unorm;
    textureDesc.size            = textureSize;
    textureDesc.sampleCount     = 1;
    textureDesc.viewFormatCount = 0;
    textureDesc.viewFormats     = nullptr;

    textureDesc.usage = (
        TextureUsage::TextureBinding | // to read the texture in a shader
        TextureUsage::StorageBinding | // to write the texture in a shader
        TextureUsage::CopyDst | // to upload the input data
        TextureUsage::CopySrc   // to save the output data
    );

    textureDesc.mipLevelCount = max_mip_level(textureSize);
    m_textureMipSizes.resize(textureDesc.mipLevelCount);
    m_textureMipSizes[0] = textureSize;

    m_texture = m_device.createTexture(textureDesc);

    Queue queue = m_device.getQueue();

    // Upload texture data for MIP level 0 to the GPU
    ImageCopyTexture destination;
    destination.texture = m_texture;
    destination.origin = { 0, 0, 0 };
    destination.aspect = TextureAspect::All;
    destination.mipLevel = 0;
    TextureDataLayout source;
    source.offset = 0;
    source.bytesPerRow = 4 * textureSize.width;
    source.rowsPerImage = textureSize.height;
    queue.writeTexture(destination, pixelData, (size_t)(4 * width * height), source, textureSize);

#if !defined(WEBGPU_BACKEND_WGPU)
    wgpuQueueRelease(queue);
#endif
}

void Application::terminate_texture() {
    m_texture.destroy();
    wgpuTextureRelease(m_texture);
}

void Application::init_texture_views() {
    TextureViewDescriptor textureViewDesc;
    textureViewDesc.aspect          = TextureAspect::All;
    textureViewDesc.baseArrayLayer  = 0;
    textureViewDesc.arrayLayerCount = 1;
    textureViewDesc.dimension       = TextureViewDimension::_2D;
    textureViewDesc.format          = TextureFormat::RGBA8Unorm;

    // Each view must correspond to only 1 MIP level at a time
    textureViewDesc.mipLevelCount = 1;

    m_textureMipViews.reserve(m_textureMipSizes.size());
    for (uint32_t level = 0; level < m_textureMipSizes.size(); ++level) {
        std::string label = "MIP level #" + std::to_string(level);
        textureViewDesc.label = label.c_str();
        textureViewDesc.baseMipLevel = level;
        m_textureMipViews.push_back(m_texture.createView(textureViewDesc));

        if (level > 0) {
            Extent3D previousSize = m_textureMipSizes[level - 1];
            m_textureMipSizes[level] = {
                previousSize.width / 2,
                previousSize.height / 2,
                previousSize.depthOrArrayLayers / 2
            };
        }
    }
}

void Application::terminate_texture_views() {
    for (TextureView v : m_textureMipViews) {
        wgpuTextureViewRelease(v);
    }
    m_textureMipViews.clear();
    m_textureMipSizes.clear();
}

void Application::init_bind_group(uint32_t nextMipLevel) {
    // Create compute bind group
    std::vector<BindGroupEntry> entries {2, Default};

    // Input buffer
    entries[0].binding = 0;
    entries[0].textureView = m_textureMipViews[nextMipLevel - 1];

    // Output buffer
    entries[1].binding = 1;
    entries[1].textureView = m_textureMipViews[nextMipLevel];

    BindGroupDescriptor bindGroupDesc;
    bindGroupDesc.layout     = m_bindGroupLayout;
    bindGroupDesc.entryCount = (uint32_t) entries.size();
    bindGroupDesc.entries    = (WGPUBindGroupEntry*) entries.data();
    m_bindGroup = m_device.createBindGroup(bindGroupDesc);
}

void Application::terminate_bind_group() {
    wgpuBindGroupRelease(m_bindGroup);
}

void Application::init_bind_group_layout() {
    // Create bind group layout
    std::vector<BindGroupLayoutEntry> bindings {2, Default};

    // Input image: MIP level 0 of the texture
    bindings[0].binding               = 0;
    bindings[0].texture.sampleType    = TextureSampleType::Float;
    bindings[0].texture.viewDimension = TextureViewDimension::_2D;
    bindings[0].visibility            = ShaderStage::Compute;

    // Output image: MIP level 1 of the texture
    bindings[1].binding                      = 1;
    bindings[1].storageTexture.access        = StorageTextureAccess::WriteOnly;
    bindings[1].storageTexture.format        = TextureFormat::RGBA8Unorm;
    bindings[1].storageTexture.viewDimension = TextureViewDimension::_2D;
    bindings[1].visibility                   = ShaderStage::Compute;

    BindGroupLayoutDescriptor bindGroupLayoutDesc;
    bindGroupLayoutDesc.entryCount = (uint32_t) bindings.size();
    bindGroupLayoutDesc.entries    = bindings.data();

    m_bindGroupLayout = m_device.createBindGroupLayout(bindGroupLayoutDesc);
}

void Application::terminate_bind_group_layout() {
    wgpuBindGroupLayoutRelease(m_bindGroupLayout);
}

void Application::init_compute_pipeline() {
    // Load compute shader
    // xxx: load the shader source from file
    ShaderModule computeShaderModule; // = ...

    // Create compute pipeline layout
    PipelineLayoutDescriptor pipelineLayoutDesc;
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts     = (WGPUBindGroupLayout*) &m_bindGroupLayout;
    m_pipelineLayout = m_device.createPipelineLayout(pipelineLayoutDesc);

    // Create compute pipeline
    ComputePipelineDescriptor computePipelineDesc;
    computePipelineDesc.compute.constantCount = 0;
    computePipelineDesc.compute.constants     = nullptr;
    computePipelineDesc.compute.entryPoint    = "compute_mipmap";
    computePipelineDesc.compute.module        = computeShaderModule;
    computePipelineDesc.layout                = m_pipelineLayout;
    m_pipeline = m_device.createComputePipeline(computePipelineDesc);
}

void Application::terminate_compute_pipeline() {
    wgpuComputePipelineRelease(m_pipeline);
    wgpuPipelineLayoutRelease(m_pipelineLayout);
}

void Application::compute() {
    Queue queue = m_device.getQueue();

    // Initialize a command encoder
    CommandEncoderDescriptor encoderDesc = Default;
    CommandEncoder encoder = m_device.createCommandEncoder(encoderDesc);

    // Create compute pass
    ComputePassDescriptor computePassDesc;
    computePassDesc.timestampWrites = nullptr;
    ComputePassEncoder computePass  = encoder.beginComputePass(computePassDesc);

    computePass.setPipeline(m_pipeline);

    for (uint32_t nextLevel = 1; nextLevel < m_textureMipSizes.size(); ++nextLevel) {
        init_bind_group(nextLevel);
        computePass.setBindGroup(0, m_bindGroup, 0, nullptr);

        uint32_t invocationCountX = m_textureMipSizes[nextLevel].width;
        uint32_t invocationCountY = m_textureMipSizes[nextLevel].height;
        uint32_t workgroupSizePerDim = 8;
        // This ceils invocationCountX / workgroupSizePerDim
        uint32_t workgroupCountX = (invocationCountX + workgroupSizePerDim - 1) / workgroupSizePerDim;
        uint32_t workgroupCountY = (invocationCountY + workgroupSizePerDim - 1) / workgroupSizePerDim;
        computePass.dispatchWorkgroups(workgroupCountX, workgroupCountY, 1);

        terminate_bind_group();
    }

    // Finalize compute pass
    computePass.end();

    // Encode and submit the GPU commands
    CommandBuffer commands = encoder.finish(CommandBufferDescriptor{});
    queue.submit(commands);

#if !defined(WEBGPU_BACKEND_WGPU)
    wgpuCommandBufferRelease(commands);
    wgpuCommandEncoderRelease(encoder);
    wgpuComputePassEncoderRelease(computePass);
    wgpuQueueRelease(queue);
#endif

}

int main() {
    Application app;
	app.init();
	app.compute();
	app.finish();
	return 0;
}
