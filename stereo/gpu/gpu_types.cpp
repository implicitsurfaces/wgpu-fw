#include <geomc/linalg/AffineTransform.h>
#include <stereo/gpu/gpu_types.h>

namespace stereo {

vec3gpu::vec3gpu(const vec3& v): v(v) {}
vec3gpu::vec3gpu(float x, float y, float z): v(x, y, z) {}
vec3gpu::vec3gpu(const vec2& xy, float z): v(xy, z) {}

vec3gpu& vec3gpu::operator=(const vec3& other) {
    v = other;
    return *this;
}

float vec3gpu::operator[](gpu_size_t i) const {
    return v[i];
}

float& vec3gpu::operator[](gpu_size_t i) {
    return v[i];
}

vec3gpu::operator vec3() const { return v; }

vec3gpu vec3gpu::operator-() const {
    return -v;
}


// mat2gpu

mat2gpu::mat2gpu(): cols{vec2(1,0), vec2(0,1)} {}
mat2gpu::mat2gpu(const mat2& m) {
    for (int c = 0; c < 2; ++c) {
        for (int r = 0; r < 2; ++r) {
            cols[c][r] = m(r, c);
        }
    }
}

mat2gpu& mat2gpu::operator=(const mat2& other) {
    for (int c = 0; c < 2; ++c) {
        for (int r = 0; r < 2; ++r) {
            cols[c][r] = other(r, c);
        }
    }
    return *this;
}

mat2gpu::operator mat2() const {
    mat2 m;
    for (int c = 0; c < 2; ++c) {
        for (int r = 0; r < 2; ++r) {
            m(r, c) = cols[c][r];
        }
    }
    return m;
}


// mat3gpu

mat3gpu::mat3gpu(): cols{vec3(1,0,0), vec3(0,1,0), vec3(0,0,1)} {}

mat3gpu::mat3gpu(const mat3& m) {
    for (int c = 0; c < 3; ++c) {
        for (int r = 0; r < 3; ++r) {
            cols[c][r] = m(r, c);
        }
    }
}

mat3gpu& mat3gpu::operator=(const mat3& other) {
    for (int c = 0; c < 3; ++c) {
        for (int r = 0; r < 3; ++r) {
            cols[c][r] = other(r, c);
        }
    }
    return *this;
}

mat3gpu::operator mat3() const {
    mat3 m;
    for (int c = 0; c < 3; ++c) {
        for (int r = 0; r < 3; ++r) {
            m(r, c) = cols[c][r];
        }
    }
    return m;
}


// mat4gpu
    
mat4gpu::mat4gpu(): cols{vec4(1,0,0,0), vec4(0,1,0,0), vec4(0,0,1,0), vec4(0,0,0,1)} {}
mat4gpu::mat4gpu(const mat4& m) {
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            cols[c][r] = m(r, c);
        }
    }
}

mat4gpu& mat4gpu::operator=(const mat4& other) {
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            cols[c][r] = other(r, c);
        }
    }
    return *this;
}

mat4gpu::operator mat4() const {
    mat4 m;
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            m(r, c) = cols[c][r];
        }
    }
    return m;
}


xf3 translation(vec3gpu x) {
    return translation(x.v);
}

} // namespace stereo
