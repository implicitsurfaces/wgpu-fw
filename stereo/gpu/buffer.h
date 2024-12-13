#pragma once

#include "webgpu/webgpu.h"
#include <geomc/linalg/Matrix.h>
#include <stereo/defs.h>


namespace stereo {

enum struct BufferKind {
    Storage,
    Uniform,
    Vertex,
    Index,
};

template <typename T>
struct DataBuffer {
private:
    wgpu::Device _device = nullptr;
    wgpu::Buffer _buffer = nullptr;
    gpu_size_t     _size;

    void _release() {
        if (_buffer) _buffer.release();
        if (_device) _device.release();
    }

public:

    DataBuffer() = default;

    DataBuffer(
        wgpu::Device device,
        gpu_size_t   size,
        BufferKind   kind,
        WGPUBufferUsageFlags extra_flags=wgpu::BufferUsage::None):
            _device(device),
            _size(size)
    {
        wgpu::BufferDescriptor buffer_bd;
        buffer_bd.size = sizeof(T) * _size;
        switch (kind) {
            case BufferKind::Uniform: {
                extra_flags |= wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
            } break;
            case BufferKind::Storage: {
                extra_flags |= wgpu::BufferUsage::Storage;
            } break;
            case BufferKind::Vertex: {
                extra_flags |= wgpu::BufferUsage::Vertex;
            } break;
            case BufferKind::Index: {
                extra_flags |= wgpu::BufferUsage::Index;
            } break;
        }
        buffer_bd.usage = extra_flags;
        buffer_bd.mappedAtCreation = false;
        _buffer = _device.createBuffer(buffer_bd);
    }

    DataBuffer(
        wgpu::Device device,
        gpu_size_t size,
        WGPUBufferUsageFlags extra_usage_flags=wgpu::BufferUsage::None):
            DataBuffer(device, size, BufferKind::Storage, extra_usage_flags) {}

    DataBuffer(const DataBuffer& other):
        _device(other._device),
        _buffer(other._buffer),
        _size(other._size)
    {
        if (_device) _device.reference();
        if (_buffer) _buffer.reference();
    }

    DataBuffer(DataBuffer&& other):
        _device(other._device),
        _buffer(other._buffer),
        _size(other._size)
    {
        other._device = nullptr;
        other._buffer = nullptr;
        other._size = 0;
    }

    ~DataBuffer() {
        _release();
    }

    DataBuffer& operator=(const DataBuffer& other) {
        _release();
        _device = other._device;
        _buffer = other._buffer;
        _size   = other._size;
        if (_device) _device.reference();
        if (_buffer) _buffer.reference();
        return *this;
    }

    DataBuffer& operator=(DataBuffer&& other) {
        std::swap(_device, other._device);
        std::swap(_buffer, other._buffer);
        std::swap(_size,   other._size);
        return *this;
    }

    wgpu::Buffer buffer() const {
        return _buffer;
    }

    void submit_write(const T* data, range1i dst_range) {
        wgpu::Queue q = _device.getQueue();
        q.writeBuffer(
            _buffer,
            dst_range.lo * sizeof(T),
            data,
            dst_range.dimensions() * sizeof(T)
        );
        q.release();
    }

    void submit_write(const std::vector<T>& data) {
        submit_write(data.data(), range1i(0, data.size() - 1));
    }

    void submit_write(const T& data, gpu_size_t index) {
        submit_write(&data, range1i(index, index));
    }

    template <size_t N>
    void submit_write(const std::array<T,N>& data) {
        submit_write(data.data(), range1i(0, N - 1));
    }

    void copy_to(DataBuffer<T>& other, gpu_size_t dst_offset, range1i src_range=range1i::full) {
        range1i actual_range = src_range & range1i(0, _size - 1);
        wgpu::Queue q = _device.getQueue();
        wgpu::CommandEncoder encoder = _device.createCommandEncoder(wgpu::Default);
        encoder.copyBufferToBuffer(
            _buffer,
            actual_range.lo * sizeof(T),
            other._buffer,
            dst_offset * sizeof(T),
            actual_range.dimensions() * sizeof(T)
        );
        wgpu::CommandBuffer commands = encoder.finish(wgpu::Default);
        encoder.release();
        q.submit(commands);
        q.release();
    }

    gpu_size_t size() const {
        return _size;
    }

    gpu_size_t offset_of(gpu_size_t index) const {
        return index * sizeof(T);
    }
    
    operator bool() const {
        return _buffer != nullptr;
    }

};

}  // namespace stereo
