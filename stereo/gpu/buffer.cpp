#include <stereo/gpu/buffer.h>

namespace stereo {


void DataBuffer::_init();
void DataBuffer::_release();

DataBuffer::DataBuffer(wgpu::Device device, size_t size);
DataBuffer::DataBuffer(const Feature2DBuffer&);
DataBuffer::DataBuffer(Feature2DBuffer&&);
DataBuffer::~DataBuffer();
    
DataBuffer& DataBuffer::operator=(const Feature2DBuffer&);
DataBuffer& DataBuffer::operator=(Feature2DBuffer&&);
    
void DataBuffer::write(const std::vector<FeaturePair>& features);
wgpu::Buffer DataBuffer::buffer();
size_t size();

} // namespace stereo
