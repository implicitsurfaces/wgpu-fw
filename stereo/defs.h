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

#include <geomc/function/Utils.h>
#include <geomc/Hash.h>

#include <webgpu/webgpu.hpp>
#include <stereo/gpu/gpu_types.h>

#define MIN_UNIFORM_PADDING_BYTES 256

namespace stereo {

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

template <typename... Args>
void release_all(Args&... items) {
    auto release = [](auto& item) { if (item) item.release(); };
    (release(items), ...);
}

} // namespace stereo
