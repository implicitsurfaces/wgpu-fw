#pragma once

// use webgpu backend:
#include <stereo/backend_def.h>

#include <cstddef>
#include <limits>
#include <type_traits>
#include <functional>
#include <ranges>
#include <optional>

#include <ankerl/unordered_dense.h>

#include <geomc/linalg/LinalgTypes.h>
#include <geomc/shape/ShapeTypes.h>
#include <geomc/function/FunctionTypes.h>
#include <geomc/Hash.h>

#include <geomc/linalg/Vec.h>

#include <webgpu/webgpu.hpp>

using namespace geom;


namespace stereo {

using uint128_t = __uint128_t;

using vec2        = Vec<float,2>;
using vec3        = Vec<float,3>;
using vec4        = Vec<float,4>;
using vec2d       = Vec<double,2>;
using vec3d       = Vec<double,3>;
using vec4d       = Vec<double,4>;
using vec2i       = Vec<int,2>;
using vec3i       = Vec<int,3>;
using vec2ui      = Vec<unsigned int,2>;
using vec3ui      = Vec<unsigned int,3>;
using vec2ub      = Vec<uint8_t,2>;
using vec3ub      = Vec<uint8_t,3>;
using vec4ub      = Vec<uint8_t,4>;
using range       = Rect<float,1>;
using range2      = Rect<float,2>;
using range3      = Rect<float,3>;
using ranged      = Rect<double,1>;
using range2d     = Rect<double,2>;
using range3d     = Rect<double,3>;
using range1i     = Rect<int,1>;
using range2i     = Rect<int,2>;
using range3i     = Rect<int,3>;
using range1ui    = Rect<unsigned int,1>;
using range2ui    = Rect<unsigned int,2>;
using range3ui    = Rect<unsigned int,3>;
using box2        = OrientedRect<float,2>;
using box3        = OrientedRect<float,3>;
using box2d       = OrientedRect<double,2>;
using box3d       = OrientedRect<double,3>;
using quat        = Quat<float>;
using quatd       = Quat<double>;
using xf2         = AffineTransform<float,2>;
using xf3         = AffineTransform<float,3>;
using xf2d        = AffineTransform<double,2>;
using xf3d        = AffineTransform<double,3>;
using mat2        = SimpleMatrix<float,2,2>;
using mat3        = SimpleMatrix<float,3,3>;
using mat4        = SimpleMatrix<float,4,4>;
using mat2d       = SimpleMatrix<double,2,2>;
using mat3d       = SimpleMatrix<double,3,3>;
using mat4d       = SimpleMatrix<double,4,4>;
using ray2        = Ray<float,2>;
using ray3        = Ray<float,3>;
using ray2d       = Ray<double,2>;
using ray3d       = Ray<double,3>;
using circle2     = Circle<float>;
using sphere3     = Sphere<float,3>;
using circle2d    = Circle<double>;
using sphere3d    = Sphere<double,3>;
using seg2        = Cylinder<float,2>;
using seg2d       = Cylinder<double,2>;
using cyl3        = Cylinder<float,3>;
using cyl3d       = Cylinder<double,3>;
using capsule2    = Capsule<float,2>;
using capsule3    = Capsule<float,3>;
using capsule2d   = Capsule<double,2>;
using capsule3d   = Capsule<double,3>;
using roundrect2  = Dilated<Rect<float,2>>;
using roundrect3  = Dilated<Rect<float,3>>;
using roundrect2d = Dilated<Rect<double,2>>;
using roundrect3d = Dilated<Rect<double,3>>;
using frustum3    = Frustum<range2>;
using tri         = Simplex<float,2>;
using trid        = Simplex<double,2>;
using tet         = Simplex<float,3>;
using tetd        = Simplex<double,3>;


template <typename K, typename V>
using DenseMap = ankerl::unordered_dense::map<K,V>;

template <typename K, typename V>
using Multimap = std::unordered_multimap<K,V>;

template <typename T>
using DenseSet = ankerl::unordered_dense::set<T>;

template <typename K, typename V>
V get_or(const DenseMap<K,V>& m, const K& k, const V& v) {
    auto i = m.find(k);
    return i == m.end() ? v : i->second;
}

template <typename K, typename V>
std::optional<V> get_or(const DenseMap<K,V>& m, const K& k) {
    auto i = m.find(k);
    if (i == m.end()) {
        return std::nullopt;
    } else {
        return i->second;
    }
}

constexpr inline uint128_t u128(uint64_t hi, uint64_t lo) {
    return (uint128_t(hi) << 64) | lo;
}

} // namespace stereo
