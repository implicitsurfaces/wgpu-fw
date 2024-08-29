#pragma once

#include <geomc/linalg/Matrix.h>
#include <geomc/linalg/Vec.h>
#include <stereo/type_defs.h>

namespace stereo {

// aligned types suitable for memory sharing with the GPU.
// alignment specs are here: https://www.w3.org/TR/WGSL/#alignment-and-size
// because we can't use the `alignas()` directive on a typedef, we commit
// the sin of using `#define` to specify these types.
// todo: these should probably be proper wrapper classes, as below
#define vec2gpu alignas(8)  Vec<float,2>
#define vec4gpu alignas(16) Vec<float,4>
#define quatgpu alignas(16) Quat<float>


// vec3s have 4 bytes of padding due to alignment
struct alignas(16) vec3gpu {
    vec3 v;
    
    vec3gpu() = default;
    vec3gpu(const vec3& v);
    vec3gpu(float x, float y, float z);
    vec3gpu(const vec2& xy, float z);
    
    vec3gpu& operator=(const vec3& other);
    
    float operator[](gpu_size_t i) const;
    float& operator[](gpu_size_t i);
    
    vec3gpu operator-() const;
    
    operator vec3() const;
};


// matrices are transposed (column major) relative to geomc's matrices (row major)

struct alignas(8) mat2gpu {
    vec2gpu cols[2];
    
    mat2gpu();
    mat2gpu(const mat2& m);
    
    mat2gpu& operator=(const mat2& other);
    operator mat2() const;
};


struct alignas(16) mat3gpu {
    // each column is a vec3gpu, which has the padding bytes!
    vec3gpu cols[3];
    
    mat3gpu();
    mat3gpu(const mat3& m);
    
    mat3gpu& operator=(const mat3& other);
    operator mat3() const;
};


struct alignas(16) mat4gpu {
    vec4gpu cols[4];
    
    mat4gpu();
    mat4gpu(const mat4& m);
    
    mat4gpu& operator=(const mat4& other);
    operator mat4() const;
};

xf3 translation(vec3gpu x);

}  // namespace stereo
