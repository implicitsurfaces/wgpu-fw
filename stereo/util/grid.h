#pragma once

#include <memory>

#include <stereo/defs.h>

namespace stereo {

template <typename T>
struct Grid2d {
    std::shared_ptr<T[]> buf;
    vec2ul dims;
    
    Grid2d() {}
    
    Grid2d(vec2ul dims):
        buf(new T[dims.x * dims.y]),
        dims(dims) {}
    
    inline T& operator()(const vec2ul& cell) {
        return buf[index(cell.x, cell.y)];
    }
    
    inline T& operator()(unsigned int x, unsigned int y) {
        return buf[index(x,y)];
    }
    
    inline T operator()(const vec2ul& cell) const {
        return buf[index(cell.x, cell.y)];
    }
    
    inline T operator()(unsigned int x, unsigned int y) const {
        return buf[index(x,y)];
    }
    
    constexpr index_t index(unsigned int x, unsigned int y) const {
        return y * dims.x + x;
    }
    
    void fill(T value) {
        std::fill(buf.get(), buf.get() + dims.x * dims.y, value);
    }
    
    inline bool contains(const vec2ul& p) const {
        return p.x < dims.x and p.y < dims.y;
    }
    
    Grid2d<T> clone() const {
        Grid2d<T> out {dims};
        std::copy(buf.get(), buf.get() + dims.x * dims.y, out.buf.get());
        return out;
    }
    
    const T* data() const {
        return buf.get();
    }
};

template <typename T>
struct Grid3d {
    std::shared_ptr<T[]> buf;
    vec3ui dims;
    
    Grid3d() {}
    
    Grid3d(vec3ui dims):
        buf(new T[dims.x * dims.y * dims.z]),
        dims(dims) {}
    
    inline T& operator()(const vec3ui& cell) {
        return buf[index(cell.x, cell.y, cell.z)];
    }
    
    inline T& operator()(unsigned int x, unsigned int y, unsigned int z) {
        return buf[index(x,y,z)];
    }
    
    inline T operator()(const vec3ui& cell) const {
        return buf[index(cell.x, cell.y, cell.z)];
    }
    
    inline T operator()(unsigned int x, unsigned int y, unsigned int z) const {
        return buf[index(x,y,z)];
    }
    
    constexpr index_t index(unsigned int x, unsigned int y, unsigned int z) const {
        return z * dims.x * dims.y + y * dims.x + x;
    }
    
    void fill(T value) {
        std::fill(buf.get(), buf.get() + dims.x * dims.y * dims.z, value);
    }
    
    inline bool contains(const vec3ui& p) const {
        return p.x < dims.x and p.y < dims.y and p.z < dims.z;
    }
    
    Grid3d<T> clone() const {
        Grid3d<T> out {dims};
        std::copy(buf.get(), buf.get() + dims.x * dims.y * dims.z, out.buf.get());
        return out;
    }
    
    const T* data() const {
        return buf.get();
    }
};

} // namespace stereo
