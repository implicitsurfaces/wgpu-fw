#pragma once

#include <stereo/defs.h>

namespace stereo {

// bare vertex format

template <typename T>
struct VertexFormat {};

template <>
struct VertexFormat<vec2> {
    static constexpr WGPUVertexFormat format = WGPUVertexFormat_Float32x2;
};

template <>
struct VertexFormat<vec3> {
    static constexpr WGPUVertexFormat format = WGPUVertexFormat_Float32x3;
};

template <>
struct VertexFormat<vec4> {
    static constexpr WGPUVertexFormat format = WGPUVertexFormat_Float32x4;
};

template <>
struct VertexFormat<float> {
    static constexpr WGPUVertexFormat format = WGPUVertexFormat_Float32;
};

template <>
struct VertexFormat<uint32_t> {
    static constexpr WGPUVertexFormat format = WGPUVertexFormat_Uint32;
};

template <>
struct VertexFormat<int32_t> {
    static constexpr WGPUVertexFormat format = WGPUVertexFormat_Sint32;
};

template <>
struct VertexFormat<Vec<uint32_t,2>> {
    static constexpr WGPUVertexFormat format = WGPUVertexFormat_Uint32x2;
};

template <>
struct VertexFormat<Vec<int32_t,2>> {
    static constexpr WGPUVertexFormat format = WGPUVertexFormat_Sint32x2;
};

// unorm vertex formats

template <typename T>
struct UnormVertexFormat {};

template <>
struct UnormVertexFormat<Vec<uint8_t,2>> {
    static constexpr WGPUVertexFormat format = WGPUVertexFormat_Unorm8x2;
};

template <>
struct UnormVertexFormat<Vec<uint8_t,4>> {
    static constexpr WGPUVertexFormat format = WGPUVertexFormat_Unorm8x4;
};

// snorm vertex formats

template <typename T>
struct SnormVertexFormat {};

template <>
struct SnormVertexFormat<Vec<int8_t,2>> {
    static constexpr WGPUVertexFormat format = WGPUVertexFormat_Snorm8x2;
};

template <>
struct SnormVertexFormat<Vec<int8_t,4>> {
    static constexpr WGPUVertexFormat format = WGPUVertexFormat_Snorm8x4;
};

template <typename T, typename U>
constexpr size_t offset_of(U T::* member_ptr) {
    T t;
    return reinterpret_cast<size_t>(&(t.*member_ptr)) - reinterpret_cast<size_t>(&t);
}

/// vertex attribute helper.
/// usage: attribute(0, Class::name_of_member);
template <typename Field, typename C>
requires (std::is_class_v<C>)
wgpu::VertexAttribute vertex_attribute(gpu_size_t location, Field C::* member_ptr) {
    wgpu::VertexAttribute attr = wgpu::Default;
    attr.shaderLocation = location;
    attr.offset = offset_of(member_ptr);
    attr.format = VertexFormat<std::remove_cvref_t<Field>>::format;
    return attr;
}

}  // namespace stereo
