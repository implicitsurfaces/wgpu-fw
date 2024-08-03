#pragma once

#include <geomc/linalg/Matrix.h>
#include <stereo/defs.h>


namespace stereo {

template <typename T>
struct DataBuffer {
private:
    wgpu::Device _device = nullptr;
    wgpu::Buffer _buffer = nullptr;
    size_t       _size;
    
    void _init(wgpu::BufferUsage extra_flags) {
        wgpu::BufferDescriptor buffer_bd;
        buffer_bd.size = sizeof(T) * _size;
        buffer_bd.usage = wgpu::BufferUsage::Storage | extra_flags;
        buffer_bd.mappedAtCreation = false;
        _buffer = _device.createBuffer(buffer_bd);
    }
    
    void _release() {
        if (_buffer) _buffer.release();
        if (_device) _device.release();
    }
    
public:
    
    DataBuffer(
        wgpu::Device device,
        size_t size,
        wgpu::BufferUsage extra_usage_flags=wgpu::BufferUsage::None):
            _device(device),
            _size(size)
    {
        _init(extra_usage_flags);
    }
    
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
    
    void submit_write(const T* data, range1i range) {
        wgpu::Queue q = _device.getQueue();
        q.writeBuffer(_buffer, range.lo * sizeof(T), data, range.dimensions() * sizeof(T));
        q.release();
    }
    
    size_t size() const {
        return _size;
    }
    
};

}  // namespace stereo
