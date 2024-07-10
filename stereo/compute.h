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

#pragma once

#include <webgpu/webgpu.hpp>

class Application {
public:
	// A function called only once at the beginning. Returns false is init failed.
	bool init();
    
	// Where the GPU computation is actually issued
	void compute();
    
	// A function called only once at the very end.
	void finish();
    
private:
    
	// Detailed steps
	bool init_device();
	void terminate_device();
    
	void init_texture();
	void terminate_texture();
    
	void init_texture_views();
	void terminate_texture_views();
    
    // attach the texture views to current + next compute variables
	void init_bind_group(uint32_t nextMipLevel);
	void terminate_bind_group();
    
    // define the properties + access of each of the compute variables
	void init_bind_group_layout();
	void terminate_bind_group_layout();
    
	void init_compute_pipeline();
	void terminate_compute_pipeline();
    
private:
	uint32_t m_bufferSize;
	// Everything that is initialized in `onInit` and needed in `onCompute`.
	wgpu::Instance        m_instance        = nullptr;
	wgpu::Device          m_device          = nullptr;
    // holds bind groups:
	wgpu::PipelineLayout  m_pipelineLayout  = nullptr;
    // knows the shader and entry point. like a shader that might
    // run one of many on an active draw surface
	wgpu::ComputePipeline m_pipeline        = nullptr;
    
	wgpu::Texture         m_texture         = nullptr;
    
	wgpu::BindGroup       m_bindGroup       = nullptr;
	wgpu::BindGroupLayout m_bindGroupLayout = nullptr;
    
	std::vector<wgpu::TextureView>            m_textureMipViews;
    // needed to figure out dispatch sizes:
	std::vector<wgpu::Extent3D>               m_textureMipSizes;
	std::unique_ptr<wgpu::ErrorCallback>      m_uncapturedErrorCallback;
	std::unique_ptr<wgpu::DeviceLostCallback> m_deviceLostCallback;
};